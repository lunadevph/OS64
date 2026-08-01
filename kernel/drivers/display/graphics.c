#include "graphics.h"
#include "display.h"
static int active;
static unsigned long frames;
int graphics_init(void){frames=0;active=display_ready()&&display_width()&&display_height();return active;}
void graphics_poll(void){if(active)frames++;}
void graphics_stop(void){active=0;}
int graphics_ready(void){return active;}
unsigned long graphics_frames(void){return frames;}
size_t graphics_width(void){return display_width();}
size_t graphics_height(void){return display_height();}
