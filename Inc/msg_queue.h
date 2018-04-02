//
// Created by MightyPork on 2018/03/31.
//

#ifndef GEX_NRF_MSG_QUEUE_H
#define GEX_NRF_MSG_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#define MQ_LEN 16
#define MQ_SLOT_LEN 64

typedef struct {
    uint8_t packet[MQ_SLOT_LEN];
} MQSlot;

typedef struct {
    MQSlot slots[MQ_LEN];
    volatile uint32_t lr;
    volatile uint32_t nw;
} MQueue;

extern MQueue usb_inq;

void mq_init(MQueue *mq);

bool mq_can_post(MQueue *mq);

bool mq_post(MQueue *mq, const uint8_t *buffer, uint32_t len);

bool mq_can_read(MQueue *mq);

bool mq_read(MQueue *mq, uint8_t *buffer);

#endif //GEX_NRF_MSG_QUEUE_H
