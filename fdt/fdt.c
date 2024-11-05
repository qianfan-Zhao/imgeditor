/*
 * fdt image editor
 * qianfan Zhao
 */
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include "imgeditor.h"
#include "structure.h"
#include "list_head.h"
#include "string_helper.h"
#include "fdt_export.h"

/*
 * U-boot mkimage is based on fdt, it maybe containes some images which
 * make the file size too large.
 */
#define FDT_EDITOR_MAX_DTB_SIZE		(64 << 20) /* 64MiB */

#define FDT_MAGIC	0xd00dfeed	/* 4: version, 4: total size */

static uint8_t fdt_magic_be32[] = { 0xd0, 0x0d, 0xfe, 0xed };

static struct structure_item structure_fdt_header[] = {
	STRUCTURE_ITEM(struct fdt_header, totalsize,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct fdt_header, off_dt_struct,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct fdt_header, off_dt_strings,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct fdt_header, off_mem_rsvmap,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct fdt_header, version,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct fdt_header, last_comp_version,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct fdt_header, boot_cpuid_phys,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct fdt_header, size_dt_strings,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct fdt_header, size_dt_struct,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM_END(),
};

struct fdt_reserve_entry {
	fdt64_t address;
	fdt64_t size;
};

struct fdt_node_header {
	fdt32_t tag;
	char name[0];
};

struct fdt_property {
	fdt32_t tag;
	fdt32_t len;
	fdt32_t nameoff;
	char data[0];
};

#define FDT_BEGIN_NODE	0x1		/* Start node: full name */
#define FDT_END_NODE	0x2		/* End node */
#define FDT_PROP	0x3		/* Property: name off,
					   size, content */
#define FDT_NOP		0x4		/* nop */
#define FDT_END		0x9

static int fdt_prop_print_multi_strings(FILE *fp, const char *ms, int sz)
{
	/* the first char can't be '\0', and the last char must be '\0' */
	if (ms[0] == '\0' || ms[sz - 1] != '\0')
		return -1;

	/* pinctrl-names = "default", "sleep"; */
	for (int i = 0; i < sz; i++) {
		if (ms[i] == '\0') {
			/* Two adjacent '\0' are not allowed */
			if (i != sz - 1 && ms[i + 1] == '\0')
				return -1;
		} else if (!isprint(ms[i])) {
			return -1;
		}
	}

	fprintf(fp, " = \"");
	for (int i = 0; i < sz; i++) {
		switch (ms[i]) {
		case '\0':
			fprintf(fp, "\"%s",
				i == sz - 1 ? ";" : ", \"");
			break;
		default:
			fprintf(fp, "%c", ms[i]);
			break;
		}
	}

	return 0;
}

static int fdtprop_feature_incbin = 0;

static int fdt_prop_print_value(FILE *fp, void *data, int sz)
{
	uint32_t *p_u32 = data;
	uint8_t *p_u8 = data;

	/* bool prop, eg:
	 * regulator-always-on;
	 */
	if (sz == 0) {
		fprintf(fp, ";");
		return 0;
	}

	if (fdtprop_feature_incbin) {
		const char *s = data;

		if (strncmp(s, "/incbin/", 8) == 0 && s[sz - 1] == '\0') {
			fprintf(fp, " = %s;", s);
			return 0;
		}
	}

	/* multi-string format? */
	if (!fdt_prop_print_multi_strings(fp, data, sz))
		return 0;

	if ((sz % 4) == 0) {
		int striped = 0, maxsz = sz / 4;

		if (maxsz > 32) {
			maxsz = 32;
			striped = 1;
		}

		fprintf(fp, " = <");
		for (int i = 0; i < maxsz; i++) {
			fprintf(fp, "0x%08x", fdt32_to_cpu(p_u32[i]));
			if (i != maxsz - 1)
				fprintf(fp, " ");
		}

		if (striped)
			fprintf(fp, "... (total = %d)", sz);

		fprintf(fp, ">;");
	} else {
		int striped = 0, maxsz = sz;

		if (maxsz > 32) {
			maxsz = 32;
			striped = 1;
		}

		fprintf(fp, " = [");
		for (int i = 0; i < maxsz; i++) {
			fprintf(fp, "0x%02x", p_u8[i]);
			if (i != sz - 1)
				fprintf(fp, " ");
		}

		if (striped)
			fprintf(fp, "... (total = %d)", sz);

		fprintf(fp, "];");
	}

	return 0;
}

static struct device_node *new_device_node(const char *name, int data_size,
					   struct device_node *parent)
{
	size_t nodesz = sizeof(struct device_node) + data_size;
	struct device_node *node = calloc(1, nodesz);

