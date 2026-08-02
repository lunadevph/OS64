#include <stdint.h>
#include <stddef.h>
#include "io.h"
#include "display.h"
#include "ansi_terminal.h"
#include "memory.h"
#include "fs.h"
#include "disk.h"
#include "elf.h"
#include "auth.h"
#include "crypto.h"
#include "panic.h"
#include "ofp.h"
#include "exceptions.h"
#include "vfs.h"
#include "varfs.h"
#include "service.h"
#include "rtc.h"
#include "timed.h"
#include "acpi.h"
#include "bootopts.h"
#include "keyboard.h"
#include "network.h"
#include "tls_client.h"
#include "process.h"
#include "log.h"
#include "pci.h"

#define P display_puts
static char shell_name[16]="sh";
static char cwd[128]="/";
static char current_user[16]="root";
static char active_command[16]="sh";
static unsigned char installed_image[262144];
static char http_output[32768];
static int keyboard_ready;
static int last_status;
static int foreground_active;
static int foreground_interrupted;
static char history_lines[16][128];
static unsigned history_count;
static unsigned boot_services_started;
static uint16_t sudo_cached_uid=65535;
static uint64_t sudo_cache_until;
static int boot_storage_ready,boot_filesystem_mounted;
static uint16_t terminal_saved[216*64];
static size_t terminal_saved_x,terminal_saved_y;
static size_t terminal_saved_width,terminal_saved_height;
static int terminal_owned;
static uint64_t background_poll_due;
static int streq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return !*a&&!*b;}
static int starts(const char*s,const char*p){while(*p&&*s==*p){s++;p++;}return !*p;}

static char input_result(char c){
    display_cursor_activity(timed_monotonic_ns());return c;
}
static char input(void){
    unsigned spin=0;
    for(;;){
        uint64_t now=timed_monotonic_ns();
        if(now>=background_poll_due){service_poll_all();background_poll_due=now+10000000ull;}
        if((++spin&0xffffu)==0)display_cursor_tick(now);
        char c=keyboard_poll();if(c)return input_result(c);
        if(display_serial_available()){
            uint8_t status=inb(0x3fd);
            if(status!=0xff&&(status&1)){
                c=(char)inb(0x3f8);if(c=='\r')return input_result('\n');
                if(c==27){
                    char a=0,b=0;
                    for(unsigned n=0;n<1000000&&!a;n++)if(inb(0x3fd)&1)a=(char)inb(0x3f8);
                    for(unsigned n=0;n<1000000&&!b;n++)if(inb(0x3fd)&1)b=(char)inb(0x3f8);
                    if(a=='['&&b=='A')return input_result(0x11);
                    if(a=='['&&b=='B')return input_result(0x12);
                    if(a=='['&&b=='D')return input_result(0x13);
                    if(a=='['&&b=='C')return input_result(0x14);
                    continue;
                }
                return input_result(c);
            }
        }
        __asm__ volatile("pause");
    }
}

int process_cancel_requested(void){
    if(!foreground_active)return 0;
    if(foreground_interrupted)return 1;
    char c=keyboard_poll();
    if(!c&&display_serial_available()){
        uint8_t status=inb(0x3fd);
        if(status!=0xff&&(status&1))c=(char)inb(0x3f8);
    }
    if(c==3)foreground_interrupted=1;
    return foreground_interrupted;
}

typedef void (*command_fn)(const char*);
typedef struct{const char*name;command_fn run;}command_t;
static void make_path(char*out,const char*dir,const char*name);
static void execute(char*line);
static int abi_dispatch(const char*name,const char*args);
static void print_ip(uint32_t address);
static void c_help(const char*a);static void c_history(const char*a);static void c_status(const char*a);
static void c_chmod(const char*a);static void c_chown(const char*a);static void c_chgrp(const char*a);static void c_umask(const char*a);static void c_sudo(const char*a);static void c_useradd(const char*a);static void c_userdel(const char*a);static void c_usermod(const char*a);static void c_passwd(const char*a);static void c_groups(const char*a);static void c_id(const char*a);static void c_who(const char*a);static void c_login(const char*a);static void c_logout(const char*a);
static void c_clear(const char*a);static void c_display(const char*a);static void c_echo(const char*a);static void c_uname(const char*a);static void c_whoami(const char*a);static void c_pwd(const char*a);static void c_cd(const char*a);static void c_ls(const char*a);static void c_cat(const char*a);static void c_xxd(const char*a);static void c_dd(const char*a);static void c_head(const char*a);static void c_tail(const char*a);static void c_wc(const char*a);static void c_stat(const char*a);static void c_basename(const char*a);static void c_dirname(const char*a);static void c_truncate(const char*a);static void c_fill(const char*a);static void c_install(const char*a);static void c_install_apps(const char*a);static void c_pm(const char*a);static void c_mkuser(const char*a);static void c_ofp(const char*a);static void c_ps(const char*a);static void c_date(const char*a);static void c_time(const char*a);static void c_ntp(const char*a);static void c_free(const char*a);static void c_ifconfig(const char*a);static void c_ping(const char*a);static void c_nslookup(const char*a);static void c_route(const char*a);static void c_curl(const char*a);static void c_browser(const char*a);static void c_lspci(const char*a);static void c_disk(const char*a);static void c_chkfs(const char*a);static void c_format(const char*a);static void c_mount(const char*a);static void c_sync(const char*a);static void c_touch(const char*a);static void c_mkdir(const char*a);static void c_rm(const char*a);static void c_cp(const char*a);static void c_mv(const char*a);static void c_nano(const char*a);static void c_su(const char*a);static void c_daemon(const char*a);static void c_reboot(const char*a);static void c_shutdown(const char*a);static void c_halt(const char*a);static void c_init(const char*a);static void c_sh(const char*a);static void c_nologin(const char*a);static void c_dmesg(const char*a);
static const command_t commands[]={{"help",c_help},{"history",c_history},{"status",c_status},{"clear",c_clear},{"display",c_display},{"echo",c_echo},{"uname",c_uname},{"whoami",c_whoami},{"who",c_who},{"groups",c_groups},{"id",c_id},{"pwd",c_pwd},{"cd",c_cd},{"ls",c_ls},{"find",c_ls},{"cat",c_cat},{"xxd",c_xxd},{"hexdump",c_xxd},{"dd",c_dd},{"head",c_head},{"tail",c_tail},{"wc",c_wc},{"stat",c_stat},{"chmod",c_chmod},{"chown",c_chown},{"chgrp",c_chgrp},{"umask",c_umask},{"basename",c_basename},{"dirname",c_dirname},{"truncate",c_truncate},{"fill",c_fill},{"install",c_install},{"install-apps",c_install_apps},{"pm",c_pm},{"mkuser",c_mkuser},{"useradd",c_useradd},{"userdel",c_userdel},{"usermod",c_usermod},{"passwd",c_passwd},{"ofp",c_ofp},{"ps",c_ps},{"date",c_date},{"time",c_time},{"ntp",c_ntp},{"free",c_free},{"ifconfig",c_ifconfig},{"ping",c_ping},{"nslookup",c_nslookup},{"host",c_nslookup},{"route",c_route},{"ip",c_route},{"curl",c_curl},{"browser",c_browser},{"lspci",c_lspci},{"diskinfo",c_disk},{"chkfs",c_chkfs},{"format",c_format},{"mount",c_mount},{"sync",c_sync},{"touch",c_touch},{"mkdir",c_mkdir},{"rm",c_rm},{"cp",c_cp},{"mv",c_mv},{"nano",c_nano},{"sudo",c_sudo},{"su",c_su},{"login",c_login},{"logout",c_logout},{"fsd",c_daemon},{"memoryd",c_daemon},{"timed",c_daemon},{"diskd",c_daemon},{"userd",c_daemon},{"acpid",c_daemon},{"netd",c_daemon},{"displayd",c_daemon},{"graphicsd",c_daemon},{"logd",c_daemon},{"dmesg",c_dmesg},{"reboot",c_reboot},{"shutdown",c_shutdown},{"halt",c_halt},{"init",c_init},{"sh",c_sh},{"nologin",c_nologin}};

