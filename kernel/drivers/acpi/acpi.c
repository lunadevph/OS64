#include "acpi.h"
#include "io.h"
void acpi_poweroff(void){outw(0x604,0x2000);outw(0xb004,0x2000);}
void acpi_reboot(void){while(inb(0x64)&2){}outb(0x64,0xfe);}
