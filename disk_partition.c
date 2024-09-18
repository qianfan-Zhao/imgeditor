/*
 * disk partition manager
 *
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include <stdlib.h>
#include "imgeditor.h"
#include "gd_private.h"

void register_disk_partitions(struct disk_partitions *dp)
{
	struct global_data *gd = imgeditor_get_gd();
	int part_cell = -1;

	if (gd->active_partitions < GD_MAX_PARTITIONS) {
		part_cell = gd->active_partitions;
		gd->active_partitions++;
	}

	if (part_cell < 0) {
		free(dp);
		return;
	}

	if (dp->score == 0)
		dp->score = DISK_PARTITIONS_SCORE_GOOD;

	if (get_verbose_level() > 1) {
		printf("register disk: %s has %zu partitions, score = %d\n",
			dp->disk_type, dp->n_parts, dp->score);

		for (size_t i = 0; i < dp->n_parts; i++) {
			struct disk_partition *part = &dp->parts[i];

			printf("    [%02zu]: %-8s %" PRIu64 ", %" PRIu64 "\n",
				i, part->name, part->start_addr, part->end_addr);
		}
	}

	gd->disk_parts_array[part_cell] = dp;
}

void register_weak_disk_partitions(struct disk_partitions *dp)
{
	dp->score = DISK_PARTITIONS_SCORE_WEAK;

	register_disk_partitions(dp);
}

struct disk_partitions *
alloc_disk_partitions(const char *disk_type, size_t n_parts)
{
	struct disk_partitions *dp;

	dp = calloc(1, sizeof(*dp) + n_parts * sizeof(dp->parts[0]));
	if (!dp)
		return dp;

	snprintf(dp->disk_type, sizeof(dp->disk_type), "%s", disk_type);
	dp->n_parts = n_parts;

	return dp;
}

void free_registed_disk_partitions(void)
{
	struct global_data *gd = imgeditor_get_gd();

	for (size_t i = 0; i < gd->active_partitions; i++) {
		struct disk_partitions *parts = gd->disk_parts_array[i];

		gd->disk_parts_array[i] = NULL;
		free(parts);
	}

	gd->active_partitions = 0;
}

const struct disk_partition *
	find_registed_partition(uint64_t start_addr,
				const char **ret_disk_type)
{
	struct global_data *gd = imgeditor_get_gd();
	struct disk_partition *best_part = NULL;
	int best_score = 0;

	for (size_t i = 0; i < gd->active_partitions; i++) {
		struct disk_partitions *dp = gd->disk_parts_array[i];

		for (size_t i = 0; i < dp->n_parts; i++) {
			struct disk_partition *part = &dp->parts[i];

			if (start_addr >= part->start_addr
			   && start_addr <= part->end_addr) {
				if (ret_disk_type)
					*ret_disk_type = dp->disk_type;

				if (dp->score > best_score) {
					best_score = dp->score;
					best_part = part;
				}
			}
		}
	}

	return best_part;
}
