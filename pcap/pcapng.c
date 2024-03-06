/*
 * imgeditor for tcpdump/wireshark pcap file.
 *
 * qianfan Zhao 2024-03-06
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
#include "pcap_common.h"

#define PRINT_LEVEL0					"%-30s: "
#define PRINT_LEVEL1					"    %-26s: "
#define PRINT_LEVEL3					"        %-22s: "

static int pcap_little_endian = 1;
static const uint8_t pcapng_le_magic[] = { 0x4d, 0x3c, 0x2b, 0x1a };
static const uint8_t pcapng_be_magic[] = { 0x1a, 0x2b, 0x3c, 0x4d };

static uint16_t pcapng_u16_to_cpu(uint16_t n)
{
	if (pcap_little_endian)
		return le16_to_cpu(n);
	return be16_to_cpu(n);
}

static uint32_t pcapng_u32_to_cpu(uint32_t n)
{
	if (pcap_little_endian)
		return le32_to_cpu(n);
	return be32_to_cpu(n);
}

static uint64_t pcapng_u64_to_cpu(uint64_t n)
{
	if (pcap_little_endian)
		return le64_to_cpu(n);
	return be64_to_cpu(n);
}

/* pcapng: common block header file encoding for every block type */
struct pcapng_block_header {
	uint32_t	block_type;
	uint32_t	block_total_length;
};

/* pcapng: section header block file encoding */
struct pcapng_section_header_block {
	uint32_t	magic;
	uint16_t	major_version;
	uint16_t	minor_version;
	uint64_t	section_length;
};

/* pcapng: interface description block file encoding */
struct pcapng_interface_description_block {
	uint16_t	linktype;
	uint16_t	reserved;
	uint32_t	snaplen;
};

/* Interface Statistics Block without actual packet, options, and trailing
   Block Total Length */
struct pcapng_interface_statistics_block {
	uint32_t	interface_id;
	uint32_t	timestamp_high;
	uint32_t	timestamp_low;
};

/* Enhanced Packet Block without actual packet, options, and trailing
   Block Total Length */
struct pcapng_enhanced_packet_block {
	uint32_t	interface_id;
	uint32_t	timestamp_high;
	uint32_t	timestamp_low;
	uint32_t	captured_len;
	uint32_t	packet_len;
};

struct pcapng_editor_private_data {
	struct pcapng_block_header		bh;
	struct pcapng_section_header_block	shb;
};

static int pcapng_editor_detect_magic(struct pcapng_editor_private_data *p)
{
	void *magic = &p->shb.magic;

	if (!memcmp(magic, pcapng_le_magic, sizeof(pcapng_le_magic))) {
		structure_force_endian(STRUCTURE_ENDIAN_FORCE_LE);
		pcap_little_endian = 1;
		return 0;
	}

	if (!memcmp(magic, pcapng_be_magic, sizeof(pcapng_be_magic))) {
		structure_force_endian(STRUCTURE_ENDIAN_FORCE_BE);
		pcap_little_endian = 0;
		return 0;
	}

	return -1;
}

static int pcapng_detect(void *private_data, int force_type, int fd)
{
	struct pcapng_editor_private_data *p = private_data;
	int ret;

	read(fd, &p->bh, sizeof(p->bh));
	read(fd, &p->shb, sizeof(p->shb));

	if (p->bh.block_type != BLOCK_TYPE_SHB) {
		fprintf_if_force_type("Error: bad block_type %08x\n",
				      p->bh.block_type);
		return -1;
	}

	ret = pcapng_editor_detect_magic(p);
	if (ret < 0) {
		uint8_t *magic = (uint8_t *)&p->shb.magic;

		fprintf_if_force_type("Error: unknown byte order magic "
				      "%02x%02x%02x%02x\n",
				       magic[0], magic[1], magic[2], magic[3]);
		return ret;
	}

	return 0;
}

static int pcapng_read(int fd, size_t offset, void *buf, size_t sz)
{
	int ret;

	fileseek(fd, offset);
	ret = read(fd, buf, sz);

	if (ret < 0) {
		fprintf(stderr, "Error: Read at 0x%08zu failed\n", offset);
		return ret;
	} else if (ret < (int)sz) {
		fprintf(stderr, "Error: incompleted read at 0x%08zu\n", offset);
		return -1;
	}

	return 0;
}

