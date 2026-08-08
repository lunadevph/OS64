#include "panic.h"
#include "display.h"
#include "log.h"
#include "vfs.h"
#include "varfs.h"
#include "pstore.h"

#include <stdint.h>
#include <stddef.h>

#define H 0xCD
#define V 0xBA
#define TL 0xC9
#define TR 0xBB
#define BL 0xC8
#define BR 0xBC
#define LS 0xCC
#define RS 0xB9
#define LC 0xC7
#define RC 0xB6

#define ATTR 0x4F

static volatile int panic_active = 0;

static void pch(char c)
{
    display_putc(c);
}

static void pst(const char *s)
{
    display_puts(s);
}

static void ph8(uint64_t v)
{
    static const char t[] = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--)
        pch(t[(v >> ((unsigned)i * 4)) & 0x0F]);
}

static void ph4(uint64_t v)
{
    static const char t[] = "0123456789ABCDEF";
    for (int i = 3; i >= 0; i--)
        pch(t[(v >> ((unsigned)i * 4)) & 0x0F]);
}

static void ph2(uint64_t v)
{
    static const char t[] = "0123456789ABCDEF";
    pch(t[(v >> 4) & 0x0F]);
    pch(t[v & 0x0F]);
}

static void ph1(uint64_t v)
{
    static const char t[] = "0123456789ABCDEF";
    pch(t[v & 0x0F]);
}

static void sp(int n)
{
    while (n-- > 0)
        pch(' ');
}

static void btop(void)
{
    pch(TL);
    for (int i = 0; i < 78; i++)
        pch(H);
    pch(TR);
    pch('\n');
}

static void bbot(void)
{
    pch(BL);
    for (int i = 0; i < 78; i++)
        pch(H);
    pch(BR);
    pch('\n');
}

static void bsep(void)
{
    pch(LS);
    for (int i = 0; i < 78; i++)
        pch(H);
    pch(RS);
    pch('\n');
}

static void bdiv(void)
{
    pch(LC);
    for (int i = 0; i < 78; i++)
        pch(0xC4);
    pch(RC);
    pch('\n');
}

static void vline(const char *s)
{
    pch(V);
    pch(' ');
    pst(s);
    int l = 0;
    while (s[l])
        l++;
    sp(76 - l);
    pch(V);
    pch('\n');
}

static int bounded_text(const char *s, int limit)
{
    int length = 0;
    if (!s)
        s = "UNSPECIFIED_KERNEL_FAILURE";
    while (*s && length < limit) {
        pch(*s++);
        length++;
    }
    return length;
}

static int canonical(uint64_t address)
{
    uint64_t upper = address >> 48;
    return upper == 0x0000 || upper == 0xFFFF;
}

