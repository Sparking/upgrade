#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include <json-c/json.h>
#include "common.h"
#include "package.h"

const char *const cmd_check_md5sum = "tar -O -I zstd -xf %s %s | md5sum";
const char *const cmd_extract_file = "tar -I zstd -xf %s -C %s %s";

static const struct {
    package_type_t type;
    const char *const name;
} package_type_map[] = {
    {PKG_OS,            "os"},
    {PKG_PATCH,         "patch"},
    {PKG_MULTI_OS,      "multi-os"},
    {PKG_MULTI_PATCH,   "multi-patch"}
};

static const struct {
    os_blob_type_t type;
    const char *const name;
} os_blob_type_map[] = {
    {OS_BLOB_BOOTLOADER,    "bootloader"},
    {OS_BLOB_KERNEL,        "kernel"},
    {OS_BLOB_ROOTFS,        "rootfs"},
    {OS_BLOB_OTHER,         "other"}
};

int decompress_package(const char *dst, const char *pkg, const char *file)
{
    int ret;

    if (dst == NULL) {
        return -1;
    }

    if (access(dst, F_OK) != 0 && (ret = shell_command("mkdir -p %s", dst)) != 0) {
        return -1;
    }

    return shell_command(cmd_extract_file, pkg, dst, file);
}

int check_md5sum(const char *path, const char md5sum[32])
{
    char buf[33];

    if (shell_command_output(buf, sizeof(buf), "md5sum %s", path) < 0) {
        return -1;
    }

    return abs(memcmp(md5sum, buf, 32));
}

static package_type_t str2type(const char *str)
{
    int i;
    package_type_t type;

    type = PKG_UNKNOWN;
    if (str == NULL) {
        return type;
    }

    for (i = 0; i < ARRAY_SIZE(package_type_map); ++i) {
        if (strcmp(package_type_map[i].name, str) == 0) {
            type = package_type_map[i].type;
            break;
        }
    }

    return type;
}

const char *package_type2name(const package_type_t t)
{
    int i;
    const char *name;

    name = "unknown";
    for (i = 0; i < ARRAY_SIZE(package_type_map); ++i) {
        if (package_type_map[i].type == t) {
            name = package_type_map[i].name;
            break;
        }
    }

    return name;
}

const char *os_blob_type2name(const os_blob_type_t t)
{
    int i;
    const char *name;

    name = "unknown";
    for (i = 0; i < ARRAY_SIZE(os_blob_type_map); ++i) {
        if (os_blob_type_map[i].type == t) {
            name = os_blob_type_map[i].name;
            break;
        }
    }

    return name;
}

int str2version(const char *str, package_version_t *ver)
{
    unsigned int a, b, c;

    if (str == NULL || ver == NULL) {
        return -1;
    }

    if ((sscanf(str, "%u.%u.%u.%16s", &a, &b, &c, ver->compile)) != 4) {
        return -1;
    }

    ver->major = (uint8_t)a;
    ver->minor = (uint8_t)b;
    ver->patch = (uint8_t)c;
    return 0;
}

static multi_os_blob_t *read_multi_os_blob_from_json_array_item(json_object *obj)
{
    size_t i;
    size_t n;
    const char *str;
    json_object *key, *value;
    multi_os_blob_t *blob;

    if (obj == NULL) {
        return NULL;
    }

    if ((key = json_object_object_get(obj, "apply id")) == NULL
            || (i = json_object_array_length(key)) == 0) {
        return NULL;
    }

    n = sizeof(multi_os_blob_t) + i * sizeof(uint32_t);
    if ((blob = (multi_os_blob_t *)malloc(n)) == NULL) {
        return NULL;
    }

    memset(blob, 0, n);
    blob->napply_id = i;
    INIT_LIST_HEAD(&blob->node);

    for (i = 0; i < blob->napply_id; ++i) {
        value = json_object_array_get_idx(key, i);
        if (json_object_get_type(value) != json_type_int) {
            goto failure;
        }

        blob->apply_id[i] = (uint32_t)json_object_get_int(value);
    }

    if ((key = json_object_object_get(obj, "name")) == NULL
            || (str = json_object_get_string(key)) == NULL
            || json_object_get_string_len(key) >= sizeof(blob->name)) {
        goto failure;
    }
    strncpy(blob->name, str, sizeof(blob->name) - 1);

    if ((key = json_object_object_get(obj, "md5sum")) == NULL
            || (str = json_object_get_string(key)) == NULL
            || json_object_get_string_len(key) != sizeof(blob->md5sum)) {
        goto failure;
    }
    memcpy(blob->md5sum, str, sizeof(blob->md5sum));

    if ((key = json_object_object_get(obj, "version")) == NULL
            || (str = json_object_get_string(key)) == NULL
            || str2version(str, &blob->version) < 0) {
        goto failure;
    }

    return blob;
failure:
    free(blob);

    return NULL;
}

