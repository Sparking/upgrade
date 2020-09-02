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
    char major;
    char minor;
    char patch;
} patch_version_t;

typedef struct {
    struct list_head node;
    char *name;
    char *version;
    uint32_t *apply_id;
    size_t    napply_id;
    char  md5sum[32];
} multi_os_blob_t;

typedef struct {
    struct list_head blobs;
} multi_os_package_t;

typedef struct {
    package_type_t type;
    void *data;
} package_t;

extern package_t *read_package(const char *pkg);

#endif /* __UPGRADE_PACKAGE_H__ */
