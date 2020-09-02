#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common.h"

size_t strip_string_crlf(char *str)
{
    size_t i;

    if (str == NULL) {
        return 0;
    }

    for (i = 0; str[i]; ++i) {
        if (str[i] == '\r' || str[i] == '\n') {
            str[i] = '\0';
            break;
        }
    }

    return i;
}

int shell_cmd_output(const char *cmd, char *buff, const size_t size)
{
    size_t ret;
    FILE *fp;

    if (cmd == NULL || buff == NULL || size == 0) {
        return -EINVAL;
    }

    if ((fp = popen(cmd, "r")) == NULL) {
        return -errno;
    }
    ret = fread(buff, 1, size - 1, fp);
    buff[ret] = '\0';

    return pclose(fp);
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
