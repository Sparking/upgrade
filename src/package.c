#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <json-c/json.h>
#include "common.h"
#include "package.h"

const char *const manifest_name = "manifest.json";
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

int str2version(const char *str, package_version_t *ver)
{
    unsigned int a, b, c;

    if (str == NULL || ver === NULL) {
        return -1;
    }

    if ((sscanf(str, "%u.%u.%u.%.16s", &a, &b, &c, ver->compile)) != 4) {
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
    if ((blob = (multi_os_blob_t *)malloc(sizeof(multi_os_blob_t)) + n) == NULL) {
        return NULL;
    }

    memset(blob, 0, n);
    blob->napply_id = i;
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

    if ((key = json_object_object_get(obj, "version")) == NULL
            || (str = json_object_get_string(key)) == NULL
            || str2version(str, &blob->version) < 0) {
        goto failure;
    }

    for (i = 0; i < blob->napply_id; ++i) {
        value = json_object_array_get_idx(key, i);
        if (json_object_get_type(value) != json_type_int) {
            goto failure;
        }

        blob->apply_id[i] = (uint32_t)json_object_get_uint64(value);
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

package_type_t read_package(const char *pkg, struct list_head *head)
{
    int ret;
    package_type_t t;
    json_object *obj;
    json_object *val;
    char buff[PATH_MAX];
    char path[PATH_MAX];
    multi_os_blob_t *mos_blob;

    t = PKG_UNKNOWN;
    if (pkg == NULL || head == NULL) {
        return t;
    }

    if ((ret = shell_cmd_output("echo /tmp/.package_info_check_`date +%s`/", path,
            sizeof(path))) < 0) {
        return t;
    }

    strip_string_crlf(path);
    snprintf(buff, sizeof(buff), "mkdir -p '%s'", path);
    if ((ret = system(buff)) < 0) {
        return t;
    }

    snprintf(buff, sizeof(buff), cmd_extract_file, pkg, path, manifest_name);
    if ((ret = system(buff)) < 0) {
        remove(path);
        return t;
    }

    snprintf(buff, sizeof(buff), "%s%s", path, manifest_name);
    if ((obj = json_object_from_file(buff)) == NULL) {
        remove(path);
        return t;
    }
    remove(path);

    if ((val = json_object_object_get(obj, "type")) == NULL
            || (t = str2type(json_object_get_string(val))) == PKG_UNKNOWN) {
        goto release_json;
    }

    if ((val = json_object_object_get(obj, "blobs")) == NULL) {
        t = PKG_UNKNOWN;
        goto release_json;
    }

    INIT_LIST_HEAD(head);
    switch (t) {
    case PKG_OS:
        //read_os_blobs_from_json_obj(obj);
        break;
    case PKG_PATCH:
        //read_patch_blobs_from_json_obj(obj);
        break;
    case PKG_MULTI_OS:
        if (read_multi_os_blobs_from_json_obj(val, head) < 0) {
            t = PKG_UNKNOWN;
        }
        break;
    case PKG_MULTI_PATCH:
        //read_multi_patch_blobs_from_json_obj(obj);
        break;
    case PKG_UNKNOWN:
    default:
        goto release_json;
    }

release_json:
    json_object_put(obj);
    return t;
}

#ifdef TEST
int main(int argc, char *argv[])
{
    package_type_t type;
    struct list_head head;
 
    if (argc < 2) {
        return -1;
    }

    if ((type = read_package(argv[1], head)) == PKG_UNKNOWN) {
        fprintf(stderr, "read invalid package\n");
        return -1;
    }

    printf("pkg type %d\n", type);

    return 0;
}
#endif