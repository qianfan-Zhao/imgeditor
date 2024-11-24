/*
 * android sparse image editor
 * qianfan Zhao
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "imgeditor.h"
#include "android_sparse.h"

struct sparse_header {
  __le32	magic;		/* 0xed26ff3a */
  __le16	major_version;	/* (0x1) - reject images with higher major versions */
  __le16	minor_version;	/* (0x0) - allow images with higer minor versions */
  __le16	file_hdr_sz;	/* 28 bytes for first revision of the file format */
  __le16	chunk_hdr_sz;	/* 12 bytes for first revision of the file format */
  __le32	blk_sz;		/* block size in bytes, must be a multiple of 4 (4096) */
  __le32	total_blks;	/* total blocks in the non-sparse output image */
  __le32	total_chunks;	/* total chunks in the sparse input image */
  __le32	image_checksum; /* CRC32 checksum of the original data, counting "don't care" */
				/* as 0. Standard 802.3 polynomial, use a Public Domain */
				/* table implementation */
};

#define SPARSE_HEADER_MAGIC	0xed26ff3a

#define CHUNK_TYPE_RAW		0xCAC1
#define CHUNK_TYPE_FILL		0xCAC2
#define CHUNK_TYPE_DONT_CARE	0xCAC3
#define CHUNK_TYPE_CRC32	0xCAC4

struct chunk_header {
  __le16	chunk_type;	/* 0xCAC1 -> raw; 0xCAC2 -> fill; 0xCAC3 -> don't care */
  __le16	reserved1;
  __le32	chunk_sz;	/* in blocks in output image */
  __le32	total_sz;	/* in bytes of chunk input file including chunk header and data */
};

static int android_sparse_chunk_sort(const void *a, const void *b)
{
	const struct android_sparse_chunk *chunka = a, *chunkb = b;

	return chunka->fs_blocknr - chunkb->fs_blocknr;
}

int android_sparse_init(struct android_sparse_input *input)
{
	input->total_chunk_size = 64;
	input->active_count = 0;
	input->error = 0;

	input->chunks = calloc(input->total_chunk_size,
				sizeof(input->chunks[0]));
	if (!input->chunks)
		return -1;

	return 0;
}

static int android_sparse_enlarge(struct android_sparse_input *input)
{
	struct android_sparse_chunk *new_chunks;
	size_t cur_bytes, total_bytes;

	if (input->active_count + 1 < input->total_chunk_size)
		return 0;

	cur_bytes = input->total_chunk_size * sizeof(input->chunks[0]);
	total_bytes = cur_bytes * 2;

	new_chunks = realloc(input->chunks, total_bytes);
	if (!new_chunks) {
		fprintf(stderr, "Error: android sparse try enlarge buffer "
				"to %zu failed\n", input->total_chunk_size * 2);
		return -1;
	}

	memset((uint8_t *)new_chunks + cur_bytes, 0, total_bytes - cur_bytes);
	input->total_chunk_size *= 2;
	input->chunks = new_chunks;
	return 0;
}

static void android_sparse_free(struct android_sparse_input *input)
{
	free(input->chunks);
	input->chunks = NULL;

	input->error = 0;
	input->total_chunk_size = input->active_count = 0;
}

int android_sparse_add_chunk(struct android_sparse_input *input,
			     uint32_t fs_blocknr, uint32_t count)
{
	struct android_sparse_chunk *chunk;
	int ret;

	if (input->error)
		return -1;

	ret = android_sparse_enlarge(input);
	if (ret < 0) {
		android_sparse_free(input);
		input->error++;
		return ret;
	}

	chunk = &input->chunks[input->active_count];
	input->active_count++;

	chunk->fs_blocknr = fs_blocknr;
	chunk->count = count;

	return 0;
}

