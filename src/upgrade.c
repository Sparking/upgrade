#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "common.h"
#include "upgrade.h"

typedef enum {
    FS_TYPE_EXT3 = 0,
    FS_TYPE_EXT4,
    FS_TYPE_UBI,
} fs_type_t;
#define FS_TYPE_MAX     (FS_TYPE_UBI + 1)
#define FS_TYPE_UNKNOWN FS_TYPE_MAX

typedef struct {
    char dev[32];
    fs_type_t type;
} partion_info_t;

static const char *fs_type_strmap[] = {
    "ext3", "ext4", "ubifs"
};

const char *fs_type_to_str(const fs_type_t type)
{
    return (type < 0 || type >= FS_TYPE_MAX) ? NULL : fs_type_strmap[type];
}

static fs_type_t str_to_fs_type(const char *type)
{
    fs_type_t i;

    if (type == NULL) {
        return FS_TYPE_UNKNOWN;
    }

    for (i = 0; i < ARRAY_SIZE(fs_type_strmap); ++i) {
        if (strcasecmp(type, fs_type_strmap[i]) == 0) {
            break;
        }
    }

    return i;
}

static int get_using_rootfs_part(partion_info_t *part)
{
    int fd;
    int ret;
    ssize_t size;
    char *root, *type;
    char *buf_s, *word, *mark;
    char buf[BUFF_SIZE];
    const char *const info_source = "/proc/cmdline";

    if (part == NULL) {
        return -EINVAL;
    }

    if ((fd = open(info_source, O_RDONLY)) < 0) {
        return -errno;
    }

    if ((size = full_read(fd, buf, BUFF_SIZE - 1)) < 0) {
        ret = -errno;
        goto end;
    }
    buf[size] = '\0';

    root = NULL;
    type = NULL;
    for (buf_s = buf; (word = strtok_r(buf_s, " ", &mark)) != NULL; buf_s = NULL) {
        if (strncmp(word, "root=", 5) == 0) {
            root = word + 5;
        } else if (strncmp(word, "rootfstype=", 11) == 0) {
            type = word + 11;
        }
    }

    if (root == NULL || type == NULL) {
        ret = -ENOMSG;
        goto end;
    }

    if (!is_device_file(root)) {
        ret = -ENODEV;
        goto end;
    }

    strncpy(part->dev, root, sizeof(part->dev) - 1);
    if ((part->type = str_to_fs_type(type)) == FS_TYPE_UNKNOWN) {
        ret = -EINVAL;
        goto end;
    }
    ret = 0;
end:
    close(fd);

    return ret;
}

static int get_backup_rootfs_part(partion_info_t *part)
{
    int ret;

    if (part == NULL) {
        return -EINVAL;
    }

    if ((ret = get_using_rootfs_part(part)) < 0) {
        return ret;
    }

    /* TODO: */

    return 0;
}

static int upgrade_rootfs(const char *img)
{
    int ret;
    partion_info_t part;
    char path[PATH_MAX];

    if (img == NULL) {
        return -EINVAL;
    }

    memset(path, 0, sizeof(path));
    if ((ret = get_backup_rootfs_part(&part)) < 0) {
        return ret;
    }

    return 0;
}

int upgrade_package(const char *pkg)
{
    if (!file_exist(pkg)) {
        return -EEXIST;
    }

    upgrade_kernel(pkg);
    upgrade_rootfs(pkg);
    return 0;
}
