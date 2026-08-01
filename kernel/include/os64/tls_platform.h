#ifndef OS64_TLS_PLATFORM_H
#define OS64_TLS_PLATFORM_H
#include <stddef.h>
#include <stdint.h>
typedef struct{int connected;} tls_socket_t;
typedef enum{TLS_PLATFORM_OK=0,TLS_PLATFORM_DNS=-1,TLS_PLATFORM_TCP=-2,TLS_PLATFORM_ENTROPY=-3,TLS_PLATFORM_CLOCK=-4}tls_platform_error_t;
int tls_socket_connect(tls_socket_t *socket,const char *host,uint16_t port);
int tls_socket_send(tls_socket_t *socket,const void *data,size_t length);
int tls_socket_receive(tls_socket_t *socket,void *data,size_t capacity,size_t *length);
void tls_socket_close(tls_socket_t *socket);
int tls_entropy(void *output,size_t length);
int tls_validation_time(uint32_t *days,uint32_t *seconds);
#endif