static uint64_t rhash(const char *reason)
{
    uint64_t hash = 1469598103934665603ULL;
    if (!reason)
        return hash;
    while (*reason)
    {
        hash ^= (uint8_t)*reason++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static char log_save_buf[6144];
static size_t log_save_size;
static const char log_hex[] = "0123456789ABCDEF";

static void panic_log_save(uint64_t id)
{
    log_save_size = 0;
    size_t pos = 0;
    size_t i = 0;
    while (i < log_count() && pos + 100 < sizeof log_save_buf)
    {
        uint64_t ts;
        int lv;
        const char *sub, *msg;
        if (!log_read(i, &ts, &lv, &sub, &msg))
        { i++; continue; }
        uint64_t sec = ts / 1000000000ULL;
        uint64_t frac = (ts % 1000000000ULL) / 1000000ULL;
        char tmp[24];
        size_t tn = 0;
        if (!sec) { tmp[tn++] = '0'; }
        else { unsigned long s = (unsigned long)sec; char r[24]; size_t rn = 0; while (s) { r[rn++] = (char)('0' + s % 10); s /= 10; } while (rn) tmp[tn++] = r[--rn]; }
        tmp[tn] = 0;
        for (size_t j = 0; tmp[j] && pos + 1 < sizeof log_save_buf; j++) log_save_buf[pos++] = tmp[j];
        if (pos + 4 < sizeof log_save_buf) { log_save_buf[pos++] = '.'; log_save_buf[pos++] = log_hex[(frac >> 8) & 0xF]; log_save_buf[pos++] = log_hex[(frac >> 4) & 0xF]; log_save_buf[pos++] = log_hex[frac & 0xF]; }
        if (pos + 8 < sizeof log_save_buf)
        {
            log_save_buf[pos++] = ' ';
            log_save_buf[pos++] = '[';
            const char *ln = lv == LOG_ERROR ? "ERROR" : lv == LOG_WARN ? "WARN " : lv == LOG_INFO ? "INFO " : "DEBUG";
            for (int k = 0; ln[k]; k++) log_save_buf[pos++] = ln[k];
            log_save_buf[pos++] = ']';
            log_save_buf[pos++] = ' ';
        }
        while (sub && *sub && pos + 1 < sizeof log_save_buf) log_save_buf[pos++] = *sub++;
        if (pos + 3 < sizeof log_save_buf) { log_save_buf[pos++] = ':'; log_save_buf[pos++] = ' '; }
        while (msg && *msg && pos + 1 < sizeof log_save_buf) log_save_buf[pos++] = *msg++;
        if (pos + 1 < sizeof log_save_buf) log_save_buf[pos++] = '\n';
        i++;
    }
    if (pos + 1 < sizeof log_save_buf) log_save_buf[pos] = 0;

    log_save_size = pos;
    if (pos == 0) return;

    char fname[64];
    size_t fn = 0;
    const char *prefix = "/mnt/logs/log-";
    while (*prefix) fname[fn++] = *prefix++;
    for (int d = 15; d >= 0; d--) fname[fn++] = log_hex[(id >> ((unsigned)d * 4)) & 0xF];
    fname[fn++] = '.'; fname[fn++] = 'l'; fname[fn++] = 'o'; fname[fn++] = 'g';
    fname[fn] = 0;

    varfs_store(fname, (const unsigned char *)log_save_buf, pos);
}

__attribute__((noreturn))
void kernel_panic(const char *reason)
{
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, r8, r9;
    uint64_t r10, r11, r12, r13, r14, r15;
    uint64_t rsp, rbp, rip;
    uint64_t cr0, cr2, cr3, cr4;
    uint64_t rflags;

    __asm__ volatile("cli");

    if (__atomic_exchange_n(&panic_active, 1, __ATOMIC_SEQ_CST))
    {
        for (;;)
            __asm__ volatile("cli; hlt");
    }

    __asm__ volatile("mov %%rax, %0" : "=r"(rax));
    __asm__ volatile("mov %%rbx, %0" : "=r"(rbx));
    __asm__ volatile("mov %%rcx, %0" : "=r"(rcx));
    __asm__ volatile("mov %%rdx, %0" : "=r"(rdx));
    __asm__ volatile("mov %%rsi, %0" : "=r"(rsi));
    __asm__ volatile("mov %%rdi, %0" : "=r"(rdi));
    __asm__ volatile("mov %%r8, %0" : "=r"(r8));
    __asm__ volatile("mov %%r9, %0" : "=r"(r9));
    __asm__ volatile("mov %%r10, %0" : "=r"(r10));
    __asm__ volatile("mov %%r11, %0" : "=r"(r11));
    __asm__ volatile("mov %%r12, %0" : "=r"(r12));
    __asm__ volatile("mov %%r13, %0" : "=r"(r13));
    __asm__ volatile("mov %%r14, %0" : "=r"(r14));
    __asm__ volatile("mov %%r15, %0" : "=r"(r15));
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile("pushfq; popq %0" : "=r"(rflags));

    __asm__ volatile("lea (%%rip), %0" : "=r"(rip));

    uint64_t cs, ss, ds, es, fs, gs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%es, %0" : "=r"(es));
    __asm__ volatile("mov %%fs, %0" : "=r"(fs));
    __asm__ volatile("mov %%gs, %0" : "=r"(gs));

    uint64_t panic_id = rhash(reason) ^ rsp ^ cr2 ^ rip;

    panic_log_save(panic_id);
    (void)pstore_write_panic(reason, log_save_buf, log_save_size);

    display_color(ATTR);
    display_clear();

    btop();

    pch(V);
    pst("                         ***  OS64 KERNEL PANIC  ***                        ");
    pch(V);
    pch('\n');

    bsep();

    vline("OS64 encountered an unrecoverable kernel fault and stopped safely.");
    vline("User-program and filesystem errors must not reach this screen.");
    vline("Record the panic ID and attach the saved kernel log when reporting it.");

    pch(V);
    pst("  Stop code: OS64_KERNEL_PANIC                                               ");
    pch(V);
    pch('\n');

    pch(V);
    pst("  Reason:    ");
    int rl = bounded_text(reason && *reason ? reason : 0, 60);
    sp(60 - rl);
    pch(V);
    pch('\n');

    bsep();

    vline("Recovery: reboot into OS64 Recovery Mode and inspect /mnt/logs.");
    vline("Do not continue using the machine after an unrecoverable kernel fault.");

    bsep();

    pch(V);
    pst("  RAX "); ph8(rax); pst("  RBX "); ph8(rbx);
    pst("  RCX "); ph8(rcx); sp(10);
    pch(V);
    pch('\n');

    pch(V);
    pst("  RDX "); ph8(rdx); pst("  RSI "); ph8(rsi);
    pst("  RDI "); ph8(rdi); sp(10);
    pch(V);
    pch('\n');

    pch(V);
    pst("  RBP "); ph8(rbp); pst("  RSP "); ph8(rsp);
    pst("  R8  "); ph8(r8); sp(10);
    pch(V);
    pch('\n');

    pch(V);
    pst("  R9  "); ph8(r9); pst("  R10 "); ph8(r10);
    pst("  R11 "); ph8(r11); sp(10);
    pch(V);
    pch('\n');

    pch(V);
    pst("  R12 "); ph8(r12); pst("  R13 "); ph8(r13);
    pst("  R14 "); ph8(r14); sp(10);
    pch(V);
    pch('\n');

    pch(V);
    pst("  R15 "); ph8(r15); pst("  RIP "); ph8(rip);
    pst("  RFL "); ph8(rflags); sp(10);
    pch(V);
    pch('\n');

    bsep();

    pch(V);
    pst("  CR0 "); ph8(cr0); pst("  CR2 "); ph8(cr2);
    pst("  CR3 "); ph8(cr3); sp(10);
    pch(V);
    pch('\n');

    pch(V);
    pst("  CR4 "); ph8(cr4); pst("  CS:"); ph4(cs);
    pst(" SS:"); ph4(ss); pst(" DS:"); ph4(ds);
    pst(" ES:"); ph4(es); pst(" FS:"); ph4(fs);
    pst(" GS:"); ph4(gs); sp(14);
    pch(V);
    pch('\n');

    bdiv();

    uint64_t vendor0 = 0, vendor1 = 0, vendor2 = 0;
    uint32_t cpuid_max = 0, family = 0, model = 0, stepping = 0;
    __asm__ volatile("cpuid"
                     : "=a"(cpuid_max), "=b"(vendor0), "=d"(vendor1), "=c"(vendor2)
                     : "a"(0), "c"(0));
    if (cpuid_max >= 1)
    {
        uint32_t eax1 = 0;
        __asm__ volatile("cpuid" : "=a"(eax1) : "a"(1), "c"(0));
        stepping = eax1 & 0xF;
        model = (eax1 >> 4) & 0xF;
        family = (eax1 >> 8) & 0xF;
        if (family == 6 || family == 15)
        {
            model |= (eax1 >> 16) & 0xF0;
            family += (eax1 >> 20) & 0xFF;
        }
    }

    pch(V);
    pst("  Vendor: ");
    for (int i = 0; i < 4; i++)
        pch((char)(vendor0 >> (i * 8)));
    for (int i = 0; i < 4; i++)
        pch((char)(vendor1 >> (i * 8)));
    for (int i = 0; i < 4; i++)
        pch((char)(vendor2 >> (i * 8)));
    pst("  Family: ");
    ph2(family);
    pst("  Model: ");
    ph2(model);
    pst("  Step: ");
    ph1(stepping);
    sp(26);
    pch(V);
    pch('\n');

    bdiv();

    uint64_t frames[6];
    int fcnt = 0;
    uint64_t walk = rbp;
    for (int i = 0; i < 6 && walk && canonical(walk); i++)
    {
        const volatile uint64_t *fp = (const volatile uint64_t *)(uintptr_t)walk;
        frames[i] = fp[1];
        walk = fp[0];
        fcnt++;
    }

    pch(V);
    pst("  Call trace:");
    sp(64);
    pch(V);
    pch('\n');

    if (fcnt == 0)
    {
        pch(V);
        pst("  (unavailable - RBP chain invalid)                                         ");
        pch(V);
        pch('\n');
    }
    else
    {
        for (int i = 0; i < fcnt; i += 3)
        {
            pch(V);
            int end = i + 3;
            if (end > fcnt)
                end = fcnt;
            int used = 1;
            for (int j = i; j < end; j++)
            {
                pch(' ');
                pch('#');
                ph1(j);
                pch(' ');
                ph8(frames[j]);
                used += 20;
            }
            sp(77 - used);
            pch(V);
            pch('\n');
        }
    }

    bdiv();

    pch(V);
    pst("  Stack dump:");
    sp(65);
    pch(V);
    pch('\n');

    if (!rsp || !canonical(rsp))
    {
        pch(V);
        pst("  Stack pointer unavailable                                                    ");
        pch(V);
        pch('\n');
    }
    else
    {
        const volatile uint64_t *stack = (const volatile uint64_t *)(uintptr_t)rsp;
        for (unsigned row = 0; row < 2; row++)
        {
            pch(V);
            int used = 1;
            for (unsigned col = 0; col < 4; col++)
            {
                unsigned idx = row * 4 + col;
                if (idx < 16)
                {
                    pch(' ');
                    ph2(idx * 8);
                    pch(' ');
                    ph8(stack[idx]);
                    pch(' ');
                    used += 21;
                }
            }
            sp(78 - used);
            pch(V);
            pch('\n');
        }
    }

    bsep();

    pch(V);
    pst("  Panic ID: 0x");
    ph8(panic_id);
    pst("    System halted    " OS64_NAME " " OS64_KERNEL_VERSION " " OS64_ARCHITECTURE);
    sp(4);
    pch(V);
    pch('\n');

    bbot();

    for (;;)
        __asm__ volatile("cli; hlt");
}
