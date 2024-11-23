/*
 * global data shared with all plugins
 * qianfan Zhao <qianfanguijin@163.com>
 */
#ifndef IMGEDITOR_GLOBAL_PRIVATE_H
#define IMGEDITOR_GLOBAL_PRIVATE_H

#include <stdint.h>
#include <unistd.h>

#define MAX_VIRTUAL_FILE		32

struct virtual_file {
	int				fd;
	off64_t				start_offset;
	int64_t				total_length;
	int				used;
};

struct disk_partitions;
#define GD_MAX_PARTITIONS		8

struct imgeditor;
#define GD_MAX_IMGEDITOR		64

struct global_data {
	int				verbose_level;

	size_t				active_partitions;
	struct disk_partitions		*disk_parts_array[GD_MAX_PARTITIONS];

	size_t				export_imgeditor_counts;
	struct imgeditor		*export_imgeditors[GD_MAX_IMGEDITOR];

	struct virtual_file		vfps[MAX_VIRTUAL_FILE];

	int				search_mode;
};

struct global_data *imgeditor_get_gd(void);

int imgeditor_core_setup_gd(void);
int imgeditor_plugin_setup_gd(void);
void imgeditor_free_gd(void);

void gd_export_imgeditor(struct imgeditor *);
struct imgeditor *gd_get_imgeditor(const char *name);

#endif