static int pcapng_list_shb_block(int fd, size_t offset, size_t *self_sz)
{
	struct pcapng_section_header_block shb;
	int ret;

	ret = pcapng_read(fd, offset, &shb, sizeof(shb));
	if (ret < 0)
		return ret;

	printf("[SHB] Version: %d.%d\n",
		pcapng_u32_to_cpu(shb.major_version),
		pcapng_u32_to_cpu(shb.minor_version));

	*self_sz = sizeof(shb);
	return 0;
}

static int pcapng_list_idb_block(int fd, size_t offset, size_t *self_sz)
{
	struct pcapng_interface_description_block idb;
	const char *linktype_name;
	uint16_t linktype;
	int ret;

	ret = pcapng_read(fd, offset, &idb, sizeof(idb));
	if (ret < 0)
		return ret;

	linktype = pcapng_u16_to_cpu(idb.linktype);
	linktype_name = pcap_get_linktype_name(linktype);
	printf("[IDB] linktype: %d", linktype);
	if (linktype_name)
		printf(" (%s)", linktype_name);
	printf(" snaplen: %d", pcapng_u32_to_cpu(idb.snaplen));
	putchar('\n');

	*self_sz = sizeof(idb);
	return 0;
}

static int pcapng_list_isb_block(int fd, size_t offset, size_t *self_sz)
{
	struct pcapng_interface_statistics_block isb;
	int ret;

	ret = pcapng_read(fd, offset, &isb, sizeof(isb));
	if (ret < 0)
		return ret;

	printf("[ISB] interface: %d\n",
		pcapng_u32_to_cpu(isb.interface_id));
	*self_sz = sizeof(isb);
	return 0;
}

static int pcapng_list_epb_block(unsigned long id, int fd, uint32_t offset,
				 size_t *self_sz)
{
	static uint64_t first_timestamp;
	struct pcapng_enhanced_packet_block epb;
	uint64_t now_timestamp, diff;
	uint32_t packet_len;
	int ret;

	ret = pcapng_read(fd, offset, &epb, sizeof(epb));
	if (ret < 0)
		return ret;

	now_timestamp = epb.timestamp_high;
	now_timestamp <<= 32;
	now_timestamp |= epb.timestamp_low;
	packet_len = pcapng_u32_to_cpu(epb.packet_len);

	if (id == 1)
		first_timestamp = now_timestamp;

	diff = now_timestamp - first_timestamp;
	printf("[EPB] %8ld", id);
	printf(" %5d.%06d", (int)(diff / 1000000), (int)(diff % 1000000));
	printf(" %4d", packet_len);

	if (epb.interface_id)
		printf(" {%d}", epb.interface_id);
	putchar('\n');

	if (get_verbose_level() > 1) { /* dump the contains */
		uint32_t datasz = packet_len;
		uint8_t *data;

		data = malloc(datasz);
		if (!data) {
			fprintf(stderr, "Error: alloc %d bytes data failed\n",
				datasz);
			return -1;
		}

		offset += sizeof(epb);

		ret = pcapng_read(fd, offset, data, datasz);
		if (ret < 0) {
			free(data);
			return ret;
		}

		printf("================ Packet ================\n");
		hexdump(data, datasz, offset);
		free(data);
	}

	*self_sz = sizeof(epb) + ADD_PADDING(packet_len);
	return 0;
}

struct pcapng_option_handle {
	uint32_t	block_type;
	uint16_t	opt_type;
	const char	*name;
	size_t		value_length;
	int		(*handle)(const struct pcapng_option_handle *h,
				  const void *data, size_t data_sz);
};

#define OPT_HANDLE(bt, ot, h)		{	\
	.block_type	= bt,			\
	.opt_type	= ot,			\
	.name		= #ot,			\
	.handle		= h,			\
}

#define OPT_HANDLE_LEN(bt, ot, h, len)	{	\
	.block_type	= bt,			\
	.opt_type	= ot,			\
	.name		= #ot,			\
	.handle		= h,			\
	.value_length	= len,			\
}

static int pcap_opt_show_string(const struct pcapng_option_handle *h,
				const void *data, size_t data_sz)
{
	const char *s = data;

	for (size_t i = 0; i < data_sz; i++) {
		if (isprint(s[i]))
			putchar(s[i]);
		else
			printf("%%%02x", s[i]);
	}

	putchar('\n');

	return 0;
}

