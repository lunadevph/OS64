#include "fs.h"
typedef struct{char name[100],mode[8],uid[8],gid[8],size[12],mtime[12],checksum[8],typeflag,linkname[100],magic[6],version[2],uname[32],gname[32],devmajor[8],devminor[8],prefix[155],padding[12];}__attribute__((packed)) header_t;
static const unsigned char *begin,*end;
static size_t octal(const char*s,size_t n){size_t v=0;while(n&&(*s==' '||*s=='0')){s++;n--;}while(n--&&*s>='0'&&*s<='7')v=v*8+(size_t)(*s++-'0');return v;}
static int path_eq(const char*a,const char*b){while(*a=='/')a++;while(*b=='/')b++;if(a[0]=='.'&&a[1]=='/')a+=2;while(*a&&*a==*b){a++;b++;}while(*a=='/')a++;while(*b=='/')b++;return !*a&&!*b;}
void fs_mount(const void*s,const void*e){begin=s;end=e;}
int fs_at(size_t wanted,fs_file_t*f){const unsigned char*p=begin;size_t i=0;while(p&&p+512<=end){const header_t*h=(const header_t*)p;if(!h->name[0])return 0;size_t z=octal(h->size,12);if(i++==wanted){f->name=h->name;f->data=p+512;f->size=z;f->type=h->typeflag?h->typeflag:'0';return 1;}p+=512+((z+511)&~(size_t)511);}return 0;}
size_t fs_count(void){size_t n=0;fs_file_t f;while(fs_at(n,&f))n++;return n;}
int fs_find(const char*p,fs_file_t*f){for(size_t i=0;fs_at(i,f);i++)if(path_eq(f->name,p))return 1;return 0;}
