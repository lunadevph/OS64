#include "vfs.h"
#include "fs.h"
#include "disk.h"
#include "varfs.h"
#include "random.h"
#include "auth.h"
#include "tmpfs.h"
#include "procfs.h"
static int starts(const char*s,const char*p){while(*p&&*s==*p){s++;p++;}return !*p;}
static int equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return !*a&&!*b;}
static int native_path(const char*p){return starts(p,"/var/")||starts(p,"/root/")||(starts(p,"/mnt/")&&!starts(p,"/mnt/os64/"));}
static const char*clean(const char*p){while(*p=='/')p++;if(p[0]=='.'&&p[1]=='/')p+=2;return p;}
static int home_owner(const char*p,uint16_t*uid,uint16_t*gid){const char*s=0;if(starts(p,"/home/"))s=p+6;else if(starts(p,"/mnt/os64/"))s=p+12;else return 0;char user[16];unsigned n=0;while(*s&&*s!='/'&&n<15)user[n++]=*s++;user[n]=0;return n&&auth_lookup(user,uid,gid,0,0);}
static uint32_t gid_group(uint16_t gid){if(gid==0)return AUTH_GROUP_ROOT;if(gid==10)return AUTH_GROUP_ADMIN;if(gid==100)return AUTH_GROUP_USERS;if(gid>=101&&gid<=107)return 1u<<(gid-98);return 0;}
static int child(const char*dir,const char*full,char*name,uint8_t*type,uint8_t source_type){dir=clean(dir);full=clean(full);size_t d=0;while(dir[d])d++;if(d){for(size_t i=0;i<d;i++)if(full[i]!=dir[i])return 0;if(full[d]!='/')return 0;full+=d+1;}if(!*full)return 0;size_t n=0;while(full[n]&&full[n]!='/')n++;if(!n)return 0;for(size_t i=0;i<n&&i<95;i++)name[i]=full[i];name[n<95?n:95]=0;*type=full[n]=='/'?1:(source_type=='5');return 1;}
void vfs_init(void){tmpfs_init();}
const char*vfs_mount_source(const char*p){if(starts(p,"/dev"))return "devfs";if(starts(p,"/proc"))return "procfs (ro)";if(starts(p,"/tmp"))return "tmpfs";if(starts(p,"/home")||starts(p,"/mnt/os64"))return "/dev/sda1 (fat32)";if(starts(p,"/var")||starts(p,"/root")||starts(p,"/mnt")){const char*t=varfs_type();return t[3]=='4'?"/dev/sda2 (ext4)":t[3]=='3'?"/dev/sda2 (ext3)":"/dev/sda2 (ext2)";}return "initramfs (ro)";}
static int stat_raw(const char*p,vfs_stat_t*st){
    if(!p||!st)return 0;
    static const char*chr[]={"/dev/net0","/dev/console","/dev/null","/dev/zero","/dev/full","/dev/empty","/dev/random","/dev/urandom"};
    static const char*blk[]={"/dev/sda","/dev/sda1","/dev/sda2"};
    for(unsigned i=0;i<8;i++)if(equal(p,chr[i])){st->size=0;st->mode=0666;st->uid=st->gid=0;st->type=VFS_TYPE_CHAR_DEVICE;st->backend=VFS_DEVFS;return 1;}
    for(unsigned i=0;i<3;i++)if(equal(p,blk[i])){st->size=0;st->mode=0660;st->uid=st->gid=0;st->type=VFS_TYPE_BLOCK_DEVICE;st->backend=VFS_DEVFS;return 1;}
    if(equal(p,"/")||equal(p,"/home")||equal(p,"/mnt")||equal(p,"/mnt/os64")||equal(p,"/var")){st->size=0;st->mode=0755;st->uid=st->gid=0;st->type=VFS_TYPE_DIRECTORY;st->backend=VFS_RAMFS;return 1;}
    if(equal(p,"/dev")){st->size=0;st->mode=0755;st->uid=st->gid=0;st->type=VFS_TYPE_DIRECTORY;st->backend=VFS_DEVFS;return 1;}
    if(equal(p,"/tmp")){st->size=0;st->mode=01777;st->uid=st->gid=0;st->type=VFS_TYPE_DIRECTORY;st->backend=VFS_TMPFS;return 1;}
    if(equal(p,"/proc")){st->size=0;st->mode=0555;st->uid=st->gid=0;st->type=VFS_TYPE_DIRECTORY;st->backend=VFS_PROCFS;return 1;}
    if(starts(p,"/tmp/")){size_t z;uint16_t m,u,g;if(tmpfs_stat(p,&z,&m,&u,&g)){st->size=z;st->mode=m;st->uid=u;st->gid=g;st->type=VFS_TYPE_FILE;st->backend=VFS_TMPFS;return 1;}}
    if(starts(p,"/proc/")){size_t z;if(procfs_stat(p,&z)){st->size=z;st->mode=0444;st->uid=st->gid=0;st->type=VFS_TYPE_FILE;st->backend=VFS_PROCFS;return 1;}}
    if(equal(p,"/root")){st->size=0;st->mode=0700;st->uid=st->gid=0;st->type=VFS_TYPE_DIRECTORY;st->backend=VFS_EXT2;return 1;}
    if(native_path(p)){size_t z;uint16_t m,u,g;if(varfs_stat(p,&z,&m,&u,&g)){st->size=z;st->mode=m;st->uid=u;st->gid=g;st->type=VFS_TYPE_FILE;st->backend=VFS_EXT2;return 1;}}
    if(starts(p,"/home/")||starts(p,"/mnt/os64/")){disk_file_t f;const char*n=p;uint16_t u=0,g=0;for(size_t i=0;p[i];i++)if(p[i]=='/')n=p+i+1;home_owner(p,&u,&g);for(unsigned i=0;disk_file_at(i,&f);i++)if(equal(n,f.name)){st->size=f.size;st->mode=f.type?0700:0600;st->uid=u;st->gid=g;st->type=f.type?VFS_TYPE_DIRECTORY:VFS_TYPE_FILE;st->backend=VFS_FAT32;return 1;}uint32_t z;unsigned char b[512];if(disk_load(p,b,&z)){st->size=z;st->mode=0600;st->uid=u;st->gid=g;st->type=VFS_TYPE_FILE;st->backend=VFS_FAT32;return 1;}}
    fs_file_t f;if(fs_find(p,&f)){st->size=f.size;st->mode=f.type=='5'?0555:0444;st->uid=st->gid=0;st->type=f.type=='5'?VFS_TYPE_DIRECTORY:VFS_TYPE_FILE;st->backend=VFS_INITRAMFS;return 1;}return 0;
}
int vfs_check_permission(const char*p,unsigned access){
    if(!p)return 0;
    if(auth_is_root())return 1;
    vfs_stat_t st;
    if(!stat_raw(p,&st)){uint16_t owner=0,gid=0;if(home_owner(p,&owner,&gid))return owner==auth_getuid();if(starts(p,"/tmp/")||starts(p,"/var/tmp/"))return 1;if(auth_current_role()==AUTH_ADMIN&&(starts(p,"/var/")||starts(p,"/mnt/")))return 1;if(auth_current_role()==AUTH_POWER&&starts(p,"/var/local/"))return 1;return 0;}
    int member=st.gid==auth_getgid()||(gid_group(st.gid)&auth_getgroups());
    unsigned bits=st.uid==auth_getuid()?(st.mode>>6):(member?(st.mode>>3):st.mode);return (bits&access)==access;
}
int vfs_stat_path(const char*p,vfs_stat_t*st){return stat_raw(p,st)&&vfs_check_permission(p,VFS_ACCESS_READ);}
int vfs_chmod(const char*p,uint16_t mode){vfs_stat_t st;if(!stat_raw(p,&st)||(!auth_is_root()&&st.uid!=auth_getuid()))return 0;if(st.backend==VFS_EXT2)return varfs_chmod(p,(uint16_t)(mode&0777u));if(st.backend==VFS_TMPFS)return tmpfs_chmod(p,mode);return 0;}
int vfs_chown(const char*p,uint16_t uid,uint16_t gid,int su,int sg){vfs_stat_t st;if(!auth_is_root()||!stat_raw(p,&st))return 0;if(st.backend==VFS_EXT2)return varfs_chown(p,uid,gid,su,sg);if(st.backend==VFS_TMPFS)return tmpfs_chown(p,uid,gid,su,sg);return 0;}
#define vfs_read vfs_read_raw
#define vfs_write vfs_write_raw
#define vfs_remove vfs_remove_raw
#define vfs_readdir vfs_readdir_raw
int vfs_read(const char*p,unsigned char*d,size_t cap,size_t*n,vfs_stat_t*st){if(!p||!d||!n||!st)return 0;if(equal(p,"/dev/null")||equal(p,"/dev/empty")){*n=0;st->size=0;st->mode=0666;st->uid=st->gid=0;st->type=VFS_TYPE_CHAR_DEVICE;st->backend=VFS_RAMFS;return 1;}if(equal(p,"/dev/zero")||equal(p,"/dev/full")){for(size_t i=0;i<cap;i++)d[i]=0;*n=cap;st->size=0;st->mode=0666;st->uid=st->gid=0;st->type=VFS_TYPE_CHAR_DEVICE;st->backend=VFS_RAMFS;return 1;}if(equal(p,"/dev/random")||equal(p,"/dev/urandom")){if(!random_read(d,cap))return 0;*n=cap;st->size=0;st->mode=0666;st->uid=st->gid=0;st->type=VFS_TYPE_CHAR_DEVICE;st->backend=VFS_RAMFS;return 1;}if(starts(p,"/home/")||starts(p,"/mnt/os64/")){uint32_t z;if(cap<512||!disk_load(p,d,&z))return 0;*n=z;st->size=z;st->mode=0666;st->uid=st->gid=0;st->type=VFS_TYPE_FILE;st->backend=VFS_FAT32;return 1;}if(native_path(p)&&varfs_load(p,d,cap,n)){st->size=*n;st->mode=0600;st->uid=st->gid=0;st->type=VFS_TYPE_FILE;st->backend=VFS_EXT2;return 1;}fs_file_t f;if(!fs_find(p,&f)||f.type=='5'||f.size>cap)return 0;for(size_t i=0;i<f.size;i++)d[i]=f.data[i];*n=f.size;st->size=f.size;st->mode=0444;st->uid=st->gid=0;st->type=VFS_TYPE_FILE;st->backend=VFS_INITRAMFS;return 1;}
int vfs_write(const char*p,const unsigned char*d,size_t size,size_t*written){if(!p||(!d&&size)||!written)return VFS_WRITE_ERROR;*written=0;if(equal(p,"/dev/full"))return VFS_WRITE_NO_SPACE;if(equal(p,"/dev/null")||equal(p,"/dev/zero")||equal(p,"/dev/empty")){*written=size;return VFS_WRITE_OK;}if(equal(p,"/dev/random")||equal(p,"/dev/urandom")){if(!random_mix(d,size))return VFS_WRITE_ERROR;*written=size;return VFS_WRITE_OK;}if((starts(p,"/home/")||starts(p,"/mnt/os64/"))&&size<=512&&disk_store(p,d,(uint32_t)size)){*written=size;return VFS_WRITE_OK;}if(native_path(p)&&varfs_store(p,d,size)){*written=size;return VFS_WRITE_OK;}return VFS_WRITE_ERROR;}
int vfs_remove(const char*p){if(!p)return 0;if(starts(p,"/home/")||starts(p,"/mnt/os64/"))return disk_remove(p);if(native_path(p))return varfs_remove(p);return 0;}
int vfs_readdir(const char*p,unsigned wanted,vfs_dirent_t*out){if(!p||!out)return 0;unsigned seen=0;if(equal(p,"/dev")){static const char*n[]={"sda","sda1","sda2","net0","console","null","zero","full","empty","random","urandom"};for(unsigned i=0;i<11;i++)if(seen++==wanted){size_t j=0;while(n[i][j]){out->name[j]=n[i][j];j++;}out->name[j]=0;out->type=i<3?VFS_TYPE_BLOCK_DEVICE:VFS_TYPE_CHAR_DEVICE;out->backend=VFS_RAMFS;return 1;}return 0;}if(equal(p,"/home")||equal(p,"/mnt/os64")){disk_file_t f;for(unsigned i=0;disk_file_at(i,&f);i++)if(seen++==wanted){size_t j=0;while(f.name[j]&&j<95){out->name[j]=f.name[j];j++;}out->name[j]=0;out->type=f.type;out->backend=VFS_FAT32;return 1;}return 0;}if(starts(p,"/home/")||starts(p,"/mnt/os64/"))return 0;if(equal(p,"/var")||equal(p,"/root")||equal(p,"/tmp")||equal(p,"/mnt")){char full[96],name[96];size_t z;for(unsigned i=0;varfs_at(i,full,sizeof full,&z);i++){uint8_t type;if(!child(p,full,name,&type,0))continue;int duplicate=0;for(unsigned q=0;q<i;q++){char prior[96],pn[96];size_t pz;uint8_t pt;if(varfs_at(q,prior,sizeof prior,&pz)&&child(p,prior,pn,&pt,0)&&equal(pn,name)){duplicate=1;break;}}if(duplicate)continue;if(seen++==wanted){size_t j=0;while(name[j]){out->name[j]=name[j];j++;}out->name[j]=0;out->type=type;out->backend=VFS_EXT2;return 1;}}}fs_file_t f;for(size_t i=0;fs_at(i,&f);i++){char name[96];uint8_t type;if(!child(p,f.name,name,&type,(uint8_t)f.type))continue;int duplicate=0;for(size_t q=0;q<i;q++){fs_file_t prior;char pn[96];uint8_t pt;if(fs_at(q,&prior)&&child(p,prior.name,pn,&pt,(uint8_t)prior.type)&&equal(pn,name)){duplicate=1;break;}}if(duplicate)continue;if(seen++==wanted){size_t j=0;while(name[j]){out->name[j]=name[j];j++;}out->name[j]=0;out->type=type;out->backend=VFS_INITRAMFS;return 1;}}if(equal(p,"/")){static const char*mounts[]={"dev","proc","home","mnt","var","tmp"};for(unsigned i=0;i<6;i++){int duplicate=0;for(size_t q=0;fs_at(q,&f);q++){char name[96];uint8_t type;if(child("/",f.name,name,&type,(uint8_t)f.type)&&equal(name,mounts[i])){duplicate=1;break;}}if(duplicate)continue;if(seen++==wanted){size_t j=0;while(mounts[i][j]){out->name[j]=mounts[i][j];j++;}out->name[j]=0;out->type=1;out->backend=VFS_RAMFS;return 1;}}}return 0;}
