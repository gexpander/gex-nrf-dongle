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

// USB RX
enum URX_STATE {
    URXS_IDLE = 0,
    URXS_MSG4SLAVE = 1,
};

#define MAX_FRAME_LEN 512

static enum URX_STATE urx_state = URXS_IDLE;
static uint32_t msg4slave_len = 0; // total len to be collected
static uint32_t msg4slave_already = 0; // already collected len
static uint8_t  msg4slave[MAX_FRAME_LEN]; // equal buffer size in GEX
static uint8_t msg4slave_addr = 0;
static uint8_t msg4slave_cksum = 0;

#define MAGIC_GW_COMMAND 0x47U // 'G'

enum GW_CMD {
    CMD_GET_ID = 'i', // 105
    CMD_MSG4SLAVE = 'm', // 109
};

void respond_gw_id() {} // TODO impl

void start_slave_cmd(uint8_t slave_addr, uint16_t frame_len, uint8_t cksum)
{
    msg4slave_len = frame_len;
    msg4slave_already = 0;
    msg4slave_addr = slave_addr;
    msg4slave_cksum = cksum;
}

/** called from the main loop, periodically */
void gw_process(void)
{
    static uint8_t buffer[MQ_SLOT_LEN];
    while (mq_read(&usb_rxq, buffer)) { // greedy - handle as many as possible
        dbg("Handling frame.");
        if (urx_state == URXS_IDLE) {
            PayloadParser pp = pp_start(buffer, MQ_SLOT_LEN, NULL);

            // handle binary commands for the gateway

            // magic twice, one inverted - denotes a gateway command
            uint16_t magic1 = pp_u8(&pp);
            uint16_t magic2 = pp_u8(&pp);
            if (magic1 == MAGIC_GW_COMMAND && magic2 == (0xFFU & (~MAGIC_GW_COMMAND))) {
                // third byte is the command code
                switch (pp_u8(&pp)) {
                    case CMD_GET_ID:
                        respond_gw_id();
                        break;

                    case CMD_MSG4SLAVE:;
                        uint8_t slave_addr = pp_u8(&pp);
                        uint16_t frame_len = pp_u16(&pp);
                        uint8_t cksum = pp_u8(&pp);

                        if (frame_len == 0 || frame_len > MAX_FRAME_LEN) {
                            dbg("Frame too big!");
                            break;
                        }

                        start_slave_cmd(slave_addr, frame_len, cksum);
                        dbg("Collecting frame for slave %02x: %d bytes", (int)slave_addr, (int)frame_len);
                        urx_state = URXS_MSG4SLAVE;
                        break;

                    default:
                        dbg("Bad cmd");
                }
            } else {
                // Bad frame??
                dbg("Bad USB frame, starts %x,%x", buffer[0],buffer[1]);
            }
        }
        else if (urx_state == URXS_MSG4SLAVE) {
            uint32_t wanted = MIN(msg4slave_len-msg4slave_already, MQ_SLOT_LEN);
            memcpy(&msg4slave[msg4slave_already], buffer, wanted);
            msg4slave_already += wanted;

            if (wanted < MQ_SLOT_LEN) {
                // this was the end
                uint8_t ck = 0;
                for (int i = 0; i < msg4slave_len; i++) {
                    ck ^= msg4slave[i];
                }
                ck = ~ck;

                if (ck != msg4slave_cksum) {
                    dbg("Checksum mismatch!");
                }
                else {
                    dbg("Verified, sending a %d B frame to slave.", (int) msg4slave_len);
                    // TODO send to slave
                }

                urx_state = URXS_IDLE;
            }
        }
    }
}

