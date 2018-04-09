//
// Created by MightyPork on 2018/03/31.
//

#include "main.h"
#include <stdbool.h>
#include <string.h>
#include <msg_queue.h>
#include "debug.h"
#include "msg_queue.h"

// the two USB messaging queues
MQueue usb_inq;

void mq_init(MQueue *mq)
{
    mq->nw = 0;
    mq->lr = MQ_LEN-1;
}

bool mq_can_post(MQueue *mq)
{
    return mq->nw != mq->lr;
}

void mq_reset(MQueue *mq)
{
    mq_init(mq);
    memset(mq->slots, 0, sizeof(mq->slots));
}

bool mq_post(MQueue *mq, const uint8_t *buffer, uint32_t len)
{
    if (!mq_can_post(mq)) {
        dbg("FULL!");
        return false;
    }
    if (len >= MQ_SLOT_LEN) {
        dbg("mq post too long!");
        return false;
    }

    memcpy(mq->slots[mq->nw].packet, buffer, len);
    // pad with zeros
    if (len < MQ_SLOT_LEN) memset(mq->slots[mq->nw].packet+len, 0, MQ_SLOT_LEN - len);

    mq->nw++;
    if (mq->nw == MQ_LEN) mq->nw = 0;

    return true;
}

bool mq_can_read(MQueue *mq)
{
    // handle LR at the end, NW at 0 (initial state)
    if (mq->lr == MQ_LEN-1) return mq->nw > 0;
    // wrap-around - if LR not at the end, always have some to read
    if (mq->nw <= mq->lr) return true;
    // forward - one between
    return mq->lr < mq->nw - 1;
}

bool mq_read(MQueue *mq, uint8_t *buffer)
{
    if (!mq_can_read(mq)) {
        return false;
    }

    mq->lr++;
    if (mq->lr == MQ_LEN) mq->lr = 0;
    memcpy(buffer, mq->slots[mq->lr].packet, MQ_SLOT_LEN);

    return true;
}
