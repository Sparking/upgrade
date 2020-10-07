#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "list.h"
#include "rbtree.h"
#include "iniparser.h"
#include "common.h"

#define INI_LINE_MAXSIZE    2048

struct ini_head {
    struct list_head list;
    struct rb_root rb;
};

struct ini_node {
    struct list_head list;
    struct rb_node rb;
};

struct ini_tag {
    char *key;
    char *value;
    struct ini_node node;
};

struct ini_section {
    char *section;
    struct ini_node node;
    struct ini_head tags;
};

struct ini_config {
    char *file;
    struct ini_head sections;
};

enum {
    INI_CONFIG_SECTION = 0,
    INI_CONFIG_KEY_VALUE,
    INI_CONFIG_EMPTY,
};

union ini_parse_block {
    char *section;
    struct {
        char *key;
        char *value;
    } kv;
};

static inline int strempty(const char *str)
{
    return (str == NULL || *str == '\0');
}

static int ini_strcmp(const char *stra, const char *strb)
{
    if (strempty(stra)) {
        return strempty(strb) ? 0 : -1;
    } else if (strempty(strb)) {
        return 1;
    }

    return strcmp(stra, strb);
}

static int ini_parse_line(char *const line, union ini_parse_block *ipb)
{
    size_t i, j;
    int line_type;

    for (i = 0; line[i] != '\0'; ++i)
        if (!isspace(line[i]))
            break;
    switch (line[i]) {
    case '\0':
    case ';':
        line_type = INI_CONFIG_EMPTY;
        break;
    case '[':
        line_type = INI_CONFIG_SECTION;
        for (j = i + 1; line[j] != '\0' && line[j] != '\r' && line[j] != '\n';
            ++j) {
            if (line[j] == ']')
                break;
        }
        if (line[j] == ']') {
            line[j] = '\0';
            ipb->section = line + i + 1;
        } else {
            line_type = INI_CONFIG_EMPTY;
        }
        break;
    default:
        line_type = INI_CONFIG_KEY_VALUE;
        for (j = i + 1; line[j] != '\0' && line[j] != '\r' && line[j] != '\n';
            ++j) {
            if (line[j] == '=')
                break;
        }
        if (line[j] == '=') {
            ipb->kv.key = line + i;
            ipb->kv.value = line + j + 1;
            while (j-- > i) {
                if (!isspace(line[j]))
                    break;
            }
            line[++j] = '\0';
            for (i = 0; ipb->kv.value[i] != '\0'
                    && ipb->kv.value[i] != '\r' && ipb->kv.value[i] != '\n';
                ++i) {
                if (!isspace(ipb->kv.value[i])) {
                    ipb->kv.value = ipb->kv.value + i;
                    break;
                }
            }
            for (i = 0; ipb->kv.value[i] != '\0'
                    && ipb->kv.value[i] != '\r' && ipb->kv.value[i] != '\n';
                ++i) {
                continue;
            }
            while (i-- > 0)
                if (!isspace(ipb->kv.value[i]))
                    break;
            ipb->kv.value[++i] = '\0';
        } else {
            line_type = INI_CONFIG_EMPTY;
        }
        break;
    }

    return line_type;
}

static void ini_config_release_section(struct ini_config *config, struct ini_section *section)
{
    list_del(&section->node.list);
    rb_erase(&section->node.rb, &config->sections.rb);
    if (section->section) {
        free(section->section);
    }
    free(section);
}

static void ini_config_release_tag(struct ini_section *section, struct ini_tag *tag)
{
    list_del(&tag->node.list);
    rb_erase(&tag->node.rb, &section->tags.rb);
    if (tag->value) {
        free(tag->value);
    }
    free(tag->key);
    free(tag);
}

static struct rb_node *ini_config_find(struct rb_root *proot, const void *new_value,
    int (*cmp)(const void *, const struct rb_node *), struct rb_node ***pnew, struct rb_node **pparent)
{
    int ret;
    struct rb_node **new, *parent;

    parent = NULL;
    new = &proot->rb_node;
    while (*new) {
        parent = *new;
        ret = cmp(new_value, parent);
        if (ret == 0) {
            return parent;
        } else if (ret < 0) {
            new = &(*new)->rb_left;
        } else {
            new = &(*new)->rb_right;
        }
    }

    if (pnew && pparent) {
        *pnew = new;
        *pparent = parent;
    }

    return NULL;
}

static int ini_section_rb_cmp(const void *value, const struct rb_node *rb)
{
    return ini_strcmp(value, rb_entry(rb, struct ini_section, node.rb)->section);
}

