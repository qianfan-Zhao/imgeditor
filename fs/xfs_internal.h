/*
 * common include file for xfs
 * qianfan Zhao <qianfanguijin@163.com>
 */
#ifndef IMGEDITOR_XFS_INTERNAL_H
#define IMGEDITOR_XFS_INTERNAL_H

#include "imgeditor.h"

#define __packed		__attribute__((packed))

#define U32_MAX			((uint32_t)~0U)
#define S32_MAX			((int32_t)(U32_MAX >> 1))
#define S32_MIN			((int32_t)(-S32_MAX - 1))

typedef struct {
	uint8_t			uuid_bytes[16];
} uuid_t;

typedef off_t			xfs_off_t;
typedef uint64_t		xfs_ino_t;
typedef uint32_t		xfs_dev_t;
typedef int64_t			xfs_daddr_t;
typedef __u32			xfs_nlink_t;

#endif
