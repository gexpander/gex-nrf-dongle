//
// Created by MightyPork on 2018/03/31.
//

#include <string.h>
#include "debug.h"
#include "msg_queue.h"
#include "gex_gateway.h"
#include "main.h"
#include "minmax.h"
#include "payload_parser.h"
#include "nrf.h"
#include "crc32.h"
#include "payload_builder.h"

enum MsgTypes {
    MSG_TYPE_NETWORK_ID = 1,
    MSG_TYPE_DATA = 2,
};

struct msg_network_id {
    uint8_t msg_type;
    uint8_t bytes[4];
};

struct msg_data {
    uint8_t msg_type;
    uint8_t dev_addr;
    uint8_t length;
    uint8_t data[32];
};

static uint8_t gex_network[4];

// USB RX
enum USB_RX_STATE {
    CMD_STATE_IDLE = 0,
    CMD_STATE_TXMSG = 1,
};

#define MAX_FRAME_LEN 512

static enum USB_RX_STATE cmd_state = CMD_STATE_IDLE;

static uint32_t txmsg_len = 0; // total len to be collected
static uint32_t txmsg_collected = 0; // already collected len
static uint8_t  txmsg_payload[MAX_FRAME_LEN]; // equal buffer size in GEX
static uint8_t txmsg_addr = 0;
static uint8_t txmsg_cksum = 0;

#define MAGIC_GW_COMMAND 0x47U // 'G'

enum GW_CMD {
    CMD_GET_ID = 'i', // 105 - get network ID
    CMD_RESET = 'r', // reset the radio and network
    CMD_ADD_NODES = 'n', // add a node by address byte
    CMD_TXMSG = 'm', // 109 - send a message
};

void respond_gw_id(void);

void handle_cmd_addnodes(PayloadParser *pp);

void handle_cmd_reset(void);

void start_slave_cmd(uint8_t slave_addr, uint16_t frame_len, uint8_t cksum)
{
    txmsg_len = frame_len;
    txmsg_collected = 0;
    txmsg_addr = slave_addr;
    txmsg_cksum = cksum;
}

void handle_txframe_chunk(const uint8_t *buffer, uint16_t size)
{
    uint32_t wanted = MIN(txmsg_len - txmsg_collected, size);
    memcpy(&txmsg_payload[txmsg_collected], buffer, wanted);
    txmsg_collected += wanted;

    if (wanted < size) {
        // this was the end - simple checksum to verify it's a valid frame
        uint8_t ck = 0;
        for (int i = 0; i < txmsg_len; i++) {
            ck ^= txmsg_payload[i];
        }
        ck = ~ck;

        if (ck != txmsg_cksum) {
            dbg("Checksum mismatch!");
        }
        else {
            dbg("Verified, sending a %d B frame to slave.", (int) txmsg_len);

            uint8_t pipe = NRF_Addr2PipeNum(txmsg_addr);
            if (pipe == 0xFF) {
                dbg("Bad slave num!");
            } else {
                uint32_t remain = txmsg_len;
                for (int i = 0; i <= txmsg_len/32; i++) {
                    uint8_t chunk = (uint8_t) MIN(remain, 32);
                    bool suc = NRF_SendPacket(pipe, &txmsg_payload[i*32], chunk);
                    remain -= chunk;

                    if (!suc) {
                        dbg("Sending failed."); // (even with retransmission)
                        break; // skip rest of the frame
                    }
                }
            }
        }

        cmd_state = CMD_STATE_IDLE;
    }
}