static int ini_tag_rb_cmp(const void *value, const struct rb_node *rb)
{
    return ini_strcmp(value, rb_entry(rb, struct ini_tag, node.rb)->key);
}

static struct ini_section *ini_config_find_section(struct ini_config *config, const char *name)
{
    void *node;

    node = ini_config_find(&config->sections.rb, name, ini_section_rb_cmp, NULL, NULL);
    if (node) {
        node = rb_entry(node, struct ini_section, node.rb);
    }

    return node;
}

static struct ini_tag *ini_config_find_tag(struct ini_section *section, const char *key)
{
    void *node;

    node = ini_config_find(&section->tags.rb, key, ini_tag_rb_cmp, NULL, NULL);
    if (node) {
        node = rb_entry(node, struct ini_tag, node.rb);
    }

    return node;
}

static struct ini_section *ini_config_add_section(struct ini_config *config, const char *name)
{
    struct rb_node *node, **new, *parent;
    struct ini_section *section;

    node = ini_config_find(&config->sections.rb, name, ini_section_rb_cmp, &new, &parent);
    if (node != NULL) {
        return rb_entry(node, struct ini_section, node.rb);
    }

    section = (struct ini_section *)malloc(sizeof(struct ini_section));
    if (section == NULL) {
        return NULL;
    }

    section->tags.rb = RB_ROOT;
    INIT_LIST_HEAD(&section->tags.list);
    if (name != NULL) {
        list_add_tail(&section->node.list, &config->sections.list);
        section->section = strdup(name);
        if (section->section == NULL) {
            free(section);
            return NULL;
        }
    } else {
        list_add(&section->node.list, &config->sections.list);
        section->section = NULL;
    }
    rb_link_node(&section->node.rb, parent, new);
    rb_insert_color(&section->node.rb, &config->sections.rb);

    return section;
}

static struct ini_tag *ini_config_new_tag(const char *key, const char *value)
{
    struct ini_tag *tag;

    tag = (struct ini_tag *)malloc(sizeof(struct ini_tag));
    if (tag == NULL) {
        return NULL;
    }

    tag->key = strdup(key);
    if (tag->key == NULL) {
        free(tag);
        return NULL;
    }

    if (value) {
        tag->value = strdup(value);
        if (tag->value == NULL) {
            free(tag->key);
            free(tag);
            return NULL;
        }
    } else {
        tag->value = NULL;
    }

    return tag;
}

static struct ini_tag *ini_config_add_tag(struct ini_section *section, const char *key, const char *value)
{
    struct ini_tag *tag;
    struct rb_node *node, **new, *parent;

    if (section == NULL || strempty(key)) {
        return NULL;
    }

    node = ini_config_find(&section->tags.rb, key, ini_tag_rb_cmp, &new, &parent);
    if (node == NULL) {
        if ((tag = ini_config_new_tag(key, value)) != NULL) {
            list_add_tail(&tag->node.list, &section->tags.list);
            rb_link_node(&tag->node.rb, parent, new);
            rb_insert_color(&tag->node.rb, &section->tags.rb);
        }
    } else {
        tag = rb_entry(node, struct ini_tag, node.rb);
        if (ini_strcmp(tag->value, value) == 0) {
            return tag;
        }

        if (tag->value) {
            free(tag->value);
            tag->value = NULL;
        }

        if (value != NULL && (tag->value = strdup(value)) == NULL) {
            ini_config_release_tag(section, tag);
            return NULL;
        }
    }

    return tag;
}

INI_CONFIG ini_config_create(const char *const file)
{
    FILE *fp;
    struct ini_config *config;
    struct ini_section *section;
    union ini_parse_block ipb;
    char line[INI_LINE_MAXSIZE];

    if (file == NULL) {
        return NULL;
    }

    config = (INI_CONFIG )malloc(sizeof(*config));
    if (config == NULL) {
        return NULL;
    }

    INIT_LIST_HEAD(&config->sections.list);
    config->sections.rb = RB_ROOT;
    config->file = strdup(file);
    fp = fopen(file, "r");
    if (fp == NULL) {
        goto err;
    }

    if ((section = ini_config_add_section(config, NULL)) == NULL) {
        goto err;
    }

    while (fgets(line, sizeof(line) - 1, fp) != NULL) {
        switch (ini_parse_line(line, &ipb)) {
        case INI_CONFIG_SECTION:
            if ((section = ini_config_add_section(config, ipb.section)) == NULL) {
                goto err;
            }
            break;
        case INI_CONFIG_KEY_VALUE:
            if (ini_config_add_tag(section, ipb.kv.key, ipb.kv.value) == NULL) {
                goto err;
            }
            break;
        default:
            break;
        }
    }
    fclose(fp);

    return config;
err:
    ini_config_release(config);
    return NULL;
}

