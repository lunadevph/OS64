#ifndef OS64_SDK_PROCESS_H
#define OS64_SDK_PROCESS_H
int os_process_run(const char *command,const char *arguments);int os_process_interrupted(void);int os_process_kill(int pid);int os_process_spawn(const char *path,const char *arguments);
#endif
