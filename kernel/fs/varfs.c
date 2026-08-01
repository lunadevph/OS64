#include "varfs.h"
#include "ext2.h"
#include "auth.h"
#include "ext_variant.h"
int varfs_probe(void){return ext2_probe();}
int varfs_format(void){return ext_variant_format(4);}int varfs_format_level(unsigned l){return ext_variant_format(l);}
int varfs_mount(void){int ok=ext2_mount();if(ok)ext_variant_detect();return ok;}const char*varfs_type(void){return ext_variant_name();}
int varfs_mounted(void){return ext2_mounted();}
int varfs_store(const char*p,const unsigned char*d,size_t n){return ext2_store(p,d,n,(uint16_t)(0666u&~auth_umask()),auth_getuid(),auth_getgid());}
int varfs_load(const char*p,unsigned char*d,size_t c,size_t*n){return ext2_load(p,d,c,n);}
int varfs_stat(const char*p,size_t*n,uint16_t*m,uint16_t*u,uint16_t*g){return ext2_stat(p,n,m,u,g);}
int varfs_chmod(const char*p,uint16_t m){return ext2_chmod(p,m);}
int varfs_chown(const char*p,uint16_t u,uint16_t g,int su,int sg){return ext2_chown(p,u,g,su,sg);}
int varfs_remove(const char*p){return ext2_remove(p);}
int varfs_at(unsigned i,char*p,size_t c,size_t*n){return ext2_at(i,p,c,n);}
int varfs_sync(void){return ext2_sync();}
void varfs_unmount(void){ext2_unmount();}
int varfs_check(unsigned*f,unsigned*b){return ext2_check(f,b);}
