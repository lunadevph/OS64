#ifndef OS64_OFP_H
#define OS64_OFP_H
typedef enum { OFP_READ=1,OFP_WRITE=2,OFP_DELETE=4 } ofp_operation_t;
void ofp_init(void);
int ofp_allowed(const char *path,ofp_operation_t operation);
unsigned ofp_policy_count(void);
#endif
