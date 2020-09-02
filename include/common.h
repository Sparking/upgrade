#ifndef __UPGRADE_COMMON_H__
#define __UPGRADE_COMMON_H__

#include <stddef.h>
#include <stdbool.h>

#define debug(fmt, ...)
#define BUFF_SIZE           4096
#define ARRAY_SIZE(array)   (sizeof(array) / sizeof(*array))

extern size_t strip_string_crlf(char *str);

extern int shell_cmd_output(const char *cmd, char *buf, const size_t size);

extern bool file_exist(const char *path);

extern bool is_device_file(const char *path);

extern bool is_regular_file(const char *path);

extern ssize_t file_size(const char *path);

extern ssize_t full_read(int fd, void *buf, size_t size);

#endif /* __UPGRADE_COMMON_H__ */
