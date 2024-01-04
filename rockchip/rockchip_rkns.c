/*
 * ref: rk-uboot: tools/rkcommon.c
 * qianfan Zhao
 */

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "imgeditor.h"
#include "rockchip.h"
#include "structure.h"
#include "gmssl/sha2.h"

#define RKNS_MAGIC			0x534e4b52

#define RKNS_HDR_HASH_NONE		0
#define RKNS_HDR_HASH_SHA256		1
#define RKNS_HDR_HASH_SHA512		2

static uint32_t rkns_hdr_hash_type = RKNS_HDR_HASH_NONE;

static void rkns_print_hash_array(uint32_t hash_type, const uint8_t *hash)
{
	size_t hash_size = 0;

	switch (hash_type) {
	case RKNS_HDR_HASH_SHA256:
		hash_size = 32;
		break;
	case RKNS_HDR_HASH_SHA512:
		hash_size = 64;
		break;
	}

	for (size_t i = 0; i < hash_size; i++)
		printf("%02x", hash[i]);
	printf("\n");
}

struct image_entry {
	/* size_and_offset: [31..16] image size, [15..0] image offset */
	uint16_t			offset;
	uint16_t			size;
	uint32_t			address;
	uint32_t			flag;
	uint32_t			counter;
	uint8_t				reserved[8];
	uint8_t				hash[64];
};

static void structure_item_print_image_entry_hash(const char *print_name_fmt,
						  const char *name,
						  const void *addr, size_t sz)
{
	structure_print_name(print_name_fmt, name);
	rkns_print_hash_array(rkns_hdr_hash_type, addr);
}

static struct structure_item structure_image_entry[] = {
	STRUCTURE_ITEM(struct image_entry, offset,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct image_entry, size,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct image_entry, address,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct image_entry, flag,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct image_entry, counter,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct image_entry, hash,		structure_item_print_image_entry_hash),
	STRUCTURE_ITEM_END(),
};

#define RKNS_HDR_IMAGE_NUMBERS		4

/* from rk35 on boot rom using the new header block
 * This is stored at SD card block 64 (where each block is 512 bytes)
 */
struct rkns_header_v2 {
	uint32_t			magic;
	uint8_t				reserved[4];
	uint32_t			size_and_nimage;
	uint32_t			hash_type;
	uint8_t				reserved1[104];
	struct image_entry		images[RKNS_HDR_IMAGE_NUMBERS];
	uint8_t				reserved2[1064];
	uint8_t				hash[512];
};

static void structure_item_print_rkns_hdr2_hash_type(const char *print_name_fmt,
						     const char *name,
						     const void *addr, size_t sz)
{
	uint32_t hash_type = *((uint32_t *)addr);

	structure_print_name(print_name_fmt, name);
	switch (hash_type) {
	case RKNS_HDR_HASH_NONE:
		printf("none\n");
		break;
	case RKNS_HDR_HASH_SHA256:
		printf("sha256\n");
		break;
	case RKNS_HDR_HASH_SHA512:
		printf("sha512\n");
		break;
	default:
		printf("0x%08x\n", hash_type);
		break;
	}
}

static void structure_item_print_rkns_hdr2_hash(const char *print_name_fmt,
						const char *name,
						const void *addr, size_t sz)
{
	const struct rkns_header_v2 *hdr =
		addr - offsetof(struct rkns_header_v2, hash);

	structure_print_name(print_name_fmt, name);
	rkns_print_hash_array(hdr->hash_type, hdr->hash);
}

static struct structure_item structure_rkns_header_v2[] = {
	STRUCTURE_ITEM(struct rkns_header_v2, size_and_nimage,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct rkns_header_v2, hash_type,	structure_item_print_rkns_hdr2_hash_type),
	STRUCTURE_ITEM(struct rkns_header_v2, hash,		structure_item_print_rkns_hdr2_hash),
	STRUCTURE_ITEM_END(),
};

struct rkns_editor_private_data {
	struct rkns_header_v2		hdr2;
};

static int rkns_detect(void *private_data, int force_type, int fd)
{
	struct rkns_editor_private_data *p = private_data;
	size_t hash_size = 0;
	uint8_t sum[SHA512_DIGEST_SIZE];
	SHA256_CTX sha256ctx;
	SHA512_CTX sha512ctx;
	int ret;

	ret = read(fd, &p->hdr2, sizeof(p->hdr2));
	if (ret < 0)
		return ret;

	if (p->hdr2.magic != RKNS_MAGIC) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

	sha256_init(&sha256ctx);
	sha512_init(&sha512ctx);

	switch (p->hdr2.hash_type) {
	case RKNS_HDR_HASH_SHA256:
		sha256_update(&sha256ctx, (uint8_t *)&p->hdr2,
			      offsetof(struct rkns_header_v2, hash));
		sha256_finish(&sha256ctx, sum);
		hash_size = SHA256_DIGEST_SIZE;
		break;
	case RKNS_HDR_HASH_SHA512:
		sha512_update(&sha512ctx, (uint8_t *)&p->hdr2,
			      offsetof(struct rkns_header_v2, hash));
		sha512_finish(&sha512ctx, sum);
		hash_size = SHA512_DIGEST_SIZE;
		break;
	}

	if (hash_size > 0 && memcmp(sum, p->hdr2.hash, hash_size)) {
		fprintf_if_force_type("Error: hash doesn't match\n");
		return -1;
	}

	rkns_hdr_hash_type = p->hdr2.hash_type;
	return 0;
}

static int rkns_list(void *private_data, int fd, int argc, char **argv)
{
	struct rkns_editor_private_data *p = private_data;

	structure_print("%-20s: ", &p->hdr2, structure_rkns_header_v2);

	for (int i = 0; i < RKNS_HDR_IMAGE_NUMBERS; i++) {
		struct image_entry *entry = &p->hdr2.images[i];

		if (entry->size == 0)
			break;

		printf("\n");
		structure_print("%-20s: ", entry, structure_image_entry);
	}

	return 0;
}

static struct imgeditor rkns_editor = {
	.name			= "rkns",
	.descriptor		= "rockchip rkns image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct rkns_header_v2),
	.private_data_size	= sizeof(struct rkns_editor_private_data),
	.detect			= rkns_detect,
	.list			= rkns_list,
};
REGISTER_IMGEDITOR(rkns_editor);
