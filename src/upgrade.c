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
#include "package.h"

static int get_device_id(uint32_t *id)
{
    *id = 123;

    return 0;
}

static int upgrade_bootloader(const char *pkg)
{
    return 0;
}

static int upgrade_kernel(const char *pkg)
{
    return 0;
}

static int upgrade_rootfs(const char *pkg)
{
    return 0;
}

static int upgrade_os(const package_t *pkg)
{
    /* get_device_id */
    size_t i;
    uint32_t id;
    os_blob_t *blob;
    os_package_t *os;
    char tmp[256];

    if (pkg == NULL) {
        return -1;
    }

    if (get_device_id(&id) != 0) {
        return -1;
    }

    os = (os_package_t *)pkg->package;
    for (i = 0; i < os->napply_id; ++i) {
        if (os->apply_id[i] == id) {
            break;
        }
    }

    if (i >= os->napply_id) {
        return -1;
    }

    list_for_each_entry(blob, &os->blobs, node) {
        if (decompress_package("/tmp", pkg->path, blob->name) != 0) {
            continue;
        }

        snprintf(tmp, sizeof(tmp), "/tmp/%s", blob->name);
        if (check_md5sum(tmp, blob->md5sum) != 0) {
            continue;
        }

        switch (blob->type) {
        case OS_BLOB_BOOTLOADER:
            upgrade_bootloader(tmp);
            break;
        case OS_BLOB_ROOTFS:
            upgrade_rootfs(tmp);
            break;
        case OS_BLOB_KERNEL:
            upgrade_kernel(tmp);
            break;
        default:
            break;
        }
    }
}

int upgrade_package(const char *pkg)
{
    package_t *package;

    if ((package = read_package(pkg)) == NULL) {
        return -1;
    }

    switch (package->type) {
    case PKG_MULTI_OS:
        break;
    case PKG_OS:
        upgrade_os(package);
        break;
    case PKG_MULTI_PATCH:
        break;
    case PKG_PATCH:
        break;
    default:
        free(package);
        return -1;
        break;
    }

    return 0;
}
