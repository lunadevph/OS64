#include "tls_client.h"
#include "tls_platform.h"
#include "ca_store.h"
#include "bearssl.h"

static br_ssl_client_context client;
static br_x509_minimal_context x509;
static unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];

static int failure(unsigned error,unsigned*detail){
    if(detail)*detail=error;
    if(error>=BR_ERR_X509_OK&&error<=BR_ERR_X509_NOT_TRUSTED)return TLS_GET_CERTIFICATE;
    return TLS_GET_HANDSHAKE;
}
int tls_https_get(const char*host,const char*path,char*output,size_t capacity,size_t*length,unsigned*detail){
    if(length)*length=0;
    if(detail)*detail=0;
    tls_socket_t socket={0};int platform=tls_socket_connect(&socket,host,443);
    if(platform==TLS_PLATFORM_DNS)return TLS_GET_DNS;
    if(platform!=TLS_PLATFORM_OK)return TLS_GET_TCP;
    unsigned char seed[48];
    if(tls_entropy(seed,sizeof seed)!=TLS_PLATFORM_OK){tls_socket_close(&socket);return TLS_GET_ENTROPY;}
    uint32_t days,seconds;
    if(tls_validation_time(&days,&seconds)!=TLS_PLATFORM_OK){tls_socket_close(&socket);return TLS_GET_CLOCK;}
    size_t count;const br_x509_trust_anchor*anchors=os64_ca_store(&count);
    br_ssl_client_init_full(&client,&x509,anchors,count);
    br_x509_minimal_set_time(&x509,days,seconds);
    br_ssl_engine_set_versions(&client.eng,BR_TLS12,BR_TLS12);
    static const uint16_t suites[]={
        BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
        BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
    };
    br_ssl_engine_set_suites(&client.eng,suites,sizeof suites/sizeof suites[0]);
    br_ssl_engine_set_buffer(&client.eng,iobuf,sizeof iobuf,1);
    br_ssl_engine_inject_entropy(&client.eng,seed,sizeof seed);
    if(!br_ssl_client_reset(&client,host,0)){unsigned e=br_ssl_engine_last_error(&client.eng);tls_socket_close(&socket);return failure(e,detail);}
    unsigned char request[512];size_t q=0,request_sent=0,used=0;
    const char*a="GET ",*b=" HTTP/1.0\r\nHost: ",*c="\r\nUser-Agent: OS64-curl/0.7\r\nConnection: close\r\nAccept: */*\r\n\r\n";
    while(*a)request[q++]=(unsigned char)*a++;
    while(*path&&q<sizeof request-1)request[q++]=(unsigned char)*path++;
    while(*b)request[q++]=(unsigned char)*b++;
    while(*host&&q<sizeof request-1)request[q++]=(unsigned char)*host++;
    while(*c&&q<sizeof request)request[q++]=(unsigned char)*c++;
    for(unsigned long cycles=0;cycles<200000;cycles++){
        unsigned state=br_ssl_engine_current_state(&client.eng);if(state&BR_SSL_CLOSED){unsigned e=br_ssl_engine_last_error(&client.eng);tls_socket_close(&socket);if(length)*length=used;return e?failure(e,detail):(used?TLS_GET_OK:TLS_GET_IO);}
        if(state&BR_SSL_SENDREC){size_t n;unsigned char*p=br_ssl_engine_sendrec_buf(&client.eng,&n);if(tls_socket_send(&socket,p,n)!=TLS_PLATFORM_OK){unsigned e=br_ssl_engine_last_error(&client.eng);if(detail)*detail=e?e:1000u+state;tls_socket_close(&socket);if(length)*length=used;if(used&&!e)return TLS_GET_OK;return e?failure(e,detail):TLS_GET_IO;}br_ssl_engine_sendrec_ack(&client.eng,n);continue;}
        if((state&BR_SSL_SENDAPP)&&request_sent<q){size_t cap;unsigned char*p=br_ssl_engine_sendapp_buf(&client.eng,&cap);size_t n=q-request_sent;if(n>cap)n=cap;for(size_t i=0;i<n;i++)p[i]=request[request_sent+i];request_sent+=n;br_ssl_engine_sendapp_ack(&client.eng,n);br_ssl_engine_flush(&client.eng,0);continue;}
        if(state&BR_SSL_RECVREC){size_t cap,n;unsigned char*p=br_ssl_engine_recvrec_buf(&client.eng,&cap);if(tls_socket_receive(&socket,p,cap,&n)!=TLS_PLATFORM_OK){if(detail)*detail=2000u+state;tls_socket_close(&socket);if(length)*length=used;return used?TLS_GET_OK:TLS_GET_IO;}if(!n){if(detail)*detail=3000u+state;tls_socket_close(&socket);if(length)*length=used;return used?TLS_GET_OK:TLS_GET_IO;}br_ssl_engine_recvrec_ack(&client.eng,n);continue;}
        if(state&BR_SSL_RECVAPP){size_t n;unsigned char*p=br_ssl_engine_recvapp_buf(&client.eng,&n);size_t take=n;if(take>capacity-used)take=capacity-used;for(size_t i=0;i<take;i++)output[used+i]=(char)p[i];used+=take;br_ssl_engine_recvapp_ack(&client.eng,n);if(used==capacity){tls_socket_close(&socket);if(length)*length=used;return TLS_GET_OK;}continue;}
    }
    tls_socket_close(&socket);return TLS_GET_HANDSHAKE;
}