static int read_multi_os_blobs_from_json_obj(json_object *list, struct list_head *header)
{
    size_t i, n;
    json_object *obj;
    multi_os_blob_t *blob, *tmp;

    if (list == NULL || header == NULL || (n = json_object_array_length(list)) == 0) {
        return -1;
    }

    for (i = 0; i < n; ++i) {
        obj = json_object_array_get_idx(list, i);
        if (json_object_get_type(obj) != json_type_object) {
            goto failure;
        }

        if ((blob = read_multi_os_blob_from_json_array_item(obj)) == NULL) {
            goto failure;
        }

        list_add_tail(&blob->node, header);
    }

    return 0;
failure:
    list_for_each_entry_safe(blob, tmp, header, node) {
        list_del(&blob->node);
        free(blob);
    }
    return -1;
}

static os_blob_t *read_os_blob_from_json_array_item(json_object *obj)
{
    const char *str;
    json_object *key;
    os_blob_t *blob;

    if (obj == NULL || (blob = (os_blob_t *)malloc(sizeof(os_blob_t))) == NULL) {
        return NULL;
    }

    memset(blob, 0, sizeof(os_blob_t));
    INIT_LIST_HEAD(&blob->node);
    if ((key = json_object_object_get(obj, "name")) == NULL
            || (str = json_object_get_string(key)) == NULL
            || json_object_get_string_len(key) >= sizeof(blob->name)) {
        goto failure;
    }
    strncpy(blob->name, str, sizeof(blob->name) - 1);

    if ((key = json_object_object_get(obj, "md5sum")) == NULL
            || (str = json_object_get_string(key)) == NULL
            || json_object_get_string_len(key) != sizeof(blob->md5sum)) {
        goto failure;
    }
    memcpy(blob->md5sum, str, sizeof(blob->md5sum));

    if ((key = json_object_object_get(obj, "type")) == NULL
            || (str = json_object_get_string(key)) == NULL) {
        goto failure;
    }

    if (strcmp(str, "rootfs") == 0) {
        blob->type = OS_BLOB_ROOTFS;
    } else if (strcmp(str, "kernel") == 0) {
        blob->type = OS_BLOB_KERNEL;
    } else if (strcmp(str, "bootloader") == 0) {
        blob->type = OS_BLOB_BOOTLOADER;
    } else {
        blob->type = OS_BLOB_OTHER;
    }

    return blob;
failure:
    free(blob);

    return NULL;
}

static int read_os_blobs_from_json_obj(json_object *list, struct list_head *header)
{
    size_t i, n;
    json_object *obj;
    os_blob_t *blob, *tmp;

    if (list == NULL || header == NULL || (n = json_object_array_length(list)) == 0) {
        return -1;
    }

    for (i = 0; i < n; ++i) {
        obj = json_object_array_get_idx(list, i);
        if (json_object_get_type(obj) != json_type_object) {
            goto failure;
        }

        if ((blob = read_os_blob_from_json_array_item(obj)) == NULL) {
            goto failure;
        }

        list_add_tail(&blob->node, header);
    }

    return 0;
failure:
    list_for_each_entry_safe(blob, tmp, header, node) {
        list_del(&blob->node);
        free(blob);
    }
    return -1;
}

