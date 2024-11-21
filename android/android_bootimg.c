/*
 * andriod boot.img editor
 * qianfan Zhao 2022-11-07
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "hash_compatible.h"
#include "imgeditor.h"
#include "json_helper.h"
#include "structure.h"
#include "string_helper.h"

typedef uint32_t u32;
typedef uint64_t u64;

#define ANDR_BOOT_MAGIC "ANDROID!"
#define ANDR_BOOT_MAGIC_SIZE 8
#define ANDR_BOOT_NAME_SIZE 16
#define ANDR_BOOT_ARGS_SIZE 512
#define ANDR_BOOT_EXTRA_ARGS_SIZE 1024

/* The bootloader expects the structure of andr_img_hdr with header
 * version 0 to be as follows: */
struct andr_img_hdr {
	/* Must be ANDR_BOOT_MAGIC. */
	char magic[ANDR_BOOT_MAGIC_SIZE];

	u32 kernel_size; /* size in bytes */
	u32 kernel_addr; /* physical load addr */

	u32 ramdisk_size; /* size in bytes */
	u32 ramdisk_addr; /* physical load addr */

	u32 second_size; /* size in bytes */
	u32 second_addr; /* physical load addr */

	u32 tags_addr; /* physical addr for kernel tags */
	u32 page_size; /* flash page size we assume */

	/* Version of the boot image header. */
	u32 header_version;

	/* Operating system version and security patch level.
	* For version "A.B.C" and patch level "Y-M-D":
	*   (7 bits for each of A, B, C; 7 bits for (Y-2000), 4 bits for M)
	*   os_version = A[31:25] B[24:18] C[17:11] (Y-2000)[10:4] M[3:0] */
	u32 os_version;

	char name[ANDR_BOOT_NAME_SIZE]; /* asciiz product name */

	char cmdline[ANDR_BOOT_ARGS_SIZE];

	u32 id[8]; /* timestamp / checksum / sha1 / etc */

	/* Supplemental command line data; kept here to maintain
	* binary compatibility with older versions of mkbootimg. */
	char extra_cmdline[ANDR_BOOT_EXTRA_ARGS_SIZE];

	/* Fields in boot_img_hdr_v1 and newer. */
	u32 recovery_dtbo_size;   /* size in bytes for recovery DTBO/ACPIO image */
	u64 recovery_dtbo_offset; /* offset to recovery dtbo/acpio in boot image */
	u32 header_size;

	/* Fields in boot_img_hdr_v2 and newer. */
	u32 dtb_size; /* size in bytes for DTB image */
	u64 dtb_addr; /* physical load address for DTB image */
} __attribute__((packed));

