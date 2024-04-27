#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void initialize_console(void);
void start_console(void);

// Register all system functions
void register_system(void);

// Register common system functions: "version", "restart", "free", "heap", "tasks"
void register_system_common(void);

// Register deep and light sleep functions
void register_system_sleep(void);
#ifdef __cplusplus
}
#endif
