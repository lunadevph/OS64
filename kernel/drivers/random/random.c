#include <stdint.h>
#include <stddef.h>
#include "bearssl.h"
#include "random.h"

static br_hmac_drbg_context random_context;
static int random_is_ready;

static int cpu_has_rdrand(void)
{
    uint32_t eax = 1, ebx, ecx, edx;
    __asm__ volatile("cpuid"
                     : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    (void)ebx;
    (void)edx;
    return (ecx & (1U << 30)) != 0;
}

static int rdrand64(uint64_t *value)
{
    unsigned char ok;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*value), "=qm"(ok));
    return ok != 0;
}

void random_init(void)
{
    uint64_t seed[8];
    if (random_is_ready || !cpu_has_rdrand())
        return;
    for (size_t i = 0; i < sizeof seed / sizeof seed[0]; ++i) {
        unsigned tries;
        for (tries = 0; tries < 16 && !rdrand64(&seed[i]); ++tries)
            __asm__ volatile("pause");
        if (tries == 16)
            return;
    }
    br_hmac_drbg_init(&random_context, &br_sha256_vtable,
                      seed, sizeof seed);
    for (size_t i = 0; i < sizeof seed / sizeof seed[0]; ++i)
        seed[i] = 0;
    random_is_ready = 1;
}

int random_ready(void)
{
    random_init();
    return random_is_ready;
}

int random_read(void *buffer, size_t size)
{
    if (!random_ready())
        return 0;
    br_hmac_drbg_generate(&random_context, buffer, size);
    return 1;
}

int random_mix(const void *buffer, size_t size)
{
    if (!random_ready())
        return 0;
    br_hmac_drbg_update(&random_context, buffer, size);
    return 1;
}