static int android_sparse_append_dontcare(int fd_target, size_t blks)
{
	struct chunk_header ch = { 0 };

	ch.chunk_type = cpu_to_le32(CHUNK_TYPE_DONT_CARE);
	ch.chunk_sz = cpu_to_le32(blks);
	ch.total_sz = sizeof(ch);
	write(fd_target, &ch, sizeof(ch));

	return 0;
}

int android_sparse_finish(struct android_sparse_input *input, int fd_target,
			  size_t block_size, size_t total_blks,
			  android_sparse_write_t write_cb,
			  void *private_data)
{
	size_t blk = 0, total_chunks = 0;
	struct sparse_header h = { 0 };
	uint64_t offset = 0;
	int ret = 0;

	h.magic = cpu_to_le32(SPARSE_HEADER_MAGIC);
	h.major_version = cpu_to_le32(1);
	h.minor_version = cpu_to_le32(0);
	h.file_hdr_sz = sizeof(h);
	h.chunk_hdr_sz = sizeof(struct chunk_header);
	h.blk_sz = cpu_to_le32(block_size);
	h.total_blks = total_blks;

	if (input->active_count == 0 || !input->chunks)
		return -1;

	qsort(input->chunks, input->active_count, sizeof(input->chunks[0]),
		android_sparse_chunk_sort);

	offset = le32_to_cpu(h.file_hdr_sz);
	for (size_t i = 0; i < input->active_count; i++) {
		struct android_sparse_chunk *chunk = &input->chunks[i];
		struct chunk_header ch = { 0 };

		fileseek(fd_target, offset);

		if (chunk->fs_blocknr + chunk->count > total_blks) {
			fprintf(stderr, "Error: write sparse chunk at #%d:%d "
				"failed. (overrange total blocks)\n",
				chunk->fs_blocknr, chunk->count);
			ret = -1;
			break;
		}

		/* appending DONT_CARE for file gaps */
		if (blk != chunk->fs_blocknr) {
			android_sparse_append_dontcare(fd_target,
						       chunk->fs_blocknr - blk);
			blk = chunk->fs_blocknr;
			offset += sizeof(ch);
			total_chunks++;
		}

		ch.chunk_type = cpu_to_le32(CHUNK_TYPE_RAW);
		ch.chunk_sz = cpu_to_le32(chunk->count);
		ch.total_sz = cpu_to_le32(chunk->count * block_size + sizeof(ch));
		write(fd_target, &ch, sizeof(ch));

		total_chunks++;
		offset += sizeof(ch);

		ret = write_cb(fd_target, chunk, private_data);
		if (ret < 0) {
			fprintf(stderr, "Error: write sparse chunk for fs block"
				" #%d failed\n",
				chunk->fs_blocknr);
			break;
		}

		offset += chunk->count * block_size;
		blk += chunk->count;
	}

	if (ret < 0) {
		android_sparse_free(input);
		return ret;
	}

	if (blk < total_blks) {
		fileseek(fd_target, offset);
		android_sparse_append_dontcare(fd_target, total_blks - blk);
		total_chunks++;
	}

	h.total_chunks = cpu_to_le32(total_chunks);
	fileseek(fd_target, 0);
	write(fd_target, &h, sizeof(h));

	return 0;
}

static int sparse_gentest_write_chunk(int fd,
				      struct android_sparse_chunk *chunk,
				      void *private_data)
{
	uint32_t tmp;

	tmp = cpu_to_le32(chunk->fs_blocknr);
	write(fd, &tmp, sizeof(tmp));

	tmp = cpu_to_le32(chunk->count);
	write(fd, &tmp, sizeof(tmp));

	return 0;
}

static int sparse_gentest(int argc, char **argv)
{
	struct android_sparse_input input;
	int fd, ret;

	if (argc < 2) {
		fprintf(stderr, "Usage: gentest filename\n");
		return -1;
	}

	fd = fileopen(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0664);
	if (fd < 0)
		return fd;

	android_sparse_init(&input);

	android_sparse_add_chunk(&input,  0, 2);
	android_sparse_add_chunk(&input,  2, 1);
	android_sparse_add_chunk(&input,  4, 2);
	android_sparse_add_chunk(&input, 10, 3);

	ret = android_sparse_finish(&input, fd, 4096, SIZE_MB(2) / 4096,
				    sparse_gentest_write_chunk,
				    NULL);
	close(fd);

	return ret;
}

