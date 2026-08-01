#ifndef OS64_DISPLAY_H
#define OS64_DISPLAY_H
#include <stddef.h>
#include <stdint.h>
void display_init(uint64_t multiboot_address);
void display_putc(char c);
void display_puts(const char *s);
void display_clear(void);
void display_color(uint8_t color);
void display_number(uint64_t n);
void display_styled(const unsigned char *text, size_t size);
int display_serial_available(void);
int display_ready(void);
size_t display_width(void);
size_t display_height(void);
unsigned long display_characters(void);
void display_cursor_left(void);
void display_cursor_right(void);
uint16_t display_cell(size_t x,size_t y);
void display_set_cell(size_t x,size_t y,uint8_t character,uint8_t attribute);
uint8_t display_codepoint_glyph(uint32_t codepoint);
void display_serial_cells(int enabled);
void display_serial_clear(void);
void display_cursor_set(size_t x,size_t y,int visible,int full_block);
void display_cursor_tick(uint64_t monotonic_ns);
void display_cursor_activity(uint64_t monotonic_ns);
void display_position(size_t *x,size_t *y);
int display_framebuffer_active(void);
int display_mode_supported(void);
int display_set_mode(unsigned width,unsigned height);
unsigned display_pixel_width(void);
unsigned display_pixel_height(void);
#endif
