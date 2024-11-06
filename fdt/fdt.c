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

#define FDT_SYMBOLS_NAME		"__symbols__"
#define FDT_ALIASES_NAME		"aliases"

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

static int fdt_prop_print_simple_value(FILE *fp, void *data, int sz)
{
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

	return -1;
}

/* such as this array: 'interrupts = <0 36 4>'
 *
 * show in 'interrupts = <0 0x24 4>' is not good.
 */
static int fdt_prop_number_array_detect_base(fdt32_t *arrays, size_t count)
{
	size_t base10 = 0, base16 = 0;

	for (size_t i = 0; i < count; i++) {
		uint32_t n = fdt32_to_cpu(arrays[i]);

		if (n <= 10 || (n % 10) == 0)
			base10++;
		else
			base16++;
	}

	if (base16 > base10)
		return 16;

	return 10;
}

static int fdt_prop_print_number_autobase(FILE *fp, uint32_t n, int base)
{
	const char *format;

	if (base == 10 || (base != 16 && (n <= 10u || n % 10 == 0)))
		format = "%d";
	else if (n > 0xffffff)
		format = "0x%08x";
	else if (n > 0xffff)
		format = "0x%06x";
	else if (n > 0xfff)
		format = "0x%04x";
	else if (n > 0xff)
		format = "0x%03x";
	else if (n > 0xf)
		format = "0x%02x";
	else
		format = "0x%x";

	fprintf(fp, format, n);
	return 0;
}