static const struct structure_item structure_andr_hdr[] = {
	STRUCTURE_ITEM(struct andr_img_hdr, kernel_addr,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct andr_img_hdr, kernel_size,		structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct andr_img_hdr, ramdisk_addr,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct andr_img_hdr, ramdisk_size,		structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct andr_img_hdr, second_addr,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct andr_img_hdr, second_size,		structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct andr_img_hdr, dtb_addr,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct andr_img_hdr, dtb_size,			structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct andr_img_hdr, recovery_dtbo_offset,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct andr_img_hdr, recovery_dtbo_size,		structure_item_print_xunsigned,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM(struct andr_img_hdr, tags_addr,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct andr_img_hdr, page_size,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct andr_img_hdr, header_version,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct andr_img_hdr, header_size,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct andr_img_hdr, os_version,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct andr_img_hdr, name,			structure_item_print_string),
	STRUCTURE_ITEM(struct andr_img_hdr, cmdline,			structure_item_print_string),
	STRUCTURE_ITEM(struct andr_img_hdr, extra_cmdline,		structure_item_print_string),
	STRUCTURE_ITEM(struct andr_img_hdr, id,				structure_item_print_x8_array,
			.flags = STRUCTURE_FLAG_NOT_SAVE),
	STRUCTURE_ITEM_END(),
};

static void fix_andr_hdr_structure(struct structure_item *items,
				   uint32_t hdr_version)
{
	uint32_t drop_offset = 0;

	switch (hdr_version) {
	case 0:
		drop_offset = offsetof(struct andr_img_hdr, recovery_dtbo_size);
		break;
	case 1:
		drop_offset = offsetof(struct andr_img_hdr, dtb_size);
		break;
	default:
		return;
	}

	for (struct structure_item *st = items; st->name; st++) {
		if (st->offset < drop_offset)
			continue;

		st->flags |= STRUCTURE_FLAG_NOT_SAVE;
	}
}

struct abootimg_editor_private_data {
	struct andr_img_hdr		head;

	size_t				total_size;
};

static void abootimg_update_sha1sum(uint8_t *buf, size_t sz_buster, void *p)
{
	hash_context_t *ctx = p;

	hash_context_update(ctx, buf, sz_buster);
}

static int abootimg_detect(void *private_data, int force_type, int fd)
{
	struct abootimg_editor_private_data *p = private_data;
	hash_context_t hash;
	uint8_t sha1sum[128];
	size_t hashsz;
	uint32_t offset;
	int ret;

	ret = read(fd, &p->head, sizeof(p->head));
	if (ret < 0)
		return ret;

	if (memcmp(p->head.magic, ANDR_BOOT_MAGIC, ANDR_BOOT_MAGIC_SIZE)) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

	if (p->head.header_version > 2) {
		fprintf_if_force_type("Error: bad header_version %d\n",
				      p->head.header_version);
		return -1;
	}

	switch (p->head.page_size) {
	case 2048:
	case 4096:
	case 8192:
	case 16384:
		break;
	default:
		fprintf_if_force_type("Error: bad page_size number %d\n",
				      p->head.page_size);
		return -1;
	}

	/* version 0 including 3 files, kernel, ramdisk, second
	 * version 1 add recovery_dtb0
	 * version 2 add dtb
	 */
	size_t n_abootimg_files = 3 + p->head.header_version;
	uint32_t abootimg_file_sizes[] = {
		p->head.kernel_size,
		p->head.ramdisk_size,
		p->head.second_size,
		p->head.recovery_dtbo_size,
		p->head.dtb_size,
	};

	hash_context_init(&hash, HASH_TYPE_SHA1);
	offset = p->head.page_size;
	for (size_t i = 0; i < n_abootimg_files; i++) {
		uint32_t sz = abootimg_file_sizes[i];

		if (sz > 0)
			dd(fd, -1, offset, 0, sz, abootimg_update_sha1sum, &hash);

		/* update sha1sum string even if the filesize is zero */
		abootimg_update_sha1sum((uint8_t *)&sz, sizeof(sz), &hash);
		offset += aligned_length(sz, p->head.page_size);
	}

	hashsz = hash_context_finish(&hash, sha1sum, sizeof(sha1sum));
	if (memcmp(sha1sum, p->head.id, hashsz)) {
		fprintf_if_force_type("Error: sha1sum doesn't match\n");
		return -1;
	}

	p->total_size = offset;
	return 0;
}

static int abootimg_summary(void *private_data, int fd, char *buf, size_t bufsz)
{
	struct abootimg_editor_private_data *p = private_data;

	snprintf_continue(&buf, &bufsz, "ID: ");
	for (size_t i = 0; i < sizeof(p->head.id) / sizeof(p->head.id[0]); i++)
		snprintf_continue(&buf, &bufsz, "%08x", p->head.id[i]);

	return 0;
}

static int64_t abootimg_total_size(void *private_data, int fd)
{
	struct abootimg_editor_private_data *p = private_data;

	return p->total_size;
}

static int abootimg_list(void *private_data, int fd, int argc, char **argv)
{
	struct abootimg_editor_private_data *p = private_data;
	struct andr_img_hdr *hdr = &p->head;
	uint8_t *p_id = (uint8_t *)hdr->id;

	if (get_verbose_level() > 1) {
		structure_print("%-30s", hdr, structure_andr_hdr);
		putchar('\n');
	}

	printf("kernel:           0x%08x 0x%08x\n", hdr->kernel_addr, hdr->kernel_size);
	printf("ramdisk:          0x%08x 0x%08x\n", hdr->ramdisk_addr, hdr->ramdisk_size);
	printf("second:           0x%08x 0x%08x\n", hdr->second_addr, hdr->second_size);
	printf("tags:             0x%08x\n", hdr->tags_addr);
	printf("pagesize:         %d\n", hdr->page_size);
	printf("header_version:   %d\n", hdr->header_version);
	printf("os_version:       0x%08x\n", hdr->os_version);
	printf("name:             %s\n", hdr->name);
	printf("cmdline:          %s\n", hdr->cmdline);
	printf("extra_cmdline:    %s\n", hdr->extra_cmdline);
	printf("id:               ");
	for (size_t i = 0; i < sizeof(hdr->id); i++)
		printf("%02x", p_id[i]);
	printf("\n");

	if (hdr->header_version > 0) {
		printf("recovery_dtbo:    0x%08" PRIx64  " 0x%08x\n",
			hdr->recovery_dtbo_offset, hdr->recovery_dtbo_size);
		printf("header_size:      %d\n", hdr->header_size);
	}

	if (hdr->header_version > 1) {
		printf("dtb:              0x%08" PRIx64 " 0x%08x\n",
			hdr->dtb_addr, hdr->dtb_size);
	}

	return 0;
}

static int abootimg_unpack_save_file(int fdimg, const char *outdir, const char *name,
				     uint64_t img_offset, uint32_t sz)
{
	char filename[1024];
	int fd_child;
	size_t n;

	if (sz != 0) {
		snprintf(filename, sizeof(filename), "%s/%s", outdir, name);
		fd_child = fileopen(filename, O_WRONLY | O_TRUNC | O_CREAT, 0664);
		if (fd_child < 0)
			return fd_child;

		n = dd(fdimg, fd_child, img_offset, 0, sz, NULL, NULL);
		close(fd_child);

		if (n != sz) {
			fprintf(stderr, "Error: write %s failed\n", filename);
			return -1;
		}
	}

	return 0;
}

#define ABOOTIMG_KERNEL			"kernel.bin"
#define ABOOTIMG_RAMDISK		"ramdisk.bin"
#define ABOOTIMG_SECOND			"second.bin"
#define ABOOTIMG_DTB			"dtb.bin"
#define ABOOTIMG_RECOVERY_DTBO		"recovery_dtbo.bin"

struct abootimg_file {
	uint32_t			min_version;

	const char			*name;
	size_t				hdr_size_offset;
};

/* the order is important */
static const struct abootimg_file abootimg_files[] = {
	{ 0, ABOOTIMG_KERNEL,		offsetof(struct andr_img_hdr, kernel_size)		},
	{ 0, ABOOTIMG_RAMDISK,		offsetof(struct andr_img_hdr, ramdisk_size)		},
	{ 0, ABOOTIMG_SECOND,		offsetof(struct andr_img_hdr, second_size)		},
	{ 1, ABOOTIMG_RECOVERY_DTBO,	offsetof(struct andr_img_hdr, recovery_dtbo_size)	},
	{ 2, ABOOTIMG_DTB,		offsetof(struct andr_img_hdr, dtb_size)			},
	{ 0, NULL,			0							},
};

static int abootimg_unpack(void *private_data, int fd, const char *outdir, int argc, char **argv)
{
	uint8_t structure_andr_hdr_fixed[sizeof(structure_andr_hdr)];
	struct abootimg_editor_private_data *p = private_data;
	struct andr_img_hdr *hdr = &p->head;
	uint64_t offset = hdr->page_size;
	const char *filelists[16] = { 0 };
	char filename[1024];
	cJSON *root, *json_files;
	size_t count = 0;
	int ret = 0;

	root = cJSON_CreateObject();
	if (!root) {
		fprintf(stderr, "Error: create json failed\n");
		return -1;
	}

	memcpy(structure_andr_hdr_fixed, &structure_andr_hdr,
		sizeof(structure_andr_hdr));

	fix_andr_hdr_structure(
		(struct structure_item *)structure_andr_hdr_fixed,
		le32_to_cpu(p->head.header_version));

	ret = structure_save_json(root, hdr,
		(struct structure_item *)structure_andr_hdr_fixed);
	if (ret < 0) {
		ret = -1;
		goto done;
	}

	for (const struct abootimg_file *file = &abootimg_files[0];
					file->name; file++) {
		__le32 *p_hdr_size = (void *)hdr + file->hdr_size_offset;
		uint32_t filesz = le32_to_cpu(*p_hdr_size);

		if (file->min_version > le32_to_cpu(hdr->header_version))
			break;
		if (filesz == 0)
			continue;

		ret = abootimg_unpack_save_file(fd, outdir, file->name,
						offset, filesz);
		if (ret < 0)
			goto done;

		filelists[count++] = file->name;
		offset += aligned_length(filesz, hdr->page_size);
	}

	json_files = cJSON_CreateStringArray(filelists, count);
	if (!json_files) {
		ret = -1;
		goto done;
	}

	cJSON_AddItemToObject(root, "files", json_files);
	snprintf(filename, sizeof(filename), "%s/abootimg.json", outdir);
	json_saveto_file(root, filename);

done:
	cJSON_Delete(root);
	return ret;
}

static int json_string_array_search(cJSON *json_files, const char *s)
{
	cJSON *json;

	cJSON_ArrayForEach(json, json_files) {
		if (strcmp(s, cJSON_GetStringValue(json)) == 0)
			return 0; /* found it */
	}

	return -1;
}

static int abootimg_do_pack(struct andr_img_hdr *hdr, cJSON *json_files,
			    const char *dir, int fd_outimg)
{
	const struct abootimg_file *afile = &abootimg_files[0];
	size_t padsz = 0, offset = le32_to_cpu(hdr->page_size);
	hash_context_t hash;
	uint8_t sha1sum[128];
	size_t hashsz;

	hash_context_init(&hash, HASH_TYPE_SHA1);

	for (; afile->name; afile++) {
		__le32 *p_hdr_size = (void *)hdr + afile->hdr_size_offset;
		char filename[1024];
		int length;
		int fd;

		if (afile->min_version > le32_to_cpu(hdr->header_version))
			break;

		if (json_string_array_search(json_files, afile->name) < 0) {
			*p_hdr_size = cpu_to_le32(0);
			abootimg_update_sha1sum((uint8_t *)p_hdr_size,
						sizeof(*p_hdr_size), &hash);
			continue;
		}

		snprintf(filename, sizeof(filename), "%s/%s", dir, afile->name);
		fd = fileopen(filename, O_RDONLY, 0);
		if (fd < 0)
			return fd;

		length = (int)filelength(fd);
		if (length < 0)
			return length;

		*p_hdr_size = cpu_to_le32(length);
		dd(fd, fd_outimg, 0, offset, length, abootimg_update_sha1sum, &hash);
		abootimg_update_sha1sum((uint8_t *)p_hdr_size,
					sizeof(*p_hdr_size), &hash);

		size_t tmp = offset + length;
		offset += aligned_length(length, le32_to_cpu(hdr->page_size));
		padsz = offset - tmp;

		close(fd);
	}

	/* padding it */
	if (padsz != 0) {
		uint8_t zero = 0;

		lseek(fd_outimg, offset - sizeof(zero), SEEK_SET);
		write(fd_outimg, &zero, sizeof(zero));
	}

	hashsz = hash_context_finish(&hash, sha1sum, sizeof(sha1sum));
	memcpy(hdr->id, sha1sum, hashsz);
	memcpy(hdr->magic, ANDR_BOOT_MAGIC, sizeof(hdr->magic));

	lseek(fd_outimg, 0, SEEK_SET);
	write(fd_outimg, hdr, sizeof(*hdr));

	return 0;
}

static void andr_hdr_erase_from(struct andr_img_hdr *hdr, size_t offset)
{
	if (offset < sizeof(*hdr))
		memset((void *)hdr + offset, 0, sizeof(*hdr) - offset);
}

static int abootimg_pack(void *private_data, const char *dir, int fd_outimg,
			 int argc, char **argv)
{
	uint8_t structure_andr_hdr_fixed[sizeof(structure_andr_hdr)];
	struct abootimg_editor_private_data *p = private_data;
	cJSON *root, *json_files;
	char filename[1024];
	int ret;

	snprintf(filename, sizeof(filename), "%s/abootimg.json", dir);
	root = json_from_file(filename);
	if (!root)
		return -1;

	json_files = cJSON_GetObjectItem(root, "files");
	if (!json_files || !cJSON_IsArray(json_files)) {
		fprintf(stderr, "Error: [files] is not found\n");
		ret = -1;
		goto done;
	}

	memcpy(structure_andr_hdr_fixed, &structure_andr_hdr,
		sizeof(structure_andr_hdr));

	fix_andr_hdr_structure(
		(struct structure_item *)structure_andr_hdr_fixed,
		le32_to_cpu(p->head.header_version));

	ret = structure_load_json(root, &p->head,
		(struct structure_item *)structure_andr_hdr_fixed);
	if (ret < 0)
		goto done;

	switch (le32_to_cpu(p->head.page_size)) {
	case 2048:
	case 4096:
	case 8192:
	case 16384:
		break;
	default:
		fprintf(stderr, "Error: invalid page size %d\n",
			le32_to_cpu(p->head.page_size));
		ret = -1;
		goto done;
	}

	switch (le32_to_cpu(p->head.header_version)) {
	case 0:
		andr_hdr_erase_from(&p->head,
			offsetof(struct andr_img_hdr, recovery_dtbo_size));
		break;
	case 1:
		andr_hdr_erase_from(&p->head,
			offsetof(struct andr_img_hdr, dtb_size));
		break;
	case 2:
		break;
	default:
		fprintf(stderr, "Error: invalid header version %d\n",
			le32_to_cpu(p->head.header_version));
		ret = -1;
		goto done;
	}

	if (!json_string_array_search(json_files, ABOOTIMG_RECOVERY_DTBO)) {
		if (le32_to_cpu(p->head.header_version) < 1) {
			fprintf(stderr, "Error: recovery dtbo need v1 "
				"or later\n");
			ret = -1;
			goto done;
		}
	} else if (!json_string_array_search(json_files, ABOOTIMG_DTB)) {
		if (le32_to_cpu(p->head.header_version) < 2) {
			fprintf(stderr, "Error: dtb need v2 or later\n");
			ret = -1;
			goto done;
		}
	}

	ret = abootimg_do_pack(&p->head, json_files, dir, fd_outimg);

done:
	cJSON_Delete(root);
	return ret;
}

static struct imgeditor abootimg_editor = {
	.name			= "abootimg",
	.descriptor		= "android boot.img editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct andr_img_hdr),
	.private_data_size	= sizeof(struct abootimg_editor_private_data),
	.detect			= abootimg_detect,
	.summary		= abootimg_summary,
	.total_size		= abootimg_total_size,
	.list			= abootimg_list,
	.unpack			= abootimg_unpack,
	.pack			= abootimg_pack,

	.search_magic		= {
		.magic		= ANDR_BOOT_MAGIC,
		.magic_sz	= sizeof(ANDR_BOOT_MAGIC) - 1,
		.magic_offset	= offsetof(struct andr_img_hdr, magic),
	}
};
REGISTER_IMGEDITOR(abootimg_editor);
