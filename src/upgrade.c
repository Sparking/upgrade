#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    int ret;
    size_t i;
    uint32_t id;
    os_blob_t *blob;
    os_package_t *os;
    char tmp[256];
    const char *name;

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

    progress_print(NULL, "Starting to upgrade system...\n");
    list_for_each_entry(blob, &os->blobs, node) {
        name = os_blob_type2name(blob->type);
        progress_print(NULL, "Decompressing %s file %s...", name, blob->name);
        if (decompress_package("/tmp/.up_dcm", pkg->path, blob->name) != 0) {
            progress_print(NULL, " fail\n");
            continue;
        }
        progress_print(NULL, " done\nUpgrading %s...", name);
        snprintf(tmp, sizeof(tmp), "/tmp/.up_dcm/%s", blob->name);
        switch (blob->type) {
        case OS_BLOB_BOOTLOADER:
            ret = upgrade_bootloader(tmp);
            break;
        case OS_BLOB_ROOTFS:
            ret = upgrade_rootfs(tmp);
            break;
        case OS_BLOB_KERNEL:
            ret = upgrade_kernel(tmp);
            break;
        default:
            /* 不处理 */
            ret = -1;
            break;
        }

        remove(tmp);
        if (ret != 0) {
            progress_print(NULL, " fail\n");
            break;
        } else {
            progress_print(NULL, " done\n");
        }
    }

    if (ret == 0) {
        progress_print(NULL, "Finish to upgrade system.\n");
    } else {
        progress_print(NULL, "Error ocurrs while upgrading!\n");
    }

    return ret;
}

int upgrade_package(const char *pkg)
{
    package_t *package;

    if ((package = read_package(pkg)) == NULL) {
        progress_clearline();
        progress_print(NULL, "Package is invalid, abort!\n");
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

#ifdef TEST
int main(int argc, char *argv[])
{
    if (argc < 2) {
        return -1;
    }

    upgrade_package(argv[1]);

    return 0;
}
#endif
