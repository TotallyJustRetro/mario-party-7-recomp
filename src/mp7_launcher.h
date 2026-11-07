#pragma once
// Cruise Navigator — native startup launcher interface

#ifdef __cplusplus
extern "C" {
#endif

// Display the native launcher. default_path is the build's configured image,
// or nullptr/empty when none is configured. Returns true when a readable image
// is selected; the runtime validates the actual disc ID when booting.
bool launcher_run(const char* default_path, char* selected_path, int selected_path_size);

// Log a message (level: 0=info, 1=warn, 2=fatal).
void cn_log(int level, const char* fmt, ...);
void cn_log_fatal(const char* msg);

// Log ring access (for in-game overlay or debug UI).
int         cn_log_count(void);
const char* cn_log_line(int i);
int         cn_log_level(int i);

#ifdef __cplusplus
}
#endif
