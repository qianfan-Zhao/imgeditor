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

	if (dp->score == 0)
		dp->score = DISK_PARTITIONS_SCORE_GOOD;

	if (get_verbose_level() > 1) {
		printf("register disk: %d has %zu partitions, score = %d\n",
			dp->type, dp->n_parts, dp->score);

		for (size_t i = 0; i < dp->n_parts; i++) {
			struct disk_partition *part = &dp->parts[i];

			printf("    [%02zu]: %-8s %" PRIu64 ", %" PRIu64 "\n",
				i, part->name, part->start_addr, part->end_addr);
		}
	}

	list_add_tail(&dp->head, &gd->partitions);
}

void register_weak_disk_partitions(struct disk_partitions *dp)
{
	dp->score = DISK_PARTITIONS_SCORE_WEAK;

	register_disk_partitions(dp);
}

struct disk_partitions *
alloc_disk_partitions(enum disk_partition_type type, size_t n_parts)
{
	struct disk_partitions *dp;

	dp = calloc(1, sizeof(*dp) + n_parts * sizeof(dp->parts[0]));
	if (!dp)
		return dp;

	list_init(&dp->head);
	dp->type = type;
	dp->n_parts = n_parts;

	return dp;
}

void free_registed_disk_partitions(void)
{
	struct global_data *gd = imgeditor_get_gd();
	struct disk_partitions *dp, *next;

	list_for_each_entry_safe(dp, next, &gd->partitions, head,
				 struct disk_partitions) {
		list_del(&dp->head);
		free(dp);
	}
}

const struct disk_partition *
	find_registed_partition(uint64_t start_addr,
				enum disk_partition_type *ret_type)
{
	struct global_data *gd = imgeditor_get_gd();
	struct disk_partition *best_part = NULL;
	int best_score = 0;
	struct disk_partitions *dp;

	list_for_each_entry(dp, &gd->partitions, head, struct disk_partitions) {
		for (size_t i = 0; i < dp->n_parts; i++) {
			struct disk_partition *part = &dp->parts[i];

			if (start_addr >= part->start_addr
			   && start_addr <= part->end_addr) {
				if (ret_type)
					*ret_type = dp->type;

				if (dp->score > best_score) {
					best_score = dp->score;
					best_part = part;
				}
			}
		}
	}

	return best_part;
}
