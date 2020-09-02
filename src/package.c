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

static void release_multi_os_blob(multi_os_blob_t *blob)
{
    if (!blob) {
        return;
    }

    if (blob->name) {
        free(blob->name);
    }

    if (blob->version) {
        free(blob->version);
    }

    if (blob->apply_id) {
        free(blob->apply_id);
    }
    list_del(&blob->node);
    free(blob);
}

static multi_os_blob_t *read_multi_os_blob_from_json_array_item(json_object *obj)
{
    size_t i;
    const char *str;
    json_object *key, *value;
    multi_os_blob_t *blob;

    if (obj == NULL || (blob = (multi_os_blob_t *)malloc(sizeof(multi_os_blob_t))) == NULL) {
        return NULL;
    }

    blob->name = NULL;
    blob->version = NULL;
    blob->apply_id = NULL;
    INIT_LIST_HEAD(&blob->node);
    if ((key = json_object_object_get(obj, "name")) == NULL) {
        goto failure;
    }

    if ((str = json_object_get_string(key)) == NULL || (blob->name = strdup(str)) == NULL) {
        goto failure;
    }

    if ((key = json_object_object_get(obj, "md5sum")) == NULL
            || (str = json_object_get_string(key)) == NULL) {
        goto failure;
    }

    if (json_object_get_string_len(key) != sizeof(blob->md5sum)) {
        goto failure;
    }
    memcpy(blob->md5sum, str, sizeof(blob->md5sum));

    if ((key = json_object_object_get(obj, "version")) == NULL) {
        goto failure;
    }

    if ((str = json_object_get_string(key)) == NULL || (blob->version = strdup(str)) == NULL) {
        goto failure;
    }

    if ((key = json_object_object_get(obj, "apply id")) == NULL
            || (blob->napply_id = json_object_array_length(key)) == 0) {
        goto failure;
    }

    if ((blob->apply_id = (uint32_t *)malloc(sizeof(uint32_t) * blob->napply_id)) == NULL) {
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
    release_multi_os_blob(blob);

    return NULL;
}

static void realse_multi_os_header(struct list_head *header)
{
    multi_os_blob_t *blob, *tmp;

    list_for_each_entry_safe(blob, tmp, header, node) {
        release_multi_os_blob(blob);
    }
}

static int read_multi_os_blobs_from_json_obj(json_object *list, struct list_head *header)
{
    size_t i, n;
    json_object *obj;
    multi_os_blob_t *blob;

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
    realse_multi_os_header(header);
    return -1;
}

static multi_os_package_t *read_multi_os_info_from_json_obj(json_object *obj)
{
    json_object *key;
    multi_os_package_t *pkg;

    if ((pkg = (multi_os_package_t *)malloc(sizeof(multi_os_package_t))) == NULL) {
        return NULL;
    }

    INIT_LIST_HEAD(&pkg->blobs);
    if ((key = json_object_object_get(obj, "blobs")) == NULL) {
        goto failure;
    }

    if (read_multi_os_blobs_from_json_obj(key, &pkg->blobs) != 0) {
        goto failure;
    }

    return pkg;
failure:
    free(pkg);
    return NULL;
}

package_t *read_package(const char *pkg)
{
    int ret;
    package_t *res;
    package_type_t t;
    json_object *obj;
    json_object *val;
    char buff[PATH_MAX];
    char path[PATH_MAX];
    multi_os_blob_t *mos_blob;
    multi_os_package_t *mos;

    if (pkg == NULL) {
        return NULL;
    }

    res = NULL;
    if ((ret = shell_cmd_output("echo /tmp/.package_info_check_`date +%s`/", path,
            sizeof(path))) < 0) {
        return res;
    }

    strip_string_crlf(path);
    snprintf(buff, sizeof(buff), "mkdir -p '%s'", path);
    if ((ret = system(buff)) < 0) {
        return res;
    }

    snprintf(buff, sizeof(buff), cmd_extract_file, pkg, path, manifest_name);
    if ((ret = system(buff)) < 0) {
        remove(path);
        return res;
    }

    snprintf(buff, sizeof(buff), "%s%s", path, manifest_name);
    if ((obj = json_object_from_file(buff)) == NULL) {
        remove(path);
        return res;
    }
    remove(path);

    if ((val = json_object_object_get(obj, "type")) == NULL
            || (t = str2type(json_object_get_string(val))) == PKG_UNKNOWN) {
        goto release_json;
    }

    if ((res = (package_t *)malloc(sizeof(package_t))) == NULL) {
        goto release_json;
    }

    res->data = NULL;
    res->type = PKG_UNKNOWN;
    switch (t) {
    case PKG_OS:
        res->data = NULL;//read_os_blobs_from_json_obj(obj);
        break;
    case PKG_PATCH:
        res->data = NULL;//read_patch_blobs_from_json_obj(obj);
        break;
    case PKG_MULTI_OS:
        mos = read_multi_os_info_from_json_obj(obj);
        list_for_each_entry(mos_blob, &mos->blobs, node) {
            snprintf(buff, sizeof(buff), cmd_check_md5sum, pkg, mos_blob->name);
            if (shell_cmd_output(buff, buff, sizeof(buff)) != 0
                    || memcmp(mos_blob->md5sum, buff, sizeof(mos_blob->md5sum)) != 0) {
                realse_multi_os_header(&mos->blobs);
                free(mos);
                free(res);
                res = NULL;
                goto release_json;
            }
        }
        res->data = mos;
        break;
    case PKG_MULTI_PATCH:
        res->data =NULL;// read_multi_patch_blobs_from_json_obj(obj);
        break;
    case PKG_UNKNOWN:
    default:
        free(res);
        goto release_json;
    }
    res->type = t;

release_json:
    json_object_put(obj);

    return res;
}

#ifdef TEST
int main(int argc, char *argv[])
{
    package_t *pkg;
 
    if (argc < 2) {
        return -1;
    }

    pkg = read_package(argv[1]);
    if (pkg == NULL) {
        return -1;
    }

    printf("os_pkg:");
    printf("{\n"
            "\tname:%s"
            "}\n",
            argv[1]);


    return 0;
}
#endif