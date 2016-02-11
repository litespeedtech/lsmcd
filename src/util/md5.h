
#ifndef __MD5_H__
#define __MD5_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

struct MD5Context
{
    uint32_t buf[4];
    uint32_t bits[2];
    unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
               unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);

#ifdef __cplusplus
}
#endif

#endif