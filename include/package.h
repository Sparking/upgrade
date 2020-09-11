#ifndef __UPGRADE_PACKAGE_H__
#define __UPGRADE_PACKAGE_H__

#include <stdint.h>
#include "list.h"

#define PKG_FILE_NAME_SIZE      128

typedef enum {
    PKG_UNKNOWN = -1,
    PKG_OS,             /* 包好系统镜像和boot，boot非必须 */
    PKG_PATCH,          /* 补丁 */
    PKG_MULTI_OS,       /* 多个设备的OS */
    PKG_MULTI_PATCH,    /* 多个设备的补丁 */
} package_type_t;

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    char    compile[16 + 1];
} package_version_t;

typedef enum {
    OS_BLOB_OTHER = -1,
    OS_BLOB_BOOTLOADER,
    OS_BLOB_ROOTFS,
    OS_BLOB_KERNEL
} os_blob_type_t;

typedef struct {
    struct list_head node;
    os_blob_type_t   type;
    char             md5sum[32];
    char             name[PKG_FILE_NAME_SIZE];
} os_blob_t;

typedef struct {
    struct list_head  blobs;
    package_version_t version;
    size_t            napply_id;
    uint32_t          apply_id[0];
} os_package_t;

typedef struct {
    struct list_head  node;
    package_version_t version;
    char              md5sum[32];
    char              name[PKG_FILE_NAME_SIZE];
    size_t            napply_id;
    uint32_t          apply_id[0];
} multi_os_blob_t;

typedef struct {
    struct list_head blobs;
} multi_os_package_t;

typedef struct {
    package_type_t type;
    char           path[PATH_MAX];
    char           package[0];
} package_t;

extern package_t *read_package(const char *pkg);

extern const char *package_type2name(const package_type_t t);

extern const char *os_blob_type2name(const os_blob_type_t t);

extern int decompress_package(const char *dst, const char *pkg, const char *file);

extern int check_md5sum(const char *path, const char md5sum[32]);

#endif /* __UPGRADE_PACKAGE_H__ */
