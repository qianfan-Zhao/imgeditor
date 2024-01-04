/*
 * timezone file editor, based on glibc/timezone/tzfile.h and RFC8536
 *
 * qianfan Zhao 2023-07-24
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "imgeditor.h"
#include "structure.h"
#include "string_helper.h"

#define TZ_MAGIC		"TZif"

struct tzhead {
	char	tzh_magic[4];		/* TZ_MAGIC */
	char	tzh_version;		/* '\0' or '2' or '3' as of 2013 */
	char	tzh_reserved[15];	/* reserved; must be zero */
	__be32	tzh_ttisutcnt;		/* coded number of trans. time flags */
	__be32	tzh_ttisstdcnt;		/* coded number of trans. time flags */
	__be32	tzh_leapcnt;		/* coded number of leap seconds */
	__be32	tzh_timecnt;		/* coded number of transition times */
	__be32	tzh_typecnt;		/* coded number of local time types */
	__be32	tzh_charcnt;		/* coded number of abbr. chars */
};

/*
 * tzif data block offsets
 */
static size_t tzif_db_time_size(int version)
{
	/* In the version 1 data block, time values are 32 bits (TIME_SIZE = 4).
	 * In the version 2+ data block, present only in version o2 and 3 files,
	 * time values are 64 bits.
	 */
	return version > 1 ? 8 : 4;
}

static size_t tzif_dbo_transition_times(struct tzhead *head, int version)
{
	return 0;
}

static size_t tzif_dbo_transition_types(struct tzhead *head, int version)
{
	return be32_to_cpu(head->tzh_timecnt) * tzif_db_time_size(version);
}

static size_t tzif_dbo_local_time_type_records(struct tzhead *head, int version)
{
	return tzif_dbo_transition_types(head, version)
			+ be32_to_cpu(head->tzh_timecnt);
}

static size_t tzif_dbo_time_zone_designations(struct tzhead *head, int version)
{
	return tzif_dbo_local_time_type_records(head, version)
			+ be32_to_cpu(head->tzh_typecnt) * 6;
}

static size_t tzif_dbo_leap_second_records(struct tzhead *head, int version)
{
	return tzif_dbo_time_zone_designations(head, version)
			+ be32_to_cpu(head->tzh_charcnt);
}

static size_t tzif_dbo_std_indicators(struct tzhead *head, int version)
{
	return tzif_dbo_leap_second_records(head, version)
			+ (be32_to_cpu(head->tzh_leapcnt) *
				(tzif_db_time_size(version) + 4));
}

static size_t tzif_dbo_ut_indicators(struct tzhead *head, int version)
{
	return tzif_dbo_std_indicators(head, version)
			+ be32_to_cpu(head->tzh_ttisstdcnt);
}

static size_t tzif_datablock_total_size(struct tzhead *head, int version)
{
	return tzif_dbo_ut_indicators(head, version)
			+ be32_to_cpu(head->tzh_ttisutcnt);
}

struct tzfile_editor_private_data {
	int		version;

	struct tzhead	v1_head;
	struct tzhead	hv_head; /* high version head */

	/* The TZif footer is present only in version 2 and 3 files */
	char		footer[128];
};

static int tzfile_detect(void *private_data, int force_type, int fd)
{
	struct tzfile_editor_private_data *p = private_data;
	size_t offset_hv_head, offset_footer;

	read(fd, &p->v1_head, sizeof(p->v1_head));
	p->version = -1;

	if (memcmp(p->v1_head.tzh_magic, TZ_MAGIC, sizeof(p->v1_head.tzh_magic))) {
		fprintf_if_force_type("Error: TZ_MAGIC of v1 doesn't match\n");
		return -1;
	}

	switch (p->v1_head.tzh_version) {
	case '\0':
		p->version = 1;
		break;
	case '2':
	case '3':
		p->version = p->v1_head.tzh_version - '0';
		break;
	default:
		fprintf_if_force_type("Error: can not detect tzfile version\n");
		return -1;
	}

	if (p->version == 1)
		return 0;

	offset_hv_head = sizeof(p->v1_head)
		+ tzif_datablock_total_size(&p->v1_head, 1);
	lseek(fd, offset_hv_head, SEEK_SET);
	read(fd, &p->hv_head, sizeof(p->hv_head));
	if (memcmp(p->hv_head.tzh_magic, TZ_MAGIC, sizeof(p->hv_head.tzh_magic))) {
		fprintf_if_force_type("Error: TZ_MAGIC of v%d doesn't match\n",
				      p->version);
		return -1;
	}

	/* read the footer */
	offset_footer = offset_hv_head + sizeof(p->hv_head)
		+ tzif_datablock_total_size(&p->hv_head, p->version);
	lseek(fd, offset_footer, SEEK_SET);
	read(fd, p->footer, sizeof(p->footer) - 1);
	if (p->footer[0] != '\n' || p->footer[strlen(p->footer) - 1] != '\n') {
		fprintf_if_force_type("Error: invalid footer format\n");
		return -1;
	}

	return 0;
}

static uint64_t makebe(uint8_t **p_buf, size_t bytes)
{
	uint8_t *p = *p_buf;
	uint64_t n = 0;

	while (bytes-- > 0) {
		n = (n << 8);
		n |= *p;
		p++;
	}

	*p_buf = p;
	return n;
}