	if (!node)
		return node;

	snprintf(node->name, sizeof(node->name), "%s", name);
	node->data_size = data_size;

	list_init(&node->head);
	list_init(&node->child);
	if (parent)
		list_add_tail(&node->head, &parent->child);

	return node;
}

#define FDT_MAX_DEPTH		32

struct device_node *
	device_node_find_byname(struct device_node *root, const char *name)
{
	struct device_node *node;

	list_for_each_entry(node, &root->child, head, struct device_node) {
		if (!strcmp(node->name, name))
			return node;
	}

	return NULL;
}

struct device_node *
	device_node_find_bypath(struct device_node *root, const char *path)
{
	char path_nonconst[strlen(path) + 1], *path_argv[FDT_MAX_DEPTH];
	int path_argc;

	memcpy(path_nonconst, path, strlen(path) + 1);
	path_argc = argv_buffer_split_with_delim(path_nonconst, "/",
				path_argv, FDT_MAX_DEPTH);
	if (path_argc == 0)
		return root;

	for (int i = 0; i < path_argc; i++) {
		const char *name = path_argv[i];

		root = device_node_find_byname(root, name);
		if (!root)
			return NULL;
	}

	return root;
}

int device_node_read_u32(struct device_node *node, uint32_t *ret_data)
{
	if (node->data_size != sizeof(uint32_t))
		return -1;

	/* data in be mode */
	*ret_data = (node->data[0] << 24)
		  | (node->data[1] << 16)
		  | (node->data[2] <<  8)
		  | (node->data[3] <<  0);
	return 0;
}

/* delete and free all the child */
int device_node_delete(struct device_node *root)
{
	struct device_node *node, *next;

	list_for_each_entry_safe(node, next, &root->child, head, struct device_node)
		device_node_delete(node);

	list_del(&root->head);
	free(root);

	return 0;
}

int device_node_delete_bypath(struct device_node *root, const char *path)
{
	struct device_node *node = device_node_find_bypath(root, path);

	if (!node)
		return -1;

	return device_node_delete(node);
}

static int fdt_unflatten(struct fdt_editor_private_data *fdt)
{
	struct device_node *root = NULL, *fdt_device_node_stack[FDT_MAX_DEPTH];
	struct device_node *device_node_prop;

	#define node_offset(node) ((unsigned long)((void *)node - (void *)hdr))
	struct fdt_header *hdr = fdt->dtb;
	struct fdt_node_header *node =
		(void *)hdr + fdt32_to_cpu(hdr->off_dt_struct);
	struct fdt_property *prop;
	const char *name;
	int depth = 0;
	int len;

	while (fdt32_to_cpu(node->tag) != FDT_END) {
		size_t nodesz = 0;

		switch (fdt32_to_cpu(node->tag)) {
		case FDT_BEGIN_NODE:
			name = node->name;
			len = strlen(name) + 1; /* include null */
			nodesz = sizeof(node->tag) + aligned_length(len, 4);

			/* push device node stack */
			fdt_device_node_stack[depth] = root;
			++depth;

			if (depth >= FDT_MAX_DEPTH) {
				fprintf(stderr, "Error: too much depth(%d)\n",
					depth);
				return -1;
			}

			root = new_device_node(len == 1 ? "/" : name, 0, root);
			if (!root) {
				fprintf(stderr, "Error: alloc device node failed\n");
				return -1;
			}

			if (depth == 1) /* this is root node */
				fdt->root = root;
			break;
		case FDT_NOP:
			nodesz = sizeof(node->tag);
			break;
		case FDT_PROP:
			prop = (struct fdt_property *)node;
			len = fdt32_to_cpu(prop->len);
			nodesz = sizeof(*prop) + aligned_length(len, 4);

			name = (const char *)hdr
				+ fdt32_to_cpu(prop->nameoff)
				+ fdt32_to_cpu(hdr->off_dt_strings);

			device_node_prop = new_device_node(name, len, root);
			if (!device_node_prop) {
				fprintf(stderr, "Error: alloc prop node %s with %d bytes data failed\n",
					name, len);
				return -1;
			}

			memcpy(device_node_prop->data, prop->data, len);
			break;
		case FDT_END_NODE:
			nodesz = sizeof(node->tag);

			if (depth == 0) {
				fprintf(stderr, "Error: find FDT_END_NODE but depth is zero\n");
				return -1;
			}

			/* pop device node stack */
			--depth;
			root = fdt_device_node_stack[depth];
			break;
		default:
			fprintf(stderr, "Error: 0x%08lx: bad fdt tag\n",
				(unsigned long)((void *)node - (void *)hdr));
			return -1;
		}

		node = (void *)node + nodesz;
	}

	return 0;
}

static int fdt_detect(void *private_data, int force_type, int fd)
{
	struct fdt_editor_private_data *fdt = private_data;
	struct fdt_header header, *hdr = &header;
	int ret;

	ret = read(fd, hdr, sizeof(*hdr));
	if (ret < 0)
		return ret;

	if (fdt32_to_cpu(hdr->magic) != FDT_MAGIC) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

	fdt->totalsize = fdt32_to_cpu(hdr->totalsize);
	if (fdt->totalsize == 0) {
		fprintf_if_force_type("Error: dtb total size is zero\n");
		return -1;
	} else if (fdt->totalsize > FDT_EDITOR_MAX_DTB_SIZE) {
		fprintf_if_force_type("Error: dtb size limited\n");
		return -1;
	}

	if (fdt32_to_cpu(hdr->off_dt_strings) > FDT_EDITOR_MAX_DTB_SIZE) {
		fprintf_if_force_type("Error: off_dt_strings overrange\n");
		return -1;
	} else if (fdt32_to_cpu(hdr->off_dt_struct) > FDT_EDITOR_MAX_DTB_SIZE) {
		fprintf_if_force_type("Error: off_dt_struct overrange\n");
		return -1;
	} else if (fdt32_to_cpu(hdr->off_mem_rsvmap) > FDT_EDITOR_MAX_DTB_SIZE) {
		fprintf_if_force_type("Error: off_mem_rsvmap overrange\n");
		return -1;
	}

	fdt->dtb = malloc(fdt->totalsize);
	if (!fdt->dtb) {
		fprintf(stderr, "Error: request %d bytes memory for dtb failed\n",
			fdt->totalsize);
		return -1;
	}

	fileseek(fd, 0);
	ret = read(fd, fdt->dtb, fdt->totalsize);
	if (ret < 0)
		return ret;

	return fdt_unflatten(fdt);
}

int fdt_editor_detect(struct fdt_editor_private_data *p, int force_type, int fd)
{
	return fdt_detect(p, force_type, fd);
}

static int64_t fdt_get_total_size(void *private_data, int fd)
{
	struct fdt_editor_private_data *fdt = private_data;

	return fdt->totalsize;
}

static int fit_detect(void *private_data, int force_type, int fd)
{
	/* fit only support force_type mode */
	if (!force_type)
		return -1;

	return fdt_detect(private_data, force_type, fd);
}

static void fdt_exit(void *private_data)
{
	struct fdt_editor_private_data *p = private_data;

	free(p->dtb);

	if (p->root)
		device_node_delete(p->root);
}

void fdt_editor_exit(struct fdt_editor_private_data *p)
{
	fdt_exit(p);
}

static int fdt_list_node_prop(struct device_node *prop, FILE *fp, int depth)
{
	int ret;

	fprintf(fp, "%*s%s", depth * 4, "", prop->name);
	ret = fdt_prop_print_value(fp, prop->data, prop->data_size);
	fprintf(fp, "\n");

	return ret;
}

static int fdt_list_node(struct device_node *root, FILE *fp, int depth)
{
	struct device_node *dev;

	if (list_empty(&root->child))
		return fdt_list_node_prop(root, fp, depth);

	fprintf(fp, "%*s%s {\n", depth * 4, "", root->name);
	list_for_each_entry(dev, &root->child, head, struct device_node) {
		int ret = fdt_list_node(dev, fp, depth + 1);
		if (ret < 0)
			return ret;
	}
	fprintf(fp, "%*s};\n", depth * 4, "");

	return 0;
}

static int fdt_show_memreserve(struct fdt_editor_private_data *fdt)
{
	struct fdt_reserve_entry *resv_entry;
	struct fdt_header *hdr = fdt->dtb;

	resv_entry = fdt->dtb + fdt32_to_cpu(hdr->off_mem_rsvmap);
	for (; resv_entry->size; resv_entry++) {
		printf("/memreserve/ 0x%016" PRIx64 " 0x%" PRIx64 ";\n",
			fdt64_to_cpu(resv_entry->address),
			fdt64_to_cpu(resv_entry->size));
	}

	return 0;
}

static int fdt_summary(void *private_data, int fd, char *buf, size_t bufsz)
{
	struct fdt_editor_private_data *fdt = private_data;
	uint32_t crc;

	crc = crc32(0, fdt->dtb, fdt->totalsize);
	snprintf(buf, bufsz, "CRC32: 0x%08x(%d)", crc, fdt->totalsize);

	return 0;
}

static int fdt_list(void *private_data, int fd, int argc, char **argv)
{
	struct fdt_editor_private_data *fdt = private_data;
	struct device_node *root;
	const char *path = "/";

	if (get_verbose_level() > 0)
		structure_print("%-20s", fdt->dtb, structure_fdt_header);

	/* imgeditor xxx -- "/images/kernel" */
	if (argc > 0) {
		path = argv[0];
	} else {
		printf("/dts-v1/;\n");
		fdt_show_memreserve(fdt);
	}

	root = device_node_find_bypath(fdt->root, path);
	if (!root) {
		fprintf(stderr, "Error: %s is not found\n", path);
		return -1;
	}

	return fdt_list_node(root, stdout, 0);
}

static int fit_unpack(void *private_data, int fd, const char *outdir,
		      int argc, char **argv)
{
	int fdtprop_feature_incbin_back = fdtprop_feature_incbin;
	struct fdt_editor_private_data *fit = private_data;
	struct device_node *image, *images;
	FILE *fp_its = NULL;
	char tmp[1024];

	images = device_node_find_bypath(fit->root, "/images");
	if (!images) {
		fprintf(stderr, "Error: \"/images\" is not found\n");
		return -1;
	}

	device_node_delete_bypath(fit->root, "/timestamp");

	/* kernel {
	 *     data-size = <0x00a0f387>;
	 *     data-position = <0x00001000>;
	 *     type = "kernel";
	 *     arch = "arm64";
	 *     os = "linux";
	 *     compression = "gzip";
	 *     entry = <0x00200000>;
	 *     load = <0x00200000>;
	 *     hash {
	 *         value = <0xc2a52734 0xec0849a8 0xb310c269 0x4440c0fa 0xad388c62 0x10dbdd04 0xae46d375 0x114df55f>;
	 *         algo = "sha256";
	 *     }
	 * }
	 */
	list_for_each_entry(image, &images->child, head, struct device_node) {
		struct device_node *data_size_node, *data_pos_node;
		struct device_node *incbin_node;
		uint32_t data_size, data_pos;
		int fd_image;

		data_size_node = device_node_find_byname(image, "data-size");
		if (!data_size_node) {
			fprintf(stderr, "Error: %s/data-size is not found\n",
				image->name);
			return -1;
		}

		if (device_node_read_u32(data_size_node, &data_size) < 0) {
			fprintf(stderr, "Error: %s/data-size is not u32\n",
				image->name);
			return -1;
		}

		data_pos_node = device_node_find_byname(image, "data-position");
		if (!data_pos_node) {
			fprintf(stderr, "Error: %s/data-position is not found\n",
				image->name);
			return -1;
		}

		if (device_node_read_u32(data_pos_node, &data_pos) < 0) {
			fprintf(stderr, "Error: %s/data-position is not u32\n",
				image->name);
			return -1;
		}

		device_node_delete(data_size_node);
		device_node_delete(data_pos_node);
		device_node_delete_bypath(image, "hash/value");
		data_size_node = data_pos_node = NULL;

		snprintf(tmp, sizeof(tmp), "%s/%s.bin", outdir, image->name);
		fd_image = fileopen(tmp, O_WRONLY | O_CREAT, 0664);
		if (fd_image < 0)
			return fd;

		dd(fd, fd_image, data_pos, 0, data_size, NULL, NULL);

		snprintf(tmp, sizeof(tmp), "/incbin/(\"./%s.bin\")",
			 image->name);
		incbin_node = new_device_node("data", strlen(tmp) + 1, NULL);
		if (!incbin_node) {
			fprintf(stderr, "Error: alloc data node for %s failed\n",
				image->name);
			return -1;
		}

		memcpy(incbin_node->data, tmp, strlen(tmp) + 1);
		list_add(&incbin_node->head, &image->child);
	}

	snprintf(tmp, sizeof(tmp), "%s/image.its", outdir);
	fp_its = fopen(tmp, "w+");
	if (!fp_its) {
		fprintf(stderr, "Error: create %s failed\n", tmp);
		return -1;
	}

	fdtprop_feature_incbin = 1;
	fprintf(fp_its, "/dts-v1/;\n");
	fdt_list_node(fit->root, fp_its, 0);
	fdtprop_feature_incbin = fdtprop_feature_incbin_back;

	fclose(fp_its);
	return 0;
}

static int64_t fit_get_total_size(void *private_data, int fd)
{
	struct fdt_editor_private_data *fit = private_data;
	struct device_node *image, *images;
	int64_t maxsz = fit->totalsize; /* total size of fdt part */

	images = device_node_find_bypath(fit->root, "/images");
	if (!images) {
		fprintf(stderr, "Error: \"/images\" is not found\n");
		return -1;
	}

	list_for_each_entry(image, &images->child, head, struct device_node) {
		struct device_node *data_size_node, *data_pos_node;
		uint32_t data_size, data_pos;
		int64_t endp;

		data_size_node = device_node_find_byname(image, "data-size");
		if (!data_size_node) {
			fprintf(stderr, "Error: %s/data-size is not found\n",
				image->name);
			return -1;
		}

		if (device_node_read_u32(data_size_node, &data_size) < 0) {
			fprintf(stderr, "Error: %s/data-size is not u32\n",
				image->name);
			return -1;
		}

		data_pos_node = device_node_find_byname(image, "data-position");
		if (!data_pos_node) {
			fprintf(stderr, "Error: %s/data-position is not found\n",
				image->name);
			return -1;
		}

		if (device_node_read_u32(data_pos_node, &data_pos) < 0) {
			fprintf(stderr, "Error: %s/data-position is not u32\n",
				image->name);
			return -1;
		}

		endp = data_pos + data_size;
		if (endp > maxsz)
			maxsz = endp;
	}

	return maxsz;
}

static struct imgeditor fdt_editor = {
	.name			= "fdt",
	.descriptor		= "device tree image editor",
	.flags			= IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST | IMGEDITOR_FLAG_SINGLE_BIN,
	.header_size		= sizeof(struct fdt_header),
	.private_data_size	= sizeof(struct fdt_editor_private_data),
	.detect			= fdt_detect,
	.summary		= fdt_summary,
	.total_size		= fdt_get_total_size,
	.list			= fdt_list,
	.exit			= fdt_exit,

	.search_magic		= {
		.magic		= fdt_magic_be32,
		.magic_sz	= sizeof(fdt_magic_be32),
		.magic_offset	= offsetof(struct fdt_header, magic),
	}
};
REGISTER_IMGEDITOR(fdt_editor);

static struct imgeditor fit_editor = {
	.name			= "fit",
	.descriptor		= "fit image editor for u-boot mkimage",
	.flags			= IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST | IMGEDITOR_FLAG_CONTAIN_MULTI_BIN,
	.header_size		= sizeof(struct fdt_header),
	.private_data_size	= sizeof(struct fdt_editor_private_data),
	.detect			= fit_detect,
	.list			= fdt_list,
	.total_size		= fit_get_total_size,
	.unpack			= fit_unpack,
	.exit			= fdt_exit,
};
REGISTER_IMGEDITOR(fit_editor);
