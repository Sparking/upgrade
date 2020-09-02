#ifndef _UPGRADE_H_
#define _UPGRADE_H_

#include <stddef.h>
#include <limits.h>

enum {
    UPGRADE_NO_ERROR = 0,
};

/**
 * -- dual rootfs partions --
 * part1: boot/kernel
 * part2: root
 * part3: root_bak
 * part4: user data
 */
extern int upgrade_package(const char *pkg);

extern const char *upgrade_err2str(const int errcode);

#endif /* _UPGRADE_H_ */