static void c_help(const char*a){while(*a==' ')a++;if(*a){P(a);P(": supports -h, --help and --version\n");return;}P("Filesystem\n----------\n  ls       find      cat       head      tail\n  cp       mv        rm        touch     truncate\n  dd       wc        stat      basename  dirname\n  mkdir    nano      xxd       chkfs\n\nSystem\n------\n  uname   ps      free    sync    mount\n  display date    time    status  reboot  shutdown\n  dmesg   lspci\n\nUsers\n-----\n  whoami  su      mkuser\n\nNetwork\n-------\n  ifconfig  route  ip      ping\n  nslookup  host   curl    browser ntp     netd\n\nPackages\n--------\n  pm list|info|install|remove|status|update\n\nUtilities\n---------\n  echo    clear   history help    teteris\n\nAdministration\n--------------\n  install install-apps format fsd memoryd timed diskd userd acpid\n  displayd graphicsd logd\n");}
static void c_history(const char*a){(void)a;for(unsigned i=0;i<history_count;i++){display_number(i+1);P("  ");P(history_lines[i]);display_putc('\n');}}
static void c_status(const char*a){(void)a;int previous=last_status;P("Last program exit status: ");display_number((unsigned long)previous);display_putc('\n');last_status=0;}
static void c_clear(const char*a){(void)a;display_clear();}
static void c_display(const char*a){
    while(*a==' ')a++;
    if(!*a||streq(a,"status")){
        P("Driver      : ");P(display_framebuffer_active()?(display_mode_supported()?"bochs-vbe":"boot framebuffer"):"vga-text");
        P("\nResolution  : ");display_number(display_pixel_width());display_putc('x');display_number(display_pixel_height());
        P("\nTerminal    : ");display_number(display_width());display_putc('x');display_number(display_height());P(" cells\n");return;
    }
    if(streq(a,"modes")){P("640x480  800x600  1024x768  1280x720  1600x900  1920x1080\n");return;}
    if(starts(a,"mode ")){
        a+=5;unsigned w=0,h=0;while(*a>='0'&&*a<='9'){w=w*10u+(unsigned)(*a-'0');a++;}
        if(*a++!='x'){P("display: expected WIDTHxHEIGHT\n");last_status=2;return;}
        while(*a>='0'&&*a<='9'){h=h*10u+(unsigned)(*a-'0');a++;}
        if(*a||!((w==640&&h==480)||(w==800&&h==600)||(w==1024&&h==768)||(w==1280&&h==720)||(w==1600&&h==900)||(w==1920&&h==1080))){P("display: unsupported mode; run 'display modes'\n");last_status=2;return;}
        if(!display_set_mode(w,h)){P("display: hardware modesetting is unavailable; select a GRUB graphics mode\n");last_status=1;return;}
        P("Display mode changed to ");display_number(w);display_putc('x');display_number(h);display_putc('\n');return;
    }
    P("Usage: display [status|modes|mode WIDTHxHEIGHT]\n");last_status=2;
}
static void c_echo(const char*a){while(*a==' ')a++;int newline=1;if(starts(a,"-n ")){newline=0;a+=3;}else if(streq(a,"-n")){newline=0;a+=2;}P(a);if(newline)display_putc('\n');}
static void c_uname(const char*a){while(*a==' ')a++;if(!*a||streq(a,"-s")){P(OS64_NAME "\n");return;}if(streq(a,"-r")){P(OS64_KERNEL_VERSION "\n");return;}if(streq(a,"-m")){P(OS64_ARCHITECTURE "\n");return;}if(streq(a,"-a")){P(OS64_NAME " " OS64_KERNEL_VERSION " " OS64_ARCHITECTURE " " OS64_SYSTEM_POLICY "\n");return;}P("uname: invalid option\nUsage: uname [-a|-s|-r|-m]\n");last_status=2;}
static void c_whoami(const char*a){(void)a;P(current_user);display_putc('\n');}
static void c_pwd(const char*a){(void)a;P(cwd);display_putc('\n');}
static void resolve_path(const char*input,char*out){
    while(*input==' ')input++;
    char raw[128];size_t n=0;
    if(*input!='/'){for(size_t i=0;cwd[i]&&n<127;i++)raw[n++]=cwd[i];if(n>1&&raw[n-1]!='/')raw[n++]='/';}
    while(*input&&n<127)raw[n++]=*input++;
    raw[n]=0;n=0;out[n++]='/';
    for(size_t i=0;raw[i];){while(raw[i]=='/')i++;if(!raw[i])break;size_t begin=i;while(raw[i]&&raw[i]!='/')i++;size_t len=i-begin;
        if(len==1&&raw[begin]=='.')continue;
        if(len==2&&raw[begin]=='.'&&raw[begin+1]=='.'){if(n>1){n--;while(n>1&&out[n-1]!='/')n--;}continue;}
        if(n>1&&out[n-1]!='/')out[n++]='/';
        for(size_t j=0;j<len&&n<127;j++)out[n++]=raw[begin+j];
    }if(n>1&&out[n-1]=='/')n--;out[n]=0;
}
static int directory_exists(const char*p){if(streq(p,"/")||streq(p,"/dev")||streq(p,"/proc")||streq(p,"/mnt")||streq(p,"/mnt/os64")||streq(p,"/home")||streq(p,"/root")||streq(p,"/var")||streq(p,"/tmp")||streq(p,"/usr")||streq(p,"/bin")||streq(p,"/sbin")||streq(p,"/etc")||streq(p,"/lib"))return 1;if(starts(p,"/home/")&&p[6]){disk_file_t d;for(unsigned i=0;disk_file_at(i,&d);i++){size_t j=0;while(d.name[j]&&p[6+j]&&((d.name[j]|32)==(p[6+j]|32)))j++;if(!d.name[j]&&!p[6+j]&&d.type)return 1;}}fs_file_t f;return fs_find(p,&f)&&f.type=='5';}
static void c_cd(const char*a){char path[128];while(*a==' ')a++;resolve_path(*a?a:"/home/root",path);if(!directory_exists(path)){P("cd: no such directory: ");P(path);display_putc('\n');last_status=1;return;}if(!vfs_check_permission(path,VFS_ACCESS_EXECUTE)){P("cd: ");P(path);P(": Permission denied\n");last_status=1;return;}size_t i=0;while(path[i]){cwd[i]=path[i];i++;}cwd[i]=0;}
static int hidden_file(const char*n){return !ofp_allowed(n,OFP_READ);}
static int protect_write(const char*p){return ofp_allowed(p,OFP_WRITE);}
static int disk_hidden(const char*n){return starts(n,"USERS.")||starts(n,"HOME.");}
static void c_ls(const char*a){int all=0,long_form=0;while(*a==' ')a++;while(*a=='-'){a++;if(!*a){P("ls: invalid option\n");last_status=2;return;}while(*a&&*a!=' '){if(*a=='a')all=1;else if(*a=='l')long_form=1;else{P("ls: invalid option -- '");display_putc(*a);P("'\n");last_status=2;return;}a++;}while(*a==' ')a++;}char path[128];resolve_path(*a?a:cwd,path);if(!directory_exists(path)){P("ls: cannot access '");P(path);P("': No such directory\n");last_status=2;return;}if(all){P(long_form?"dr-xr-xr-x .\ndr-xr-xr-x ..\n":"d .\nd ..\n");}vfs_dirent_t e;for(unsigned i=0;vfs_readdir(path,i,&e);i++){char full[224];size_t n=0;for(size_t j=0;path[j]&&n<223;j++)full[n++]=path[j];if(n>1&&full[n-1]!='/')full[n++]='/';for(size_t j=0;e.name[j]&&n<223;j++)full[n++]=e.name[j];full[n]=0;if(hidden_file(full)||disk_hidden(e.name))continue;if(long_form){if(e.type==VFS_TYPE_DIRECTORY)P("dr-xr-xr-x ");else if(e.type==VFS_TYPE_CHAR_DEVICE)P("crw-rw-rw- ");else if(e.type==VFS_TYPE_BLOCK_DEVICE)P("brw-rw---- ");else P("-r--r--r-- ");}else{if(e.type==VFS_TYPE_DIRECTORY)P("d ");else if(e.type==VFS_TYPE_CHAR_DEVICE)P("c ");else if(e.type==VFS_TYPE_BLOCK_DEVICE)P("b ");else P("- ");}P(e.name);display_putc('\n');}}
static void c_cat(const char*a){while(*a==' ')a++;if(!*a){P("cat: missing operand\n");last_status=2;return;}char path[128];resolve_path(a,path);if(!ofp_allowed(path,OFP_READ)){P("cat: ");P(path);P(": Operation not permitted\n");last_status=1;return;}unsigned char data[512];size_t n;vfs_stat_t st;if(!vfs_read(path,data,sizeof data,&n,&st)){P("cat: cannot open '");P(path);P("'\n");last_status=1;return;}display_styled(data,n);if(n&&data[n-1]!='\n')display_putc('\n');}
static void hexbyte(unsigned char v){const char*h="0123456789abcdef";display_putc(h[v>>4]);display_putc(h[v&15]);}
static void c_xxd(const char*a){while(*a==' ')a++;if(!*a){P("xxd: missing operand\n");last_status=2;return;}char p[128];resolve_path(a,p);if(!ofp_allowed(p,OFP_READ)){P("xxd: Operation not permitted\n");last_status=1;return;}unsigned char data[512];size_t n;vfs_stat_t st;if(!vfs_read(p,data,sizeof data,&n,&st)){P("xxd: cannot read '");P(p);P("'\n");last_status=1;return;}for(size_t i=0;i<n;i+=16){hexbyte(i>>8);hexbyte(i);P(": ");for(size_t j=0;j<16;j++){if(i+j<n)hexbyte(data[i+j]);else P("  ");display_putc(' ');}P(" ");for(size_t j=0;j<16&&i+j<n;j++){unsigned char c=data[i+j];display_putc(c>=32&&c<127?(char)c:'.');}display_putc('\n');}}
static unsigned parse_size(const char*s,int*ok){unsigned long n=0;*ok=0;if(!*s)return 0;while(*s>='0'&&*s<='9'){if(n>16384){return 0;}n=n*10+(unsigned)(*s-'0');s++;}if(*s=='k'||*s=='K'){n*=1024;s++;}if(*s)return 0;*ok=n<=16384;return (unsigned)n;}
static int file_read_arg(const char*a,char*path,unsigned char*data,size_t cap,size_t*n,vfs_stat_t*st){while(*a==' ')a++;if(!*a)return 0;resolve_path(a,path);return ofp_allowed(path,OFP_READ)&&vfs_read(path,data,cap,n,st);}
static void c_dd(const char*a){char in[128]={0},out[128]={0};unsigned bs=512,count=0;while(*a){while(*a==' ')a++;if(!*a)break;const char*start=a;while(*a&&*a!=' ')a++;size_t len=(size_t)(a-start);if(len>3&&start[0]=='i'&&start[1]=='f'&&start[2]=='='){size_t n=0;for(size_t i=3;i<len&&n<127;i++)in[n++]=start[i];in[n]=0;}else if(len>3&&start[0]=='o'&&start[1]=='f'&&start[2]=='='){size_t n=0;for(size_t i=3;i<len&&n<127;i++)out[n++]=start[i];out[n]=0;}else if(len>3&&start[0]=='b'&&start[1]=='s'&&start[2]=='='){char v[16];size_t n=0;for(size_t i=3;i<len&&n<15;i++)v[n++]=start[i];v[n]=0;int ok;bs=parse_size(v,&ok);if(!ok||!bs){P("dd: invalid block size\n");return;}}else if(len>6&&starts(start,"count=")){char v[16];size_t n=0;for(size_t i=6;i<len&&n<15;i++)v[n++]=start[i];v[n]=0;int ok;count=parse_size(v,&ok);if(!ok){P("dd: invalid count\n");return;}}else{P("dd: unrecognized operand\n");return;}}if(!in[0]||!out[0]){P("dd: usage: dd if=FILE of=FILE [bs=N] [count=N]\n");return;}char ip[128],op[128];resolve_path(in,ip);resolve_path(out,op);if(!ofp_allowed(ip,OFP_READ)||!protect_write(op)){P("dd: Operation not permitted\n");return;}size_t requested=count?(size_t)bs*count:sizeof installed_image;if(requested>sizeof installed_image){P("dd: requested transfer exceeds 256 KiB\n");return;}size_t n,written;vfs_stat_t st;if(!vfs_read(ip,installed_image,requested,&n,&st)){P("dd: failed to read input\n");return;}if(count&&n>requested)n=requested;int result=vfs_write(op,installed_image,n,&written);if(result==VFS_WRITE_NO_SPACE){P("dd: error writing output: No space left on device\n");return;}if(result!=VFS_WRITE_OK||written!=n){P("dd: error writing output\n");return;}display_number((n+bs-1)/bs);P("+0 records in\n");display_number((written+bs-1)/bs);P("+0 records out\n");display_number(written);P(" bytes copied\n");}
static int line_args(const char*a,unsigned*lines,const char**file){*lines=10;while(*a==' ')a++;if(starts(a,"-n ")){a+=3;while(*a==' ')a++;char v[16];unsigned n=0;while(*a&&*a!=' '&&n<15)v[n++]=*a++;v[n]=0;int ok;*lines=parse_size(v,&ok);if(!ok)return 0;}while(*a==' ')a++;*file=a;return **file!=0;}
static void c_head(const char*a){unsigned lines;const char*f;if(!line_args(a,&lines,&f)){P("head: usage: head [-n LINES] FILE\n");return;}char p[128];size_t n;vfs_stat_t st;if(!file_read_arg(f,p,installed_image,sizeof installed_image,&n,&st)){P("head: cannot read file\n");return;}unsigned seen=0;for(size_t i=0;i<n&&seen<lines;i++){display_putc((char)installed_image[i]);if(installed_image[i]=='\n')seen++;}if(n&&seen<lines&&installed_image[n-1]!='\n')display_putc('\n');}
static void c_tail(const char*a){unsigned lines;const char*f;if(!line_args(a,&lines,&f)){P("tail: usage: tail [-n LINES] FILE\n");return;}char p[128];size_t n;vfs_stat_t st;if(!file_read_arg(f,p,installed_image,sizeof installed_image,&n,&st)){P("tail: cannot read file\n");return;}size_t start=n;unsigned seen=0;while(start&&seen<=lines){start--;if(installed_image[start]=='\n'&&start+1<n){if(seen==lines){start++;break;}seen++;}}for(size_t i=start;i<n;i++)display_putc((char)installed_image[i]);if(n&&installed_image[n-1]!='\n')display_putc('\n');}
static void c_wc(const char*a){while(*a==' ')a++;int only=0;if(starts(a,"-l ")){only=1;a+=3;}else if(starts(a,"-w ")){only=2;a+=3;}else if(starts(a,"-c ")){only=3;a+=3;}char p[128];size_t n;vfs_stat_t st;if(!file_read_arg(a,p,installed_image,sizeof installed_image,&n,&st)){P("wc: cannot read file\n");return;}unsigned lines=0,words=0,inword=0;for(size_t i=0;i<n;i++){unsigned char c=installed_image[i];if(c=='\n')lines++;int space=c==' '||c=='\n'||c=='\t'||c=='\r';if(!space&&!inword){words++;inword=1;}if(space)inword=0;}if(!only||only==1){display_number(lines);display_putc(' ');}if(!only||only==2){display_number(words);display_putc(' ');}if(!only||only==3){display_number(n);display_putc(' ');}P(p);display_putc('\n');}
static const char*backend_name(vfs_backend_t b){if(b==VFS_INITRAMFS)return "initramfs";if(b==VFS_FAT32)return "fat32";if(b==VFS_EXT2)return varfs_type();if(b==VFS_TMPFS)return "tmpfs";if(b==VFS_PROCFS)return "procfs";if(b==VFS_DEVFS)return "devfs";return "ramfs";}
static void c_stat(const char*a){while(*a==' ')a++;if(!*a){P("stat: missing operand\n");last_status=2;return;}char p[128];resolve_path(a,p);vfs_stat_t st;if(!vfs_stat_path(p,&st)){P("stat: cannot stat '");P(p);P("'\n");last_status=1;return;}P("  File: ");P(p);P("\n  Size: ");display_number(st.size);P("\tType: ");P(st.type==VFS_TYPE_DIRECTORY?"directory":st.type==VFS_TYPE_CHAR_DEVICE?"character device":st.type==VFS_TYPE_BLOCK_DEVICE?"block device":"regular file");P("\nDevice: ");P(backend_name(st.backend));P("\tMode: ");display_number(st.mode);P("\tUid: ");display_number(st.uid);P("\tGid: ");display_number(st.gid);P("\nMount: ");P(vfs_mount_source(p));display_putc('\n');}
static void c_basename(const char*a){while(*a==' ')a++;if(!*a){P("basename: missing operand\n");return;}const char*b=a;for(const char*p=a;*p&&*p!=' ';p++)if(*p=='/'&&p[1])b=p+1;while(*b&&*b!=' '){display_putc(*b);b++;}display_putc('\n');}
static void c_dirname(const char*a){while(*a==' ')a++;if(!*a){P("dirname: missing operand\n");return;}size_t n=0,last=0;while(a[n]&&a[n]!=' '){if(a[n]=='/')last=n;n++;}if(!last){P(".\n");return;}if(last==0){P("/\n");return;}for(size_t i=0;i<last;i++)display_putc(a[i]);display_putc('\n');}
static void c_truncate(const char*a){while(*a==' ')a++;if(!starts(a,"-s ")){P("truncate: usage: truncate -s SIZE FILE\n");return;}a+=3;while(*a==' ')a++;char v[16];unsigned z=0;while(*a&&*a!=' '&&z<15)v[z++]=*a++;v[z]=0;int ok;unsigned target=parse_size(v,&ok);while(*a==' ')a++;if(!ok||!*a){P("truncate: invalid size or missing file\n");return;}char p[128];resolve_path(a,p);if(!protect_write(p)){P("truncate: Operation not permitted\n");return;}size_t n=0,written;vfs_stat_t st;if(vfs_read(p,installed_image,sizeof installed_image,&n,&st)){if(n>target)n=target;}else n=0;while(n<target)installed_image[n++]=0;int result=vfs_write(p,installed_image,target,&written);if(result==VFS_WRITE_NO_SPACE)P("truncate: No space left on device\n");else if(result!=VFS_WRITE_OK||written!=target)P("truncate: write failed\n");}
static void c_fill(const char*a){while(*a==' ')a++;uint32_t limit=1;if(*a){if(streq(a,"-b"))limit=2048;else if(streq(a,"-a"))limit=0xffffffffu;else{P("fill: usage: fill [-b|-a]\n");return;}}if(!disk_mounted()){P("fill: /home is not mounted\n");return;}uint32_t before=disk_free_clusters(),added=disk_fill(limit),after=disk_free_clusters();P("fill: allocated ");display_number(added*512u);P(" bytes to /home/FILL.BIN; ");display_number(after*512u);P(" bytes free\n");if(!added&&before)P("fill: directory or I/O limit reached\n");else if(!after)P("fill: No space left on device\n");}
static void result_line(const char*text,int ok){display_color(ok?0x0a:0x0c);P(ok?"[ OK ] ":"[FAIL] ");display_color(0x07);P(text);display_putc('\n');}
 static void c_install_apps(const char*a){while(*a==' ')a++;char source[64],name[16],path[96];size_t n=0;while(*a&&*a!=' '&&n<63)source[n++]=*a++;source[n]=0;while(*a==' ')a++;n=0;while(*a&&*a!=' '&&n<15)name[n++]=*a++;name[n]=0;if(!source[0]){P("install-apps: usage: install-apps PROGRAM [NAME]\n");return;}if(!varfs_mounted()){P("install-apps: /var is not mounted\n");return;}fs_file_t app;int found=0;if(source[0]=='/')found=fs_find(source,&app);else{make_path(path,"bin/",source);found=fs_find(path,&app);if(!found){make_path(path,"sbin/",source);found=fs_find(path,&app);}if(!found){make_path(path,"usr/bin/",source);found=fs_find(path,&app);}}if(!found||app.type=='5'){P("install-apps: source executable not found in initramfs\n");return;}if(app.size>sizeof installed_image){P("install-apps: executable exceeds 256 KiB installation limit\n");return;}if(!name[0]){const char*p=source;for(size_t i=0;source[i];i++)if(source[i]=='/')p=source+i+1;n=0;while(p[n]&&n<15){name[n]=p[n];n++;}name[n]=0;}n=0;const char*prefix="/var/apps/";while(prefix[n]){path[n]=prefix[n];n++;}for(size_t i=0;name[i]&&n<sizeof path-1;i++)path[n++]=name[i];path[n]=0;if(!varfs_store(path,app.data,app.size)||!varfs_chmod(path,0755)||!varfs_sync()){P("install-apps: write failed\n");return;}P("Installed application ");P(source);P(" as ");P(name);P(" in /var/apps\n");}