static int fdt_prop_print_simple_number(FILE *fp, void *data, int sz)
{
	uint32_t *p_u32 = data;
	uint8_t *p_u8 = data;

	if ((sz % 4) == 0) {
		int striped = 0, maxsz = sz / 4;
		int base;

		if (maxsz > 32) {
			maxsz = 32;
			striped = 1;
		}

		base = fdt_prop_number_array_detect_base(p_u32, maxsz);

		fprintf(fp, " = <");
		for (int i = 0; i < maxsz; i++) {
			uint32_t n = fdt32_to_cpu(p_u32[i]);

			fdt_prop_print_number_autobase(fp, n, base);
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

static int string_arrays_index(const char **arrays, const char *s)
{
	for (int i = 0; arrays[i]; i++) {
		if (!strcmp(arrays[i], s))
			return i;
	}

	return -1;
}

static const char *fdt_u32_prop_lists[] = {
	"#address-cells",
	"#clock-cells",
	"#cooling-cells",
	"#dma-cells",
	"#gpio-cells",
	"#interrupt-cells",
	"#phy-cells",
	"#pwm-cells",
	"#reset-cells",
	"#size-cells",
	NULL,
};

static int fdt_prop_print_u32_number(FILE *fp, const char *propname,
				     void *data, size_t data_size)
{
	fdt32_t *p_number = data;
	int ret;

	if (data_size != 4)
		return -1;

	ret = string_arrays_index(fdt_u32_prop_lists, propname);
	if (ret < 0)
		return ret;

	fprintf(fp, " = <%d>;", fdt32_to_cpu(*p_number));
	return 0;
}

static int fdt_prop_print_reg(FILE *fp, const char *propname, void *data,
			      size_t data_size)
{
	fdt32_t *p = data;

	if (strcmp(propname, "reg") != 0 || (data_size % 4) != 0)
		return -1;

	fprintf(fp, " = <");
	for (size_t i = 0; i < data_size; i += 4) {
		uint32_t n = fdt32_to_cpu(*p);
		int base = n <= 10 ? 10 : 16;

		fdt_prop_print_number_autobase(fp, n, base);
		if (i != data_size - 4)
			fprintf(fp, " ");

		p++;
	}
	fprintf(fp, ">;");

	return 0;
}

static int device_node_read_cells(struct device_node *node,
				  const char *cell_name, uint32_t *cell_size)
{
	struct device_node *prop =
		device_node_find_byname(node, cell_name);

	if (!prop)
		return -1;

	return device_node_read_u32(prop, cell_size);
}

static int _fdt_prop_print_phandles(struct fdt_editor_private_data *fdt,
				    const char *type_name, const char *cell_name,
				    size_t fix_cell_size,
				    FILE *fp,
				    const char *propname,
				    void *data, size_t data_size)
{
	/* osc24M: #clock-cells = <0>;
	 * pll6:   #clock-cells = <1>;
	 * clocks = <&osc24M>, <&pll6 1>, <&pll5 1>;
	 */
	if (data_size < 4)
		return -1;

	for (int check_only = 1; check_only >= 0; check_only--) {
		size_t sz = data_size;
		fdt32_t *p = data;

		if (!check_only)
			fprintf(fp, " = ");

		while (sz >= 4) {
			struct device_node *target;
			uint32_t phandle = fdt32_to_cpu(*p);
			uint32_t cell_size = fix_cell_size;
			int ret;

			sz -= 4;
			p++;

			target = device_node_search_phandle(fdt->root, phandle);
			if (!target) {
				fprintf(stderr, "Warnning: %s %d is not found\n",
					type_name, phandle);
				return -1;
			}

			/* doesn't has a label */
			if (target->label[0] == '\0')
				return -1;

			if (cell_name) {
				/* the default cell size is 0 */
				ret = device_node_read_cells(target, cell_name,
							     &cell_size);
				if (ret < 0)
					return ret;
			}

			if (cell_size * 4 > sz)
				return -1;

			if (!check_only)
				fprintf(fp, "<&%s", target->label);

			for (uint32_t i = 0; i < cell_size; i++) {
				if (!check_only) {
					uint32_t n = fdt32_to_cpu(*p);

					if (n == 0xffffffff)
						fprintf(fp, " (~0)");
					else
						fprintf(fp, " %u", fdt32_to_cpu(*p));
				}

				sz -= 4;
				p++;
			}

			if (!check_only) {
				fprintf(fp, ">");

				if (sz > 0)
					fprintf(fp, ", ");
			}
		}

		if (!check_only)
			fprintf(fp, ";");
	}

	return 0;
}

static int fdt_prop_print_phandles(struct fdt_editor_private_data *fdt,
				   const char *type_name,
				   const char *cell_name, size_t fix_cell_size,
				   FILE *fp,
				   const char *propname,
				   void *data, size_t data_size)
{
	if (strcmp(type_name, propname) != 0)
		return -1;

	return _fdt_prop_print_phandles(fdt, type_name, cell_name, fix_cell_size,
					fp, propname, data, data_size);
}

static int string_ending_in(const char *s, const char *match)
{
	size_t len = strlen(match);

	return strlen(s) > len
		&& !strcmp(s + strlen(s) - len, match);
}

static int fdt_prop_print_gpios(struct fdt_editor_private_data *fdt,
				FILE *fp, const char *propname,
				void *data, size_t data_size)
{
	int is_gpio = 0;

	is_gpio |= !strcmp(propname, "gpio");
	is_gpio |= !strcmp(propname, "gpios");
	is_gpio |= string_ending_in(propname, "-gpio");
	is_gpio |= string_ending_in(propname, "-gpios");

	if (!is_gpio)
		return -1;

	return _fdt_prop_print_phandles(fdt, "gpios", "#gpio-cells", 0,
					fp, propname, data, data_size);
}

static int fdt_prop_print_pinctrl(struct fdt_editor_private_data *fdt,
				  FILE *fp, const char *propname,
				  void *data, size_t data_size)
{
	/* pinctrl-0 or pinctrl-1 or pinctrl-x but not pinctrl-names */
	char *endp;

	if (strncmp(propname, "pinctrl-", 8) != 0)
		return -1;

	strtol(propname + 8, &endp, 10);
	if (*endp != '\0')
		return -1;

	return _fdt_prop_print_phandles(fdt, "pinctrl", NULL, 0,
					fp, propname, data, data_size);
}

static int fdt_prop_print_supply(struct fdt_editor_private_data *fdt,
				 FILE *fp, const char *propname,
				 void *data, size_t data_size)
{
	if (!string_ending_in(propname, "-supply"))
		return -1;

	return _fdt_prop_print_phandles(fdt, "supply", NULL, 0,
					fp, propname, data, data_size);
}

static int fdt_prop_print_phandle_with_cells(struct fdt_editor_private_data *fdt,
					     FILE *fp, const char *propname,
					     void *data, size_t data_size)
{
	static const struct phandle_with_cell {
		const char *name;
		const char *cell_name;
	} phandles[] = {
		{ "clocks", "#clock-cells" },
		{ "dmas", "#dma-cells" },
		{ "resets", "#reset-cells" },
		{ "phys", "#phy-cells" },
		{ "pwms", "#pwm-cells" },
		{ "thermal-sensors", "#thermal-sensor-cells" },
		{ "cooling-device", "#cooling-cells" },
		{ "interconnects", "#interconnect-cells" },
		{ "power-domains", "#power-domain-cells" },
		{ NULL, NULL },
	};

	for (size_t i = 0; phandles[i].name; i++) {
		int ret = fdt_prop_print_phandles(fdt,
						  phandles[i].name,
						  phandles[i].cell_name, 0,
						  fp, propname, data, data_size);
		if (ret == 0)
			return ret;
	}

	return -1;
}

static int fdt_prop_print_phandle_fix_cells(struct fdt_editor_private_data *fdt,
					    FILE *fp, const char *propname,
					    void *data, size_t data_size)
{
	/* allwinner,sram = <&ve_sram 1>; */

	static const struct phandle_with_fix_cells {
		const char *name;
		size_t fix_cell_size;
	} phandles[] = {
		{ "allwinner,sram", 1 },
		{ NULL, 0 },
	};

	for (size_t i = 0; phandles[i].name; i++) {
		int ret = fdt_prop_print_phandles(fdt,
						  phandles[i].name,
						  NULL, phandles[i].fix_cell_size,
						  fp, propname, data, data_size);
		if (ret == 0)
			return ret;
	}

	return -1;
}

static int fdt_prop_print_phandle_nocells(struct fdt_editor_private_data *fdt,
					  FILE *fp, const char *propname,
					  void *data, size_t data_size)
{
	int phandle_index;

	/* emac {
	 *   phy = <&phy1>;
	 *   status = "okay";
	 * };
	 *
	 * allwinner,pipelines = <&mixer0>, <&mixer1>;
	 * trip = <&cpu_hot_trip>;
	 * remote-endpoint = <&mixer0_out_tcon_top>;
	 * interrupt-affinity = <&cpu0>, <&cpu1>, <&cpu2>, <&cpu3>;
	 * operating-points-v2 = <&cpu0_opp_table>;
	 * syscon = <&ccu>;
	 * cpu-idle-states = <&CPU_SLEEP_0 &CLUSTER_SLEEP_0>;
	 */
	static const char *phandles[] = {
		"phy",
		"allwinner,pipelines",
		"interrupt-parent",
		"trip",
		"remote-endpoint",
		"interrupt-affinity",
		"operating-points-v2",
		"syscon",
		"cpu-idle-states",
		NULL,
	};

	phandle_index = string_arrays_index(phandles, propname);
	if (phandle_index < 0)
		return phandle_index;

	return _fdt_prop_print_phandles(fdt, phandles[phandle_index], NULL, 0,
					fp, propname, data, data_size);
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

static int device_node_match_phandle(struct device_node *node, uint32_t phandle)
{
	struct device_node *prop;

	list_for_each_entry(prop, &node->child, head, struct device_node) {
		if (!strcmp(prop->name, "phandle") && prop->data_size == 4) {
			uint32_t n = phandle - 1; /* not eq */

			device_node_read_u32(prop, &n);
			if (n == phandle) /* matched */
				return 0;
		}
	}

	return -1;
}

struct device_node *
	device_node_search_phandle(struct device_node *root, uint32_t phandle)
{
	struct device_node *node;

	list_for_each_entry(node, &root->child, head, struct device_node) {
		struct device_node *matched;

		if (device_node_match_phandle(node, phandle) == 0)
			return node;

		if (!list_empty(&node->child)) {
			matched = device_node_search_phandle(node, phandle);
			if (matched)
				return matched;
		}
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

/* The dtc will create a section named as "__symbols__" if '-@|--symbols'
 * is enabled when compile dts.
 */
static int fdt_load_symbols(struct fdt_editor_private_data *fdt)
{
	struct device_node *symbols
		= device_node_find_byname(fdt->root, FDT_SYMBOLS_NAME);
	struct device_node *sym;

	if (!symbols)
		return 0;

	/* __symbols__ {
	 *   dummy = "/clocks/dummy";
	 *   osc24M = "/clocks/clk@01c20050";
	 *   osc32k = "/clocks/clk@0";
	 */
	list_for_each_entry(sym, &symbols->child, head, struct device_node) {
		struct device_node *target;
		char devpath[256];

		snprintf(devpath, sizeof(devpath), "%.*s",
			 (int)sym->data_size, (const char *)sym->data);
		target = device_node_find_bypath(fdt->root, devpath);

		if (!target) {
			fprintf(stderr, "Warnning: target of symbol %s is "
				"not found\n",
				sym->name);
			continue;
		}

		snprintf(target->label, sizeof(target->label), "%s", sym->name);
	}

	return 0;
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

			root->is_node = 1;

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

	fdt_load_symbols(fdt);
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

static int fdt_list_node_prop(struct fdt_editor_private_data *fdt,
			      struct device_node *prop, FILE *fp, int depth)
{
	int ret;

	if (!strcmp(prop->name, "phandle") && !fdt->keep_phandle)
		return 0;

	fprintf(fp, "%*s%s", depth * 4, "", prop->name);

#define try(todo, ...) do {		\
	ret = todo(__VA_ARGS__);	\
	if (ret == 0)			\
		goto done;		\
} while (0)

	try(fdt_prop_print_simple_value, fp, prop->data, prop->data_size);
	try(fdt_prop_print_u32_number, fp, prop->name, prop->data, prop->data_size);
	try(fdt_prop_print_reg, fp, prop->name, prop->data, prop->data_size);

	if (get_verbose_level() == 0) {
		try(fdt_prop_print_phandle_nocells, fdt,
		    fp, prop->name, prop->data, prop->data_size);

		try(fdt_prop_print_gpios, fdt,
		    fp, prop->name, prop->data, prop->data_size);

		try(fdt_prop_print_pinctrl, fdt,
		    fp, prop->name, prop->data, prop->data_size);

		try(fdt_prop_print_supply, fdt,
		    fp, prop->name, prop->data, prop->data_size);

		try(fdt_prop_print_phandle_with_cells, fdt,
		    fp, prop->name, prop->data, prop->data_size);

		try(fdt_prop_print_phandle_fix_cells, fdt,
		    fp, prop->name, prop->data, prop->data_size);
	}
#undef try

	ret = fdt_prop_print_simple_number(fp, prop->data, prop->data_size);
done:
	fprintf(fp, "\n");
	return ret;
}

/*
 * aliases {
 *   ethernet0 = "/soc@01c00000/ethernet@01c0b000";
 *   serial0 = "/soc@01c00000/serial@01c28000";
 */
static int fdt_list_alias_nodes(struct fdt_editor_private_data *fdt,
				struct device_node *symbols,
				struct device_node *aliases, FILE *fp, int depth)
{
	struct device_node *prop;

	list_for_each_entry(prop, &aliases->child, head, struct device_node) {
		struct device_node *target;
		char devpath[256];

		snprintf(devpath, sizeof(devpath), "%.*s",
			 (int)prop->data_size, (const char *)prop->data);

		target = device_node_find_bypath(fdt->root, devpath);
		if (!target) {
			fprintf(stderr, "Warnning: alias %s's target is not"
				" found\n", prop->name);
			fdt_list_node_prop(fdt, prop, fp, depth);
		} else if (target->label[0] == '\0') {
			fprintf(stderr, "Warnning: alias %s's target doesn't"
				" has a label\n", prop->name);
			fdt_list_node_prop(fdt, prop, fp, depth);
		} else {
			fprintf(fp, "%*s%s = &%s;\n",
				depth * 4, "",
				prop->name,
				target->label);
		}
	}

	return 0;
}

static int fdt_list_node(struct fdt_editor_private_data *fdt,
			 struct device_node *root,
			 FILE *fp, int depth)
{
	struct device_node *dev;
	int ret = 0;

	if (!strcmp(root->name, FDT_SYMBOLS_NAME) && !fdt->keep_symbols)
		return 0;

	if (!root->is_node && list_empty(&root->child))
		return fdt_list_node_prop(fdt, root, fp, depth);

	fprintf(fp, "\n");
	fprintf(fp, "%*s", depth * 4, "");
	if (root->label[0] != '\0')
		fprintf(fp, "%s: ", root->label);
	fprintf(fp, "%s {\n", root->name);

	if (!strcmp(root->name, FDT_ALIASES_NAME) && !fdt->keep_symbols) {
		struct device_node *symbols =
			device_node_find_byname(fdt->root, FDT_SYMBOLS_NAME);

		if (symbols) {
			ret = fdt_list_alias_nodes(fdt, symbols, root,
						   fp, depth + 1);
			goto skip_childs;
		}
	}

	list_for_each_entry(dev, &root->child, head, struct device_node) {
		ret = fdt_list_node(fdt, dev, fp, depth + 1);
		if (ret < 0)
			return ret;
	}

skip_childs:
	fprintf(fp, "%*s};\n", depth * 4, "");
	return ret;
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

	if (get_verbose_level() > 0) {
		structure_print("%-20s", fdt->dtb, structure_fdt_header);
		fdt->keep_symbols = 1;
		fdt->keep_aliases = 1;
		fdt->keep_phandle = 1;
	}

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

	return fdt_list_node(fdt, root, stdout, 0);
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
	fdt_list_node(fit, fit->root, fp_its, 0);
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
