#include "vfs.h"
#include "tmpfs.h"
#include "procfs.h"
#include "auth.h"

/* Raw backend entry points are kept private to the VFS implementation. */
int vfs_read_raw(const char*,unsigned char*,size_t,size_t*,vfs_stat_t*);
int vfs_write_raw(const char*,const unsigned char*,size_t,size_t*);
int vfs_remove_raw(const char*);
int vfs_readdir_raw(const char*,unsigned,vfs_dirent_t*);

int vfs_read(const char*p,unsigned char*d,size_t cap,size_t*n,vfs_stat_t*st){
    if(p&&p[0]=='/'&&p[1]=='p'&&p[2]=='r'&&p[3]=='o'&&p[4]=='c'&&p[5]=='/')return vfs_check_permission(p,VFS_ACCESS_READ)&&procfs_read(p,d,cap,n)&&vfs_stat_path(p,st);
    if(p&&p[0]=='/'&&p[1]=='t'&&p[2]=='m'&&p[3]=='p'&&p[4]=='/')return vfs_check_permission(p,VFS_ACCESS_READ)&&tmpfs_load(p,d,cap,n)&&vfs_stat_path(p,st);
    return vfs_check_permission(p,VFS_ACCESS_READ)&&vfs_read_raw(p,d,cap,n,st);
}
int vfs_write(const char*p,const unsigned char*d,size_t n,size_t*w){
    if(p&&p[0]=='/'&&p[1]=='t'&&p[2]=='m'&&p[3]=='p'&&p[4]=='/'){if(!vfs_check_permission(p,VFS_ACCESS_WRITE)||!tmpfs_store(p,d,n,(uint16_t)(0666u&~auth_umask()),auth_getuid(),auth_getgid()))return VFS_WRITE_ERROR;*w=n;return VFS_WRITE_OK;}
    return vfs_check_permission(p,VFS_ACCESS_WRITE)?vfs_write_raw(p,d,n,w):VFS_WRITE_ERROR;
}
int vfs_remove(const char*p){if(!vfs_check_permission(p,VFS_ACCESS_WRITE))return 0;if(p&&p[0]=='/'&&p[1]=='t'&&p[2]=='m'&&p[3]=='p'&&p[4]=='/')return tmpfs_remove(p);return vfs_remove_raw(p);}
int vfs_readdir(const char*p,unsigned i,vfs_dirent_t*e){
    if(p&&p[0]=='/'&&p[1]=='p'&&p[2]=='r'&&p[3]=='o'&&p[4]=='c'&&!p[5]){if(!procfs_at(i,e->name,sizeof e->name))return 0;e->type=VFS_TYPE_FILE;e->backend=VFS_PROCFS;return 1;}
    if(p&&p[0]=='/'&&p[1]=='t'&&p[2]=='m'&&p[3]=='p'&&!p[4]){char full[96];size_t z;if(!tmpfs_at(i,full,sizeof full,&z))return 0;unsigned s=5,n=0;while(full[s]&&n<95)e->name[n++]=full[s++];e->name[n]=0;e->type=VFS_TYPE_FILE;e->backend=VFS_TMPFS;return 1;}
    return vfs_check_permission(p,VFS_ACCESS_READ|VFS_ACCESS_EXECUTE)&&vfs_readdir_raw(p,i,e);
}