static int pcap_opt_show_tsresol(const struct pcapng_option_handle *h,
				 const void *data, size_t data_sz)
{
	uint8_t ts = *(const uint8_t *)data;

	printf("%d", ts);

	switch (ts) {
	case 6:
		printf(" (usec)");
		break;
	case 9:
		printf(" (nsec)");
	}

	putchar('\n');
	return 0;
}

static int pcap_opt_show_u32(const struct pcapng_option_handle *h,
			     const void *data, size_t data_sz)
{
	uint32_t n = *(const uint32_t *)data;

	n = pcapng_u32_to_cpu(n);

	printf("%d\n", n);
	return 0;
}

static int pcap_opt_show_u64(const struct pcapng_option_handle *h,
			     const void *data, size_t data_sz)
{
	uint64_t n = *(const uint64_t *)data;

	n = pcapng_u64_to_cpu(n);

	printf("%" PRIu64 "\n", n);
	return 0;
}

static int pcap_opt_show_isb_ts(const struct pcapng_option_handle *h,
				const void *data, size_t data_sz)
{
	uint64_t ts;
	uint32_t tsbuf[2];

	memcpy(tsbuf, data, sizeof(tsbuf));

	ts = pcapng_u32_to_cpu(tsbuf[0]);
	ts <<= 32;
	ts |= pcapng_u32_to_cpu(tsbuf[1]);

	printf("%" PRIu64 "\n", ts);
	return 0;
}

/* https://wiki.wireshark.org/Development/PcapNg#options-working */
static const struct pcapng_option_handle option_handlers[] = {
	OPT_HANDLE(0, OPT_COMMENT, pcap_opt_show_string),
	OPT_HANDLE(BLOCK_TYPE_SHB, OPT_SHB_USERAPPL, pcap_opt_show_string),
	OPT_HANDLE(BLOCK_TYPE_IDB, OPT_IDB_NAME, pcap_opt_show_string),
	OPT_HANDLE_LEN(BLOCK_TYPE_IDB, OPT_IDB_TSRESOL, pcap_opt_show_tsresol, 1),
	OPT_HANDLE_LEN(BLOCK_TYPE_EPB, OPT_EPB_FLAGS, pcap_opt_show_u32, 4),
	OPT_HANDLE_LEN(BLOCK_TYPE_ISB, OPT_ISB_STARTTIME, pcap_opt_show_isb_ts, 8),
	OPT_HANDLE_LEN(BLOCK_TYPE_ISB, OPT_ISB_ENDTIME, pcap_opt_show_isb_ts, 8),
	OPT_HANDLE_LEN(BLOCK_TYPE_ISB, OPT_ISB_IFRECV, pcap_opt_show_u64, 8),
	OPT_HANDLE_LEN(BLOCK_TYPE_ISB, OPT_ISB_IFDROP, pcap_opt_show_u64, 8),
	OPT_HANDLE(0, 0, NULL), /* end list */
};

static int pcapng_list_option(uint32_t block_type, struct pcap_option *opt,
			      void *opt_data)
{
	const struct pcapng_option_handle *h = option_handlers;
	uint16_t opt_type = pcapng_u16_to_cpu(opt->type);
	uint16_t length = pcapng_u16_to_cpu(opt->value_length);
	int found = 0;

	for (; h->handle; h++) {
		/* do not compare h->block_type if it is zero */
		if (h->block_type == 0 && h->opt_type == opt_type) {
			found = 1;
			break;
		}

		if (h->block_type == block_type && h->opt_type == opt_type) {
			found = 1;
			break;
		}
	}

	if (!found) {
		printf("option: %d, %d\n", opt_type, length);
		return 0;
	}

	if (h->value_length != 0 && h->value_length != length) {
		fprintf(stderr, "Error: handle option %s failed. "
			"(value length doesn't match %zu != %d)\n",
			h->name, h->value_length, length);
		return -1;
	}

	printf("        %-20s: ", h->name);

	return h->handle(h, opt_data, length);
}

