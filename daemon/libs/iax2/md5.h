#ifndef MD5_H
#define MD5_H

#ifndef _MSC_VER
#include <inttypes.h>
#else
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
#endif

struct IAX_MD5Context {
	uint32_t buf[4];
	uint32_t bits[2];
	union {
	    uint8_t in[64];
	    uint32_t in_32[16];
	};
};

void IAX_MD5Init(struct IAX_MD5Context *context);
void IAX_MD5Update(struct IAX_MD5Context *context, uint8_t const *buf, unsigned int len);
void IAX_MD5Final(uint8_t digest[16], struct IAX_MD5Context *context);
void IAX_MD5Transform(uint32_t buf[4], uint32_t const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct IAX_MD5Context MD5_CTX;

#endif /* !MD5_H */