typedef struct{const char*name,*version,*category,*license,*upstream,*payload,*sha256;int essential;const char*description;}package_t;
#define PACKAGE_ROW(name,version,category,license,upstream,payload,sha256,essential,description) {name,version,category,license,upstream,payload,sha256,essential,description},
static const package_t packages[]={OS64_PACKAGE_CATALOG(PACKAGE_ROW)};
#undef PACKAGE_ROW
static const package_t*package_find(const char*n){for(size_t i=0;i<sizeof packages/sizeof packages[0];i++)if(streq(n,packages[i].name))return &packages[i];return 0;}
static void package_path(char*out,const char*prefix,const char*name){size_t n=0;while(prefix[n]){out[n]=prefix[n];n++;}for(size_t i=0;name[i]&&n<95;i++)out[n++]=name[i];out[n]=0;}
static int package_installed(const char*name){char path[96];size_t size;uint16_t mode;package_path(path,"/var/apps/",name);return varfs_mounted()&&varfs_stat(path,&size,&mode,0,0);}
static int buffer_contains(const char*buffer,size_t length,const char*word){size_t wn=0;while(word[wn])wn++;if(!wn||wn>length)return 0;for(size_t i=0;i+wn<=length;i++){size_t j=0;while(j<wn&&buffer[i+j]==word[j])j++;if(j==wn)return 1;}return 0;}
static int package_http_body(const char**body,size_t*length){size_t total=*length;if(!starts(http_output,"HTTP/1.1 200")&&!starts(http_output,"HTTP/1.0 200"))return -1;for(size_t i=0;i+3<total;i++)if(http_output[i]=='\r'&&http_output[i+1]=='\n'&&http_output[i+2]=='\r'&&http_output[i+3]=='\n'){*body=http_output+i+4;*length=total-i-4;return 1;}return -2;}
static int package_fetch(const char*relative,const char**body,size_t*length){char path[192]="/lunadevph/OS64/main/";size_t n=21,i=0;while(relative[i]&&n+1<sizeof path)path[n++]=relative[i++];if(relative[i])return -5;path[n]=0;unsigned detail=0;int result=tls_https_get("raw.githubusercontent.com",path,http_output,sizeof http_output-1,length,&detail);if(result!=1)return result;http_output[*length]=0;return package_http_body(body,length);}
static int base64_value(unsigned char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;}
static int package_decode(const char*input,size_t length,size_t*out_length){unsigned value=0,bits=0,padding=0;size_t out=0;for(size_t i=0;i<length;i++){unsigned char c=(unsigned char)input[i];if(c==' '||c=='\r'||c=='\n'||c=='\t')continue;if(c=='='){padding++;continue;}if(padding)return 0;int digit=base64_value(c);if(digit<0)return 0;value=(value<<6)|(unsigned)digit;bits+=6;if(bits>=8){bits-=8;if(out>=sizeof installed_image)return 0;installed_image[out++]=(unsigned char)(value>>bits);value&=(1u<<bits)-1u;}}if(padding>2||bits>=6||out<20)return 0;*out_length=out;return 1;}
static int package_valid_elf(size_t size){return size>=64&&installed_image[0]==0x7f&&installed_image[1]=='E'&&installed_image[2]=='L'&&installed_image[3]=='F'&&installed_image[4]==2&&installed_image[5]==1&&installed_image[18]==0x3e&&installed_image[19]==0;}
static int package_hash_valid(const package_t*p,size_t size){uint8_t digest[32];crypto_sha256(installed_image,size,digest);for(unsigned i=0;i<32;i++){char a=p->sha256[i*2],b=p->sha256[i*2+1];unsigned hi=(unsigned)(a<='9'?a-'0':a-'a'+10),lo=(unsigned)(b<='9'?b-'0':b-'a'+10);if(digest[i]!=(uint8_t)((hi<<4)|lo))return 0;}return 1;}
static int package_install(const package_t*p){char target[96];const char*body=0;size_t body_length=0,image_length=0;if(package_installed(p->name))return 1;if(!varfs_mounted())return -1;P("pm: downloading https://raw.githubusercontent.com/");P("lunadevph/OS64/main/");P(p->payload);P("\n");int fetched=package_fetch(p->payload,&body,&body_length);if(fetched!=1)return -2;if(!package_decode(body,body_length,&image_length)||!package_valid_elf(image_length))return -3;if(!package_hash_valid(p,image_length))return -5;package_path(target,"/var/apps/",p->name);if(!varfs_store(target,installed_image,image_length)||!varfs_chmod(target,0755)||!varfs_sync())return -4;return 0;}
static void pm_help(void){P("Usage: pm COMMAND [PACKAGE]\n\nInstall OS64 packages from the official HTTPS repository.\n\nCommands:\n  list               list available packages\n  info PACKAGE       show package information\n  install PACKAGE    download and install into /var/apps\n  install essentials install the essential package set\n  remove PACKAGE     remove an installed package\n  status             show installed package state\n  update             fetch and cache the remote catalog\n\nRepository:\n  https://raw.githubusercontent.com/lunadevph/OS64/main/packages\n\nOptions:\n  --help             display this help\n  --version          display version information\n");}
static void c_pm(const char*a){
 while(*a==' ')a++;
 if(!*a||streq(a,"--help")){pm_help();return;}
 if(streq(a,"--version")){P("OS64 pm " OS64_KERNEL_VERSION "\n");return;}
 char action[16],name[16];size_t n=0;while(*a&&*a!=' '&&n+1<sizeof action)action[n++]=*a++;action[n]=0;while(*a==' ')a++;n=0;while(*a&&*a!=' '&&n+1<sizeof name)name[n++]=*a++;name[n]=0;while(*a==' ')a++;if(*a){P("pm: too many arguments\n");last_status=2;return;}
 if(streq(action,"list")||streq(action,"status")){P("PACKAGE    VERSION  CATEGORY     STATE       DESCRIPTION\n");for(size_t i=0;i<sizeof packages/sizeof packages[0];i++){P(packages[i].name);size_t z=0;while(packages[i].name[z])z++;while(z++<11)display_putc(' ');P(packages[i].version);P("      ");P(packages[i].category);z=0;while(packages[i].category[z])z++;while(z++<13)display_putc(' ');P(package_installed(packages[i].name)?"installed   ":"available   ");P(packages[i].description);display_putc('\n');}return;}
 if(streq(action,"update")){const char*body=0;size_t length=0;P("pm: fetching repository catalog...\n");int result=package_fetch("packages/packages.json",&body,&length);if(result!=1||!buffer_contains(body,length,"\"schema\": 1")){P("pm: remote catalog download or validation failed\n");last_status=69;return;}for(size_t i=0;i<sizeof packages/sizeof packages[0];i++)if(!buffer_contains(body,length,packages[i].name)){P("pm: remote catalog is incomplete\n");last_status=65;return;}if(varfs_mounted()&&!varfs_store("/var/lib/os64/packages.json",(const unsigned char*)body,length))P("pm: warning: catalog validated but cache write failed\n");else if(varfs_mounted())varfs_sync();P("pm: remote catalog verified: ");display_number(OS64_PACKAGE_COUNT);P(" packages available\n");return;}
 if(!streq(action,"info")&&!streq(action,"install")&&!streq(action,"remove")){P("pm: unknown command '");P(action);P("'\n");pm_help();last_status=2;return;}
 if(streq(action,"install")&&streq(name,"essentials")){unsigned installed=0;if(!varfs_mounted()){P("pm: /var is not mounted; install OS64 or format the data disk first\n");last_status=1;return;}for(size_t i=0;i<sizeof packages/sizeof packages[0];i++)if(packages[i].essential){int result=package_install(&packages[i]);if(result<0){P("pm: failed to download or install ");P(packages[i].name);display_putc('\n');last_status=74;return;}if(result==0){P("pm: installed ");P(packages[i].name);display_putc('\n');installed++;}}P("pm: essential package set ready (");display_number(installed);P(" newly installed)\n");return;}
 const package_t*p=package_find(name);if(!name[0]||!p){P("pm: package '");P(name);P("' was not found\n");last_status=1;return;}
 if(streq(action,"info")){P("Package:   ");P(p->name);P("\nVersion:   ");P(p->version);P("\nCategory:  ");P(p->category);P("\nLicense:   ");P(p->license);P("\nUpstream:  ");P(p->upstream);P("\nEssential: ");P(p->essential?"yes\n":"no\n");P("Source:    raw.githubusercontent.com\nPayload:   ");P(p->payload);P("\nState:     ");P(package_installed(p->name)?"installed\n":"not installed\n");P("About:     ");P(p->description);display_putc('\n');return;}
 if(streq(action,"install")){P("pm: installing ");P(name);P(" ");P(p->version);P("...\n");int result=package_install(p);if(result==1){P("pm: ");P(name);P(" is already installed\n");return;}if(result==-1)P("pm: /var is not mounted; install OS64 or format the data disk first\n");else if(result==-2)P("pm: HTTPS download failed or repository returned an error\n");else if(result==-3)P("pm: payload is malformed or is not an OS64 x86_64 ELF\n");else if(result==-5)P("pm: SHA-256 payload verification failed; package was not installed\n");else if(result<0)P("pm: installation failed\n");else{P("pm: installed /var/apps/");P(name);P("\n");return;}last_status=result==-1?1:74;return;}
 if(streq(action,"remove")){char target[96];package_path(target,"/var/apps/",name);if(!package_installed(name)){P("pm: ");P(name);P(" is not installed\n");last_status=1;return;}if(!varfs_remove(target)||!varfs_sync()){P("pm: removal failed\n");last_status=74;return;}P("pm: removed ");P(name);P("\n");return;}
}
static void c_mkuser(const char*a){while(*a==' ')a++;char user[16];size_t u=0;while(*a&&*a!=' '&&u<15){char c=*a++;if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')){P("mkuser: invalid username\n");return;}user[u++]=c;}user[u]=0;if(u<3){P("mkuser: username must contain at least 3 characters\n");return;}if(!varfs_mounted()||!disk_mounted()){P("mkuser: /home and /var must be mounted\n");return;}P("New password: ");char pass[64];size_t pn=0;for(;;){char c=input();if(c=='\n'){display_putc('\n');break;}if((c=='\b'||c==127)&&pn)pn--;else if(c>=' '&&c<='~'&&pn<63)pass[pn++]=c;}pass[pn]=0;if(pn<8){P("mkuser: password must contain at least 8 characters\n");return;}if(!auth_add(user,pass)){P("mkuser: account already exists or table is full\n");return;}unsigned char shadow[4096];size_t sn=auth_export_shadow(shadow,sizeof shadow);if(!sn||!varfs_store("/var/lib/os64/shadow",shadow,sn)||!disk_create(user,1)||!disk_sync()){P("mkuser: failed to persist account\n");return;}P("Created account ");P(user);P(" with /home/");P(user);P("\n");}
static void c_ofp(const char*a){(void)a;P("OFP (OS64 File Protection): active\nPolicies loaded: ");display_number(ofp_policy_count());P("\nViolations: kernel panic NOT_PERMITTED\n");}
static void c_dmesg(const char*a){(void)a;log_dump();}
static void c_ps(const char*a){while(*a==' ')a++;if(*a&&!streq(a,"-a")&&!streq(a,"-e")){P("ps: usage: ps [-a|-e]\n");last_status=2;return;}P("PID  PPID TYPE    STATE     COMMAND\n1    0    init    running   /sbin/init\n");for(size_t i=0;i<service_count();i++){display_number(i+2);P(i+2<10?"    ":"   ");P("1    kworker ");const char*s=service_state_name((int)i);P(s);size_t n=0;while(s[n])n++;while(n++<9)display_putc(' ');P("[");P(service_name((int)i));P("]\n");}display_number(service_count()+2);P("   1    user    running   /bin/sh\n");}
static void two(uint8_t v){if(v<10)display_putc('0');display_number(v);}
static void signed_ms(int64_t ns){if(ns<0){display_putc('-');display_number((unsigned long)(-(ns/1000000ll)));}else{display_putc('+');display_number((unsigned long)(ns/1000000ll));}P(" ms");}
static void c_time(const char*a){while(*a==' ')a++;if(streq(a,"--sync")){P("Synchronizing with ");P(timed_server());P("... ");if(timed_sync())P("OK\n");else P("FAILED\n");return;}if(streq(a,"--status")){P("Timezone: ");P(timed_timezone());P("\nSource: ");P(timed_synchronized()?"NTP synchronized":"hardware RTC fallback");P("\nServer: ");P(timed_server());display_putc('\n');return;}if(*a){P("time: usage: time [--sync|--status]\n");return;}rtc_time_t t;if(!timed_now(&t)){P("time: timed service is stopped or RTC unavailable\n");return;}two(t.hour);display_putc(':');two(t.minute);display_putc(':');two(t.second);display_putc(' ');P(timed_timezone());display_putc('\n');}
static void ntp_help(void){P("Usage: ntp [COMMAND] [ARGUMENT]\n\nSynchronize and inspect the OS64 system clock.\n\nCommands:\n  status             show synchronization status\n  sync               synchronize immediately\n  server             show the configured NTP server\n  server HOST        set the NTP server\n  enable             enable automatic synchronization\n  disable            disable automatic synchronization\n\nOptions:\n  --help              display this help\n  --version           display version information\n");}
static int ntp_privileged(void){if(streq(current_user,"root"))return 1;P("ntp: permission denied\n");last_status=77;return 0;}
static void ntp_status(void){
 P("NTP service    : ");P(timed_enabled()?"enabled\n":"disabled\n");
 P("Synchronization: ");P(timed_synchronized()?"synchronized\n":"not synchronized\n");
 P("Server         : ");P(timed_server());display_putc('\n');
 P("Address        : ");if(timed_last_address())print_ip(timed_last_address());else P("unresolved");display_putc('\n');
 P("Stratum        : ");if(timed_last_stratum())display_number(timed_last_stratum());else P("-");display_putc('\n');
 P("Last sync      : ");if(timed_last_sync_epoch()){rtc_time_t t;if(timed_now(&t)){display_number(t.year);display_putc('-');two(t.month);display_putc('-');two(t.day);display_putc(' ');two(t.hour);display_putc(':');two(t.minute);display_putc(':');two(t.second);P(" GMT+8");}}else P("never");display_putc('\n');
 P("Clock offset   : ");if(timed_synchronized())signed_ms(timed_last_offset_ns());else P("-");display_putc('\n');
 P("Round trip     : ");if(timed_synchronized()){display_number((unsigned long)(timed_last_delay_ns()/1000000ll));P(" ms");}else P("-");display_putc('\n');
}
static void ntp_failure(int result){
 if(result==NETWORK_NTP_DOWN)P("ntp: network is unavailable\n");
 else if(result==NETWORK_NTP_DNS){P("ntp: could not resolve '");P(timed_server());P("'\n");}
 else if(result==NETWORK_NTP_ROUTE)P("ntp: could not reach the configured server\n");
 else if(result==NETWORK_NTP_SEND)P("ntp: failed to send request\n");
 else if(result==NETWORK_NTP_TIMEOUT)P("ntp: timed out waiting for server response\n");
 else{P("ntp: invalid response from server\nntp: system clock was not changed\n");}
}
static void c_ntp(const char*a){
 while(*a==' ')a++;
 if(!*a||streq(a,"status")){ntp_status();return;}
 if(streq(a,"--help")){ntp_help();return;}if(streq(a,"--version")){P(OS64_NAME " ntp " OS64_KERNEL_VERSION "\n");return;}
 if(streq(a,"server")){P(timed_server());display_putc('\n');return;}
 if(starts(a,"server ")){if(!ntp_privileged())return;a+=7;while(*a==' ')a++;char host[96];size_t n=0;while(*a&&*a!=' '&&n+1<sizeof host)host[n++]=*a++;host[n]=0;while(*a==' ')a++;if(*a||!n||!timed_set_server(host)){P("ntp: invalid hostname or configuration storage unavailable\n");last_status=2;return;}P("NTP server changed to ");P(host);P("\n");return;}
 if(streq(a,"enable")||streq(a,"disable")){if(!ntp_privileged())return;int on=streq(a,"enable");if(!timed_set_enabled(on)){P("ntp: could not save configuration\n");last_status=74;return;}P(on?"Automatic network time synchronization enabled.\n":"Automatic network time synchronization disabled.\n");return;}
 if(streq(a,"sync")||streq(a,"sync --verbose")){if(!ntp_privileged())return;uint32_t address;P("Resolving ");P(timed_server());P("... ");if(network_resolve(timed_server(),&address,4200000)!=1){P("FAILED\nntp: could not resolve '");P(timed_server());P("'\n");last_status=68;return;}P("OK\nConnecting to ");print_ip(address);P(":123... OK\nSending NTP request... OK\nReceiving NTP response... ");int result=timed_sync();if(result!=1){P("FAILED\n");ntp_failure(result);last_status=result==NETWORK_NTP_TIMEOUT?75:65;return;}P("OK\n\nServer       : ");P(timed_server());P("\nAddress      : ");print_ip(timed_last_address());P("\nStratum      : ");display_number(timed_last_stratum());P("\nClock offset : ");signed_ms(timed_last_offset_ns());P("\nRound trip   : ");display_number((unsigned long)(timed_last_delay_ns()/1000000ll));P(" ms\n\nSystem clock synchronized successfully.\nLocal time: ");rtc_time_t t;if(timed_now(&t)){two(t.hour);display_putc(':');two(t.minute);display_putc(':');two(t.second);P(" GMT+8\n");}return;}
 ntp_help();last_status=2;
}
static void c_date(const char*a){(void)a;rtc_time_t t;if(!timed_now(&t)){P("date: timed service is stopped or RTC unavailable\n");return;}display_number(t.year);display_putc('-');two(t.month);display_putc('-');two(t.day);display_putc(' ');two(t.hour);display_putc(':');two(t.minute);display_putc(':');two(t.second);display_putc(' ');P(timed_timezone());display_putc('\n');}
static void human_bytes(uint64_t bytes){const char*unit="B";uint64_t scale=1;if(bytes>=1024ull*1024ull*1024ull){unit="GiB";scale=1024ull*1024ull*1024ull;}else if(bytes>=1024ull*1024ull){unit="MiB";scale=1024ull*1024ull;}else if(bytes>=1024ull){unit="KiB";scale=1024ull;}display_number(bytes/scale);if(scale>1){display_putc('.');display_number((bytes%scale)*10/scale);}P(unit);}
static void c_free(const char*a){while(*a==' ')a++;int human=0;if(*a){if(streq(a,"-h")||streq(a,"--human"))human=1;else{P("free: usage: free [-h]\n");return;}}uint64_t physical=memory_total_kib()*1024ull;size_t heap=memory_heap_bytes(),used=memory_used_bytes(),available=memory_free_bytes();P("Physical usable: ");if(human)human_bytes(physical);else{display_number(physical/1024);P(" KiB");}P(" (Multiboot memory map)\nKernel heap:    total ");if(human)human_bytes(heap);else{display_number(heap/1024);P(" KiB");}P(", used ");if(human)human_bytes(used);else{display_number(used/1024);P(" KiB");}P(", free ");if(human)human_bytes(available);else{display_number(available/1024);P(" KiB");}P("\nAllocations:    ");display_number(memory_allocation_count());P(" active\nUnallocated physical pages require page-allocator accounting.\n");}
static void print_ip(uint32_t a){for(unsigned i=0;i<4;i++){if(i)display_putc('.');display_number((a>>(i*8))&255u);}}
static void c_ifconfig(const char*a){(void)a;if(!network_ready()){P("ifconfig: net0 is unavailable\n");return;}const uint8_t*m=network_mac();P("net0: flags=UP,RUNNING mtu 1500 driver ");P(network_driver());P("\n    inet ");print_ip(network_address());P(" netmask ");print_ip(network_netmask());P(" gateway ");print_ip(network_gateway());P("\n    dns ");print_ip(network_dns_server());P("\n    ether ");for(unsigned i=0;i<6;i++){if(i)display_putc(':');hexbyte(m[i]);}P("\n    RX packets ");display_number(network_rx_packets());P("  TX packets ");display_number(network_tx_packets());display_putc('\n');}
static int argument_word(const char*a,char*out,size_t cap){while(*a==' ')a++;size_t n=0;while(*a&&*a!=' '&&n+1<cap)out[n++]=*a++;out[n]=0;return n!=0;}
static void c_nslookup(const char*a){char name[128];if(!argument_word(a,name,sizeof name)){P("nslookup: usage: nslookup NAME\n");return;}P("Server:  ");print_ip(network_dns_server());P("\nAddress: ");print_ip(network_dns_server());P("#53\n\n");uint32_t address;int result=network_resolve(name,&address,4000000);if(result==1){P("Name:    ");P(name);P("\nAddress: ");print_ip(address);display_putc('\n');}else if(result==-1)P("nslookup: network is down\n");else if(result==-3)P("nslookup: invalid hostname\n");else if(result==-4)P("nslookup: no A record found\n");else P("nslookup: resolution failed or timed out\n");}
static void c_ping(const char*a){char name[128];if(!argument_word(a,name,sizeof name)){P("ping: usage: ping HOST\n");return;}uint32_t address;int resolved=network_resolve(name,&address,4000000);if(resolved!=1){P("ping: ");P(name);P(": Temporary failure in name resolution\n");return;}P("PING ");P(name);P(" (");print_ip(address);P(") from ");print_ip(network_address());P("\n");unsigned long rounds=0;uint8_t type=0,code=0;int result=network_ping(address,4000000,&rounds,&type,&code);if(result==1){P("64 bytes from ");print_ip(address);P(": icmp_seq=1 ttl=64 spins=");display_number(rounds);P("\n1 packets transmitted, 1 received\n");}else if(result==-1)P("ping: network is down\n");else if(result==-2)P("ping: next-hop ARP resolution failed\n");else if(result==-3){P("From ");print_ip(network_gateway());P(": Destination Unreachable (ICMP type ");display_number(type);P(" code ");display_number(code);P(")\n");}else P("ping: request timed out\n");}
static void c_route(const char*a){while(*a==' ')a++;if(streq(active_command,"ip")&&*a&&!streq(a,"route")){P("ip: usage: ip route\n");return;}P("10.0.2.0/24 dev net0 src 10.0.2.15\ndefault via 10.0.2.2 dev net0\n");}
static int parse_url(const char*url,int*secure,char*host,char*path){size_t u;if(starts(url,"https://")){*secure=1;u=8;}else if(starts(url,"http://")){*secure=0;u=7;}else return 0;size_t h=0;while(url[u]&&url[u]!='/'&&h<127)host[h++]=url[u++];host[h]=0;if(!h)return 0;size_t p=0;if(!url[u])path[p++]='/';else while(url[u]&&p<127)path[p++]=url[u++];path[p]=0;return 1;}
static int redirect_location(char*out,size_t cap){const char*key="Location:";for(size_t i=0;i+9<sizeof http_output&&http_output[i];i++){size_t j=0;while(key[j]&&http_output[i+j]==key[j])j++;if(!key[j]){i+=j;while(http_output[i]==' ')i++;size_t n=0;while(http_output[i]&&http_output[i]!='\r'&&http_output[i]!='\n'&&n+1<cap)out[n++]=http_output[i++];out[n]=0;return n!=0;}}return 0;}
static void tls_error(int result,unsigned detail){if(result==TLS_GET_DNS)P("curl: DNS resolution failed\n");else if(result==TLS_GET_TCP)P("curl: TCP connection failed\n");else if(result==TLS_GET_ENTROPY)P("curl: TLS entropy source unavailable\n");else if(result==TLS_GET_CLOCK)P("curl: certificate validation clock unavailable\n");else if(result==TLS_GET_CERTIFICATE){P("curl: certificate validation failed (BearSSL X.509 error ");display_number(detail);P(")\n");}else if(result==TLS_GET_HANDSHAKE){P("curl: TLS handshake failed (BearSSL error ");display_number(detail);P(")\n");}else{P("curl: TLS transport I/O failed (stage ");display_number(detail);P(")\n");}}
static int curl_stream_http(const char*host,const char*path){
    uint32_t address;
    if(network_resolve(host,&address,4000000)!=1)return -2;
    if(network_tcp_connect(address,80,4000000)!=1)return -3;
    char request[512];size_t n=0;
    const char*parts[]={"GET ",path," HTTP/1.1\r\nHost: ",host,
        "\r\nUser-Agent: OS64-curl/0.7\r\nAccept: */*\r\nConnection: close\r\n\r\n"};
    for(unsigned part=0;part<5;part++)
        for(size_t i=0;parts[part][i]&&n+1<sizeof request;i++)request[n++]=parts[part][i];
    if(network_tcp_send(request,n,4000000)!=1){network_tcp_close(10000);return -3;}
    int received=0;
    while(!process_cancel_requested()){
        size_t length=0;
        int result=network_tcp_receive(http_output,sizeof http_output,&length,250000);
        for(size_t i=0;i<length;i++)if(http_output[i]!='\r')display_putc(http_output[i]);
        if(length)received=1;
        if(result==0)break;
        if(result<0&&result!=-2){network_tcp_close(10000);return -4;}
    }
    network_tcp_close(10000);
    if(process_cancel_requested()){P("\n^C\n");return 130;}
    return received?1:-4;
}
static void c_curl(const char*a){
    while(*a==' ')a++;
    int follow=0,live=0;
    while(*a=='-'){
        if(a[1]=='L'&&(a[2]==' '||!a[2]))follow=1;
        else if(a[1]=='l'&&(a[2]==' '||!a[2]))live=1;
        else{P("curl: usage: curl [-L] [-l] URL\n");last_status=2;return;}
        a+=2;while(*a==' ')a++;
    }
    char url[256];
    if(!argument_word(a,url,sizeof url)){P("curl: usage: curl [-L] [-l] URL\n");last_status=2;return;}
    if(live&&!starts(url,"http://")&&!starts(url,"https://")){
        char expanded[256]="http://";size_t n=7,i=0;
        while(url[i]&&n+1<sizeof expanded)expanded[n++]=url[i++];
        expanded[n]=0;for(i=0;expanded[i];i++)url[i]=expanded[i];url[i]=0;
    }
    int was_secure=0;
    for(unsigned redirects=0;;redirects++){
        int secure;char host[128],path[128];
        if(!parse_url(url,&secure,host,path)){P("curl: URL must begin with http:// or https://\n");last_status=2;return;}
        if(was_secure&&!secure){P("curl: refusing HTTPS-to-HTTP redirect downgrade\n");last_status=1;return;}
        if(live){
            if(secure){P("curl: -l streaming currently supports HTTP endpoints only\n");last_status=2;return;}
            P("* Live stream from ");P(host);P(" (Ctrl+C to stop)\n");
            int result=curl_stream_http(host,path);
            last_status=result==130?130:(result==1?0:1);
            if(result==-2)P("curl: DNS resolution failed\n");
            else if(result==-3)P("curl: TCP connection failed\n");
            else if(result==-4)P("curl: stream receive failed\n");
            return;
        }
        size_t length=0;unsigned detail=0;
        P("* ");P(secure?"TLS connecting to ":"Connecting to ");P(host);P("\n");
        int result=secure?tls_https_get(host,path,http_output,sizeof http_output-1,&length,&detail):network_http_get(host,path,http_output,sizeof http_output-1,&length,4000000);
        if(result!=1){if(process_cancel_requested()){P("\n^C\n");last_status=130;}else if(secure)tls_error(result,detail);else if(result==-2)P("curl: DNS resolution failed\n");else P("curl: TCP connection failed\n");return;}
        http_output[length]=0;
        if(follow&&redirects<5&&redirect_location(url,sizeof url)){was_secure=secure;P("* Following redirect to ");P(url);P("\n");continue;}
        for(size_t i=0;i<length;i++)if(http_output[i]!='\r')display_putc(http_output[i]);
        if(length&&http_output[length-1]!='\n')display_putc('\n');
        return;
    }
}
static char ascii_lower(char c){return c>='A'&&c<='Z'?(char)(c+('a'-'A')):c;}
static int browser_tag(const char*tag,const char*name){size_t i=0;if(tag[0]=='/')tag++;while(name[i]&&ascii_lower(tag[i])==name[i])i++;return !name[i]&&(tag[i]==0||tag[i]==' '||tag[i]=='/');}
static void browser_render(const char*response,size_t length){
    size_t body=0;for(size_t i=0;i+3<length;i++)if(response[i]=='\r'&&response[i+1]=='\n'&&response[i+2]=='\r'&&response[i+3]=='\n'){body=i+4;break;}
    if(!body)for(size_t i=0;i+1<length;i++)if(response[i]=='\n'&&response[i+1]=='\n'){body=i+2;break;}
    int in_tag=0,skip=0,pending_space=0;char tag[20];size_t tn=0;unsigned column=0;
    for(size_t i=body;i<length;i++){
        char c=response[i];
        if(c=='<'){in_tag=1;tn=0;continue;}
        if(in_tag){if(c=='>'){tag[tn]=0;in_tag=0;if(browser_tag(tag,"script")||browser_tag(tag,"style"))skip=tag[0]!='/' ;if(browser_tag(tag,"p")||browser_tag(tag,"br")||browser_tag(tag,"div")||browser_tag(tag,"h1")||browser_tag(tag,"h2")||browser_tag(tag,"li")){if(column){display_putc('\n');column=0;}pending_space=0;}continue;}if(tn+1<sizeof tag)tag[tn++]=ascii_lower(c);continue;}
        if(skip)continue;
        if(c=='&'){const char*entity=response+i;if(i+5<length&&starts(entity,"&amp;")){c='&';i+=4;}else if(i+4<length&&starts(entity,"&lt;")){c='<';i+=3;}else if(i+4<length&&starts(entity,"&gt;")){c='>';i+=3;}else if(i+6<length&&starts(entity,"&quot;")){c='"';i+=5;}else if(i+6<length&&starts(entity,"&nbsp;")){c=' ';i+=5;}}
        if(c==' '||c=='\t'||c=='\r'||c=='\n'){pending_space=column!=0;continue;}
        if(pending_space){if(column+1>=display_width()){display_putc('\n');column=0;}else{display_putc(' ');column++;}pending_space=0;}
        if(column+1>=display_width()){display_putc('\n');column=0;}
        if((unsigned char)c>=32){display_putc(c);column++;}
    }
    if(column)display_putc('\n');
}
static void c_browser(const char*a){
    while(*a==' ')a++;
    if(!*a||streq(a,"--help")){P("Usage: browser URL\n\nFetch an HTTP or HTTPS page and render readable text.\nTLS uses the OS64 CA store, SNI, and hostname verification.\n");return;}
    char url[256];if(!argument_word(a,url,sizeof url)){last_status=2;return;}
    if(!starts(url,"http://")&&!starts(url,"https://")){char expanded[256]="https://";size_t n=8,i=0;while(url[i]&&n+1<sizeof expanded)expanded[n++]=url[i++];expanded[n]=0;for(i=0;expanded[i];i++)url[i]=expanded[i];url[i]=0;}
    int was_secure=0;
    for(unsigned redirects=0;redirects<=5;redirects++){
        int secure;char host[128],path[128];if(!parse_url(url,&secure,host,path)){P("browser: invalid URL\n");last_status=2;return;}
        if(was_secure&&!secure){P("browser: refusing HTTPS-to-HTTP redirect downgrade\n");last_status=1;return;}
        P("Loading ");P(url);P("...\n");size_t length=0;unsigned detail=0;
        int result=secure?tls_https_get(host,path,http_output,sizeof http_output-1,&length,&detail):network_http_get(host,path,http_output,sizeof http_output-1,&length,4000000);
        if(result!=1){P("browser: ");if(secure){if(result==TLS_GET_DNS)P("DNS resolution failed");else if(result==TLS_GET_TCP)P("TCP connection failed");else if(result==TLS_GET_CERTIFICATE)P("certificate validation failed");else if(result==TLS_GET_HANDSHAKE)P("TLS handshake failed");else P("HTTPS transport failed");if(detail){P(" (code ");display_number(detail);display_putc(')');}}else P(result==-2?"DNS resolution failed":"HTTP connection failed");display_putc('\n');last_status=1;return;}
        http_output[length]=0;char location[256];if(redirect_location(location,sizeof location)){was_secure=secure;if(location[0]=='/'){size_t n=0;const char*scheme=secure?"https://":"http://";for(size_t i=0;scheme[i]&&n+1<sizeof url;i++)url[n++]=scheme[i];for(size_t i=0;host[i]&&n+1<sizeof url;i++)url[n++]=host[i];for(size_t i=0;location[i]&&n+1<sizeof url;i++)url[n++]=location[i];url[n]=0;}else{size_t i=0;while(location[i]&&i+1<sizeof url){url[i]=location[i];i++;}url[i]=0;}continue;}
        P("\n");browser_render(http_output,length);return;
    }
    P("browser: too many redirects\n");last_status=1;
}
static void hexword(uint16_t value){hexbyte((unsigned char)(value>>8));hexbyte((unsigned char)value);}
static void c_lspci(const char*a){
    while(*a==' ')a++;
    if(*a&&!streq(a,"-n")){P("Usage: lspci [-n]\n");last_status=2;return;}
    int numeric=streq(a,"-n");
    pci_device_t device;for(unsigned i=0;pci_device_at(i,&device);i++){hexbyte(device.bus);display_putc(':');hexbyte(device.slot);display_putc('.');display_putc((char)('0'+device.function));P("  ");hexword(device.vendor);display_putc(':');hexword(device.device);if(!numeric){P("  ");P(pci_class_name(device.class_code,device.subclass));}display_putc('\n');}
    if(!pci_device_count()){P("lspci: no PCI devices discovered\n");last_status=1;}
}
static void c_disk(const char*a){(void)a;P("/dev/sda1: ");P(disk_label());P(disk_mounted()?" FAT32 mounted on /home\n":" not mounted\n");P("/dev/sda2: ");P(varfs_type());P(varfs_mounted()?" mounted on /var\n":" not mounted\n");}
static void c_chkfs(const char*a){while(*a==' ')a++;int home=0,native=0;if(!*a||streq(a,"-a"))home=native=1;else if(streq(a,"/dev/sda1")||streq(a,"/home"))home=1;else if(streq(a,"/dev/sda2")||streq(a,"/var"))native=1;else{P("chkfs: usage: chkfs [-a|/dev/sda1|/dev/sda2]\n");return;}int failed=0;if(home){uint32_t files=0,clusters=0;P("Checking /dev/sda1 (FAT32)... ");if(disk_formatted()&&disk_check(&files,&clusters)){P("clean, ");display_number(files);P(" entries, ");display_number(clusters);P(" clusters used\n");}else{P("FAILED\n");failed=1;}}if(native){unsigned files=0,blocks=0;P("Checking /dev/sda2 (");P(varfs_type());P(")... ");if(varfs_probe()&&varfs_check(&files,&blocks)){P("clean, ");display_number(files);P(" files, ");display_number(blocks);P(" blocks used\n");}else{P("FAILED\n");failed=1;}}last_status=failed?1:0;}
static void c_format(const char*a){while(*a==' ')a++;const char*target="/dev/sda";size_t i=0;while(target[i]&&a[i]==target[i])i++;if(target[i]||!(a[i]==0||a[i]==' ')){P("format: usage: format /dev/sda [ext2|ext3|ext4]\n");last_status=2;return;}a+=i;while(*a==' ')a++;unsigned level=4;if(*a){if(streq(a,"ext2"))level=2;else if(streq(a,"ext3"))level=3;else if(streq(a,"ext4"))level=4;else{P("format: unknown filesystem '");P(a);P("'\nformat: usage: format /dev/sda [ext2|ext3|ext4]\n");last_status=2;return;}}if(!disk_format()||!varfs_format_level(level)){P("format: I/O error\n");last_status=1;return;}disk_create("root",1);unsigned char shadow[4096];size_t n=auth_export_shadow(shadow,sizeof shadow);varfs_store("/var/lib/os64/shadow",shadow,n);disk_sync();P("Created MBR: /dev/sda1 FAT32 on /home, /dev/sda2 ");P(varfs_type());P(" on /var\n");}
static void c_mount(const char*a){while(*a==' ')a++;if(*a&&!streq(a,"-a")){P("mount: usage: mount [-a]\n");return;}P("initramfs on / type initramfs (ro)\nprocfs on /proc type procfs (ro)\ntmpfs on /tmp type tmpfs (rw)\ndevfs on /dev type devfs (rw)\n");if(disk_mount())P("/dev/sda1 on /home type fat32 (rw)\n/dev/sda1 on /mnt/os64 type fat32 (rw)\n");if(varfs_mount()){P("/dev/sda2 on /var type ");P(varfs_type());P(" (rw)\n/dev/sda2 on /root type ");P(varfs_type());P(" (rw)\n/dev/sda2 on /mnt type ");P(varfs_type());P(" (rw)\n");}}
static void c_sync(const char*a){(void)a;P(disk_sync()?"Filesystems synchronized\n":"sync: I/O error\n");}
static int user_path(const char*p){return starts(p,"/mnt/")||starts(p,"/home/")||starts(p,"/root/")||starts(p,"/var/")||starts(p,"/tmp/");}
static void c_touch(const char*a){while(*a==' ')a++;if(!*a){P("touch: missing file operand\n");last_status=2;return;}char p[128];size_t written=0;resolve_path(a,p);if(!protect_write(p)){P("touch: cannot touch '");P(p);P("': Operation not permitted\n");last_status=1;return;}if(!user_path(p)||vfs_write(p,0,0,&written)!=VFS_WRITE_OK){P("touch: cannot create file on this filesystem\n");last_status=1;}}
static void c_mkdir(const char*a){while(*a==' ')a++;if(starts(a,"-p "))a+=3;while(*a==' ')a++;if(!*a){P("mkdir: missing operand\n");last_status=2;return;}char p[128];resolve_path(a,p);if(!protect_write(p)){P("mkdir: cannot create directory '");P(p);P("': Operation not permitted\n");last_status=1;return;}if(!user_path(p)||!disk_create(p,1)){P("mkdir: cannot create directory on this filesystem\n");last_status=1;}}
static void c_rm(const char*a){while(*a==' ')a++;int force=0;if(starts(a,"-f ")){force=1;a+=3;}while(*a==' ')a++;if(!*a){if(!force){P("rm: missing operand\n");last_status=2;}return;}char p[128];resolve_path(a,p);if(!protect_write(p)){P("rm: cannot remove '");P(p);P("': Operation not permitted\n");last_status=1;return;}if(!user_path(p)||!vfs_remove(p)){if(!force){P("rm: cannot remove '");P(p);P("'\n");last_status=1;}}}
static int two_args(const char*a,char*x,char*y){while(*a==' ')a++;int i=0;while(*a&&*a!=' '&&i<127)x[i++]=*a++;x[i]=0;while(*a==' ')a++;i=0;while(*a&&*a!=' '&&i<127)y[i++]=*a++;y[i]=0;return x[0]&&y[0];}
static void c_cp(const char*a){char x[128],y[128],xp[128],yp[128];size_t n,written;vfs_stat_t st;if(!two_args(a,x,y)){P("cp: missing operand\n");last_status=2;return;}resolve_path(x,xp);resolve_path(y,yp);if(!protect_write(yp)){P("cp: Operation not permitted\n");last_status=1;return;}if(!vfs_read(xp,installed_image,sizeof installed_image,&n,&st)||vfs_write(yp,installed_image,n,&written)!=VFS_WRITE_OK||written!=n){P("cp: cannot copy between these filesystems\n");last_status=1;}}
static void c_mv(const char*a){char x[128],y[128],xp[128],yp[128];size_t n,written;vfs_stat_t st;if(!two_args(a,x,y)){P("mv: missing operand\n");last_status=2;return;}resolve_path(x,xp);resolve_path(y,yp);if(!protect_write(xp)||!protect_write(yp)){P("mv: Operation not permitted\n");last_status=1;return;}if(!vfs_read(xp,installed_image,sizeof installed_image,&n,&st)||vfs_write(yp,installed_image,n,&written)!=VFS_WRITE_OK||written!=n||!vfs_remove(xp)){P("mv: cannot move between these filesystems\n");last_status=1;}}
#define NANO_CAPACITY 16384
static unsigned char nano_buffer[NANO_CAPACITY];
static unsigned char nano_cut[512];
static size_t nano_cut_length;
static void nano_text(size_t x,size_t y,size_t width,const char*s,uint8_t attr){
    size_t i=0;while(i<width){uint8_t c=s&&s[i]?(uint8_t)s[i]:' ';display_set_cell(x+i,y,c,attr);i++;if(!s||!s[i-1])s=0;}
}
static void nano_number_text(char*out,size_t cap,size_t value){
    char reverse[24];size_t n=0;if(!value)reverse[n++]='0';
    while(value&&n<sizeof reverse){reverse[n++]=(char)('0'+value%10);value/=10;}
    size_t i=0;while(n&&i+1<cap)out[i++]=reverse[--n];out[i]=0;
}
static size_t nano_line_of(const unsigned char*b,size_t position){
    size_t line=0;for(size_t i=0;i<position;i++)if(b[i]=='\n')line++;return line;
}
static size_t nano_line_start(const unsigned char*b,size_t position){
    while(position&&b[position-1]!='\n')position--;
    return position;
}
static size_t nano_line_end(const unsigned char*b,size_t length,size_t position){
    while(position<length&&b[position]!='\n')position++;
    return position;
}
static size_t nano_position_for_line(const unsigned char*b,size_t length,size_t line){
    size_t position=0;while(line&&position<length){if(b[position++]=='\n')line--;}return position;
}
static void nano_status(size_t width,size_t height,const char*message){
    nano_text(0,height-3,width,message,0x70);
}
static void nano_draw(const char*path,size_t length,size_t position,size_t top_line,size_t left_column,int modified,const char*message){
    size_t width=display_width(),height=display_height(),body=height>4?height-4:1;
    nano_text(0,0,width,"  OS64 nano 0.7",0x70);
    size_t name_x=20;if(name_x<width)nano_text(name_x,0,width-name_x,path,0x70);
    if(modified&&width>12)nano_text(width-12,0,12,"[ Modified ]",0x70);
    size_t source=nano_position_for_line(nano_buffer,length,top_line);
    for(size_t row=0;row<body;row++){
        size_t end=nano_line_end(nano_buffer,length,source),column=0,out=0;
        while(source<end&&out<width){
            unsigned char c=nano_buffer[source++];
            size_t advance=c=='\t'?4-(column%4):1;
            while(advance--){if(column>=left_column&&out<width)display_set_cell(out++,row+1,c=='\t'?' ':(c>=32&&c<127?c:'.'),0x07);column++;}
        }
        while(out<width)display_set_cell(out++,row+1,' ',0x07);
        if(source<length&&nano_buffer[source]=='\n')source++;
    }
    nano_status(width,height,message&&*message?message:"");
    nano_text(0,height-2,width,"^G Help   ^O Write Out   ^W Where Is   ^K Cut   ^U Paste",0x70);
    nano_text(0,height-1,width,"^X Exit   ^C Location    Arrows Move   Home/End   PgUp/PgDn",0x70);
    size_t line=nano_line_of(nano_buffer,position);
    size_t column=position-nano_line_start(nano_buffer,position);
    size_t cursor_y=line>=top_line?line-top_line+1:1;
    size_t cursor_x=column>=left_column?column-left_column:0;
    if(cursor_y>=height-3)cursor_y=height-4;
    if(cursor_x>=width)cursor_x=width-1;
    display_cursor_set(cursor_x,cursor_y,1,1);
}
static int nano_write(const char*path,size_t length,char*message,size_t message_cap){
    size_t written=0;int result=vfs_write(path,nano_buffer,length,&written);
    if(result!=VFS_WRITE_OK||written!=length){
        const char*error=result==VFS_WRITE_NO_SPACE?"Error writing file: No space left on device":"Error writing file";
        size_t i=0;while(error[i]&&i+1<message_cap){message[i]=error[i];i++;}message[i]=0;return 0;
    }
    char count[24];nano_number_text(count,sizeof count,written);
    const char*prefix="Wrote ";size_t n=0,i=0;while(prefix[i]&&n+1<message_cap)message[n++]=prefix[i++];
    i=0;while(count[i]&&n+1<message_cap)message[n++]=count[i++];
    const char*suffix=" bytes";i=0;while(suffix[i]&&n+1<message_cap)message[n++]=suffix[i++];message[n]=0;return 1;
}
static void c_nano(const char*a){
    while(*a==' ')a++;
    if(!*a){P("nano: missing filename\n");last_status=1;return;}
    if(display_width()<40||display_height()<8){P("nano: terminal must be at least 40x8\n");last_status=1;return;}
    char path[128];resolve_path(a,path);
    if(!protect_write(path)){P("nano: ");P(path);P(": Operation not permitted\n");last_status=1;return;}
    if(!user_path(path)){P("nano: read-only filesystem\n");last_status=1;return;}
    size_t length=0,position=0,top_line=0,left_column=0;vfs_stat_t st;
    if(!vfs_read(path,nano_buffer,sizeof nano_buffer,&length,&st))length=0;
    if(length>=sizeof nano_buffer){P("nano: file exceeds the 16 KiB editor limit\n");last_status=1;return;}
    int modified=0,running=1;char message[80]="New Buffer";
    display_cursor_set(0,0,0,1);
    while(running){
        size_t line=nano_line_of(nano_buffer,position),column=position-nano_line_start(nano_buffer,position);
        size_t body=display_height()>4?display_height()-4:1;
        if(line<top_line)top_line=line;else if(line>=top_line+body)top_line=line-body+1;
        if(column<left_column)left_column=column;else if(column>=left_column+display_width())left_column=column-display_width()+1;
        nano_draw(path,length,position,top_line,left_column,modified,message);message[0]=0;
        uint8_t key=(uint8_t)input();
        if(key==24){
            if(modified){
                nano_status(display_width(),display_height(),"Save modified buffer?  Y Yes   N No   ^C Cancel");
                uint8_t answer=(uint8_t)input();
                if(answer=='y'||answer=='Y'){if(nano_write(path,length,message,sizeof message)){modified=0;running=0;}}
                else if(answer=='n'||answer=='N')running=0;
                else{const char*s="Exit cancelled";size_t i=0;while(s[i]&&i+1<sizeof message){message[i]=s[i];i++;}message[i]=0;}
            }else running=0;
        }else if(key==15){
            if(nano_write(path,length,message,sizeof message))modified=0;
        }else if(key==23){
            char query[48];size_t query_length=0;
            for(;;){
                char prompt[64]="Search: ";size_t n=8;
                for(size_t i=0;i<query_length&&n+1<sizeof prompt;i++)prompt[n++]=query[i];
                prompt[n]=0;nano_status(display_width(),display_height(),prompt);
                display_cursor_set(n,display_height()-3,1,1);
                uint8_t search_key=(uint8_t)input();
                if(search_key=='\n')break;
                if(search_key==27){query_length=0;break;}
                if((search_key=='\b'||search_key==127)&&query_length)query_length--;
                else if(search_key>=32&&search_key<127&&query_length+1<sizeof query)query[query_length++]=(char)search_key;
            }
            if(query_length){
                size_t found=length,start=position<length?position+1:0;
                for(unsigned pass=0;pass<2&&found==length;pass++){
                    size_t begin=pass?0:start,end=pass?start:length;
                    for(size_t i=begin;i+query_length<=end;i++){
                        size_t j=0;while(j<query_length&&nano_buffer[i+j]==(unsigned char)query[j])j++;
                        if(j==query_length){found=i;break;}
                    }
                }
                if(found<length){position=found;const char*s="Search match";size_t i=0;while(s[i]&&i+1<sizeof message){message[i]=s[i];i++;}message[i]=0;}
                else{const char*s="Search text not found";size_t i=0;while(s[i]&&i+1<sizeof message){message[i]=s[i];i++;}message[i]=0;}
            }
        }else if(key==7){
            const char*s="Help: ^O save, ^X exit, ^W search, ^K cut, ^U paste";
            size_t i=0;while(s[i]&&i+1<sizeof message){message[i]=s[i];i++;}message[i]=0;
        }else if(key==3){
            char l[24],c[24];nano_number_text(l,sizeof l,line+1);nano_number_text(c,sizeof c,column+1);
            const char*p="line ";size_t n=0,i=0;while(p[i]&&n+1<sizeof message)message[n++]=p[i++];
            i=0;while(l[i]&&n+1<sizeof message)message[n++]=l[i++];
            p=", column ";i=0;while(p[i]&&n+1<sizeof message)message[n++]=p[i++];
            i=0;while(c[i]&&n+1<sizeof message)message[n++]=c[i++];message[n]=0;
        }else if(key==11){
            size_t begin=nano_line_start(nano_buffer,position),end=nano_line_end(nano_buffer,length,position);
            if(end<length)end++;
            nano_cut_length=end-begin;if(nano_cut_length>sizeof nano_cut)nano_cut_length=sizeof nano_cut;
            for(size_t i=0;i<nano_cut_length;i++)nano_cut[i]=nano_buffer[begin+i];
            size_t removed=end-begin;for(size_t i=end;i<length;i++)nano_buffer[i-removed]=nano_buffer[i];
            length-=removed;position=begin;modified=1;
        }else if(key==21){
            size_t amount=nano_cut_length;if(amount>sizeof nano_buffer-length)amount=sizeof nano_buffer-length;
            for(size_t i=length;i>position;i--)nano_buffer[i+amount-1]=nano_buffer[i-1];
            for(size_t i=0;i<amount;i++)nano_buffer[position+i]=nano_cut[i];
            position+=amount;length+=amount;if(amount)modified=1;
        }else if(key==0x11){
            size_t start=nano_line_start(nano_buffer,position);
            if(start){size_t previous_end=start-1,previous_start=nano_line_start(nano_buffer,previous_end);size_t target=previous_start+column;position=target<previous_end?target:previous_end;}
        }else if(key==0x12){
            size_t end=nano_line_end(nano_buffer,length,position);
            if(end<length){size_t next=end+1,next_end=nano_line_end(nano_buffer,length,next);size_t target=next+column;position=target<next_end?target:next_end;}
        }else if(key==0x13&&position)position--;
        else if(key==0x14&&position<length)position++;
        else if(key==0x91)position=nano_line_start(nano_buffer,position);
        else if(key==0x92)position=nano_line_end(nano_buffer,length,position);
        else if(key==0x93){size_t jump=body;while(jump--&&position){size_t start=nano_line_start(nano_buffer,position);position=start?start-1:0;}position=nano_line_start(nano_buffer,position);}
        else if(key==0x94){size_t jump=body;while(jump--&&position<length){position=nano_line_end(nano_buffer,length,position);if(position<length)position++;}}
        else if(key==0x96&&position<length){for(size_t i=position+1;i<length;i++)nano_buffer[i-1]=nano_buffer[i];length--;modified=1;}
        else if((key=='\b'||key==127)&&position){for(size_t i=position;i<length;i++)nano_buffer[i-1]=nano_buffer[i];position--;length--;modified=1;}
        else if((key=='\n'||key=='\t'||(key>=32&&key<127))&&length<sizeof nano_buffer){
            for(size_t i=length;i>position;i--)nano_buffer[i]=nano_buffer[i-1];
            nano_buffer[position++]=key;length++;modified=1;
        }
    }
    display_cursor_set(0,0,0,1);display_clear();last_status=0;
}
static void set_session_user(const char*user){if(!auth_select(user))return;size_t n=0;while(user[n]&&n<15){current_user[n]=user[n];n++;}current_user[n]=0;}
static size_t read_secret(const char*prompt,char*out,size_t cap){P(prompt);size_t n=0;for(;;){char c=input();if(c=='\n'){display_putc('\n');break;}if((c=='\b'||c==127)&&n)n--;else if(c>=' '&&c<='~'&&n+1<cap)out[n++]=c;}out[n]=0;return n;}
static int persist_accounts(void){unsigned char data[4096];size_t n=auth_export_shadow(data,sizeof data);if(!n||!varfs_store("/var/lib/os64/shadow",data,n))return 0;varfs_chmod("/var/lib/os64/shadow",0600);varfs_chown("/var/lib/os64/shadow",0,0,1,1);n=auth_export_accounts(data,sizeof data);if(!n||!varfs_store("/var/lib/os64/accounts",data,n))return 0;varfs_chmod("/var/lib/os64/accounts",0600);varfs_chown("/var/lib/os64/accounts",0,0,1,1);return varfs_sync();}
static int require_admin(const char*cmd){if(auth_is_admin())return 1;P(cmd);P(": permission denied\n");last_status=77;return 0;}
static int octal_mode(const char*s,uint16_t*out){unsigned v=0,n=0;while(*s>='0'&&*s<='7'&&n<4){v=(v<<3)+(unsigned)(*s-'0');s++;n++;}if(*s||!n||v>0777)return 0;*out=(uint16_t)v;return 1;}
static int split_two(const char*a,char*x,size_t xc,const char**rest){while(*a==' ')a++;size_t n=0;while(*a&&*a!=' '){if(n+1<xc)x[n++]=*a;a++;}x[n]=0;while(*a==' ')a++;*rest=a;return n&&*a;}
static uint16_t group_gid(const char*n){uint32_t g=auth_group_from_name(n);if(g==AUTH_GROUP_ROOT)return 0;if(g==AUTH_GROUP_ADMIN)return 10;if(g==AUTH_GROUP_USERS)return 100;if(g)return (uint16_t)(98u+__builtin_ctz(g));return 65535;}
static void c_chmod(const char*a){char mode[8];const char*file;if(!split_two(a,mode,sizeof mode,&file)){P("chmod: usage: chmod MODE FILE\n");last_status=2;return;}uint16_t m;if(!octal_mode(mode,&m)){P("chmod: invalid mode\n");last_status=2;return;}char p[128];resolve_path(file,p);if(!vfs_chmod(p,m)){P("chmod: changing permissions of '");P(p);P("': Operation not permitted\n");last_status=1;}}
static void c_chown(const char*a){char user[16];const char*file;if(!split_two(a,user,sizeof user,&file)){P("chown: usage: chown USER FILE\n");last_status=2;return;}uint16_t uid,gid;if(!auth_lookup(user,&uid,&gid,0,0)){P("chown: invalid user\n");last_status=1;return;}char p[128];resolve_path(file,p);if(!vfs_chown(p,uid,gid,1,1)){P("chown: Operation not permitted\n");last_status=1;}}
static void c_chgrp(const char*a){char group[16];const char*file;if(!split_two(a,group,sizeof group,&file)){P("chgrp: usage: chgrp GROUP FILE\n");last_status=2;return;}uint16_t gid=group_gid(group);if(gid==65535){P("chgrp: invalid group\n");last_status=1;return;}char p[128];resolve_path(file,p);if(!vfs_chown(p,0,gid,0,1)){P("chgrp: Operation not permitted\n");last_status=1;}}
static void print_octal(uint16_t v){display_putc('0');display_putc((char)('0'+((v>>6)&7)));display_putc((char)('0'+((v>>3)&7)));display_putc((char)('0'+(v&7)));display_putc('\n');}
static void c_umask(const char*a){while(*a==' ')a++;if(!*a){print_octal(auth_umask());return;}uint16_t m;if(!octal_mode(a,&m)){P("umask: invalid mask\n");last_status=2;return;}auth_set_umask(m);}
static auth_role_t parse_role(const char*s,int*ok){*ok=1;if(streq(s,"administrator")||streq(s,"admin"))return AUTH_ADMIN;if(streq(s,"power"))return AUTH_POWER;if(streq(s,"standard")||streq(s,"user"))return AUTH_STANDARD;if(streq(s,"guest"))return AUTH_GUEST;*ok=0;return AUTH_STANDARD;}
static int valid_username(const char*u){size_t n=0;while(u[n]){char c=u[n++];if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')||n>15)return 0;}return n>=3;}
static void c_useradd(const char*a){if(!require_admin("useradd"))return;while(*a==' ')a++;auth_role_t role=AUTH_STANDARD;if(starts(a,"-r ")){a+=3;char r[20];size_t n=0;while(*a&&*a!=' '&&n<19)r[n++]=*a++;r[n]=0;int ok;role=parse_role(r,&ok);if(!ok||(role==AUTH_ADMIN&&!auth_is_root())){P("useradd: invalid or unauthorized role\n");last_status=1;return;}while(*a==' ')a++;}char user[16];size_t n=0;while(*a&&*a!=' '&&n<15)user[n++]=*a++;user[n]=0;if(!valid_username(user)){P("useradd: invalid username\n");last_status=2;return;}char p1[64],p2[64];if(read_secret("New password: ",p1,sizeof p1)<8){P("useradd: password must contain at least 8 characters\n");last_status=1;return;}read_secret("Retype password: ",p2,sizeof p2);if(!streq(p1,p2)){P("useradd: passwords do not match\n");last_status=1;return;}if(!auth_add_role(user,p1,role)||!disk_create(user,1)||!persist_accounts()){P("useradd: could not create account\n");last_status=1;return;}P("useradd: created ");P(user);P(" (");P(auth_role_name(role));P(")\n");}
static void c_userdel(const char*a){if(!require_admin("userdel"))return;while(*a==' ')a++;if(!*a||streq(a,"root")||streq(a,current_user)||!auth_delete(a)||!persist_accounts()){P("userdel: cannot remove account\n");last_status=1;return;}P("userdel: account removed; home retained\n");}
static void c_usermod(const char*a){if(!require_admin("usermod"))return;while(*a==' ')a++;if(!starts(a,"-r ")){P("usermod: usage: usermod -r ROLE USER\n");last_status=2;return;}a+=3;char role_name[20];const char*user;if(!split_two(a,role_name,sizeof role_name,&user)){last_status=2;return;}int ok;auth_role_t role=parse_role(role_name,&ok);if(!ok||(role==AUTH_ADMIN&&!auth_is_root())||!auth_set_role(user,role)||!persist_accounts()){P("usermod: failed\n");last_status=1;}}
static void c_passwd(const char*a){while(*a==' ')a++;const char*user=*a?a:current_user;if(!streq(user,current_user)&&!auth_is_admin()){P("passwd: permission denied\n");last_status=77;return;}if(!auth_is_root()&&streq(user,current_user)){char old[64];read_secret("Current password: ",old,sizeof old);if(!auth_check(user,old)){P("passwd: Authentication failure\n");last_status=1;return;}}char p1[64],p2[64];if(read_secret("New password: ",p1,sizeof p1)<8){P("passwd: password too short\n");last_status=1;return;}read_secret("Retype password: ",p2,sizeof p2);if(!streq(p1,p2)||!auth_set_password(user,p1)||!persist_accounts()){P("passwd: password unchanged\n");last_status=1;return;}P("passwd: password updated successfully\n");}
static void c_groups(const char*a){(void)a;uint32_t g=auth_getgroups();for(unsigned i=0;i<10;i++)if(g&(1u<<i)){P(auth_group_name(i));display_putc(' ');}display_putc('\n');}
static void c_id(const char*a){(void)a;P("uid=");display_number(auth_getuid());P("(");P(auth_current_user());P(") gid=");display_number(auth_getgid());P(" role=");P(auth_role_name(auth_current_role()));P(" groups=");c_groups("");}
static void c_who(const char*a){(void)a;P(auth_current_user());P(" console  active\n");}
static void c_su(const char*a){while(*a==' ')a++;char user[16];size_t n=0;while(*a&&*a!=' '&&n<15)user[n++]=*a++;user[n]=0;if(!n){P("su: missing user\n");return;}if(auth_nologin(user)){P("su: account is not available\n");last_status=1;return;}if(!auth_is_root()){char pass[64];read_secret("Password: ",pass,sizeof pass);if(!auth_check(user,pass)){P("su: Authentication failure\n");last_status=1;return;}}set_session_user(user);P("Switched user to ");P(current_user);display_putc('\n');}
static void c_login(const char*a){c_su(a);}
static void c_logout(const char*a){(void)a;if(auth_current_role()==AUTH_GUEST)P("Guest session data discarded.\n");set_session_user("nobody");cwd[0]='/';cwd[1]=0;P("Logged out. Use login USER to start a session.\n");}
static void c_sudo(const char*a){while(*a==' ')a++;if(!*a){P("sudo: missing command\n");last_status=2;return;}if(!auth_is_root()&&!auth_is_admin()&&!auth_has_group(AUTH_GROUP_WHEEL)){P("sudo: user is not allowed to execute commands as root\n");last_status=77;return;}char original[16];size_t n=0;while(current_user[n]&&n<15){original[n]=current_user[n];n++;}original[n]=0;uint16_t uid=auth_getuid();uint64_t now=timed_monotonic_ns();if(!auth_is_root()&&(sudo_cached_uid!=uid||now>=sudo_cache_until)){char pass[64];read_secret("[sudo] password: ",pass,sizeof pass);if(!auth_check(original,pass)){log_submit(LOG_WARN,"security","failed sudo authentication");P("sudo: authentication failure\n");last_status=1;return;}sudo_cached_uid=uid;sudo_cache_until=now+300000000000ull;}char cmd[16];n=0;while(*a&&*a!=' '&&n<15)cmd[n++]=*a++;cmd[n]=0;while(*a==' ')a++;log_submit(LOG_INFO,"security","sudo command executed");set_session_user("root");int rc=abi_dispatch(cmd,a);set_session_user(original);last_status=rc;}
static void c_daemon(const char*a){int id=service_find(active_command);while(*a==' ')a++;if(!*a||streq(a,"status")){P(active_command);P(" is ");P(service_state_name(id));P(": ");P(service_description(id));P(" (exit ");display_number((unsigned long)service_exit_code(id));P(", generation ");display_number(service_generation(id));P(", cycles ");display_number(service_cycles(id));P(", errors ");display_number(service_errors(id));P(", metric ");display_number(service_metric(id));if(id==service_find("timed")){P(", updates ");display_number(timed_updates());P(", uptime ");display_number(timed_uptime());P("s");}P(")\n");return;}if(streq(a,"start")){P(active_command);P(service_wait_ready(id,8)?" ready\n":" failed to become ready\n");return;}if(streq(a,"stop")){service_stop(id);P(active_command);P(" stopped with exit 0\n");return;}if(streq(a,"restart")){service_stop(id);P(active_command);P(service_wait_ready(id,8)?" restarted and ready\n":" restart failed\n");return;}P(active_command);P(": usage: ");P(active_command);P(" [start|stop|restart|status]\n");}
static void stop_filesystems(void){P("[init] syncing filesystems... ");P(varfs_sync()&&disk_sync()?"done\n":"failed\n");if(varfs_mounted()){varfs_unmount();P("[init] unmounted /var\n");}if(disk_mounted()){disk_unmount();P("[init] unmounted /home\n");}}
static void c_reboot(const char*a){(void)a;if(!service_running(service_find("acpid"))){P("reboot: acpid service is stopped\n");return;}stop_filesystems();P("Rebooting through ACPI...\n");acpi_reboot();}
static void c_shutdown(const char*a){(void)a;if(!service_running(service_find("acpid"))){P("shutdown: acpid service is stopped\n");return;}stop_filesystems();P("Powering off through ACPI...\n");acpi_poweroff();c_halt(0);}
static void c_halt(const char*a){(void)a;P("System halted.\n");for(;;)__asm__ volatile("cli;hlt");}
static void c_init(const char*a){(void)a;P("init: PID 1 is already running\n");}
static void c_sh(const char*a){(void)a;P("sh: already running the OS64 shell\n");}
static void c_nologin(const char*a){(void)a;P("This account is not available.\n");}

static void command_usage(const char*name){P("Usage: ");P(name);if(streq(name,"cp")||streq(name,"mv"))P(" SOURCE DEST");else if(streq(name,"cd")||streq(name,"cat")||streq(name,"xxd")||streq(name,"hexdump")||streq(name,"touch")||streq(name,"mkdir")||streq(name,"rm")||streq(name,"nano")||streq(name,"su"))P(" OPERAND");else if(streq(name,"install-apps"))P(" PROGRAM [NAME]");else if(streq(name,"pm"))P(" COMMAND [PACKAGE]");else if(streq(name,"install"))P(" /dev/sda");else if(streq(name,"ping"))P(" HOST");else if(streq(name,"nslookup")||streq(name,"host"))P(" NAME");else if(streq(name,"curl"))P(" [-L] [-l] URL");else if(streq(name,"browser"))P(" URL");else if(streq(name,"lspci"))P(" [-n]");else if(streq(name,"ip"))P(" route");else if(streq(name,"fill"))P(" [-b|-a]");else if(streq(name,"dd"))P(" if=FILE of=FILE [bs=N] [count=N]");else if(streq(name,"head")||streq(name,"tail"))P(" [-n LINES] FILE");else if(streq(name,"wc"))P(" [-l|-w|-c] FILE");else if(streq(name,"stat")||streq(name,"basename")||streq(name,"dirname"))P(" FILE");else if(streq(name,"truncate"))P(" -s SIZE FILE");else if(streq(name,"time")||streq(name,"ntp"))P(" [--sync|--status]");else if(service_find(name)>=0)P(" [start|stop|restart|status]");else if(streq(name,"display"))P(" [status|modes|mode WIDTHxHEIGHT]");else if(streq(name,"format"))P(" [/dev/sda]");else if(streq(name,"ls")||streq(name,"find"))P(" [PATH]");else if(streq(name,"echo"))P(" [TEXT ...]");P("\nCommon options: -h, --help, --version\n");}
static int command_authorized(const char*n,const char*a){while(*a==' ')a++;if(streq(n,"install")||streq(n,"format")||streq(n,"reboot")||streq(n,"shutdown")||streq(n,"halt")||streq(n,"chown")){if(auth_is_root())return 1;}else if(streq(n,"useradd")||streq(n,"userdel")||streq(n,"usermod")||streq(n,"mkuser")||streq(n,"mount")||streq(n,"chgrp")){if(auth_is_admin())return 1;}else if(service_find(n)>=0&&*a&&!streq(a,"status")){if(auth_is_admin())return 1;}else if(streq(n,"install-apps")||(streq(n,"pm")&&(starts(a,"install ")||starts(a,"remove ")||streq(a,"update")))){if(auth_is_admin()||auth_current_role()==AUTH_POWER)return 1;}else return 1;P(n);P(": permission denied\n");last_status=77;return 0;}
static int abi_dispatch(const char*name,const char*args){if(!streq(name,"status"))last_status=0;if(!command_authorized(name,args))return last_status;size_t n=0;while(name[n]&&n<15){active_command[n]=name[n];n++;}active_command[n]=0;const char*a=args;while(*a==' ')a++;if(streq(name,"ntp")){c_ntp(args);return last_status;}if(streq(name,"pm")){c_pm(args);return last_status;}if(streq(name,"install")){c_install(args);return last_status;}if((streq(a,"-h")&&!streq(name,"free"))||streq(a,"--help")){command_usage(name);return 0;}if(streq(a,"--version")){P(name);P(" (" OS64_NAME " coreutils) " OS64_KERNEL_VERSION "\n");return 0;}for(size_t i=0;i<sizeof commands/sizeof commands[0];i++)if(streq(name,commands[i].name)){commands[i].run(args);return last_status;}P("ELF: unsupported application service\n");return 126;}
static void abi_write(const char*s){if(s){if(terminal_owned)ansi_write(s);else P(s);}}static void abi_putc(char c){if(terminal_owned){ansi_putchar((unsigned char)c);ansi_flush();}else display_putc(c);}
static int abi_read_file(const char*p,unsigned char*d,os64_size_t cap,os64_size_t*n){if(!p||!d||!n||!ofp_allowed(p,OFP_READ))return 0;size_t z;vfs_stat_t st;if(!vfs_read(p,d,(size_t)cap,&z,&st))return 0;*n=(os64_size_t)z;return 1;}
static int abi_write_file(const char*p,const unsigned char*d,os64_size_t n){if(!p||!d||!ofp_allowed(p,OFP_WRITE))return 0;size_t written=0;return vfs_write(p,d,(size_t)n,&written)==VFS_WRITE_OK&&written==(size_t)n;}
static void*abi_allocate(os64_size_t n){return kmalloc((size_t)n);}
static void abi_deallocate(void*p){kfree(p);}
static void*abi_reallocate(void*p,os64_size_t n){return krealloc(p,(size_t)n);}
static int abi_clock_get(os64_datetime_t*out){rtc_time_t t;if(!out||!timed_now(&t))return 0;out->year=t.year;out->month=t.month;out->day=t.day;out->hour=t.hour;out->minute=t.minute;out->second=t.second;return 1;}
static const char*abi_user(void){return current_user;}static const char*abi_cwd(void){return cwd;}
static size_t terminal_origin_x(void){size_t w=display_width();return w>80?(w-80)/2:0;}
static size_t terminal_origin_y(void){size_t h=display_height();return h>25?(h-25)/2:0;}
static int abi_terminal_acquire(void){if(terminal_owned)return 0;terminal_owned=1;terminal_saved_width=display_width();terminal_saved_height=display_height();for(size_t y=0;y<terminal_saved_height;y++)for(size_t x=0;x<terminal_saved_width;x++)terminal_saved[y*216+x]=display_cell(x,y);display_position(&terminal_saved_x,&terminal_saved_y);display_cursor_set(0,0,0,1);display_serial_cells(0);for(size_t y=0;y<terminal_saved_height;y++)for(size_t x=0;x<terminal_saved_width;x++)display_set_cell(x,y,' ',0x07);display_serial_cells(1);display_serial_clear();ansi_reset();return 1;}
static void abi_terminal_release(void){if(!terminal_owned)return;display_cursor_set(0,0,0,1);display_serial_cells(0);for(size_t y=0;y<terminal_saved_height;y++)for(size_t x=0;x<terminal_saved_width;x++){uint16_t c=terminal_saved[y*216+x];display_set_cell(x,y,(uint8_t)c,(uint8_t)(c>>8));}display_serial_cells(1);display_serial_clear();display_cursor_set(terminal_saved_x,terminal_saved_y,1,1);terminal_owned=0;}
static unsigned abi_terminal_key(void){if(!terminal_owned)return 0;return (unsigned)(uint8_t)input();}
static void abi_terminal_cell(unsigned x,unsigned y,unsigned ch,unsigned char attr){if(terminal_owned&&x<80&&y<25)display_set_cell(terminal_origin_x()+x,terminal_origin_y()+y,display_codepoint_glyph(ch),attr);}
static void abi_terminal_cursor(unsigned x,unsigned y,int visible,int block){if(terminal_owned)display_cursor_set(terminal_origin_x()+(x<80?x:79),terminal_origin_y()+(y<25?y:24),visible,block);}
static unsigned abi_terminal_width(void){return display_width()<80?(unsigned)display_width():80u;}static unsigned abi_terminal_height(void){return display_height()<25?(unsigned)display_height():25u;}
static unsigned abi_terminal_poll_key(void){
    if(!terminal_owned)return 0;
    char c=keyboard_poll();
    if(!c&&display_serial_available()){uint8_t status=inb(0x3fd);if(status!=0xff&&(status&1))c=(char)inb(0x3f8);}
    if(c)display_cursor_activity(timed_monotonic_ns());
    return (unsigned)(uint8_t)c;
}
static int abi_read_directory(const char*p,unsigned index,os64_dirent_t*out){if(!p||!out)return 0;vfs_dirent_t e;if(!vfs_readdir(p,index,&e))return 0;size_t i=0;while(e.name[i]&&i<95){out->name[i]=e.name[i];i++;}out->name[i]=0;out->type=e.type;out->backend=(unsigned char)e.backend;return 1;}
static unsigned abi_getuid(void){return auth_getuid();}static unsigned abi_getgid(void){return auth_getgid();}static int abi_setuid(unsigned uid){if(uid==auth_getuid())return 1;if(!auth_is_root()||!auth_select_uid((uint16_t)uid))return 0;size_t n=0;const char*u=auth_current_user();while(u[n]&&n<15){current_user[n]=u[n];n++;}current_user[n]=0;return 1;}static int abi_setgid(unsigned gid){return gid==auth_getgid();}static int abi_check_permission(const char*p,unsigned m){return vfs_check_permission(p,m&7u);}static unsigned abi_get_groups(void){return auth_getgroups();}static int abi_is_admin(void){return auth_is_admin();}static int abi_is_root(void){return auth_is_root();}
static unsigned long abi_system_query(const char*n){if(!n)return 0;if(streq(n,"process.interrupted"))return process_cancel_requested();if(streq(n,"time.monotonic_ms"))return (unsigned long)(timed_monotonic_ns()/1000000ull);if(streq(n,"time.uptime_seconds"))return timed_uptime();if(streq(n,"memory.total_kib"))return memory_total_kib();if(streq(n,"memory.heap_bytes"))return memory_heap_bytes();if(streq(n,"memory.used_bytes"))return memory_used_bytes();if(streq(n,"memory.free_bytes"))return memory_free_bytes();if(streq(n,"memory.allocations"))return memory_allocation_count();if(streq(n,"network.ready"))return network_ready();if(streq(n,"network.rx_packets"))return network_rx_packets();if(streq(n,"network.tx_packets"))return network_tx_packets();if(streq(n,"disk.mounted"))return disk_mounted();if(streq(n,"var.mounted"))return varfs_mounted();if(streq(n,"users.count"))return auth_user_count();if(streq(n,"terminal.serial"))return display_serial_available();if(streq(n,"terminal.unicode"))return 1;if(streq(n,"terminal.width"))return display_width();if(streq(n,"terminal.height"))return display_height();if(streq(n,"display.width"))return display_pixel_width();if(streq(n,"display.height"))return display_pixel_height();if(streq(n,"services.count"))return service_count();if(streq(n,"services.ready")){unsigned ready=0;for(size_t i=0;i<service_count();i++)if(service_ready((int)i))ready++;return ready;}if(starts(n,"service.")){int id=service_find(n+8);return id>=0?(unsigned long)service_running(id):0;}return 0;}
static const os64_api_t app_api={OS64_ABI_VERSION,abi_dispatch,abi_write,abi_putc,abi_read_file,abi_write_file,abi_allocate,abi_deallocate,abi_reallocate,abi_clock_get,abi_user,abi_cwd,abi_terminal_acquire,abi_terminal_release,abi_terminal_key,abi_terminal_cell,abi_terminal_cursor,abi_terminal_width,abi_terminal_height,abi_read_directory,abi_system_query,abi_terminal_poll_key,abi_getuid,abi_getgid,abi_setuid,abi_setgid,abi_check_permission,abi_get_groups,abi_is_admin,abi_is_root};
static void make_path(char*out,const char*dir,const char*name){while(*dir)*out++=*dir++;while(*name&&*name!=' ')*out++=*name++;*out=0;}
static void configure_shell(void){fs_file_t f;if(!fs_find("etc/shells",&f))return;size_t start=0,end=0;while(end<f.size&&f.data[end]!='\n'){if(f.data[end]=='/')start=end+1;end++;}size_t n=0;while(start<end&&n<15)shell_name[n++]=(char)f.data[start++];shell_name[n]=0;}
static int install_fail(const char*stage,const char*detail){result_line(stage,0);P("install: ");P(detail);display_putc('\n');last_status=1;return 0;}
static int install_confirm(void){
    char answer[8]={0};unsigned length=0;
    display_color(0x0e);P("[WARN] This will erase every partition on /dev/sda.\n");display_color(0x07);
    P("Type INSTALL to continue: ");
    for(;;){char c=input();if(c=='\n'){display_putc('\n');break;}if((c=='\b'||c==127)&&length){length--;display_putc('\b');}else if(c>=' '&&c<='~'&&length<7){answer[length++]=c;display_putc(c);}}
    return streq(answer,"INSTALL");
}
static void c_install(const char*a){
    while(*a==' ')a++;
    if(streq(a,"--help")){P("Usage: install /dev/sda\n\nInstall " OS64_NAME " onto the primary disk.\nAll existing data on the target is destroyed.\n");return;}
    if(!streq(current_user,"root")){P("install: permission denied\n");last_status=77;return;}
    if(!streq(a,"/dev/sda")){P("install: explicit target required\nUsage: install /dev/sda\n");last_status=2;return;}
    fs_file_t kernel,boot;
    if(!fs_find("boot/kernel.bin",&kernel)||!fs_find("boot/os64-boot.img",&boot)){install_fail("Checking installation media","kernel or bootloader payload is missing");return;}
    const unsigned char*is=bootopts_initrd_start();const unsigned char*ie=bootopts_initrd_end();
    if(!is||ie<=is){install_fail("Checking installation media","initramfs payload is missing");return;}
    P("\n" OS64_NAME " " OS64_KERNEL_VERSION " installer\nTarget: /dev/sda\nLayout: boot + /home (FAT32) + /var (ext4)\n\n");
    result_line("Checking installation media",1);
    if(!install_confirm()){P("Installation cancelled; the disk was not changed.\n");last_status=1;return;}
    P("\nPreparing disk...\n\n");
    if(!disk_format()){install_fail("Creating partition table","disk formatting failed");return;}
    result_line("Creating partition table",1);result_line("Formatting /home as FAT32",1);
    if(!varfs_format()){install_fail("Formatting /var as ext4","native filesystem formatting failed");return;}
    result_line("Formatting /var as ext4",1);
    if(!disk_install_boot_area(boot.data,boot.size)){install_fail("Installing bootloader","boot sector write failed");return;}
    result_line("Installing bootloader",1);
    if(!disk_store_large("KERNEL.BIN",kernel.data,kernel.size)||!disk_store_large("INITRD.TAR",is,(size_t)(ie-is))){install_fail("Installing system files","kernel or initramfs write failed");return;}
    result_line("Installing system files",1);
    if(!disk_create("root",1)){install_fail("Creating /home/root","home directory creation failed");return;}
    result_line("Creating /home/root",1);
    if(!persist_accounts()){install_fail("Installing account database","protected account data write failed");return;}
    result_line("Installing account database",1);
    if(!varfs_sync()||!disk_sync()){install_fail("Synchronizing filesystems","one or more writes could not be committed");return;}
    result_line("Synchronizing filesystems",1);
    P("\nInstallation complete.\n\n  System  " OS64_NAME " " OS64_KERNEL_VERSION "\n  Boot     installed\n  /home    FAT32\n  /var     ext4\n\nRemove the installation media and run reboot.\n");last_status=0;
}
static void boot_rule(void){size_t columns=display_width();if(columns>62)columns=62;while(columns--)display_putc('-');display_putc('\n');}
static void boot_service_result(int id,const char*name,const char*description,int ok){
    display_color(ok?0x0a:0x0c);P(ok?"[ OK ]":"[FAIL]");display_color(0x07);P(" [");P(name);P("] ");
    P(service_state_name(id));P(" pid=");display_number((unsigned)id+2);P(" exit=");display_number((unsigned)service_exit_code(id));
    display_color(0x08);P("  ");P(description);display_color(0x07);display_putc('\n');
    P("       cooperative background worker; cycles=");display_number(service_cycles(id));P(" metric=");display_number(service_metric(id));
    if(service_errors(id)){P(" errors=");display_number(service_errors(id));}display_putc('\n');
}
static void start_boot_services(void){
    static const char*n[]={"fsd","memoryd","displayd","graphicsd","timed","diskd","userd","acpid","netd","logd"};
    static const char*d[]={"Virtual filesystems","Memory manager","Console display","Framebuffer graphics","System clock","Storage devices","Accounts and sessions","Power management","Network stack","Kernel message logger"};
    boot_services_started=0;
    for(unsigned i=0;i<10;i++){
        if(bootopts_recovery()&&(streq(n[i],"diskd")||streq(n[i],"userd")||streq(n[i],"netd"))){
            display_color(0x0e);P("[SKIP]");display_color(0x07);P("  ");P(n[i]);
            size_t length=0;while(n[i][length])length++;while(length++<11)display_putc(' ');
            display_color(0x08);P("Disabled in recovery mode\n");display_color(0x07);continue;
        }
        int id=service_find(n[i]);int ok=service_wait_ready(id,8);
        boot_service_result(id,n[i],d[i],ok);if(ok)boot_services_started++;
    }
}
static void boot_report(void){if(!keyboard_ready){display_color(0x0e);P("[WARN] PS/2 keyboard unavailable; serial input active\n");display_color(0x07);}if(bootopts_debug()){display_color(0x0b);P("[INFO] ");display_color(0x07);P("Command line: ");P(bootopts_command_line());P("\n");}if(bootopts_recovery()){display_color(0x0e);P("[WARN] Read-only recovery mode\n");display_color(0x07);}}
static void boot_summary(void){
    unsigned failed=(unsigned)service_count()-boot_services_started;
    boot_rule();display_color(boot_services_started==10?0x0a:0x0e);
    P(boot_services_started==10?"System ready":"System ready with warnings");display_color(0x07);P("\n\n");
    P("  Hostname       os64\n");
    P("  Kernel         " OS64_KERNEL_VERSION " (" OS64_ARCHITECTURE ")\n");
    P("  Boot mode      ");P(bootopts_mode());P("\n");
    P("  Memory         ");human_bytes(memory_total_kib()*1024ull);P(" usable physical; ");human_bytes(memory_free_bytes());P(" tracked heap free\n");
    P("  Console        ");display_number(display_width());P("x");display_number(display_height());
    if(display_framebuffer_active()){P(" cells, ");display_number(display_pixel_width());P("x");display_number(display_pixel_height());P(" framebuffer\n");}else P(" cells, text mode\n");
    P("  Rootfs         ");P(boot_filesystem_mounted?"initramfs mounted read-only":"unavailable");P("\n");
    P("  Storage        ");if(disk_mounted()&&varfs_mounted()){P("/home FAT32, /var ");P(varfs_type());P(" mounted\n");}else if(boot_storage_ready)P("detected but not mounted\n");else P("not configured\n");
    P("  Network        ");if(network_ready()){P("net0 (");P(network_driver());P(") ");print_ip(network_address());P("\n");}else P("unavailable\n");
    P("  Clock          ");P(timed_synchronized()?"NTP synchronized":"RTC fallback");P(" (" );P(timed_timezone());P(")\n");
    P("  Services       ");display_number(boot_services_started);P(" ready");if(failed){P(", ");display_number(failed);P(" failed or skipped");}P("\n");
    P("  Users          ");display_number(auth_user_count());P(" loaded\n");
    P("  Boot time      ");display_number(timed_monotonic_ns()/1000000ull);P(" ms\n\n");
}
static void show_motd(void){fs_file_t m;if(fs_find("etc/motd",&m)&&m.size)display_styled(m.data,m.size);else P("Welcome to " OS64_NAME "!\n\nKernel " OS64_KERNEL_VERSION "\nArchitecture: " OS64_ARCHITECTURE "\n\nType \"help\" for available commands.\n");display_putc('\n');}
static void prompt(void){display_color(0x0a);P(current_user);display_color(0x07);P("@os64:");display_color(0x09);P(streq(cwd,"/home/root")?"~":cwd);display_color(0x07);P("# ");}
static void move_left(size_t count){while(count--)display_cursor_left();}
static void move_right(size_t count){while(count--)display_cursor_right();}
static void replace_input(char*line,size_t*n,size_t*position,const char*value){
    move_left(*position);for(size_t i=0;i<*n;i++)display_putc(' ');move_left(*n);*n=*position=0;
    while(value[*n]&&*n+1<128){line[*n]=value[*n];display_putc(value[*n]);(*n)++;}*position=*n;
}
static void remember(const char*line){if(!*line)return;if(history_count==16){for(unsigned i=1;i<16;i++)for(unsigned j=0;j<128;j++)history_lines[i-1][j]=history_lines[i][j];history_count--;}unsigned n=0;while(line[n]&&n<127){history_lines[history_count][n]=line[n];n++;}history_lines[history_count][n]=0;history_count++;}
static void shell_loop(void){
    char line[128];
    for(;;){
        service_poll_all();prompt();
        size_t prompt_x,prompt_y;display_position(&prompt_x,&prompt_y);
        display_cursor_set(prompt_x,prompt_y,1,1);
        size_t n=0,position=0;unsigned history_pos=history_count;int cancelled=0;
        for(;;){
            char c=input();
            if(c==3){move_right(n-position);P("^C\n");line[0]=0;cancelled=1;break;}
            if(c=='\n'){
                /*
                 * Hide the cursor without changing the terminal's logical
                 * output position. Moving it to (0, 0) caused command output
                 * to restart at the top-left and overwrite/redraw old text.
                 */
                move_right(n-position);
                display_putc('\n');
                size_t cursor_x, cursor_y;
                display_position(&cursor_x, &cursor_y);
                display_cursor_set(cursor_x, cursor_y, 0, 1);
                line[n]=0;
                break;
            }
            if(c==0x11&&history_count){if(history_pos)history_pos--;replace_input(line,&n,&position,history_lines[history_pos]);}
            else if(c==0x12&&history_count){if(history_pos<history_count)history_pos++;replace_input(line,&n,&position,history_pos<history_count?history_lines[history_pos]:"");}
            else if(c==0x13&&position){display_cursor_left();position--;}
            else if(c==0x14&&position<n){display_cursor_right();position++;}
            else if((c=='\b'||c==127)&&position){position--;n--;display_cursor_left();for(size_t i=position;i<n;i++){line[i]=line[i+1];display_putc(line[i]);}display_putc(' ');move_left(n-position+1);}
            else if(c>=' '&&c<='~'&&n+1<sizeof line){for(size_t i=n;i>position;i--)line[i]=line[i-1];line[position]=(char)c;n++;for(size_t i=position;i<n;i++)display_putc(line[i]);position++;move_left(n-position);}
        }
        if(cancelled){last_status=130;continue;}

        /*
         * An empty command should only produce the next prompt. Avoid
         * passing an empty string through executable lookup.
         */
        size_t first_non_space=0;
        while(line[first_non_space]==' ')first_non_space++;
        if(!line[first_non_space]){
            last_status=0;
            continue;
        }

        remember(line);
        execute(line);
    }
}
static unsigned edit_distance(const char*a,const char*b){unsigned prev[17],cur[17],bn=0;while(b[bn]&&bn<16)bn++;for(unsigned j=0;j<=bn;j++)prev[j]=j;for(unsigned i=1;a[i-1]&&i<=16;i++){cur[0]=i;for(unsigned j=1;j<=bn;j++){unsigned add=cur[j-1]+1,del=prev[j]+1,sub=prev[j-1]+(a[i-1]!=b[j-1]);cur[j]=add<del?(add<sub?add:sub):(del<sub?del:sub);}for(unsigned j=0;j<=bn;j++)prev[j]=cur[j];}unsigned an=0;while(a[an]&&an<16)an++;return prev[bn]+(a[an]?4:0);}
static void suggest_command(const char*name){const char*best=0;unsigned score=4;for(size_t i=0;i<sizeof commands/sizeof commands[0];i++){unsigned d=edit_distance(name,commands[i].name);if(d<score){score=d;best=commands[i].name;}}if(best){P("\nDid you mean?\n\n    ");P(best);P("\n");}}
static void execute(char*line){
    while(*line==' ')line++;
    char*args=line;
    while(*args&&*args!=' ')args++;
    char saved=*args;
    *args=0;
    for(char*p=line;*p;p++)if(*p>='A'&&*p<='Z')*p=(char)(*p-'A'+'a');
    char requested[32];size_t requested_n=0;while(line[requested_n]&&requested_n+1<sizeof requested){requested[requested_n]=line[requested_n];requested_n++;}requested[requested_n]=0;
    if(bootopts_cli()&&(streq(requested,"sysmgr")||streq(requested,"filemm")||streq(requested,"fileman")||streq(requested,"netcfg")||streq(requested,"usercfg")||streq(requested,"setup")||streq(requested,"teteris"))){last_status=126;P(shell_name);P(": ");P(requested);P(": full-screen application disabled in CLI boot mode\n");return;}
    char path[160];fs_file_t app;int found=0;
    if(starts(line,"/bin/")||starts(line,"/sbin/"))found=fs_find(line,&app);
    else{make_path(path,"bin/",line);found=fs_find(path,&app);if(!found){make_path(path,"sbin/",line);found=fs_find(path,&app);}if(!found){make_path(path,"usr/bin/",line);found=fs_find(path,&app);}}
    *args=saved;if(!found&&varfs_mounted()){size_t pn=0;const char*pre="/var/apps/";while(pre[pn]){path[pn]=pre[pn];pn++;}for(size_t i=0;line[i]&&line[i]!=' '&&pn<sizeof path-1;i++)path[pn++]=line[i];path[pn]=0;size_t z;if(varfs_load(path,installed_image,sizeof installed_image,&z)){app.name=path;app.data=installed_image;app.size=z;app.type='0';found=1;}}if(!found){last_status=127;P(shell_name);P(": ");P(requested);P(": command not found\n");suggest_command(requested);return;}
    foreground_active=1;foreground_interrupted=0;
    int status=elf_execute(app.data,app.size,saved?args+1:args,&app_api);
    foreground_active=0;
    if(terminal_owned)abi_terminal_release();
    if(status<0){
        last_status=126;
        P(shell_name);P(": invalid or incompatible ELF64 executable (status 126)\n");
    }else{
        last_status=status&255;
        if(last_status&&last_status!=130){
            P(line);P(": exited with status ");
            display_number((unsigned long)last_status);display_putc('\n');
        }
    }
}

void kernel_main(uint64_t multiboot){
    display_init(multiboot);keyboard_ready=keyboard_init();bootopts_init(multiboot);
    memory_init(multiboot);pci_initialize();service_init();if(!bootopts_initrd_start()||!bootopts_initrd_end()||bootopts_initrd_end()<=bootopts_initrd_start())kernel_panic("INITRD_NOT_FOUND");fs_mount(bootopts_initrd_start(),bootopts_initrd_end());vfs_init();boot_filesystem_mounted=1;configure_shell();
    display_color(0x0b);
    P("\n  ____   _____  __   _  _\n");
    P(" / __ \\ / ___/ / /_ | || |\n");
    P("| |  | |\\__ \\| '_ \\| || |_\n");
    P("| |__| |___/ /| (_) |__   _|\n");
    P(" \\____/|____/  \\___/   |_|\n\n");
    display_color(0x0f);P("  " OS64_NAME " " OS64_KERNEL_VERSION);display_color(0x07);P("  " OS64_DESCRIPTION "\n");
    P("  " OS64_ARCHITECTURE "  |  ");P(bootopts_mode());P(" boot  |  policy: " OS64_SYSTEM_POLICY "\n");
    boot_rule();display_color(0x08);P("Copyright (C) 2026 OS64 Project\n");display_color(0x07);
    P("\nInitializing system services\n\n");boot_report();
    fs_file_t keys;if(fs_find("etc/shadow",&keys))auth_load(keys.data,keys.size);
    disk_init();boot_storage_ready=disk_formatted();
    if(!bootopts_recovery()&&disk_mount()&&varfs_mount()){
        boot_storage_ready=boot_filesystem_mounted=1;
        unsigned char saved[4096];size_t saved_n;
        if(varfs_load("/var/lib/os64/shadow",saved,sizeof saved,&saved_n))auth_load(saved,saved_n);
        if(varfs_load("/var/lib/os64/accounts",saved,sizeof saved,&saved_n))auth_load_accounts(saved,saved_n);
        set_session_user("root");
        disk_create("root",1);
    }else if(!bootopts_recovery()&&!disk_formatted()){
        display_color(0x0e);P("[WARN] Storage is unformatted; run install /dev/sda\n");display_color(0x07);
    }
    start_boot_services();boot_summary();show_motd();shell_loop();
}