int ini_config_set(INI_CONFIG config, const char *section_name,
    const char *key, const char *value)
{
    int status;
    struct ini_tag *tag;
    struct ini_section *section;

    status = 0;
    section = ini_config_find_section((struct ini_config *)config, section_name);
    if (section == NULL) {
        status = 1;
        section = ini_config_add_section((struct ini_config *)config, section_name);
        if (section == NULL) {
            return -1;
        }
    }

    tag = ini_config_add_tag(section, key, value);
    if (tag == NULL && status) {
        ini_config_release_section((struct ini_config *)config, section);
        return -1;
    }

    return 0;
}

const char *ini_config_get(INI_CONFIG config, const char *section_name,
    const char *key, const char *default_value)
{
    struct ini_section *section;
    struct ini_tag *tag;

    if (key == NULL) {
        return NULL;
    }

    section = ini_config_find_section((struct ini_config *)config, section_name);
    if (section == NULL) {
        return default_value;
    }

    tag = ini_config_find_tag(section, key);
    if (tag == NULL || tag->value == NULL) {
        return default_value;
    }

    return tag->value;
}

static int ini_config_clear_section_node(struct ini_section *section)
{
    struct ini_tag *tag, *tmp_tag;

    if (section == NULL) {
        return -1;
    }

    list_for_each_entry_safe(tag, tmp_tag, &section->tags.list, node.list) {
        ini_config_release_tag(section, tag);
    }

    return 0;
}

int ini_config_clear_section(INI_CONFIG config, const char *section)
{
    return ini_config_clear_section_node(ini_config_find_section((struct ini_config *)config, section));
}

static int ini_config_erase_section_node(struct ini_config *config, struct ini_section *section)
{
    if (ini_config_clear_section_node(section) == -1) {
        return -1;
    }

    ini_config_release_section(config, section);

    return 0;
}

int ini_config_erase_section(INI_CONFIG config, const char *section)
{
    return ini_config_erase_section_node((struct ini_config *)config,
            ini_config_find_section((struct ini_config *)config, section));
}

int ini_config_erase_key(INI_CONFIG config, const char *section_name, const char *key)
{
    struct ini_section *section;
    struct ini_tag *tag;

    if (config == NULL || key == NULL)
        return -1;

    section = ini_config_find_section((struct ini_config *)config, section_name);
    if (section == NULL)
        return -1;

    tag = ini_config_find_tag(section, key);
    if (tag == NULL)
        return -1;

    ini_config_release_tag(section, tag);

    return 0;
}

int ini_config_save2filestream(INI_CONFIG config, FILE *fp)
{
    struct ini_tag *tag;
    struct ini_section *section;

    if (config == NULL || fp == NULL) {
        return -1;
    }

    list_for_each_entry (section, &((struct ini_config *)config)->sections.list, node.list) {
        if (section->section) {
            fprintf(fp, "[%s]\n", section->section);
        }

        list_for_each_entry (tag, &section->tags.list, node.list) {
            if (tag->value) {
                fprintf(fp, "%s = %s\n", tag->key, tag->value);
            } else {
                fprintf(fp, "%s =\n", tag->key);
            }
        }
    }
    fflush(fp);

    return 0;
}

int ini_config_saveas(INI_CONFIG config, const char *file)
{
    int ret;
    FILE *fp;

    if (file_exist(file) && truncate(file, 0) != 0) {
        unlink(file);
    }

    fp = fopen(file, "w");
    if (fp == NULL)
        return -1;

    ret = ini_config_save2filestream(config, fp);
    fclose(fp);

    return ret;
}

int ini_config_save(INI_CONFIG config)
{
    return ini_config_saveas(config, ((struct ini_config *)config)->file);
}

int ini_config_clear(INI_CONFIG config)
{
    struct ini_section *section, *temp_section;

    if (config == NULL) {
        return -1;
    }

    list_for_each_entry_safe(section, temp_section, &((struct ini_config *)config)->sections.list, node.list) {
        ini_config_erase_section_node((struct ini_config *)config, section);
    }
    ((struct ini_config *)config)->sections.rb = RB_ROOT;

    return 0;
}

void ini_config_release(INI_CONFIG config)
{
    if (ini_config_clear(config) == 0) {
        if (((struct ini_config *)config)->file) {
            free(((struct ini_config *)config)->file);
        }

        free(config);
    }
}
