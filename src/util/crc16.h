#ifndef CRC16_H
#define CRC16_H
unsigned short crc16_ccitt(const void *buf, int len,
                           unsigned short crc_last = 0);
#endif // CRC16_H