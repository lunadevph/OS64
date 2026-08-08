#include "ps2.h"
#include "io.h"

#define PS2_DATA_PORT 0x60u
#define PS2_STATUS_PORT 0x64u
#define PS2_COMMAND_PORT 0x64u
#define PS2_TIMEOUT 100000u

static int controller_ok;
static int keyboard_ok;
static int mouse_ok;
static uint8_t mouse_packet[3];
static unsigned mouse_packet_size;

static int wait_input_empty(void) {
    for (unsigned n = 0; n < PS2_TIMEOUT; n++)
        if (!(inb(PS2_STATUS_PORT) & 0x02u)) return 1;
    return 0;
}

static int wait_output_full(void) {
    for (unsigned n = 0; n < PS2_TIMEOUT; n++)
        if (inb(PS2_STATUS_PORT) & 0x01u) return 1;
    return 0;
}

static int write_command(uint8_t value) {
    if (!wait_input_empty()) return 0;
    outb(PS2_COMMAND_PORT, value);
    return 1;
}

static int write_data(uint8_t value) {
    if (!wait_input_empty()) return 0;
    outb(PS2_DATA_PORT, value);
    return 1;
}

static int read_wait(uint8_t *value) {
    if (!value || !wait_output_full()) return 0;
    *value = inb(PS2_DATA_PORT);
    return 1;
}

int ps2_controller_init(void) {
    uint8_t config;
    controller_ok = keyboard_ok = mouse_ok = 0;

    /* Disable both ports and discard stale controller output. */
    if (!write_command(0xadu) || !write_command(0xa7u)) return 0;
    for (unsigned n = 0; n < 32 && (inb(PS2_STATUS_PORT) & 1u); n++)
        (void)inb(PS2_DATA_PORT);

    if (!write_command(0x20u) || !read_wait(&config)) return 0;

    /*
     * Keep controller translation enabled: the keyboard decoder consumes
     * translated Set-1 scan codes. Disable the second port clock and IRQs;
     * OS64 currently polls the first port.
     */
    config = (uint8_t)((config | 0x40u | 0x20u) & ~(0x01u | 0x02u | 0x10u));
    if (!write_command(0x60u) || !write_data(config)) return 0;
    if (!write_command(0xaeu)) return 0;
    controller_ok = 1;
    return 1;
}

int ps2_keyboard_command(uint8_t value) {
    uint8_t response;
    for (unsigned retry = 0; retry < 3; retry++) {
        if (!write_data(value) || !read_wait(&response)) return 0;
        if (response == 0xfau) return 1;
        if (response != 0xfeu) return 0;
    }
    return 0;
}

int ps2_keyboard_initialize(void) {
    uint8_t response;
    if (!controller_ok || !write_data(0xffu) || !read_wait(&response)) return 0;
    if (response != 0xfau || !read_wait(&response)) return 0;
    if (response != 0xaau && response != 0x00u) return 0;

    /* Device Set 2 plus i8042 translation gives the decoder Set-1 bytes. */
    if (!ps2_keyboard_command(0xf0u) || !ps2_keyboard_command(0x02u) ||
        !ps2_keyboard_command(0xf4u)) return 0;
    keyboard_ok = 1;
    return 1;
}

int ps2_data_available(void) {
    uint8_t status = inb(PS2_STATUS_PORT);
    return controller_ok && (status & 0x01u) && !(status & 0x20u);
}

uint8_t ps2_read_data(void) {
    return ps2_data_available() ? inb(PS2_DATA_PORT) : 0;
}

static int mouse_command(uint8_t value) {
    uint8_t reply;
    for (unsigned retry = 0; retry < 3; retry++) {
        if (!write_command(0xd4u) || !write_data(value) || !read_wait(&reply)) return 0;
        if (reply == 0xfau) return 1;
        if (reply != 0xfeu) return 0;
    }
    return 0;
}

int ps2_mouse_initialize(void) {
    uint8_t config, reply;
    mouse_ok = 0;
    mouse_packet_size = 0;
    if (!controller_ok || !write_command(0xa8u)) return 0;
    if (!write_command(0x20u) || !read_wait(&config)) return 0;
    config = (uint8_t)(config & ~(0x02u | 0x20u));
    if (!write_command(0x60u) || !write_data(config)) return 0;
    if (!mouse_command(0xffu) || !read_wait(&reply) || reply != 0xaau ||
        !read_wait(&reply)) return 0;
    if (!mouse_command(0xf6u) || !mouse_command(0xf4u)) return 0;
    mouse_ok = 1;
    return 1;
}

void ps2_mouse_shutdown(void) {
    if (mouse_ok) (void)mouse_command(0xf5u);
    (void)write_command(0xa7u);
    mouse_ok = 0;
    mouse_packet_size = 0;
}

int ps2_mouse_poll(int *delta_x, int *delta_y, unsigned *buttons) {
    if (!mouse_ok) return 0;
    uint8_t status = inb(PS2_STATUS_PORT);
    if (!(status & 0x01u) || !(status & 0x20u)) return 0;
    uint8_t value = inb(PS2_DATA_PORT);
    if (!mouse_packet_size && !(value & 0x08u)) return 0;
    mouse_packet[mouse_packet_size++] = value;
    if (mouse_packet_size != 3) return 0;
    mouse_packet_size = 0;
    if (mouse_packet[0] & 0xc0u) return 0;
    if (delta_x) *delta_x = (int)(int8_t)mouse_packet[1];
    if (delta_y) *delta_y = -(int)(int8_t)mouse_packet[2];
    if (buttons) *buttons = mouse_packet[0] & 7u;
    return 1;
}

int ps2_controller_ready(void) { return controller_ok; }
int ps2_keyboard_ready(void) { return keyboard_ok; }
int ps2_mouse_ready(void) { return mouse_ok; }
