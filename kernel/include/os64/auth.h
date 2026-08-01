#ifndef OS64_AUTH_H
#define OS64_AUTH_H
#include <stddef.h>
#include <stdint.h>
typedef enum{AUTH_ROOT,AUTH_ADMIN,AUTH_POWER,AUTH_STANDARD,AUTH_GUEST}auth_role_t;
#define AUTH_GROUP_ROOT (1u<<0)
#define AUTH_GROUP_ADMIN (1u<<1)
#define AUTH_GROUP_USERS (1u<<2)
#define AUTH_GROUP_DEVELOPERS (1u<<3)
#define AUTH_GROUP_OPERATORS (1u<<4)
#define AUTH_GROUP_NETWORK (1u<<5)
#define AUTH_GROUP_AUDIO (1u<<6)
#define AUTH_GROUP_VIDEO (1u<<7)
#define AUTH_GROUP_STORAGE (1u<<8)
#define AUTH_GROUP_WHEEL (1u<<9)
int auth_load(const unsigned char *data,size_t size);
size_t auth_user_count(void);
int auth_check(const char *user,const char *password);
int auth_nologin(const char *user);
int auth_add(const char *user,const char *password);
int auth_add_role(const char *user,const char *password,auth_role_t role);
int auth_delete(const char *user);int auth_set_password(const char *user,const char *password);int auth_set_role(const char *user,auth_role_t role);int auth_set_groups(const char *user,uint32_t groups);
int auth_select(const char *user);int auth_select_uid(uint16_t uid);const char *auth_current_user(void);uint16_t auth_getuid(void);uint16_t auth_getgid(void);uint32_t auth_getgroups(void);auth_role_t auth_current_role(void);int auth_is_root(void);int auth_is_admin(void);int auth_has_group(uint32_t group);int auth_lookup(const char *user,uint16_t *uid,uint16_t *gid,auth_role_t *role,uint32_t *groups);
const char *auth_role_name(auth_role_t role);uint32_t auth_group_from_name(const char *name);const char *auth_group_name(unsigned index);
uint16_t auth_umask(void);void auth_set_umask(uint16_t mask);
size_t auth_export_shadow(unsigned char *data,size_t capacity);
size_t auth_export_accounts(unsigned char *data,size_t capacity);int auth_load_accounts(const unsigned char *data,size_t size);
#endif
