#include "tmpfs.h"
#define FILES 32
#define DATA 2048
typedef struct{char path[96];unsigned char data[DATA];size_t size;uint16_t mode,uid,gid;}node_t;
static node_t nodes[FILES];
static int eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return !*a&&!*b;}
static node_t*find(const char*p){for(unsigned i=0;i<FILES;i++)if(nodes[i].path[0]&&eq(nodes[i].path,p))return &nodes[i];return 0;}
void tmpfs_init(void){for(unsigned i=0;i<FILES;i++)nodes[i].path[0]=0;}
int tmpfs_store(const char*p,const unsigned char*d,size_t n,uint16_t m,uint16_t u,uint16_t g){if(!p||(!d&&n)||n>DATA)return 0;node_t*x=find(p);if(!x)for(unsigned i=0;i<FILES;i++)if(!nodes[i].path[0]){x=&nodes[i];size_t z=0;while(p[z]&&z<95){x->path[z]=p[z];z++;}x->path[z]=0;x->mode=m;x->uid=u;x->gid=g;break;}if(!x)return 0;for(size_t i=0;i<n;i++)x->data[i]=d[i];x->size=n;return 1;}
int tmpfs_load(const char*p,unsigned char*d,size_t c,size_t*n){node_t*x=find(p);if(!x||x->size>c)return 0;for(size_t i=0;i<x->size;i++)d[i]=x->data[i];*n=x->size;return 1;}
int tmpfs_stat(const char*p,size_t*n,uint16_t*m,uint16_t*u,uint16_t*g){node_t*x=find(p);if(!x)return 0;if(n)*n=x->size;if(m)*m=x->mode;if(u)*u=x->uid;if(g)*g=x->gid;return 1;}
int tmpfs_remove(const char*p){node_t*x=find(p);if(!x)return 0;x->path[0]=0;x->size=0;return 1;}
int tmpfs_at(unsigned w,char*p,size_t c,size_t*n){unsigned seen=0;for(unsigned i=0;i<FILES;i++)if(nodes[i].path[0]&&seen++==w){size_t z=0;while(nodes[i].path[z]&&z+1<c){p[z]=nodes[i].path[z];z++;}p[z]=0;if(n)*n=nodes[i].size;return 1;}return 0;}
int tmpfs_chmod(const char*p,uint16_t m){node_t*x=find(p);if(!x)return 0;x->mode=m&0777;return 1;}int tmpfs_chown(const char*p,uint16_t u,uint16_t g,int su,int sg){node_t*x=find(p);if(!x)return 0;if(su)x->uid=u;if(sg)x->gid=g;return 1;}
