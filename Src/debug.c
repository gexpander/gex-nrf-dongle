//
// Created by MightyPork on 2018/03/31.
//

#include "debug.h"
#include "main.h"
#include <stdarg.h>
#include "usart.h"

#define BUFLEN 128
static char buff[BUFLEN];
void dbg(const char *format, ...)
{
    MX_USART_DmaWaitReady();

    va_list args;
    va_start (args, format);
    uint32_t count = (uint32_t) vsprintf (buff, format, args);
    if (count < BUFLEN - 2) {
        buff[count++] = '\r';
        buff[count++] = '\n';
        buff[count] = 0;
    }
    va_end (args);

    MX_USART_DmaSend((uint8_t *) buff, count);

//    char c;
//    char* ptr = buff;
//    while ((c = *ptr++) != 0) {
//        txchar(c);
//    }
//    txchar('\r');
//    txchar('\n');
}
