#ifndef OS64_CRYPTO_H
#define OS64_CRYPTO_H
#include <stddef.h>
#include <stdint.h>
void pbkdf2_sha256(const uint8_t *password,size_t plen,const uint8_t *salt,size_t slen,uint32_t rounds,uint8_t out[32]);
void crypto_sha256(const void *data,size_t size,uint8_t out[32]);
#endif