/* Following a Raw or Fill or CRC32 chunk is data.
 *  For a Raw chunk, it's the data in chunk_sz * blk_sz.
 *  For a Fill chunk, it's 4 bytes of the fill data.
 *  For a CRC32 chunk, it's 4 bytes of CRC32
 */

struct sparse_editor_private_data {
	struct sparse_header	head;
	int			fd_zero;
};

static int sparse_detect(void *private_data, int force_type, int fd)
{
	struct sparse_editor_private_data *p = private_data;
	int ret;

	ret = read(fd, &p->head, sizeof(p->head));
	if (ret < 0)
		return ret;

	if (p->head.magic != SPARSE_HEADER_MAGIC) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

	if (p->head.major_version != 1 || p->head.minor_version != 0) {
		fprintf_if_force_type("Error: bad version in head %d.%d\n",
				      p->head.major_version,
				      p->head.minor_version);
		return -1;
	}

	if (p->head.file_hdr_sz != sizeof(p->head)) {
		fprintf_if_force_type("Error: file_hdr_sz doesn't match\n");
		return -1;
	}

	return 0;
}

static int sparse_list(void *private_data, int fd, int argc, char **argv)
{
	struct sparse_editor_private_data *p = private_data;
	struct sparse_header *header = &p->head;
	size_t offset;
	uint32_t fill;

	printf("magic:           0x%x\n", header->magic);
	printf("major_version:   0x%x\n", header->major_version);
	printf("minor_version:   0x%x\n", header->minor_version);
	printf("file_hdr_sz:     %d\n", header->file_hdr_sz);
	printf("chunk_hdr_sz:    %d\n", header->chunk_hdr_sz);
	printf("blk_sz:          %d\n", header->blk_sz);
	printf("total_blks:      %d\n", header->total_blks);
	printf("total_chunks:    %d\n", header->total_chunks);

	offset = header->file_hdr_sz;
	for (size_t i = 0; i < header->total_chunks; i++) {
		struct chunk_header chunk;
		const char *type;
		int n;

		lseek(fd, offset, SEEK_SET);
		n = read(fd, &chunk, sizeof(chunk));
		if (n != (int)sizeof(chunk)) {
			fprintf(stderr, "Error: read chunk #%zu failed\n", i);
			return -1;
		}

		switch (chunk.chunk_type) {
		case CHUNK_TYPE_RAW:
			type = "RAW";
			break;
		case CHUNK_TYPE_DONT_CARE:
			type = "---";
			break;
		case CHUNK_TYPE_FILL:
			type = "FIL";
			break;
		case CHUNK_TYPE_CRC32:
			type = "CRC";
			break;
		default:
			fprintf(stderr, "Error: unknown chunk type in offset 0x%08lx\n",
				(unsigned long)offset);
			return -1;
		}

		printf("chunk #%04zu: %s %9d blocks, size = 0x%08x",
			i, type, chunk.chunk_sz, chunk.total_sz);

		switch (chunk.chunk_type) {
		case CHUNK_TYPE_FILL:
			/* n means the bytes count to fill */
			n = chunk.total_sz - sizeof(chunk);
			if (n != sizeof(fill)) {
				fprintf(stderr, "Error: bad fill size %d\n",
					chunk.total_sz);
				return -1;
			}

			/* continue reading the fill number */
			read(fd, &fill, sizeof(fill));
			printf(" fill with 0x%08x", fill);
			break;
		}

		printf("\n");
		offset += chunk.total_sz;
	}

	return 0;
}