void gw_handle_usb_out(uint8_t *buffer)
{
    if (cmd_state == CMD_STATE_IDLE) {
        PayloadParser pp = pp_start(buffer, 64, NULL);

        // handle binary commands for the gateway

        // magic twice, one inverted - denotes a gateway command
        const uint16_t magic1 = pp_u8(&pp);
        const uint16_t magic2 = pp_u8(&pp);
        if (magic1 == MAGIC_GW_COMMAND && magic2 == (0xFFU & (~MAGIC_GW_COMMAND))) {
            // third byte is the command code
            switch (pp_u8(&pp)) {
                case CMD_GET_ID:
                    respond_gw_id();
                    break;

                case CMD_RESET:
                    handle_cmd_reset();
                    break;

                case CMD_ADD_NODES:
                    // payload is: u8-count, u8[] node addresses
                    handle_cmd_addnodes(&pp);
                    break;

                case CMD_TXMSG:;
                    // u8-slave-addr, u16-len, u8-checksum
                    // the message is sent in the following frames.
                    uint8_t slave_addr = pp_u8(&pp);
                    uint16_t frame_len = pp_u16(&pp);
                    uint8_t cksum = pp_u8(&pp);

                    if (frame_len == 0 || frame_len > MAX_FRAME_LEN) {
                        dbg("Frame too big!");
                        break;
                    }

                    start_slave_cmd(slave_addr, frame_len, cksum);
                    dbg("Collecting frame for slave %02x: %d bytes", (int)slave_addr, (int)frame_len);
                    cmd_state = CMD_STATE_TXMSG;

                    // handle the rest as payload
                    uint32_t len;
                    const uint8_t *tail = pp_tail(&pp, &len);
                    handle_txframe_chunk(tail, (uint16_t) len);
                    break;

                default:
                    dbg("Bad cmd");
            }
        } else {
            // Bad frame??
            dbg("Bad USB frame, starts %x,%x", buffer[0],buffer[1]);
        }
    }
    else if (cmd_state == CMD_STATE_TXMSG) {
        handle_txframe_chunk(buffer, 64);
    }
}

void handle_cmd_reset(void)
{
    NRF_ResetPipes();
}

void handle_cmd_addnodes(PayloadParser *pp)
{
    uint8_t count = pp_u8(pp);
    for(int i = 0; i < count; i++) {
        uint8_t node = pp_u8(pp);
        uint8_t pipenum;
        bool suc = NRF_AddPipe(node, &pipenum);
        if (!suc) {
            dbg("Failed to add node.");
        } else {
            dbg("Bound node %02x to pipe %d", node, pipenum);
        }
    }
}

void respond_gw_id(void)
{
    dbg("> respond_gw_id");
    struct msg_network_id m = {
        .msg_type = MSG_TYPE_NETWORK_ID,
        .bytes = {
            gex_network[0],
            gex_network[1],
            gex_network[2],
            gex_network[3],
        }
    };

    bool suc = mq_post(&usb_inq, (uint8_t *) &m, sizeof(m));
    if (!suc) {
        dbg("IN que overflow!!");
    }
}

/**
 * Compute the GEX network ID based on the gateway MCU unique ID.
 * The ID is a 4-byte code that must be entered in each node to join the network.
 */
static void compute_network_id(void)
{
    uint8_t uid[12];
    PayloadBuilder pb = pb_start(uid, 12, NULL);
    pb_u32(&pb, LL_GetUID_Word0());
    pb_u32(&pb, LL_GetUID_Word1());
    pb_u32(&pb, LL_GetUID_Word2());

    uint32_t ck = CRC32_Start();
    for (int i = 0; i < 12; i++) {
        ck = CRC32_Add(ck, uid[i]);
    }
    ck = CRC32_End(ck);

    pb = pb_start(gex_network, 4, NULL);
    pb_u32(&pb, ck);

    dbg("Dongle network ID: %02X-%02X-%02X-%02X",
        gex_network[0], gex_network[1], gex_network[2], gex_network[3]);
}

void gw_setup_radio(void)
{
    bool suc;
    dbg("Init NRF");

    NRF_Init(NRF_SPEED_2M);

    compute_network_id();
    NRF_SetBaseAddress(gex_network);

    // TODO by config
    uint8_t pipenum;
    suc = NRF_AddPipe(0x01, &pipenum);
    dbg("Pipe added? %d, num %d", (int)suc, (int)pipenum);

    NRF_ModeRX(); // base state is RX

//    dbg("Send a packet");
//
//    suc = NRF_SendPacket(pipenum, (uint8_t *) "AHOJ", 5);
//    dbg("Suc? %d", (int)suc);
}

void EXTI2_IRQHandler(void)
{
    struct msg_data m;
    m.msg_type = MSG_TYPE_DATA;

    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_2);

    uint8_t pipenum;
    m.length = NRF_ReceivePacket(m.data, &pipenum);
    if (m.length == 0) {
        dbg("IRQ but no msg!");
    }
    else {
        dbg("Msg RXd from nordic!");

        m.dev_addr = NRF_PipeNum2Addr(pipenum);

        bool suc = mq_post(&usb_inq, (uint8_t *) &m, sizeof(m));
        if (!suc) {
            dbg("IN que overflow!!");
        }
    }
}
