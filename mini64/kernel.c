#include <stdint.h>
#include <stddef.h>

#define VGA_BUF ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

static uint8_t attr = 0x0F;
static int row, col;

static volatile uint8_t *fb;
static int fb_w, fb_h, fb_pitch;
static int fb_rows, fb_cols;

static void fb_putc(char c);
#include "../kernel/drivers/display/font8x16.h"

static void putc(char c)
{
    if (fb) { fb_putc(c); return; }
    if (c == '\n') { col = 0; row++; }
    else { VGA_BUF[row * VGA_COLS + col] = (uint16_t)(((uint16_t)attr << 8) | (uint8_t)c); col++; }
    if (col == VGA_COLS) { col = 0; row++; }
    if (row == VGA_ROWS)
    {
        for (int y = 1; y < VGA_ROWS; y++)
            for (int x = 0; x < VGA_COLS; x++)
                VGA_BUF[(y - 1) * VGA_COLS + x] = VGA_BUF[y * VGA_COLS + x];
        for (int x = 0; x < VGA_COLS; x++)
            VGA_BUF[(VGA_ROWS - 1) * VGA_COLS + x] = (uint16_t)(((uint16_t)attr << 8) | ' ');
        row = VGA_ROWS - 1;
    }
}

static void puts(const char *s) { while (*s) putc(*s++); }

static void pnum(uint32_t n)
{
    char b[16]; int i = 0;
    if (!n) { putc('0'); return; }
    while (n) { b[i++] = (char)('0' + n % 10); n /= 10; }
    while (i) putc(b[--i]);
}

static void fb_init(uint32_t mb_info)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)mb_info;
    if (!p) return;
    uint32_t total = *(const uint32_t *)p;
    for (uint32_t off = 8; off + 8 <= total; )
    {
        uint32_t type = *(const uint32_t *)(p + off);
        uint32_t size = *(const uint32_t *)(p + off + 4);
        if (size < 8) return;
        if (type == 8 && size >= 38)
        {
            uint64_t base = *(const uint64_t *)(p + off + 8);
            uint8_t kind = p[off + 29];
            uint8_t bpp = p[off + 28];
            uint32_t pitch = *(const uint32_t *)(p + off + 16);
            uint32_t w = *(const uint32_t *)(p + off + 20);
            uint32_t h = *(const uint32_t *)(p + off + 24);
            if (kind == 1 && bpp == 32 && base < 0x100000000ull && w >= 640 && h >= 400)
            {
                fb = (volatile uint8_t *)(uintptr_t)base;
                fb_pitch = pitch;
                fb_w = w;
                fb_h = h;
                fb_cols = w / 8;
                fb_rows = h / 16;
                if (fb_cols > 80) fb_cols = 80;
                if (fb_rows > 30) fb_rows = 30;
            }
        }
        off += (size + 7) & ~7u;
    }
}

static void fb_scroll(void)
{
    for (int y = 0; y < (fb_rows - 1) * 16; y++)
        for (int x = 0; x < fb_w * 4; x++)
            fb[y * fb_pitch + x] = fb[(y + 16) * fb_pitch + x];
    for (int y = (fb_rows - 1) * 16; y < fb_rows * 16; y++)
        for (int x = 0; x < fb_w * 4; x += 4)
        {
            fb[y * fb_pitch + x] = 0;
            fb[y * fb_pitch + x + 1] = 0;
            fb[y * fb_pitch + x + 2] = 0;
            fb[y * fb_pitch + x + 3] = 0;
        }
}

static void fb_putc(char c)
{
    if (c == '\n') { col = 0; row++; return; }
    if ((unsigned char)c < 32 || (unsigned char)c > 127) c = '?';
    int bx = col * 8;
    int by = row * 16;
    const uint8_t *glyph = os64_font8x16 + (uint8_t)c * 16;
    uint32_t fg = 0x00C0C0C0;
    uint32_t bg = 0x00303030;
    for (int gy = 0; gy < 16; gy++)
    {
        uint8_t bits = glyph[gy];
        for (int gx = 0; gx < 8; gx++)
        {
            int px = bx + gx;
            int py = by + gy;
            if (px < fb_w && py < fb_h)
            {
                uint32_t color = (bits & (1 << (7 - gx))) ? fg : bg;
                uint32_t *pix = (uint32_t *)(fb + py * fb_pitch + px * 4);
                *pix = color;
            }
        }
    }
    col++;
    if (col == fb_cols) { col = 0; row++; }
    if (row == fb_rows) { row = fb_rows - 1; fb_scroll(); }
}

