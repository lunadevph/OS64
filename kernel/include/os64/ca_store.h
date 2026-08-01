#ifndef OS64_CA_STORE_H
#define OS64_CA_STORE_H
#include "bearssl.h"
#include <stddef.h>
const br_x509_trust_anchor *os64_ca_store(size_t *count);
#endif
