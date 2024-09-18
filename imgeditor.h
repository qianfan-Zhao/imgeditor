/*
 * imgeditor common include file
 * qianfan Zhao
 */
#ifndef IMAGE_EDITOR_H
#define IMAGE_EDITOR_H

#include <stdio.h>
#include <stdint.h>
#include <endian.h>
#include <unistd.h>
#include <linux/types.h> /* import __le16, __le32... */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "list_head.h"

int virtual_file_open(const char *filename, int flags, mode_t t, off64_t offset);
int virtual_file_dup(int ref_fd, off64_t offset);
int virtual_file_close(int fd);

int get_verbose_level(void);

#define SIZE_KB(x)				((x) << 10)
#define SIZE_MB(x)				((x) << 20)
#define SIZE_GB(x)				((x) << 30)
#define SIZEMASK_KB				(SIZE_KB(1) - 1)
#define SIZEMASK_MB				(SIZE_MB(1) - 1)
#define SIZEMASK_GB				(SIZE_GB(1) - 1)

const char *smart_format_size(uint64_t sz, char *buf, size_t bufsz);

#define le16_to_cpu(x)				le16toh(x)
#define be16_to_cpu(x)				be16toh(x)
#define le32_to_cpu(x)				le32toh(x)
#define be32_to_cpu(x)				be32toh(x)
#define le64_to_cpu(x)				le64toh(x)
#define be64_to_cpu(x)				be64toh(x)
#define cpu_to_le16(x)				htole16(x)
#define cpu_to_le32(x)				htole32(x)
#define cpu_to_le64(x)				htole64(x)
#define cpu_to_be16(x)				htobe16(x)
#define cpu_to_be32(x)				htobe32(x)
#define cpu_to_be64(x)				htobe64(x)

#define fprintf_if_force_type(fmt, ...) 	do {	\
	if (force_type) {				\
		fprintf(stderr, fmt, ##__VA_ARGS__);	\
	}						\
} while (0)

int fileopen(const char *file, int flags, mode_t mode);
int64_t filelength(int fd);
off64_t filestart(int fd);
off64_t fileseek(int fd, off64_t offset);
/* Return zero on successful */
int fileread(int fd, void *buf, size_t sz);

void hexdump(const void *buf, size_t bufsz, unsigned long baseaddr);

/* SINGLE_BIN: the firmware is generated by padding other control bytes.
 * MULTI_BIN:  the firmware is merged by multi binary files.
 */
#define IMGEDITOR_FLAG_SINGLE_BIN			(0 << 0)
#define IMGEDITOR_FLAG_CONTAIN_MULTI_BIN		(1 << 0)

#define IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST		(1 << 1)

struct imgmagic {
	const void		*magic;
	size_t			magic_sz;
	size_t			magic_offset;
	int64_t			next_search_offset;
};

struct imgeditor {
	const char		*name;
	const char		*descriptor;
	unsigned int		flags;
	size_t			header_size;
	size_t			private_data_size;
	void			*private_data;
	struct list_head	head;

	/* p: point to private_data alloced by the core level. */
	int			(*init)(void *p);
	int			(*detect)(void *p, int force_type, int fd);
	int			(*summary)(void *p, int fd, char *buf, size_t bufsz);
	int64_t			(*total_size)(void *p, int fd);
	int			(*list)(void *p, int fd, int argc, char **argv);
	int			(*pack)(void *p, const char *dir, int fd_outimg, int argc, char **argv);
	int			(*unpack)(void *p, int fd, const char *outdir, int argc, char **argv);
	int			(*main)(void *p, int argc, char **argv);
	void			(*exit)(void *p);

	struct imgmagic		search_magic;
};

#define IMGEDITOR_PLUGIN_STRUCT_VERSION	0x103

void register_imgeditor(struct imgeditor *editor);

#if BUILD_IMGEDITOR_CORE > 0
#define REGISTER_IMGEDITOR(e)						\
static void __attribute__((constructor)) register_imgeditor_##e(void)	\
{									\
	register_imgeditor(&e);						\
}
#endif

struct imgeditor_plugin {
	unsigned int		plugin_version;
	const char		*name;
	const char		*version;
	struct imgeditor	**editors;

	/* runtime data */
	struct list_head	head;
	void			*dl;
	char			path[512];
};

/* imgeditor core want share global data to all plugins. */
int imgeditor_plugin_setup_gd(void);
void imgeditor_free_gd(void);

