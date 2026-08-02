#include "app_abi.h"

static const char *logo[] = {
    "   ____  _____ __  _  ",
    "  / __ \\/ ___// /_| | ",
    " / /_/ /\\__ \\/ __/ /_",
    " \\____//____/\\__/___/",
    "                     "
};

static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return !*a&&!*b;}
static void put_number(const os64_api_t *api,unsigned long value){char digits[24];unsigned used=0;if(!value)digits[used++]='0';while(value){digits[used++]=(char)('0'+value%10);value/=10;}while(used)api->putc(digits[--used]);}
static void label(const os64_api_t *api,const char *name,int color){if(color)api->write("\x1b[96m");api->write(name);if(color)api->write("\x1b[0m");api->write(": ");}
static void prefix(const os64_api_t *api,unsigned row,int color){if(color)api->write("\x1b[94m");api->write(logo[row]);if(color)api->write("\x1b[0m");api->write("  ");}
static void memory_line(const os64_api_t *api){unsigned long free_kib=api->system_query("memory.free_bytes")/1024,total_mib=api->system_query("memory.total_kib")/1024;put_number(api,free_kib);api->write(" KiB kernel heap free / ");put_number(api,total_mib);api->write(" MiB detected");}

int _start(const os64_api_t *api,const char *args){
    if(!api||api->version!=OS64_ABI_VERSION)return 126;
    while(args&&*args==' ')args++;
    if(args&&equal(args,"--help")){api->write("Usage: sysfetch [--plain|--help|--version]\n\nDisplay a compact OS64 system summary.\n");return 0;}
    if(args&&equal(args,"--version")){api->write("OS64 sysfetch 1.1\n");return 0;}
    int color=!(args&&equal(args,"--plain"));
    if(args&&*args&&!equal(args,"--plain")){api->write("sysfetch: unknown option\n");return 2;}

    prefix(api,0,color);if(color)api->write("\x1b[1;97m");api->write(api->current_user());api->write("@os64");if(color)api->write("\x1b[0m");api->putc('\n');
    prefix(api,1,color);api->write("--------------------\n");
    prefix(api,2,color);label(api,"OS",color);api->write("OS64 1.0\n");
    prefix(api,3,color);label(api,"Kernel",color);api->write("1.0 x86_64\n");
    prefix(api,4,color);label(api,"Uptime",color);unsigned long up=api->system_query("time.uptime_seconds");put_number(api,up/3600);api->write("h ");put_number(api,(up/60)%60);api->write("m ");put_number(api,up%60);api->write("s\n");
    api->write("                         ");label(api,"Shell",color);api->write("sh\n");
    api->write("                         ");label(api,"Terminal",color);put_number(api,api->system_query("terminal.width"));api->putc('x');put_number(api,api->system_query("terminal.height"));api->write(" cells\n");
    api->write("                         ");label(api,"Display",color);put_number(api,api->system_query("display.width"));api->putc('x');put_number(api,api->system_query("display.height"));api->putc('\n');
    api->write("                         ");label(api,"Memory",color);memory_line(api);api->putc('\n');
    api->write("                         ");label(api,"Storage",color);api->write(api->system_query("disk.mounted")&&api->system_query("var.mounted")?"/home and /var mounted\n":"live initramfs only\n");
    api->write("                         ");label(api,"Network",color);api->write(api->system_query("network.ready")?"net0 connected":"offline");api->write(" (RX ");put_number(api,api->system_query("network.rx_packets"));api->write(", TX ");put_number(api,api->system_query("network.tx_packets"));api->write(")\n");
    api->write("                         ");label(api,"Services",color);put_number(api,api->system_query("services.ready"));api->putc('/');put_number(api,api->system_query("services.count"));api->write(" ready\n");
    api->write("                         ");label(api,"Users",color);put_number(api,api->system_query("users.count"));api->write(" configured\n");
    if(color)api->write("                         \x1b[40m  \x1b[41m  \x1b[42m  \x1b[43m  \x1b[44m  \x1b[45m  \x1b[46m  \x1b[47m  \x1b[0m\n");
    return 0;
}