static void chunk_buffer_fill(uint8_t *buffer, size_t sz_buster, void *p)
{
	uint32_t fill_number = *((uint32_t *)p);
	uint32_t *p_fill = (uint32_t *)buffer;

	/* sz_buster is always aligned with 4 bytes due to
	 * sparse_header's block_size is aligned
	 */
	for (size_t i = 0; i < sz_buster / sizeof(fill_number); i++)
		p_fill[i] = fill_number;

}

static int sparse_unpack2fd(void *private_data, int fd, int fdout, int argc, char **argv)
{
	struct sparse_editor_private_data *p = private_data;
	struct sparse_header *header = &p->head;
	size_t offset_in = 0, offset_out = 0;
	int fast = 0;
	uint32_t fill;

	/* options:
	 * --fast: skip all CHUNK_TYPE_DONT_CARE parts
	 */
	for (int i = 0; i < argc; i++) {
		const char *s = argv[i];

		if (!strcmp(s, "--fast"))
			++fast;
	}

	offset_in = header->file_hdr_sz;
	for (size_t i = 0; i < header->total_chunks; i++) {
		uint32_t total_data_size;
		struct chunk_header chunk;
		size_t n;

		lseek(fd, offset_in, SEEK_SET);
		n = read(fd, &chunk, sizeof(chunk));
		if (n != (int)sizeof(chunk)) {
			fprintf(stderr, "Error: read chunk #%zu failed\n", i);
			return -1;
		}

		total_data_size = chunk.total_sz - sizeof(chunk);

		switch (chunk.chunk_type) {
		case CHUNK_TYPE_RAW:
			/* chunk #3869: RAW         1 blocks, size = 0x0000100c */
			n = dd(fd, fdout, offset_in + sizeof(chunk), offset_out,
				total_data_size, NULL, NULL);
			if (n != total_data_size) {
				fprintf(stderr, "Error: write %d bytes failed\n",
					total_data_size);
				return -1;
			}

			offset_out += total_data_size;
			break;
		case CHUNK_TYPE_DONT_CARE:
			/* chunk #3883: ---     32701 blocks, size = 0x0000000c */
			total_data_size = chunk.chunk_sz * header->blk_sz;
			if (fast == 0)
				dd(-1, fdout, 0, offset_out, total_data_size, NULL, NULL);
			offset_out += total_data_size;
			break;
		case CHUNK_TYPE_FILL:
			/* chunk #0017: FIL     32734 blocks, size = 0x00000010 fill with 0x00000000 */
			total_data_size = chunk.chunk_sz * header->blk_sz;
			n = chunk.total_sz - sizeof(chunk);
			if (n != sizeof(fill)) {
				fprintf(stderr, "Error: bad fill size %d\n",
					chunk.total_sz);
				return -1;
			}

			/* reading fill number and fill it */
			read(fd, &fill, sizeof(fill));
			dd(-1, fdout, 0, offset_out, total_data_size,
				chunk_buffer_fill, &fill);
			offset_out += total_data_size;
			break;
		case CHUNK_TYPE_CRC32:
			break;
		default:
			fprintf(stderr, "Error: unknown chunk type in offset 0x%08lx\n",
				(unsigned long)offset_in);
			return -1;
		}

		offset_in += chunk.total_sz;
	}

	return 0;
}

static int sparse_main(void *private_data, int argc, char **argv)
{
	if (!strcmp(argv[0], "gentest"))
		return sparse_gentest(argc, argv);

	return -1;
}

static struct imgeditor sparse_editor = {
	.name			= "asparse",
	.descriptor		= "android sparse image editor",
	.flags			= IMGEDITOR_FLAG_SINGLE_BIN,
	.header_size		= sizeof(struct sparse_header),
	.private_data_size	= sizeof(struct sparse_editor_private_data),
	.detect			= sparse_detect,
	.list			= sparse_list,
	.main			= sparse_main,
	.unpack2fd		= sparse_unpack2fd,
};
REGISTER_IMGEDITOR(sparse_editor);
