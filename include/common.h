#ifndef __UPGRADE_COMMON_H__
#define __UPGRADE_COMMON_H__

#include <stddef.h>
#include <stdbool.h>

#define debug(fmt, ...)
#define BUFF_SIZE           4096
#define ARRAY_SIZE(array)   (sizeof(array) / sizeof(*array))

extern void progress_print(void *reserved, const char *fmt, ...);

extern void progress_clearline(void);

extern size_t clear_line_crlf(char *str);

extern int shell_command(const char *fmt, ...);

extern int shell_command_output(char *output, size_t n, const char *fmt, ...);

extern bool file_exist(const char *path);

extern bool is_device_file(const char *path);

extern bool is_regular_file(const char *path);

extern ssize_t file_size(const char *path);

extern ssize_t full_read(int fd, void *buf, size_t size);

extern ssize_t full_write(int fd, const void *buf, size_t size);

#endif /* __UPGRADE_COMMON_H__ */
