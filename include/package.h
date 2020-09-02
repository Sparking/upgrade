#ifndef __UPGRADE_PACKAGE_H__
#define __UPGRADE_PACKAGE_H__

#include <stdint.h>
#include "list.h"

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

typedef struct {
    struct list_head  node;
    package_version_t version;
    char              md5sum[32];
    char              name[128];
    size_t            napply_id;
    uint32_t          apply_id[0];
} multi_os_blob_t;

typedef multi_os_blob_t os_packge_t;

extern package_type_t read_package(const char *pkg, struct list_head *blobs);

#endif /* __UPGRADE_PACKAGE_H__ */
