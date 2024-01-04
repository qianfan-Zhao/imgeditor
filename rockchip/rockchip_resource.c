/*
 * rockchip resource_tool(RSCE) image editor
 * qianfan Zhao 2022-11-05
 */
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "imgeditor.h"
#include "gmssl/sha1.h"

#define RSCE_MAGIC			0x45435352 /* RSCE */
#define RSCE_BLKSIZE			512
#define RSCE_MAX_INDEX_TABLE_ENTRY	16

static uint32_t rsce_bytes_to_blksz(size_t bytes)
{
	return (bytes + RSCE_BLKSIZE - 1) / RSCE_BLKSIZE;
}

static uint32_t rsce_blksz_to_bytes(uint32_t blks)
{
	return blks * RSCE_BLKSIZE;
}

/*
 * all of the size defined in rsce_header is number of blocks, not bytes.
 */
struct rsce_header {
	uint32_t			magic;
	uint16_t			rsce_header_version;
	uint16_t			index_table_version;
	uint8_t				this_struct_size;
	uint8_t				index_table_offset;
	uint8_t				index_table_entry_size;
	uint32_t			index_table_entry_num;
};

#define INDEX_TABLE_ENTRY_MAGIC		0x52544e45 /* ENTR */

/*
 * content_offset is number of blocks and content_size is bytes.
 */
struct index_table_entry {
	uint32_t			magic;
	char				path[220];
	uint8_t				hash[32];
	uint32_t			hash_size;
	uint32_t			content_offset;
	uint32_t			contect_size;
};

struct rsce_editor_private_data {
	struct rsce_header		head;
	struct index_table_entry	tables[RSCE_MAX_INDEX_TABLE_ENTRY];
};

static void rsce_update_sha1sum(uint8_t *buf, size_t sz_buster, void *p)
{
	SHA1_CTX *ctx = p;

	sha1_update(ctx, buf, sz_buster);
}

static int rsce_detect(void *private_data, int force_type, int fd)
{
	struct rsce_editor_private_data *p = private_data;
	uint8_t sha1sum[SHA1_DIGEST_SIZE];
	SHA1_CTX ctx;
	int ret;

	ret = read(fd, &p->head, sizeof(p->head));
	if (ret < 0)
		return ret;

	if (p->head.magic != RSCE_MAGIC) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

	if (p->head.this_struct_size != rsce_bytes_to_blksz(sizeof(p->head))) {
		fprintf_if_force_type("Error: structure size doesn't match\n");
		return -1;
	}

	if (p->head.index_table_entry_size != rsce_bytes_to_blksz(sizeof(p->tables[0]))) {
		fprintf_if_force_type("Error: index table size doesn't match\n");
		return -1;
	}

	if (p->head.index_table_entry_num > RSCE_MAX_INDEX_TABLE_ENTRY) {
		fprintf_if_force_type("Error: we only support %d index table entry\n",
				      RSCE_MAX_INDEX_TABLE_ENTRY);
		return -1;
	}

	/* read the whole index table entrys */
	lseek(fd, rsce_blksz_to_bytes(p->head.index_table_offset), SEEK_SET);
	read(fd, p->tables,
		rsce_blksz_to_bytes(p->head.index_table_entry_size)
		* p->head.index_table_entry_num);

	for (uint32_t i = 0; i < p->head.index_table_entry_num; i++) {
		struct index_table_entry *entry = &p->tables[i];

		if (entry->magic != INDEX_TABLE_ENTRY_MAGIC) {
			fprintf_if_force_type("Error: index table entry %d has bad magic\n", i);
			return -1;
		}

		if (entry->hash_size > sizeof(entry->hash)) {
			fprintf_if_force_type("Error: hash size overflow\n");
			return -1;
		}

		sha1_init(&ctx);
		dd(fd, -1, rsce_blksz_to_bytes(entry->content_offset), 0,
		   entry->contect_size, rsce_update_sha1sum, &ctx);
		sha1_finish(&ctx, sha1sum);

		if (memcmp(sha1sum, entry->hash, sizeof(sha1sum))) {
			fprintf_if_force_type("Error: sha1sum doesn't match\n");
			return -1;
		}
	}

	return 0;
}

static int rsce_list(void *private_data, int fd, int argc, char **argv)
{
	struct rsce_editor_private_data *p = private_data;

	printf("header version: %04x-%04x\n",
		p->head.rsce_header_version, p->head.index_table_version);

	for (uint32_t i = 0; i < p->head.index_table_entry_num; i++) {
		struct index_table_entry *entry = &p->tables[i];

		printf("%02d: %s\n", i + 1, entry->path);
		printf("    hash_size: %d\n", entry->hash_size);
		printf("    hash: ");
		for (uint32_t i = 0; i < entry->hash_size; i++)
			printf("%02x", entry->hash[i]);
		printf("\n");
		printf("    offset: 0x%08x blks, 0x%08x bytes\n",
			entry->content_offset, rsce_blksz_to_bytes(entry->content_offset));
		printf("    size: %d\n", entry->contect_size);
	}

	return 0;
}

static int rsce_unpack(void *private_data, int fd, const char *outdir, int argc, char **argv)
{
	struct rsce_editor_private_data *p = private_data;

	for (uint32_t i = 0; i < p->head.index_table_entry_num; i++) {
		struct index_table_entry *entry = &p->tables[i];
		uint32_t offset = rsce_blksz_to_bytes(entry->content_offset);
		char tmpbuf[1024];
		int fd_child;
		size_t n;

		snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s", outdir, entry->path);
		fd_child = fileopen(tmpbuf, O_WRONLY | O_CREAT, 0664);
		if (fd_child < 0)
			return fd_child;

		n = dd(fd, fd_child, offset, 0, entry->contect_size, NULL, NULL);
		close(fd_child);

		if (n != entry->contect_size) {
			fprintf(stderr, "Error: save %s failed\n", tmpbuf);
			return -1;
		}
	}

	return 0;
}

static int rsce_pack(void *private_data, const char *dir, int fd_outimg, int argc, char **argv)
{
	fprintf(stderr, "rsce_pack is not impl now\n");
	return -1;
}

static struct imgeditor rsce_editor = {
	.name			= "rkresource",
	.descriptor		= "rockchip resource_tool(RSCE) image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	/* at least one index_table_entry */
	.header_size		= sizeof(struct rsce_header) + sizeof(struct index_table_entry),
	.private_data_size	= sizeof(struct rsce_editor_private_data),
	.detect			= rsce_detect,
	.list			= rsce_list,
	.unpack			= rsce_unpack,
	.pack			= rsce_pack,
};

REGISTER_IMGEDITOR(rsce_editor);
