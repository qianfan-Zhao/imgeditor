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

static int verbose = 0;
#define verbose_printf(fmt, ...) do {			\
	if (verbose > 0)				\
		printf(fmt, ##__VA_ARGS__);		\
} while (0)

#define FDT_EDITOR_MAX_DTB_SIZE		(2 << 20) /* 2MiB */

typedef uint32_t			fdt32_t;
typedef uint64_t			fdt64_t;
#define fdt32_to_cpu(x)			be32_to_cpu(x)
#define fdt64_to_cpu(x)			be64_to_cpu(x)

#define FDT_MAGIC	0xd00dfeed	/* 4: version, 4: total size */

static uint8_t fdt_magic_be32[] = { 0xd0, 0x0d, 0xfe, 0xed };

struct fdt_header {
	fdt32_t magic;			 /* magic word FDT_MAGIC */
	fdt32_t totalsize;		 /* total size of DT block */
	fdt32_t off_dt_struct;		 /* offset to structure */
	fdt32_t off_dt_strings;		 /* offset to strings */
	fdt32_t off_mem_rsvmap;		 /* offset to memory reserve map */
	fdt32_t version;		 /* format version */
	fdt32_t last_comp_version;	 /* last compatible version */

	/* version 2 fields below */
	fdt32_t boot_cpuid_phys;	 /* Which physical CPU id we're
					    booting on */
	/* version 3 fields below */
	fdt32_t size_dt_strings;	 /* size of the strings block */

	/* version 17 fields below */
	fdt32_t size_dt_struct;		 /* size of the structure block */
};

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

struct fdt_editor_private_data {
	void				*dtb;
	uint32_t			totalsize;
};

static void fdt_exit(void *private_data)
{
	struct fdt_editor_private_data *p = private_data;

	free(p->dtb);
}

static int fdt_detect(void *private_data, int force_type, int fd)
{
	struct fdt_editor_private_data *p = private_data;
	struct fdt_header header, *hdr = &header;
	int ret;

	ret = read(fd, hdr, sizeof(*hdr));
	if (ret < 0)
		return ret;

	if (fdt32_to_cpu(hdr->magic) != FDT_MAGIC) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

	p->totalsize = fdt32_to_cpu(hdr->totalsize);
	if (p->totalsize > FDT_EDITOR_MAX_DTB_SIZE) {
		fprintf_if_force_type("Error: dtb size limited\n");
		return -1;
	}

	p->dtb = malloc(p->totalsize);
	if (!p->dtb) {
		fprintf(stderr, "Error: request %d bytes memory for dtb failed\n",
			p->totalsize);
		return -1;
	}

	fileseek(fd, 0);
	ret = read(fd, p->dtb, p->totalsize);
	if (ret < 0)
		return ret;

	return 0;
}

static int fdt_prop_print_multi_strings(const char *ms, int sz)
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

	printf(" = \"");
	for (int i = 0; i < sz; i++) {
		switch (ms[i]) {
		case '\0':
			printf("\"%s",
				i == sz - 1 ? ";" : ", \"");
			break;
		default:
			printf("%c", ms[i]);
			break;
		}
	}

	return 0;
}

static int fdt_prop_print_value(void *data, int sz)
{
	uint32_t *p_u32 = data;
	uint8_t *p_u8 = data;

	/* bool prop, eg:
	 * regulator-always-on;
	 */
	if (sz == 0) {
		printf(";");
		return 0;
	}

	/* multi-string format? */
	if (!fdt_prop_print_multi_strings(data, sz))
		return 0;

	if ((sz % 4) == 0) {
		sz /= 4;

		printf(" = <");
		for (int i = 0; i < sz; i++) {
			printf("0x%08x", fdt32_to_cpu(p_u32[i]));
			if (i != sz - 1)
				printf(" ");
		}
		printf(">;");
	} else {
		printf(" = [");
		for (int i = 0; i < sz; i++) {
			printf("0x%02x", p_u8[i]);
			if (i != sz - 1)
				printf(" ");
		}
		printf("];");
	}

	return 0;
}

static int fdt_scan_struct(struct fdt_header *hdr)
{
	#define node_offset(node) ((unsigned long)((void *)node - (void *)hdr))
	struct fdt_node_header *node =
		(void *)hdr + fdt32_to_cpu(hdr->off_dt_struct);
	struct fdt_property *prop;
	const char *name;
	int depth = 0;
	int len;

	while (fdt32_to_cpu(node->tag) != FDT_END) {
		size_t nodesz = 0;;

		verbose_printf("0x%08lx: ", node_offset(node));

		switch (fdt32_to_cpu(node->tag)) {
		case FDT_BEGIN_NODE:
			name = node->name;
			len = strlen(name) + 1; /* include null */
			nodesz = sizeof(node->tag) + aligned_length(len, 4);

			printf("%*s%s {", depth * 4, "", len == 1 ? "/" : name);
			depth++;
			break;
		case FDT_NOP:
			printf("%*snop", depth * 4, "");
			nodesz = sizeof(node->tag);
			break;
		case FDT_PROP:
			prop = (struct fdt_property *)node;
			len = fdt32_to_cpu(prop->len);
			nodesz = sizeof(*prop) + aligned_length(len, 4);

			name = (const char *)hdr
				+ fdt32_to_cpu(prop->nameoff)
				+ fdt32_to_cpu(hdr->off_dt_strings);
			printf("%*s%s", depth * 4, "", name);

			fdt_prop_print_value(prop->data, len);
			break;
		case FDT_END_NODE:
			depth--;
			printf("%*s}", depth * 4, "");
			nodesz = sizeof(node->tag);
			break;
		default:
			fprintf(stderr, "Error: 0x%08lx: bad fdt tag\n",
				(unsigned long)((void *)node - (void *)hdr));
			return -1;
		}

		printf("\n");
		node = (void *)node + nodesz;
	}

	return 0;
}

static int fdt_list(void *private_data, int fd, int argc, char **argv)
{
	struct fdt_editor_private_data *p = private_data;
	struct fdt_reserve_entry *resv_entry;
	struct fdt_header *hdr = p->dtb;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--verbose"))
			++verbose;
	}

	if (verbose > 0)
		structure_print("%-20s: ", hdr, structure_fdt_header);

	resv_entry = p->dtb + fdt32_to_cpu(hdr->off_mem_rsvmap);
	for (; resv_entry->size; resv_entry++) {
		verbose_printf("0x%08lx: ", (unsigned long)((void *)resv_entry - p->dtb));
		printf("/memreserve/ 0x%016" PRIx64 " 0x%" PRIx64 ";\n",
			fdt64_to_cpu(resv_entry->address),
			fdt64_to_cpu(resv_entry->size));
	}

	return fdt_scan_struct(hdr);
}

static struct imgeditor fdt_editor = {
	.name			= "fdt",
	.descriptor		= "device tree image editor",
	.flags			= IMGEDITOR_FLAG_SINGLE_BIN,
	.header_size		= sizeof(struct fdt_header),
	.private_data_size	= sizeof(struct fdt_editor_private_data),
	.detect			= fdt_detect,
	.list			= fdt_list,
	.exit			= fdt_exit,

	.search_magic		= {
		.magic		= fdt_magic_be32,
		.magic_sz	= sizeof(fdt_magic_be32),
		.magic_offset	= offsetof(struct fdt_header, magic),
	}
};
REGISTER_IMGEDITOR(fdt_editor);
