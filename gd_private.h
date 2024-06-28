/*
 * global data shared with all plugins
 * qianfan Zhao <qianfanguijin@163.com>
 */
#ifndef IMGEDITOR_GLOBAL_PRIVATE_H
#define IMGEDITOR_GLOBAL_PRIVATE_H

#include <unistd.h>
#include "list_head.h"

#define MAX_VIRTUAL_FILE		32

struct virtual_file {
	int				fd;
	off64_t				start_offset;
	int64_t				total_length;
	int				used;
};

struct global_data {
	int				verbose_level;

	/* the list head for register_disk_partitions */
	struct list_head		partitions;

	struct virtual_file		vfps[MAX_VIRTUAL_FILE];
};

struct global_data *imgeditor_get_gd(void);

int imgeditor_core_setup_gd(void);
int imgeditor_plugin_setup_gd(void);
void imgeditor_free_gd(void);

#endif
