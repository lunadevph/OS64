#ifndef OS64_SDK_CONFIG_H
#define OS64_SDK_CONFIG_H
typedef int(*os_config_callback)(const char *key,const char *value,void *context);int os_config_parse(const char *text,os_config_callback callback,void *context);int os_config_read(const char *path,os_config_callback callback,void *context);
#endif
