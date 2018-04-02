//
// Created by MightyPork on 2018/04/03.
//

#ifndef GEX_NRF_CRC32_H
#define GEX_NRF_CRC32_H

uint32_t CRC32_Start(void);

uint32_t CRC32_Add(uint32_t cksum, uint8_t byte);

uint32_t CRC32_End(uint32_t cksum);


#endif //GEX_NRF_CRC32_H