static void fb_clear(void)
{
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
        {
            uint32_t *pix = (uint32_t *)(fb + y * fb_pitch + x * 4);
            *pix = 0x00303030;
        }
    row = 0; col = 0;
}

static inline void outb(uint16_t port, uint8_t v)
{
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outw(uint16_t port, uint16_t v)
{
    __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint16_t inw(uint16_t port)
{
    uint16_t v;
    __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static int disk_wait_ready(void);
static unsigned pending_writes;
static int disk_flush(void)
{
    if (!pending_writes) return 1;
    if (!disk_wait_ready()) return 0;
    outb(0x1F7, 0xE7);
    if (!disk_wait_ready()) return 0;
    pending_writes = 0;
    return 1;
}

static int disk_wait_ready(void)
{
    for (unsigned n = 0; n < 1000000; n++)
        if (!(inb(0x1F7) & 0x80)) return 1;
    return 0;
}
static int disk_wait_data(void)
{
    for (unsigned n = 0; n < 1000000; n++)
    {
        uint8_t s = inb(0x1F7);
        if (s & 1) return 0;
        if (s & 8) return 1;
    }
    return 0;
}
static int disk_select(uint32_t lba, uint8_t cmd)
{
    if (!disk_wait_ready()) return 0;
    outb(0x1F6, (uint8_t)(0xE0 | ((lba >> 24) & 15)));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, cmd);
    return 1;
}
static int disk_read(uint32_t lba, uint8_t *buf)
{
    if (!disk_select(lba, 0x20) || !disk_wait_data()) return 0;
    for (int i = 0; i < 256; i++)
    {
        uint16_t w = inw(0x1F0);
        buf[i * 2] = (uint8_t)w;
        buf[i * 2 + 1] = (uint8_t)(w >> 8);
    }
    return 1;
}
static int disk_write(uint32_t lba, const uint8_t *buf)
{
    if (!disk_select(lba, 0x30) || !disk_wait_data()) return 0;
    for (int i = 0; i < 256; i++)
        outw(0x1F0, (uint16_t)(buf[i * 2] | ((uint16_t)buf[i * 2 + 1] << 8)));
    if (!disk_wait_ready()) return 0;
    pending_writes++;
    /* Keep recovery fast while bounding the amount of uncommitted data. */
    return pending_writes < 128 || disk_flush();
}


#define HOME_START 2048
#define HOME_SECTORS 129024

static uint32_t vol_start, reserved, fat_sectors, data_start, root_cluster;
static uint32_t next_free_cluster = 3;
static uint8_t sectors_per_cluster, fat_count;
static int fat_mounted;

static uint32_t r16(const uint8_t *b, int o) { return (uint32_t)b[o] | ((uint32_t)b[o + 1] << 8); }
static uint32_t r32(const uint8_t *b, int o) { return r16(b, o) | ((uint32_t)r16(b, o + 2) << 16); }
static void w16(uint8_t *b, int o, uint32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }
static void w32(uint8_t *b, int o, uint32_t v) { w16(b, o, v); w16(b, o + 2, v >> 16); }

static void zero(uint8_t *b, int n) { for (int i = 0; i < n; i++) b[i] = 0; }

static uint32_t cluster_lba(uint32_t c) { return vol_start + data_start + (c - 2) * sectors_per_cluster; }
static uint32_t fat_get(uint32_t c)
{
    uint8_t b[512];
    uint32_t off = c * 4;
    if (!disk_read(vol_start + reserved + off / 512, b)) return 0x0FFFFFFF;
    return r32(b, off % 512) & 0x0FFFFFFF;
}
static int fat_set(uint32_t c, uint32_t v)
{
    uint32_t off = c * 4, sec = off / 512, pos = off % 512;
    uint8_t b[512];
    for (uint8_t f = 0; f < fat_count; f++)
    {
        uint32_t lba = vol_start + reserved + (uint32_t)f * fat_sectors + sec;
        if (!disk_read(lba, b)) return 0;
        w32(b, pos, v);
        if (!disk_write(lba, b)) return 0;
    }
    return 1;
}

static int fat_init(void)
{
    uint8_t b[512];
    if (!disk_read(0, b)) return 0;
    if (b[510] != 0x55 || b[511] != 0xAA) return 0;
    uint8_t type = b[450];
    if (type != 0x0C && type != 0x0B) return 0;
    vol_start = r32(b, 454);
    if (!disk_read(vol_start, b)) return 0;
    if (b[510] != 0x55 || b[511] != 0xAA) return 0;
    if (r16(b, 11) != 512 || r32(b, 82) != 0x33544146) return 0;
    sectors_per_cluster = b[13];
    reserved = r16(b, 14);
    fat_count = b[16];
    fat_sectors = r32(b, 36);
    root_cluster = r32(b, 44);
    data_start = reserved + (uint32_t)fat_count * fat_sectors;
    next_free_cluster = 3;
    if (disk_read(vol_start + 1, b))
    {
        uint32_t hint = r32(b, 492);
        if (hint >= 3) next_free_cluster = hint;
    }
    fat_mounted = 1;
    return disk_flush();
}

static int fat_check(void)
{
    uint8_t b[512];
    if (!fat_init()) return 0;
    if (!disk_read(vol_start + 1, b)) return 0;
    if (r32(b, 0) != 0x41615252 || r32(b, 484) != 0x61417272 ||
        r32(b, 508) != 0xAA550000) return 0;
    uint32_t root = fat_get(root_cluster);
    return root != 0 && root != 0x0FFFFFF7;
}

static int fat_format(void)
{
    uint8_t b[512];
    zero(b, 512);
    b[446 + 4] = 0x0C;
    w32(b, 446 + 8, HOME_START);
    w32(b, 446 + 12, HOME_SECTORS);
    b[462 + 4] = 0x7F;
    w32(b, 462 + 8, HOME_START + HOME_SECTORS);
    w32(b, 462 + 12, 131072);
    b[510] = 0x55; b[511] = 0xAA;
    if (!disk_write(0, b)) return 0;

    zero(b, 512);
    b[0] = 0xEB; b[1] = 0x58; b[2] = 0x90;
    const char *oem = "OS64FAT ";
    for (int i = 0; i < 8; i++) b[3 + i] = (uint8_t)oem[i];
    w16(b, 11, 512);
    b[13] = 1;
    w16(b, 14, 32);
    b[16] = 2;
    b[21] = 0xF8;
    w16(b, 24, 63);
    w16(b, 26, 16);
    w32(b, 28, HOME_START);
    w32(b, 32, HOME_SECTORS);
    w32(b, 36, 993);
    w32(b, 44, 2);
    w16(b, 48, 1);
    w16(b, 50, 6);
    b[64] = 0x80;
    b[66] = 0x29;
    w32(b, 67, 0x64060001);
    const char *label = "OS64 HOME  ";
    for (int i = 0; i < 11; i++) b[71 + i] = (uint8_t)label[i];
    const char *fstype = "FAT32   ";
    for (int i = 0; i < 8; i++) b[82 + i] = (uint8_t)fstype[i];
    b[510] = 0x55; b[511] = 0xAA;
    if (!disk_write(HOME_START, b) || !disk_write(HOME_START + 6, b)) return 0;

    zero(b, 512);
    w32(b, 0, 0x41615252);
    w32(b, 484, 0x61417272);
    w32(b, 488, 126997);
    w32(b, 492, 3);
    w32(b, 508, 0xAA550000);
    if (!disk_write(HOME_START + 1, b) || !disk_write(HOME_START + 7, b)) return 0;

    zero(b, 512);
    putc('[');
    uint32_t pbar2 = 0;
    for (uint8_t f = 0; f < 2; f++)
        for (uint32_t s = 0; s < 993; s++)
        {
            if (s == 0) { w32(b, 0, 0x0FFFFFF8); w32(b, 4, 0xFFFFFFFF); w32(b, 8, 0x0FFFFFFF); }
            if (!disk_write(HOME_START + 32 + (uint32_t)f * 993 + s, b)) return 0;
            if (s == 0) zero(b, 512);
            uint32_t want = ((uint32_t)f * 993 + s + 1) * 40 / (2 * 993);
            while (pbar2 < want) { putc('#'); pbar2++; }
        }
    putc(']'); puts("100%\n");

    zero(b, 512);
    for (int i = 0; i < 11; i++) b[i] = (uint8_t)label[i];
    b[11] = 8;
    if (!disk_write(cluster_lba(2), b)) return 0;

    vol_start = HOME_START;
    reserved = 32; fat_count = 2; fat_sectors = 993;
    sectors_per_cluster = 1; root_cluster = 2; data_start = 2018;
    next_free_cluster = 3;
    fat_mounted = 1;
    return disk_flush();
}

static void name83(const char *n, uint8_t *out)
{
    for (int i = 0; i < 11; i++) out[i] = ' ';
    int i = 0, j = 8;
    while (*n && *n != '.' && *n != '/' && i < 8)
    {
        char c = *n++;
        out[i++] = (uint8_t)(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    while (*n && *n != '.') n++;
    if (*n == '.') { n++; while (*n && j < 11) { char c = *n++; out[j++] = (uint8_t)(c >= 'a' && c <= 'z' ? c - 32 : c); } }
}

static int find_entry(const char *name, uint8_t *root, int *slot)
{
    uint8_t key[11];
    name83(name, key);
    if (!disk_read(cluster_lba(root_cluster), root)) return 0;
    for (int i = 0; i < 16; i++)
    {
        uint8_t *e = root + i * 32;
        if (e[0] && e[0] != 0xE5)
        {
            int m = 1;
            for (int j = 0; j < 11; j++) if (e[j] != key[j]) { m = 0; break; }
            if (m) { *slot = i; return 1; }
        }
    }
    return 0;
}

static uint32_t allocate_cluster(void)
{
    uint32_t max = (HOME_SECTORS - data_start) / sectors_per_cluster + 2;
    if (next_free_cluster < 3 || next_free_cluster >= max) next_free_cluster = 3;
    uint32_t start = next_free_cluster;
    for (uint32_t c = start; c < max; c++)
        if (fat_get(c) == 0)
        {
            fat_set(c, 0x0FFFFFFF);
            uint8_t b[512];
            zero(b, 512);
            if (!disk_write(cluster_lba(c), b)) return 0;
            next_free_cluster = c + 1;
            return c;
        }
    for (uint32_t c = 3; c < start; c++)
        if (fat_get(c) == 0)
        {
            fat_set(c, 0x0FFFFFFF);
            uint8_t b[512];zero(b, 512);
            if (!disk_write(cluster_lba(c), b)) return 0;
            next_free_cluster = c + 1;
            return c;
        }
    return 0;
}

static int disk_store_large(const char *name, const uint8_t *data, uint32_t size)
{
    uint8_t root[512];
    int slot;
    if (find_entry(name, root, &slot))
        root[slot * 32] = 0xE5;

    if (!disk_read(cluster_lba(root_cluster), root)) return 0;
    for (slot = 0; slot < 16; slot++)
        if (!root[slot * 32] || root[slot * 32] == 0xE5) break;
    if (slot == 16) return 0;

    uint32_t first = allocate_cluster();
    if (!first) return 0;
    uint32_t tail = first;
    uint32_t written = 0;
    uint8_t b[512];

    uint32_t total = (size + 511) / 512;
    uint32_t pbar = 0;
    putc('[');

    while (written < size)
    {
        zero(b, 512);
        uint32_t chunk = (size - written) < 512 ? (size - written) : 512;
        for (uint32_t i = 0; i < chunk; i++) b[i] = data[written + i];
        if (!disk_write(cluster_lba(tail), b)) return 0;
        written += 512;
        if (total)
        {
            uint32_t want = written / 512 * 40 / total;
            while (pbar < want) { putc('#'); pbar++; }
        }
        if (written < size)
        {
            uint32_t next = allocate_cluster();
            if (!next) return 0;
            fat_set(tail, next);
            tail = next;
        }
    }

    while (pbar < 40) { putc('#'); pbar++; }
    putc(']');
    pnum(100); puts("%\n");

    uint8_t *e = root + slot * 32;
    zero(e, 32);
    name83(name, e);
    e[11] = 0x20;
    w32(e, 28, size);
    w16(e, 20, first >> 16);
    w16(e, 26, first);
    return disk_write(cluster_lba(root_cluster), root) && disk_flush();
}

typedef struct { uint32_t start; uint32_t end; } module_t;

static int parse_modules(uint32_t mb_info, module_t *mods, int max_mods, char names[][16])
{
    int count = 0;
    uint32_t total = *(uint32_t *)mb_info;
    uint32_t off = 8;
    while (off + 8 <= total)
    {
        uint32_t type = *(uint32_t *)(mb_info + off);
        uint32_t size = *(uint32_t *)(mb_info + off + 4);
        if (type == 0) break;
        if (type == 3 && size >= 16 && count < max_mods)
        {
            mods[count].start = *(uint32_t *)(mb_info + off + 8);
            mods[count].end = *(uint32_t *)(mb_info + off + 12);
            const char *str = (const char *)(mb_info + off + 16);
            int slen = (int)size - 16;
            int ni = 0;
            while (ni < slen && ni < 15 && str[ni]) { names[count][ni] = str[ni]; ni++; }
            names[count][ni] = 0;
            count++;
        }
        if (size < 8) break;
        off += (size + 7) & ~7u;
    }
    return count;
}

static int streq(const char *a, const char *b) { while (*a && *b && *a == *b) { a++; b++; } return !*a && !*b; }
static int startswith(const char *s, const char *pre) { while (*pre) if (*s++ != *pre++) return 0; return 1; }

static void list_dir(void)
{
    uint8_t root[512];
    if (!disk_read(cluster_lba(root_cluster), root)) { puts("  Cannot read root.\n"); return; }
    int n = 0;
    for (int i = 0; i < 16; i++)
    {
        uint8_t *e = root + i * 32;
        if (!e[0] || e[0] == 0xE5 || (e[11] & 8)) continue;
        char name[13]; int p = 0;
        for (int j = 0; j < 8 && e[j] != ' '; j++) name[p++] = (char)e[j];
        if (e[8] != ' ') { name[p++] = '.'; for (int j = 8; j < 11 && e[j] != ' '; j++) name[p++] = (char)e[j]; }
        name[p] = 0;
        puts("  ");
        if (e[11] & 0x10) puts("[DIR] ");
        else puts("[FIL] ");
        puts(name); putc(' ');
        if (!(e[11] & 0x10)) { pnum(r32(e, 28)); puts(" bytes"); }
        putc('\n');
        n++;
    }
    if (!n) puts("  (empty)\n");
}

static int read_bootmode(char *buf, int max)
{
    uint8_t root[512];
    int slot;
    if (!find_entry("BOOTMODE.CFG", root, &slot)) return 0;
    uint8_t *e = root + slot * 32;
    uint32_t cluster = r16(e, 26) | ((uint32_t)r16(e, 20) << 16);
    uint32_t size = r32(e, 28);
    if (size > (uint32_t)max - 1) size = max - 1;
    uint32_t pos = 0;
    while (pos < size)
    {
        if (!disk_read(cluster_lba(cluster), (uint8_t *)buf + pos)) return 0;
        pos += 512;
        if (pos < size)
        {
            cluster = fat_get(cluster);
            if (cluster >= 0x0FFFFFF8) break;
        }
    }
    buf[pos] = 0;
    return 1;
}

static void do_recover(const uint8_t *kdata, uint32_t ksize,
    const uint8_t *idata, uint32_t isize,
    const uint8_t *bldata, uint32_t blsize)
{
    if (!fat_mounted)
    {
        if (!fat_format()) { puts("  Failed to format partition.\n"); return; }
        puts("  Partition formatted.\n");
    }

    puts("  Writing KERNEL.BIN... ");
    if (disk_store_large("KERNEL.BIN", kdata, ksize))
    {
        pnum(ksize);
        puts(" bytes written.\n");
    }
    else
    {
        puts("FAILED.\n");
        return;
    }

    if (idata)
    {
        puts("  Writing INITRD.TAR... ");
        if (disk_store_large("INITRD.TAR", idata, isize))
        {
            pnum(isize);
            puts(" bytes written.\n");
        }
        else
        {
            puts("FAILED.\n");
            return;
        }
    }

    if (bldata && blsize)
    {
        uint32_t sectors = (blsize + 511) / 512;
        puts("  Writing bootloader... ");
        putc('[');
        uint32_t pbar = 0;
        for (uint32_t i = 0; i < sectors; i++)
        {
            if (!disk_write(i, bldata + i * 512)) { puts("FAILED.\n"); return; }
            uint32_t want = (i + 1) * 40 / sectors;
            while (pbar < want) { putc('#'); pbar++; }
        }
        while (pbar < 40) { putc('#'); pbar++; }
        putc(']'); puts("100%\n");
    }

    if (!disk_flush()) { puts("  Final disk synchronization FAILED.\n"); return; }
    puts("  OS64 recovery complete. Type 'boot' to restart.\n");
}

static char kbd_get(void)
{
    for (;;)
    {
        if (!(inb(0x64) & 1)) continue;
        uint8_t sc = inb(0x60);
        if (sc & 0x80) continue;
        static const uint8_t map[] = {
            0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
            'q','w','e','r','t','y','u','i','o','p','[',']',0,0,
            'a','s','d','f','g','h','j','k','l',';','\'','`',0,
            '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
        };
        if (sc < sizeof map && map[sc]) return (char)map[sc];
        if (sc == 0x1C) return '\n';
        if (sc == 0x0E) return '\b';
        if (sc == 0x48) return 0x1B;
        if (sc == 0x50) return 0x1A;
        if (sc == 0x4B) return 0x03;
        if (sc == 0x4D) return 0x04;
        if (sc == 0x47) return 0x01;
        if (sc == 0x4F) return 0x02;
        if (sc == 0x53) return 0x7F;
        if (sc == 0x0F) return '\t';
        return 0;
    }
}

static const char *cmd_list[] = {
    "boot", "recover", "recover-with-bl", "bootmode", "ls", "format", "clear", "help", 0
};

#define HIST_MAX 8
static char history[HIST_MAX][128];
static int hist_count;

static void hist_add(const char *line)
{
    if (!*line) return;
    if (hist_count && streq(history[hist_count - 1], line)) return;
    int i;
    for (i = 0; i < hist_count; i++) {}
    if (hist_count < HIST_MAX) hist_count++;
    for (i = hist_count - 1; i > 0; i--)
        for (int j = 0; j < 128; j++) history[i][j] = history[i - 1][j];
    for (int j = 0; j < 128 && line[j]; j++) history[0][j] = line[j];
    history[0][127] = 0;
}

static void readline(char *buf, int max)
{
    int pos = 0, len = 0;
    buf[0] = 0;
    int h = -1;
    char saved[128];
    saved[0] = 0;
    for (;;)
    {
        char c = kbd_get();
        if (c == 0) continue;
        if (c == '\n') { buf[pos] = 0; putc('\n'); hist_add(buf); return; }
        if (c == '\b')
        {
            if (pos)
            {
                pos--;
                for (int i = pos; i < len; i++) buf[i] = buf[i + 1];
                len--;
                buf[len] = 0;
                putc('\b');
                for (int i = pos; i < len; i++) putc(buf[i]);
                putc(' ');
                for (int i = pos; i < len; i++) putc('\b');
            }
        }
        else if (c == 0x7F)
        {
            if (pos < len)
            {
                for (int i = pos; i < len; i++) buf[i] = buf[i + 1];
                len--;
                buf[len] = 0;
                for (int i = pos; i < len; i++) putc(buf[i]);
                putc(' ');
                for (int i = pos; i <= len; i++) putc('\b');
            }
        }
        else if (c == 0x01)
        {
            while (pos > 0) { pos--; putc('\b'); }
        }
        else if (c == 0x02)
        {
            while (pos < len) { putc(buf[pos]); pos++; }
        }
        else if (c == 0x03)
        {
            if (pos > 0) { pos--; putc('\b'); }
        }
        else if (c == 0x04)
        {
            if (pos < len) { putc(buf[pos]); pos++; }
        }
        else if (c == '\t')
        {
            int mcount = 0, mlen = 0;
            const char *match = 0;
            for (int i = 0; cmd_list[i]; i++)
            {
                int j;
                for (j = 0; cmd_list[i][j] && buf[j] && cmd_list[i][j] == buf[j]; j++) {}
                if (!buf[j] || !cmd_list[i][j])
                {
                    if (!mcount) { match = cmd_list[i]; mlen = j; }
                    mcount++;
                }
            }
            if (mcount == 1 && match)
            {
                const char *s = match + mlen;
                while (*s && pos + 1 < max)
                {
                    for (int i = len; i > pos; i--) buf[i] = buf[i - 1];
                    buf[pos] = *s;
                    pos++; len++; s++;
                    putc(buf[pos - 1]);
                    for (int i = pos; i < len; i++) putc(buf[i]);
                    for (int i = pos; i < len; i++) putc('\b');
                }
            }
            else if (mcount > 1)
            {
                putc('\n');
                for (int i = 0; cmd_list[i]; i++)
                {
                    int j;
                    for (j = 0; cmd_list[i][j] && buf[j] && cmd_list[i][j] == buf[j]; j++) {}
                    if (!buf[j] || !cmd_list[i][j])
                    {
                        puts("  ");
                        puts(cmd_list[i]);
                        putc('\n');
                    }
                }
                puts("  Mini64> ");
                for (int i = 0; i < len; i++) putc(buf[i]);
                for (int i = pos; i < len; i++) putc('\b');
            }
        }
        else if (c >= 32 && c < 127 && len + 1 < max)
        {
            for (int i = len; i > pos; i--) buf[i] = buf[i - 1];
            buf[pos] = c;
            pos++; len++;
            buf[len] = 0;
            putc(c);
            for (int i = pos; i < len; i++) putc(buf[i]);
            for (int i = pos; i < len; i++) putc('\b');
        }
        else if (c == 0x1B)
        {
            if (h == -1)
            {
                int i;
                for (i = 0; i < len; i++) saved[i] = buf[i];
                saved[i] = 0;
            }
            if (h + 1 < hist_count)
            {
                h++;
                for (int i = 0; i < len; i++) putc('\b'), putc(' '), putc('\b');
                for (pos = 0; history[h][pos]; pos++) buf[pos] = history[h][pos];
                len = pos; buf[len] = 0;
                puts(buf);
                pos = len;
            }
        }
        else if (c == 0x1A)
        {
            if (h > 0)
            {
                h--;
                for (int i = 0; i < len; i++) putc('\b'), putc(' '), putc('\b');
                for (pos = 0; history[h][pos]; pos++) buf[pos] = history[h][pos];
                len = pos; buf[len] = 0;
                puts(buf);
                pos = len;
            }
            else if (h == 0)
            {
                h = -1;
                for (int i = 0; i < len; i++) putc('\b'), putc(' '), putc('\b');
                for (len = 0; saved[len]; len++) buf[len] = saved[len];
                buf[len] = 0;
                puts(buf);
                pos = len;
            }
        }
    }
}

void kernel_main(uint32_t mb_info)
{
    module_t mods[4];
    char mod_names[4][16];
    int mod_count = parse_modules(mb_info, mods, 4, mod_names);

    const uint8_t *kdata = 0, *idata = 0, *bldata = 0;
    uint32_t ksize = 0, isize = 0, blsize = 0;
    for (int i = 0; i < mod_count; i++)
    {
        uint32_t sz = mods[i].end - mods[i].start;
        if (streq(mod_names[i], "kernel") || streq(mod_names[i], "kernel.bin"))
        { kdata = (const uint8_t *)(uintptr_t)mods[i].start; ksize = sz; }
        if (streq(mod_names[i], "initrd") || streq(mod_names[i], "initrd.tar"))
        { idata = (const uint8_t *)(uintptr_t)mods[i].start; isize = sz; }
        if (streq(mod_names[i], "bootloader") || streq(mod_names[i], "os64-boot.img"))
        { bldata = (const uint8_t *)(uintptr_t)mods[i].start; blsize = sz; }
    }

    fb_init(mb_info);
    if (fb) fb_clear();

    puts("\n");
    puts("  ============================\n");
    puts("   Mini64 - OS64 Rescue System\n");
    puts("  ============================\n");
    puts("\n");

    if (kdata)
    {
        puts("  kernel.bin module: ");
        pnum(ksize); puts(" bytes\n");
    }
    if (idata)
    {
        puts("  initrd.tar module: ");
        pnum(isize); puts(" bytes\n");
    }
    if (bldata)
    {
        puts("  bootloader module: ");
        pnum(blsize); puts(" bytes\n");
    }
    puts("\n");

    if (fat_init())
        puts("  FAT32 partition detected.\n");
    else
        puts("  No FAT32 partition found (format on recover).\n");

    puts("  Type 'help' for commands.\n");
    puts("\n");

    char line[128];
    for (;;)
    {
        puts("  Mini64> ");
        readline(line, sizeof line);

        if (streq(line, "help"))
        {
            puts("\n");
            puts("  Commands:\n");
            puts("    boot                 Restart the system\n");
            puts("    recover              Install OS64 kernel+initrd to disk\n");
            puts("    recover-with-bl      Install kernel+initrd+bootloader\n");
            puts("    bootmode [mode]      Read/set boot mode (e.g. minios)\n");
            puts("    status               Show rescue and disk status\n");
            puts("    chkfs                Validate FAT32 metadata\n");
            puts("    ls                   List files on the FAT32 partition\n");
            puts("    format               Create a new FAT32 partition\n");
            puts("    help                 Show this help\n");
            puts("\n");
        }
        else if (streq(line, "boot"))
        {
            puts("  Rebooting...\n");
            outb(0x64, 0xFE);
            for (;;) __asm__ volatile("cli; hlt");
        }
        else if (streq(line, "recover"))
        {
            if (!kdata)
            {
                puts("  No kernel.bin module available.\n");
                puts("  GRUB must pass kernel.bin and initrd.tar as modules:\n");
                puts("    multiboot2 /boot/mini64/mini64.bin\n");
                puts("    module2 /boot/kernel.bin kernel\n");
                puts("    module2 /boot/initrd.tar initrd\n");
            }
            else
            {
                do_recover(kdata, ksize, idata, isize, 0, 0);
            }
        }
        else if (streq(line, "recover-with-bl"))
        {
            if (!kdata)
            {
                puts("  No kernel.bin module available.\n");
                puts("  GRUB must pass kernel.bin, initrd.tar, and bootloader:\n");
                puts("    multiboot2 /boot/mini64/mini64.bin\n");
                puts("    module2 /boot/kernel.bin kernel\n");
                puts("    module2 /boot/initrd.tar initrd\n");
                puts("    module2 /boot/os64-boot.img bootloader\n");
            }
            else if (!bldata)
            {
                puts("  No bootloader module available (recovering without it).\n");
                do_recover(kdata, ksize, idata, isize, 0, 0);
            }
            else
            {
                do_recover(kdata, ksize, idata, isize, bldata, blsize);
            }
        }
        else if (streq(line, "ls"))
        {
            if (!fat_mounted)
            {
                if (!fat_init()) { puts("  No FAT32 partition.\n"); continue; }
            }
            list_dir();
        }
        else if (streq(line, "status"))
        {
            puts("\n  Rescue environment : ready\n");
            puts("  kernel.bin module  : "); puts(kdata ? "loaded\n" : "missing\n");
            puts("  initrd.tar module  : "); puts(idata ? "loaded\n" : "missing\n");
            puts("  bootloader module  : "); puts(bldata ? "loaded\n" : "missing\n");
            puts("  FAT32 volume       : "); puts((fat_mounted || fat_init()) ? "mounted\n" : "not found\n");
            puts("\n");
        }
        else if (streq(line, "chkfs"))
        {
            puts("  Checking partition table, FAT32 BPB, FSInfo, and root chain... ");
            puts(fat_check() ? "OK\n" : "FAILED\n");
        }
        else if (streq(line, "format"))
        {
            puts("  Creating FAT32 partition...\n");
            if (fat_format()) puts("  Format complete.\n");
            else puts("  Format failed.\n");
        }
        else if (streq(line, "clear"))
        {
            if (fb) fb_clear();
            else
            {
                row = 0; col = 0;
                for (int y = 0; y < VGA_ROWS; y++)
                    for (int x = 0; x < VGA_COLS; x++)
                        VGA_BUF[y * VGA_COLS + x] = (uint16_t)(((uint16_t)attr << 8) | ' ');
            }
        }
        else if (streq(line, "bootmode") || startswith(line, "bootmode "))
        {
            const char *arg = line + 8;
            while (*arg == ' ') arg++;
            if (!fat_mounted && !fat_init()) { puts("  No FAT32 partition.\n"); continue; }
            if (!*arg)
            {
                char mode[64];
                if (read_bootmode(mode, sizeof mode) && mode[0])
                    { puts("  Boot mode: "); puts(mode); putc('\n'); }
                else
                    puts("  Boot mode: normal (default)\n");
            }
            else
            {
                uint32_t len = 0; while (arg[len]) len++;
                uint8_t tmp[512];
                zero(tmp, 512);
                for (uint32_t i = 0; i < len && i < 511; i++) tmp[i] = (uint8_t)arg[i];
                disk_store_large("BOOTMODE.CFG", tmp, len);
                puts("  Boot mode set to: ");
                puts(arg);
                putc('\n');
            }
        }
        else if (*line)
        {
            puts("  Unknown. Type 'help'.\n");
        }
    }
}