package_t *read_package(const char *pkg)
{
    int ret;
    size_t i, n;
    char md5sum[32 + 1];
    package_t *package;
    package_type_t t;
    json_object *obj;
    json_object *val;
    json_object *val1;
    json_object *blob_obj;
    struct list_head *head;
    multi_os_blob_t *mos_tmp;
    multi_os_blob_t *mos_blob;
    os_blob_t *os_blob, *os_tmp;
    const char *const tmp_path = "/tmp/.upgrade_check_manifest";
    const char *const manifest = "/tmp/.upgrade_check_manifest/manifest.json";

    if (pkg == NULL) {
        return NULL;
    }

    progress_print(NULL, "Read package from %s.\n", pkg);
    if ((ret = shell_command("mkdir -p %s", tmp_path)) < 0) {
        return NULL;
    }

    if ((ret = shell_command(cmd_extract_file, pkg, tmp_path, "manifest.json")) < 0) {
        progress_print(NULL, "Package does not contain the valid information!\n");
        unlink(manifest);
        return NULL;
    }

    if ((obj = json_object_from_file(manifest)) == NULL) {
        progress_print(NULL, "The package information is broken!\n");
        unlink(manifest);
        return NULL;
    }
    unlink(manifest);

    progress_print(NULL, "Starting to parse package information...\n");
    package = NULL;
    if ((val = json_object_object_get(obj, "type")) == NULL
            || (t = str2type(json_object_get_string(val))) == PKG_UNKNOWN
            || (blob_obj = json_object_object_get(obj, "blobs")) == NULL) {
        progress_print(NULL, "The package information is not available!\n");
        goto release_json;
    }

    switch (t) {
    case PKG_OS:
        if ((val = json_object_object_get(obj, "apply id")) == NULL
                || (n = json_object_array_length(val)) == 0) {
            progress_print(NULL, "The package is unavailable for upgrading!\n");
            goto release_json;
        }

        i = sizeof(uint32_t) * n + sizeof(package_t) + sizeof(os_package_t);
        if ((package = (package_t *)malloc(i)) == NULL) {
            progress_clearline();
            progress_print(NULL, "System has no enough memories to allocate!\n");
            goto release_json;
        }

        memset(package, 0, i);
        for (i = 0; i < n; ++i) {
            val1 = json_object_array_get_idx(val, i);
            if (json_object_get_type(val1) != json_type_int) {
                free(package);
                package = NULL;
                progress_clearline();
                progress_print(NULL, "The package is unavailable for upgrading!\n");
                goto release_json;
            }
            ((os_package_t *)package->package)->apply_id[i] = (uint32_t)json_object_get_int(val1);
        }
        ((os_package_t *)package->package)->napply_id = n;

        if ((val = json_object_object_get(obj, "version")) != NULL
                && json_object_get_type(val) == json_type_string) {
            str2version(json_object_get_string(val), &((os_package_t *)package->package)->version);
            progress_print(NULL, "The package version is %s.\n", json_object_get_string(val));
        }

        head = &((os_package_t *)package->package)->blobs;
        INIT_LIST_HEAD(head);
        if (read_os_blobs_from_json_obj(blob_obj, head) < 0) {
            free(package);
            package = NULL;
            progress_print(NULL, "The package is unavailable for upgrading!\n");
            goto release_json;
        }

        /* md5sum check */
        list_for_each_entry(os_blob, head, node) {
            progress_print(NULL, "Check md5sum of the package %s file...", os_blob_type2name(os_blob->type));
            if (shell_command_output(md5sum, sizeof(md5sum), cmd_check_md5sum, pkg, os_blob->name) < 0
                    || memcmp(md5sum, os_blob->md5sum, sizeof(os_blob->md5sum)) != 0) {
                progress_print(NULL, "\tfailed\n");
                list_for_each_entry_safe(os_blob, os_tmp, head, node) {
                    list_del(&os_blob->node);
                    free(os_blob);
                }
                free(package);
                package = NULL;
                goto release_json;
            }
            progress_print(NULL, "\tpass\n");
        }
        break;
    case PKG_MULTI_OS:
        if ((package = (package_t *)malloc(sizeof(package_t) + sizeof(multi_os_package_t))) == NULL) {
            goto release_json;
        }

        head = &((multi_os_package_t *)package->package)->blobs;
        INIT_LIST_HEAD(head);
        if (read_multi_os_blobs_from_json_obj(blob_obj, head) < 0) {
            free(package);
            package = NULL;
            break;
        }

        /* md5sum check */
        list_for_each_entry(mos_blob, head, node) {
            if (shell_command_output(md5sum, sizeof(md5sum), cmd_check_md5sum, pkg, mos_blob->name) < 0
                    || memcmp(md5sum, mos_blob->md5sum, sizeof(mos_blob->md5sum)) != 0) {
                list_for_each_entry_safe(mos_blob, mos_tmp, head, node) {
                    list_del(&mos_blob->node);
                    free(mos_blob);
                }
                free(package);
                package = NULL;
                goto release_json;
            }
        }
        break;
    case PKG_PATCH:
        break;
    case PKG_MULTI_PATCH:
        break;
    case PKG_UNKNOWN:
    default:
        goto release_json;
    }
    package->type = t;
    strncpy(package->path, pkg, sizeof(package->path) - 1);

release_json:
    json_object_put(obj);
    return package;
}
