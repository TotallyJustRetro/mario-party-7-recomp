#pragma once
// Cruise Navigator — launcher interface

#ifdef __cplusplus
extern "C" {
#endif

// Show the launcher window if no disc is configured.
// Returns true with iso_path filled if the user picked a valid disc.
// Returns false if the user closed the window (exit).
bool launcher_run(char* iso_path, int max_len);

// Log a message (level: 0=info, 1=warn, 2=fatal).
// Fatal messages are also written to crash_log.txt next to the exe.
void cn_log(int level, const char* fmt, ...);
void cn_log_fatal(const char* msg);

// Log ring access (for in-game overlay or debug UI)
int         cn_log_count(void);
const char* cn_log_line(int i);
int         cn_log_level(int i);

#ifdef __cplusplus
}
#endif
