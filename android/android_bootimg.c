/*
 * andriod boot.img editor
 * qianfan Zhao 2022-11-07
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "imgeditor.h"
#include "gmssl/sha1.h"

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

/* When a boot header is of version 0, the structure of boot image is as
 * follows:
 *
 * +-----------------+
 * | boot header     | 1 page
 * +-----------------+
 * | kernel          | n pages
 * +-----------------+
 * | ramdisk         | m pages
 * +-----------------+
 * | second stage    | o pages
 * +-----------------+
 *
 * n = (kernel_size + page_size - 1) / page_size
 * m = (ramdisk_size + page_size - 1) / page_size
 * o = (second_size + page_size - 1) / page_size
 *
 * 0. all entities are page_size aligned in flash
 * 1. kernel and ramdisk are required (size != 0)
 * 2. second is optional (second_size == 0 -> no second)
 * 3. load each element (kernel, ramdisk, second) at
 *    the specified physical address (kernel_addr, etc)
 * 4. prepare tags at tag_addr.  kernel_args[] is
 *    appended to the kernel commandline in the tags.
 * 5. r0 = 0, r1 = MACHINE_TYPE, r2 = tags_addr
 * 6. if second_size != 0: jump to second_addr
 *    else: jump to kernel_addr
 */

/* When the boot image header has a version of 2, the structure of the boot
 * image is as follows:
 *
 * +---------------------+
 * | boot header         | 1 page
 * +---------------------+
 * | kernel              | n pages
 * +---------------------+
 * | ramdisk             | m pages
 * +---------------------+
 * | second stage        | o pages
 * +---------------------+
 * | recovery dtbo/acpio | p pages
 * +---------------------+
 * | dtb                 | q pages
 * +---------------------+
 *
 * n = (kernel_size + page_size - 1) / page_size
 * m = (ramdisk_size + page_size - 1) / page_size
 * o = (second_size + page_size - 1) / page_size
 * p = (recovery_dtbo_size + page_size - 1) / page_size
 * q = (dtb_size + page_size - 1) / page_size
 *
 * 0. all entities are page_size aligned in flash
 * 1. kernel, ramdisk and DTB are required (size != 0)
 * 2. recovery_dtbo/recovery_acpio is required for recovery.img in non-A/B
 *    devices(recovery_dtbo_size != 0)
 * 3. second is optional (second_size == 0 -> no second)
 * 4. load each element (kernel, ramdisk, second, dtb) at
 *    the specified physical address (kernel_addr, etc)
 * 5. If booting to recovery mode in a non-A/B device, extract recovery
 *    dtbo/acpio and apply the correct set of overlays on the base device tree
 *    depending on the hardware/product revision.
 * 6. prepare tags at tag_addr.  kernel_args[] is
 *    appended to the kernel commandline in the tags.
 * 7. r0 = 0, r1 = MACHINE_TYPE, r2 = tags_addr
 * 8. if second_size != 0: jump to second_addr
 *    else: jump to kernel_addr
 */

struct abootimg_editor_private_data {
	struct andr_img_hdr		head;
};

static void abootimg_update_sha1sum(uint8_t *buf, size_t sz_buster, void *p)
{
	SHA1_CTX *ctx = p;

	sha1_update(ctx, buf, sz_buster);
}

static int abootimg_detect(void *private_data, int force_type, int fd)
{
	struct abootimg_editor_private_data *p = private_data;
	uint8_t sha1sum[SHA1_DIGEST_SIZE];
	uint32_t offset;
	SHA1_CTX ctx;
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

	sha1_init(&ctx);
	offset = p->head.page_size;
	for (size_t i = 0; i < n_abootimg_files; i++) {
		uint32_t sz = abootimg_file_sizes[i];

		if (sz > 0)
			dd(fd, -1, offset, 0, sz, abootimg_update_sha1sum, &ctx);

		/* update sha1sum string even if the filesize is zero */
		abootimg_update_sha1sum((uint8_t *)&sz, sizeof(sz), &ctx);
		offset += aligned_length(sz, p->head.page_size);
	}
	sha1_finish(&ctx, sha1sum);

	if (memcmp(sha1sum, p->head.id, sizeof(sha1sum))) {
		fprintf_if_force_type("Error: sha1sum doesn't match\n");
		return -1;
	}

	return 0;
}

static int abootimg_list(void *private_data, int fd, int argc, char **argv)
{
	struct abootimg_editor_private_data *p = private_data;
	struct andr_img_hdr *hdr = &p->head;
	uint8_t *p_id = (uint8_t *)hdr->id;

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
		fd_child = fileopen(filename, O_WRONLY | O_CREAT, 0664);
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
	const char			*name;
	uint32_t			size;
};

static int abootimg_unpack(void *private_data, int fd, const char *outdir, int argc, char **argv)
{
	struct abootimg_editor_private_data *p = private_data;
	struct andr_img_hdr *hdr = &p->head;
	uint64_t offset = hdr->page_size;
	int ret = 0;

	struct abootimg_file files[] = {
		{ "kernel.bin",		hdr->kernel_size	},
		{ "ramdisk.bin",	hdr->ramdisk_size	},
		{ "second.bin",		hdr->second_size	},
		{ "recovery_dtbo.bin",	hdr->recovery_dtbo_size	},
		{ "dtb.bin",		hdr->dtb_size		},
		{ NULL,			0			},
	};

	for (struct abootimg_file *file = &files[0]; file->name; file++) {
		ret = abootimg_unpack_save_file(fd, outdir, file->name,
						offset, file->size);
		if (ret < 0)
			return ret;

		offset += aligned_length(file->size, hdr->page_size);
	}

	return ret;
}

static struct imgeditor abootimg_editor = {
	.name			= "abootimg",
	.descriptor		= "android boot.img editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct andr_img_hdr),
	.private_data_size	= sizeof(struct abootimg_editor_private_data),
	.detect			= abootimg_detect,
	.list			= abootimg_list,
	.unpack			= abootimg_unpack,

	.search_magic		= {
		.magic		= ANDR_BOOT_MAGIC,
		.magic_sz	= sizeof(ANDR_BOOT_MAGIC) - 1,
		.magic_offset	= offsetof(struct andr_img_hdr, magic),
	}
};
REGISTER_IMGEDITOR(abootimg_editor);
