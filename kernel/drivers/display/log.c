#include "log.h"
#include "timed.h"
#include "display.h"
#include <stdint.h>
#include <stddef.h>

static const char *level_names[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};
static volatile int log_initialized;

typedef struct {
    uint64_t timestamp;
    int level;
    const char *subsystem;
    char msg[LOG_MSG_SIZE];
} log_entry_t;

static log_entry_t buffer[LOG_MAX_ENTRIES];
static size_t head;
static size_t count;
static int current_level;

void log_init(void)
{
    head = 0;
    count = 0;
    current_level = LOG_DEBUG;
    log_initialized = 1;
    log_submit(LOG_INFO, "logd", "logging service initialized");
}

static size_t inc(size_t v)
{
    return (v + 1) % LOG_MAX_ENTRIES;
}

void log_submit(int level, const char *subsystem, const char *msg)
{
    if (level < current_level)
        return;

    log_entry_t *e = &buffer[head];
    e->timestamp = timed_monotonic_ns();
    e->level = level;
    e->subsystem = subsystem;

    size_t i = 0;
    while (msg[i] && i + 1 < LOG_MSG_SIZE)
    {
        e->msg[i] = msg[i];
        i++;
    }
    e->msg[i] = 0;

    head = inc(head);
    if (count < LOG_MAX_ENTRIES)
        count++;
}

int log_read(size_t index, uint64_t *timestamp, int *level, const char **subsystem, const char **msg)
{
    if (index >= count)
        return 0;

    size_t pos;
    if (count < LOG_MAX_ENTRIES)
        pos = index;
    else
        pos = (head + index) % LOG_MAX_ENTRIES;

    log_entry_t *e = &buffer[pos];
    if (timestamp)
        *timestamp = e->timestamp;
    if (level)
        *level = e->level;
    if (subsystem)
        *subsystem = e->subsystem;
    if (msg)
        *msg = e->msg;
    return 1;
}

size_t log_count(void)
{
    return count;
}

int log_level(void)
{
    return current_level;
}

void log_set_level(int level)
{
    current_level = level;
}

void log_poll(void)
{
}

void log_dump(void)
{
    for (size_t i = 0; i < count; i++)
    {
        uint64_t ts;
        int lv;
        const char *sub, *msg;
        if (log_read(i, &ts, &lv, &sub, &msg))
        {
            uint64_t sec = ts / 1000000000ULL;
            uint64_t ms = (ts % 1000000000ULL) / 1000000ULL;
            display_color(0x07);
            display_number(sec);
            display_putc('.');
            if (ms < 100)
                display_putc('0');
            if (ms < 10)
                display_putc('0');
            display_number(ms);
            display_putc(' ');
            display_putc('[');
            int l = lv >= 0 && lv <= 3 ? lv : 1;
            display_color(l >= 2 ? 0x0c : l == 1 ? 0x07 : 0x08);
            display_puts(level_names[l]);
            display_color(0x07);
            display_puts("] ");
            display_puts(sub);
            display_puts(": ");
            display_puts(msg);
            display_putc('\n');
        }
    }
}