static int pcapng_list_options(int fd, uint32_t offset, uint32_t block_type,
			       size_t option_sz)
{
	int hexdump_options = get_verbose_level() > 1;
	void *tmpbuf = malloc(option_sz);
	size_t option_offset = 0;
	int ret;

	if (!tmpbuf) {
		fprintf(stderr, "Error: %s alloc %zu option buffer failed\n",
			__func__, option_sz);
		return -1;
	}

	ret = pcapng_read(fd, offset, tmpbuf, option_sz);
	if (ret < 0)
		goto done;

	if (hexdump_options) {
		printf("================ Options ================\n");
		hexdump(tmpbuf, option_sz, offset);
	}

	while (1) {
		struct pcap_option *opt = tmpbuf + option_offset;
		uint16_t value_length;

		value_length = pcapng_u16_to_cpu(opt->value_length);

		if (pcapng_u16_to_cpu(opt->type) == OPT_ENDOFOPT)
			break;

		if (value_length == 0) {
			fprintf(stderr, "Error: option value_length is zero "
				"at offset 0x%08lx\n",
				offset + option_offset);
			ret = -1;
			goto done;
		}

		value_length = ADD_PADDING(value_length);
		if (option_offset + value_length >= option_sz) {
			fprintf(stderr, "Error: option overrange at offset "
					"0x%08lx\n",
				offset + option_offset);
			ret = -1;
			goto done;
		}

		ret = pcapng_list_option(block_type, opt,
					 (void *)(opt + 1));
		if (ret < 0)
			goto done;

		option_offset += sizeof(struct pcap_option);
		option_offset += value_length;
	}

done:
	if (ret < 0) {
		fprintf(stderr, "Error: list options at offset 0x%08x failed\n",
			offset);
		if (!hexdump_options)
			hexdump(tmpbuf, option_sz, offset);
	}

	free(tmpbuf);
	return ret;
}

static int pcapng_list_block(struct pcapng_editor_private_data *p, int fd)
{
	int64_t total = filelength(fd);
	struct pcapng_block_header bh;
	unsigned long epbid = 1;
	uint32_t offset = 0;

	/* pcapng block layouts
	 *
	 * [pcapng_block_header]
	 * [pcapng_xxx_block] + [block_data]
	 * [options]
	 * pcapng_block_header.block_total_length
	 */
	while (offset < total) {
		uint32_t child_offset = offset + sizeof(bh);
		uint32_t block_total_length;
		size_t child_block_sz = 0;
		int option_sz;
		int ret;

		ret = pcapng_read(fd, offset, &bh, sizeof(bh));
		if (ret < 0)
			return ret;

		block_total_length = pcapng_u32_to_cpu(bh.block_total_length);

		printf("%08X +%04d ", offset, block_total_length);

		switch (pcapng_u32_to_cpu(bh.block_type)) {
		case BLOCK_TYPE_SHB:
			ret = pcapng_list_shb_block(fd, child_offset,
						    &child_block_sz);
			break;
		case BLOCK_TYPE_IDB:
			ret = pcapng_list_idb_block(fd, child_offset,
						    &child_block_sz);
			break;
		case BLOCK_TYPE_ISB:
			ret = pcapng_list_isb_block(fd, child_offset,
						    &child_block_sz);
			break;
		case BLOCK_TYPE_EPB:
			ret = pcapng_list_epb_block(epbid, fd, child_offset,
						    &child_block_sz);
			++epbid;
			break;
		default:
			fprintf(stderr, "Error: unknown block type 0x%08x at "
				"offset 0x%08x\n",
				pcapng_u32_to_cpu(bh.block_type),
				child_offset);
			ret = -1;
			break;
		}

		/* skip all options by default */
		if (ret < 0 || get_verbose_level() == 0)
			goto next;

		option_sz = block_total_length;
		option_sz -= sizeof(bh);
		option_sz -= child_block_sz;
		option_sz -= sizeof(bh.block_total_length);
		if (option_sz == 0) /* no option */
			goto next;

		if (option_sz <= (int)sizeof(struct pcap_option)) {
			fprintf(stderr, "Error: bad option size %d\n",
				option_sz);
			return ret;
		}

		child_offset += child_block_sz;
		ret = pcapng_list_options(fd, child_offset,
					  pcapng_u32_to_cpu(bh.block_type),
					  option_sz);
	next:
		if (ret < 0)
			return ret;
		offset += block_total_length;
	}

	return 0;
}

static int pcapng_main(void *private_data, int fd, int argc, char **argv)
{
	struct pcapng_editor_private_data *p = private_data;

	return pcapng_list_block(p, fd);
}

static struct imgeditor pcapng_editor = {
	.name			= "pcapng",
	.descriptor		= "wireshark pcapng file editor",
	.header_size		= sizeof(struct pcapng_block_header) +
				  sizeof(struct pcapng_section_header_block),
	.private_data_size	= sizeof(struct pcapng_editor_private_data),
	.detect			= pcapng_detect,
	.list			= pcapng_main,
};
REGISTER_IMGEDITOR(pcapng_editor);
