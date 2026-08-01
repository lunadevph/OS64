#ifndef OS64_TLS_CLIENT_H
#define OS64_TLS_CLIENT_H
#include <stddef.h>
typedef enum{TLS_GET_OK=1,TLS_GET_DNS=-1,TLS_GET_TCP=-2,TLS_GET_ENTROPY=-3,TLS_GET_CLOCK=-4,TLS_GET_CERTIFICATE=-5,TLS_GET_HANDSHAKE=-6,TLS_GET_IO=-7}tls_get_result_t;
int tls_https_get(const char *host,const char *path,char *output,size_t capacity,size_t *length,unsigned *detail);
#endif
