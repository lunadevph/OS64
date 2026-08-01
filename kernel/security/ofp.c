#include "ofp.h"
#include "auth.h"
typedef struct{const char*path;unsigned allowed;}policy_t;
static const policy_t policies[]={{"etc/shadow",0},{"etc/passwd",OFP_READ},{"sbin/init",OFP_READ}};
static int same(const char*a,const char*b){while(*a=='/'||(a[0]=='.'&&a[1]=='/'))a+=a[0]=='.'?2:1;while(*a&&*a==*b){a++;b++;}while(*a=='/')a++;return !*a&&!*b;}
void ofp_init(void){}
unsigned ofp_policy_count(void){return sizeof policies/sizeof policies[0];}
int ofp_allowed(const char*p,ofp_operation_t op){if(auth_is_root())return 1;for(unsigned i=0;i<ofp_policy_count();i++)if(same(p,policies[i].path))return (policies[i].allowed&(unsigned)op)!=0;return 1;}