#define IMGEDITOR_PLUGIN_INIT(name)					\
static void __attribute__((constructor))				\
		imgeditor_plugin_setup_gd_##name(void)			\
{									\
	imgeditor_plugin_setup_gd();					\
}									\
static void __attribute__((destructor))					\
		imgeditor_plugin_free_gd_##name(void)			\
{									\
	imgeditor_free_gd();						\
}									\

#define IMGEDITOR_PLUGIN1(a)			{ &a, NULL }
#define IMGEDITOR_PLUGIN2(a, b)			{ &a, &b, NULL }
#define IMGEDITOR_PLUGIN3(a, b, c)		{ &a, &b, &c, NULL }

#define _IMGEDITOR_CAT(a, b)			a##b
#define IMGEDITOR_CAT(a, b)			_IMGEDITOR_CAT(a, b)

#define _IMGEDITOR_NUM_PLUGIN(_0, _1, _2, _3, COUNT, ...) COUNT
#define IMGEDITOR_NUM_PLUGIN(...)					\
	_IMGEDITOR_NUM_PLUGIN(__VA_ARGS__, 4, 3, 2, 1)

#define IMGEDITOR_PLUGIN(...)						\
	IMGEDITOR_CAT(IMGEDITOR_PLUGIN, IMGEDITOR_NUM_PLUGIN(__VA_ARGS__))(__VA_ARGS__)

#ifndef IMGEDITOR_PLUGIN_VERSION
#define IMGEDITOR_PLUGIN_VERSION	__TIME__ " " __DATE__
#endif

#define REGISTER_IMGEDITOR_PLUGIN(a, ...)				\
	IMGEDITOR_PLUGIN_INIT(a)					\
	struct imgeditor_plugin imgeditor_plugin = {			\
		.plugin_version	= IMGEDITOR_PLUGIN_STRUCT_VERSION,	\
		.name = #a,						\
		.version = IMGEDITOR_PLUGIN_VERSION,			\
		.editors = (struct imgeditor *[])			\
				IMGEDITOR_PLUGIN(a, ##__VA_ARGS__),	\
	}

#define aligned_length(len, align)	(((len) + (align) - 1) / (align) * (align))

size_t dd(int fdsrc, int fddst, off_t offt_src, off_t offt_dst, size_t sz,
	  void (*bufscan)(uint8_t *buf, size_t sz_buster, void *p),
	  void *private_data);

uint64_t dd64(int fdsrc, int fddst, off64_t offt_src, off64_t offt_dst, uint64_t sz,
	      void (*bufscan)(uint8_t *buf, size_t sz_buster, void *p),
	      void *private_data);

void hexdump(const void *buf, size_t sz, unsigned long baseaddr);
int xstring(const char *str, const char **endp, uint8_t *buf, size_t len);

void rc4_encode(unsigned char *buf, int len, unsigned char key[16]);
uint32_t crc32 (uint32_t crc, const uint8_t *p, size_t len);

struct ksmap {
	unsigned long	key;
	const char	*str;
};

const struct ksmap *ksmap_find(const struct ksmap *maps, unsigned long key);

/*
 * `imgeditor -s` will search all readable images, we can print the partition
 * name if we known the partitions.
 */
struct disk_partition {
	char		name[32];	/* maybe null such as dos */
	uint64_t	start_addr;
	uint64_t	end_addr;
};

enum disk_partition_type {
	DISK_PARTITION_GPT,
	DISK_PARTITION_MBR,
	DISK_PARTITION_OTHERS,
};

static inline const char *disk_partition_type_name(enum disk_partition_type t)
{
	switch (t) {
	case DISK_PARTITION_GPT:
		return "GPT";
	case DISK_PARTITION_MBR:
		return "MBR";
	default:
		break;
	}

	return "???";
}

#define DISK_PARTITIONS_SCORE_WEAK	10
#define DISK_PARTITIONS_SCORE_NORMAL	20
#define DISK_PARTITIONS_SCORE_GOOD	80
#define DISK_PARTITIONS_SCORE_PERFECT	100

struct disk_partitions {
	struct list_head		head;
	enum disk_partition_type	type;
	int				score;	/* the bigger the better */
	size_t				n_parts;
	struct disk_partition		parts[0];
};

struct disk_partitions *
alloc_disk_partitions(enum disk_partition_type type, size_t n_parts);
void register_disk_partitions(struct disk_partitions *dp);
void register_weak_disk_partitions(struct disk_partitions *dp);

const struct disk_partition *
find_registed_partition(uint64_t start_addr, enum disk_partition_type *ret_type);

void free_registed_disk_partitions(void);

#endif
