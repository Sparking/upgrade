#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common.h"

int shell_command(const char *fmt, ...)
{
    int ret;
    va_list ap;
    char command[PATH_MAX];

    va_start(ap, fmt);
    ret = vsnprintf(command, sizeof(command), fmt, ap);
    va_end(ap);
    if (ret < 0) {
        return -EINVAL;
    }

    return system(command);
}

int shell_command_output(char *output, size_t n, const char *fmt, ...)
{
    int ret;
    FILE *fp;
    va_list ap;
    size_t size;
    char command[PATH_MAX];

    va_start(ap, fmt);
    ret = vsnprintf(command, sizeof(command), fmt, ap);
    va_end(ap);
    if (ret < 0) {
        return -EINVAL;
    }

    if ((fp = popen(command, "r")) == NULL) {
        return -errno;
    }

    if ((size = fread(output, 1, n, fp)) == n) {
        --size;
    }
    output[size] = '\0';

    return pclose(fp);
}

size_t clear_line_crlf(char *str)
{
    size_t i;

    if (str == NULL) {
        return 0;
    }

    for (i = 0; str[i] != '\0'; ++i) {
        if (str[i] == '\r' || str[i] == '\n') {
            str[i] = '\0';
            break;
        }
    }

    return i;
}

bool file_exist(const char *path)
{
    return (path != NULL && access(path, F_OK) == 0);
}

bool is_regular_file(const char *path)
{
    struct stat st;

    return (file_exist(path) && stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

bool is_device_file(const char *path)
{
    struct stat st;

    return (file_exist(path) && stat(path, &st) == 0 && (st.st_mode & (S_IFBLK | S_IFCHR)));
}

ssize_t file_size(const char *path)
{
    struct stat st;

    if (!file_exist(path)) {
        return -EEXIST;
    }

    if (stat(path, &st) != 0 && !S_ISREG(st.st_mode)) {
        return -EINVAL;
    }

    return st.st_size;
}

ssize_t full_read(int fd, void *buf, size_t size)
{
    ssize_t ret;
    ssize_t total;

    total = 0;
    do {
        if ((ret = read(fd, buf + total, size)) < 0) {
            if (errno != EAGAIN && errno != EINTR) {
                total = -errno;
                break;
            }

            continue;
        } else if (ret == 0) {
            break;
        }

        size -= ret;
        total += ret;
    } while (1);

    return total;
}