static int tzif_list_transition_times(struct tzhead *head, int version,
				      uint8_t *data_block)
{
	uint8_t *transition_times = data_block + tzif_dbo_transition_times(head, version);
	uint8_t *transition_types = data_block + tzif_dbo_transition_types(head, version);
	uint8_t *localtime_type_records = data_block + tzif_dbo_local_time_type_records(head, version);
	char *designations = (char *)data_block + tzif_dbo_time_zone_designations(head, version);
	uint32_t timecnt = be32_to_cpu(head->tzh_timecnt);

	for (uint32_t i = 0; i < timecnt; i++) {
		uint64_t t = makebe(&transition_times, tzif_db_time_size(version));
		int transition_type = transition_types[i];
		uint8_t *p_type_record = &localtime_type_records[transition_type * 6];
		int32_t gmtoff;
		int desigidx;
		time_t utc_time;
		struct tm tm;
		char buf[64];

		printf("\t\t%3d/%3d: ", i + 1, timecnt);
		if (version > 1) {
			printf("0x%016" PRIx64, t);
			utc_time = (int64_t)t;
		} else {
			printf("0x%08" PRIx64, t);
			utc_time = (int32_t)t;
		}

		gmtoff = (int32_t)makebe(&p_type_record, 4);
		makebe(&p_type_record, 1); /* Daylight Saving Time flag */
		desigidx = (int)makebe(&p_type_record, 1);

		for (int j = 0; j < 2; j++) {
			if (j == 1)
				utc_time += gmtoff;

			gmtime_r(&utc_time, &tm);
			asctime_r(&tm, buf);
			buf[strlen(buf) - 1] = '\0'; /* remove the last '\n' */

			if (j == 0) {
				printf(" %s UT", buf); /* utc time */
			} else {
				printf(" = %s %s gmtoff = %d",
					buf,
					designations + desigidx,
					gmtoff);
			}
		}

		printf("\n");
	}

	return 0;
}

static int tzif_list_local_time_type_records(struct tzhead *head, int version,
					     uint8_t *data_block)
{
	uint8_t *p = data_block + tzif_dbo_local_time_type_records(head, version);
	char *designation = (char *)data_block + tzif_dbo_time_zone_designations(head, version);
	uint32_t typecnt = be32_to_cpu(head->tzh_typecnt);

	/* local time type records:
	 *
	 * ------------------------
	 * | utoff(4) | dst | idx |
	 * ------------------------
	 */
	for (uint32_t i = 0; i < typecnt; i++) {
		int32_t utoff = makebe(&p, 4);
		uint8_t dst = makebe(&p, 1);
		uint8_t idx = makebe(&p, 1);

		printf("\t\t%08x(%02d:%02dh) %s %s\n",
			utoff, utoff / 3600 /* hour */, utoff % 3600 / 60, /* min */
			designation + idx,
			dst ? "(Daylight Saving Time)" : ""
			);
	}

	return 0;
}

static int tzif_list(struct tzhead *head, int version, uint8_t *data_block)
{
	printf("Version %d\n", version);
	printf("\tUT/local indicators: %d\n", be32_to_cpu(head->tzh_ttisutcnt));
	printf("\tstandard/wall indicators: %d\n", be32_to_cpu(head->tzh_ttisstdcnt));
	printf("\tleap-second records: %d\n", be32_to_cpu(head->tzh_leapcnt));
	printf("\ttransition times: %d\n", be32_to_cpu(head->tzh_timecnt));
	printf("\tlocal time type records: %d\n", be32_to_cpu(head->tzh_typecnt));
	printf("\ttime zone designations: %d\n", be32_to_cpu(head->tzh_charcnt));
	printf("\n");

	/* tzdb-2023c doesn't define any timecnt in the front V1 data block */
	if (be32_to_cpu(head->tzh_timecnt) > 0) {
		printf("\ttransition times:\n");
		tzif_list_transition_times(head, version, data_block);
	}

	/* the fount V1 of tzdb-2023c has only one '\0' defined in
	 * time zone designation region.
	 */
	if (be32_to_cpu(head->tzh_charcnt) > 1) {
		printf("\tlocal time type records:\n");
		tzif_list_local_time_type_records(head, version, data_block);
	}

	return 0;
}

static int tzfile_list(void *private_data, int fd, int argc, char **argv)
{
	struct tzfile_editor_private_data *p = private_data;
	uint8_t *db1 = NULL, *db2 = NULL;
	size_t blksz;

	blksz = tzif_datablock_total_size(&p->v1_head, 1);
	db1 = calloc(1, blksz);
	if (!db1) {
		fprintf(stderr, "Error: Alloc %zu bytes for v1 failed\n",
			blksz);
		return -1;
	}

	lseek(fd, sizeof(p->v1_head), SEEK_SET);
	read(fd, db1, blksz);
	tzif_list(&p->v1_head, 1, db1);

	if (p->version > 1) {
		lseek(fd, sizeof(p->v1_head) + blksz + sizeof(p->hv_head), SEEK_SET);

		blksz = tzif_datablock_total_size(&p->hv_head, p->version);
		db2 = calloc(1, blksz);
		if (!db2) {
			fprintf(stderr, "Error: Alloc %zu bytes for v%d failed\n",
				blksz, p->version);
			free(db1);
			return -1;
		}
		read(fd, db2, blksz);

		tzif_list(&p->hv_head, p->version, db2);

		printf("Footer:\n\t%.*s\n",
			(int)strlen(p->footer) - 2, p->footer + 1);
	}

	return 0;
}

static struct imgeditor tzfile_editor = {
	.name			= "tzfile",
	.descriptor		= "timezone file editor",
	.flags			= IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= sizeof(struct tzhead),
	.private_data_size	= sizeof(struct tzfile_editor_private_data),
	.detect			= tzfile_detect,
	.list			= tzfile_list,
};
REGISTER_IMGEDITOR(tzfile_editor);
