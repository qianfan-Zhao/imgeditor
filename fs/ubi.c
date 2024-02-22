/*
 * ubi image editor
 * qianfan Zhao
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "imgeditor.h"
#include "ubi.h"
#include "structure.h"
#include "minilzo.h"

#define PRINT_LEVEL0					"%-30s: "
#define PRINT_LEVEL1					"    %-26s: "
#define PRINT_LEVEL3					"        %-22s: "

static void print_magic_info(uint32_t magic)
{
	switch (magic) {
	case UBI_EC_HDR_MAGIC:
		printf("(EC)");
		break;
	case UBI_VID_HDR_MAGIC:
		printf("(VID)");
		break;
	case UBIFS_NODE_MAGIC:
		printf("(UBI NODE)");
		break;
	}
}

static void structure_item_print_magic(const char *print_name_fmt,
				       const char *name,
				       const void *addr, size_t sz)
{
	__le32 le_magic = *((const __le32 *)addr);

	structure_print_name(print_name_fmt, name);
	printf("0x%08x", le32_to_cpu(le_magic));
	print_magic_info(le32_to_cpu(le_magic));
	printf("\n");
}

static void structure_item_print_be_magic(const char *print_name_fmt,
				          const char *name,
				          const void *addr, size_t sz)
{
	__be32 be_magic = *((const __be32 *)addr);

	structure_print_name(print_name_fmt, name);
	printf("0x%08x", be32_to_cpu(be_magic));
	print_magic_info(be32_to_cpu(be_magic));
	printf("\n");
}

static struct structure_item structure_ubi_ec_hdr[] = {
	STRUCTURE_ITEM(struct ubi_ec_hdr, magic,		structure_item_print_be_magic),
	STRUCTURE_ITEM(struct ubi_ec_hdr, version,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_ec_hdr, ec,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_ec_hdr, vid_hdr_offset,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_ec_hdr, data_offset,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_ec_hdr, image_seq,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_ec_hdr, hdr_crc,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM_END(),
};

static struct structure_item structure_ubi_vid_hdr[] = {
	STRUCTURE_ITEM(struct ubi_vid_hdr, magic,		structure_item_print_be_magic),
	STRUCTURE_ITEM(struct ubi_vid_hdr, version,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, vol_type,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, copy_flag,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, compat,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, vol_id,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, lnum,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, data_size,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, used_ebs,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, data_pad,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, data_crc,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, sqnum,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct ubi_vid_hdr, hdr_crc,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM_END(),
};

static struct structure_item structure_ubi_vtbl_record[] = {
	STRUCTURE_ITEM(struct ubi_vtbl_record, reserved_pebs,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct ubi_vtbl_record, alignment,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_vtbl_record, data_pad,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_vtbl_record, vol_type,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_vtbl_record, upd_marker,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct ubi_vtbl_record, name_len,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct ubi_vtbl_record, name,		structure_item_print_string),
	STRUCTURE_ITEM(struct ubi_vtbl_record, flags,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct ubi_vtbl_record, crc,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM_END(),
};

static const char *ubifs_node_short_names[] = {
	[UBIFS_INO_NODE]	= "inode node",
	[UBIFS_DATA_NODE]	= "data node",
	[UBIFS_DENT_NODE]	= "dent node",
	[UBIFS_XENT_NODE]	= "xent node",
	[UBIFS_TRUN_NODE]	= "trun node",
	[UBIFS_PAD_NODE]	= "pad node",
	[UBIFS_SB_NODE]		= "sblock node",
	[UBIFS_MST_NODE]	= "master node",
	[UBIFS_REF_NODE]	= "LEBref node",
	[UBIFS_IDX_NODE]	= "index node",
	[UBIFS_CS_NODE]		= "commit node",
	[UBIFS_ORPH_NODE]	= "orphan node",
	[UBIFS_AUTH_NODE]	= "auth node",
	[UBIFS_SIG_NODE]	= "sign node",
};

static const char *ubifs_node_names[] = {
	[UBIFS_INO_NODE]	= "inode node",
	[UBIFS_DATA_NODE]	= "data node",
	[UBIFS_DENT_NODE]	= "directory entry node",
	[UBIFS_XENT_NODE]	= "extended attribute node",
	[UBIFS_TRUN_NODE]	= "truncation node",
	[UBIFS_PAD_NODE]	= "padding node",
	[UBIFS_SB_NODE]		= "superblock node",
	[UBIFS_MST_NODE]	= "master node",
	[UBIFS_REF_NODE]	= "LEB reference node",
	[UBIFS_IDX_NODE]	= "index node",
	[UBIFS_CS_NODE]		= "commit start node",
	[UBIFS_ORPH_NODE]	= "orphan node",
	[UBIFS_AUTH_NODE]	= "authentication node",
	[UBIFS_SIG_NODE]	= "signature node",
};

static void structure_item_print_ubifs_ch_node(const char *print_name_fmt,
					       const char *name,
					       const void *addr, size_t sz)
{
	uint8_t node = *((const uint8_t *)addr);

	structure_print_name(print_name_fmt, name);
	printf("0x%02x", node);
	if (node < UBIFS_NODE_TYPES_CNT)
		printf(" (%s)", ubifs_node_names[node]);
	printf("\n");
}

static const char *ubifs_group_type_descriptors[] = {
	[UBIFS_NO_NODE_GROUP] = "this node is not part of a group",
	[UBIFS_IN_NODE_GROUP] = "this node is a part of a group",
	[UBIFS_LAST_OF_NODE_GROUP] = "this node is the last in a group",
};

static void structure_item_print_ubifs_ch_group_type(const char *print_name_fmt,
						     const char *name,
						     const void *addr, size_t sz)
{
	uint8_t group_type = *((const uint8_t *)addr);

	structure_print_name(print_name_fmt, name);
	printf("0x%02x", group_type);

	switch (group_type) {
	case UBIFS_NO_NODE_GROUP:
	case UBIFS_IN_NODE_GROUP:
	case UBIFS_LAST_OF_NODE_GROUP:
		printf(" (%s)", ubifs_group_type_descriptors[group_type]);
		break;
	}

	printf("\n");
}

static struct structure_item structure_ubifs_ch[] = {
	STRUCTURE_ITEM(struct ubifs_ch, magic,			structure_item_print_magic),
	STRUCTURE_ITEM(struct ubifs_ch, crc,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ubifs_ch, sqnum,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ch, len,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ch, node_type,		structure_item_print_ubifs_ch_node),
	STRUCTURE_ITEM(struct ubifs_ch, group_type,		structure_item_print_ubifs_ch_group_type),
	STRUCTURE_ITEM_END(),
};

static void structure_item_print_ubifs_hash_array(const char *print_name_fmt,
						  const char *name,
						  const void *addr, size_t sz)
{
	structure_print_name(print_name_fmt, name);
	for (size_t i = 0; i < sz; i++)
		printf("%02x", ((const uint8_t *)addr)[i]);
	printf("\n");
}

static void structure_item_print_ubifs_compr_type(const char *print_name_fmt,
						  const char *name,
						  const void *addr, size_t sz)
{
	uint16_t compr_type = le16_to_cpu(*((const __le16 *)addr));

	structure_print_name(print_name_fmt, name);
	printf("%02x ", compr_type);

	switch (compr_type) {
	case UBIFS_COMPR_NONE:
		printf("(NONE)");
		break;
	case UBIFS_COMPR_LZO:
		printf("(LZO)");
		break;
	case UBIFS_COMPR_ZSTD:
		printf("(ZSTD)");
		break;
	}

	printf("\n");
}

static void structure_item_print_ubifs_ino_mode(const char *print_name_fmt,
						const char *name,
						const void *addr, size_t sz)
{
	uint32_t mode = le32_to_cpu(*((const __le32 *)addr));

	structure_print_name(print_name_fmt, name);
	printf("%o\n", mode);
}

static void structure_item_print_ubifs_ino_flags(const char *print_name_fmt,
						 const char *name,
						 const void *addr, size_t sz)
{
	uint32_t flags = le32_to_cpu(*((const __le32 *)addr));

	structure_print_name(print_name_fmt, name);
	printf("0x%08x ( ", flags);

	if (flags & UBIFS_COMPR_FL)
		printf("COMPR ");
	if (flags & UBIFS_SYNC_FL)
		printf("SYNC ");
	if (flags & UBIFS_IMMUTABLE_FL)
		printf("IMMUTABLE ");
	if (flags & UBIFS_APPEND_FL)
		printf("APPEND ");
	if (flags & UBIFS_DIRSYNC_FL)
		printf("DIRSYNC ");
	if (flags & UBIFS_XATTR_FL)
		printf("XATTR ");
	if (flags & UBIFS_CRYPT_FL)
		printf("CRYPT ");
	printf(")\n");
}

static struct structure_item structure_ubifs_ino_node[] = {
	STRUCTURE_ITEM(struct ubifs_ino_node, key,		structure_item_print_ubifs_hash_array),
	STRUCTURE_ITEM(struct ubifs_ino_node, creat_sqnum,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, size,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, atime_sec,	structure_item_print_unix_epoch),
	STRUCTURE_ITEM(struct ubifs_ino_node, ctime_sec,	structure_item_print_unix_epoch),
	STRUCTURE_ITEM(struct ubifs_ino_node, mtime_sec,	structure_item_print_unix_epoch),
	STRUCTURE_ITEM(struct ubifs_ino_node, atime_nsec,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, ctime_nsec,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, mtime_nsec,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, nlink,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, uid,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, gid,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, mode,		structure_item_print_ubifs_ino_mode),
	STRUCTURE_ITEM(struct ubifs_ino_node, flags,		structure_item_print_ubifs_ino_flags),
	STRUCTURE_ITEM(struct ubifs_ino_node, data_len,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, xattr_cnt,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, xattr_size,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, xattr_names,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_ino_node, compr_type,	structure_item_print_ubifs_compr_type),

	STRUCTURE_ITEM_END(),
};

static struct structure_item structure_ubifs_data_node[] = {
	STRUCTURE_ITEM(struct ubifs_data_node, key,		structure_item_print_ubifs_hash_array),
	STRUCTURE_ITEM(struct ubifs_data_node, size,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_data_node, compr_type,	structure_item_print_ubifs_compr_type),
	STRUCTURE_ITEM(struct ubifs_data_node, compr_size,	structure_item_print_unsigned),

	STRUCTURE_ITEM_END(),
};

static void structure_item_print_ubifs_dent_name(const char *print_name_fmt,
						 const char *name,
						 const void *addr, size_t sz)
{
	const struct ubifs_dent_node *dent = addr - offsetof(struct ubifs_dent_node, name);

	structure_print_name(print_name_fmt, name);
	for (int i = 0; i < le16_to_cpu(dent->nlen); i++)
		printf("%c", dent->name[i]);
	printf("\n");
}

static const char *ubifs_itype_names[] = {
	[UBIFS_ITYPE_REG] = "regular file",
	[UBIFS_ITYPE_DIR] = "directory",
	[UBIFS_ITYPE_LNK] = "soft link",
	[UBIFS_ITYPE_BLK] = "block device",
	[UBIFS_ITYPE_CHR] = "character device",
	[UBIFS_ITYPE_FIFO] = "fifo",
	[UBIFS_ITYPE_SOCK] = "socket",
};

static const char *ubifs_itype_short_names[] = {
	[UBIFS_ITYPE_REG] = "file",
	[UBIFS_ITYPE_DIR] = "dir",
	[UBIFS_ITYPE_LNK] = "link",
	[UBIFS_ITYPE_BLK] = "blk",
	[UBIFS_ITYPE_CHR] = "chr",
	[UBIFS_ITYPE_FIFO] = "fifo",
	[UBIFS_ITYPE_SOCK] = "sock",
};

static void structure_item_print_ubifs_dent_type(const char *print_name_fmt,
						 const char *name,
						 const void *addr, size_t sz)
{
	uint8_t type = *((const uint8_t *)addr);

	structure_print_name(print_name_fmt, name);
	printf("0x%02x ", type);
	if (type < UBIFS_ITYPES_CNT)
		printf("(%s)", ubifs_itype_names[type]);
	printf("\n");
}

static struct structure_item structure_ubifs_dent_node[] = {
	STRUCTURE_ITEM(struct ubifs_dent_node, key,		structure_item_print_ubifs_hash_array),
	STRUCTURE_ITEM(struct ubifs_dent_node, inum,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_dent_node, type,		structure_item_print_ubifs_dent_type),
	STRUCTURE_ITEM(struct ubifs_dent_node, nlen,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_dent_node, name,		structure_item_print_ubifs_dent_name),

	STRUCTURE_ITEM_END(),
};

static struct structure_item structure_ubifs_mst_node[] = {
	STRUCTURE_ITEM(struct ubifs_mst_node, highest_inum,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, cmt_no,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, flags,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, log_lnum,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, root_lnum,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, root_offs,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, root_len,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, gc_lnum,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, ihead_lnum,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, ihead_offs,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, index_size,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, total_free,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, total_dirty,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, total_used,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, total_dead,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, total_dark,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, lpt_lnum,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, lpt_offs,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, ltab_offs,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, lsave_lnum,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, lsave_offs,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, lscan_lnum,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, empty_lebs,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, leb_cnt,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_mst_node, hash_root_idx,	structure_item_print_ubifs_hash_array),
	STRUCTURE_ITEM(struct ubifs_mst_node, hash_lpt,		structure_item_print_ubifs_hash_array),
	STRUCTURE_ITEM(struct ubifs_mst_node, hmac,		structure_item_print_ubifs_hash_array),

	STRUCTURE_ITEM_END(),
};

static void structure_item_print_ubifs_sb_node_flags(const char *print_name_fmt,
						     const char *name,
						     const void *addr, size_t sz)
{
	uint32_t flags = le32_to_cpu(*((const __le32 *)addr));

	structure_print_name(print_name_fmt, name);
	printf("0x%08x ( ", flags);

	if (flags & UBIFS_FLG_BIGLPT)
		printf("BITLPT ");
	if (flags & UBIFS_FLG_SPACE_FIXUP)
		printf("SPACE_FIXUP ");
	if (flags & UBIFS_FLG_DOUBLE_HASH)
		printf("DOUBLE_HASH ");
	if (flags & UBIFS_FLG_ENCRYPTION)
		printf("ENCRYPTION ");
	if (flags & UBIFS_FLG_AUTHENTICATION)
		printf("AUTHENTICATION ");
	printf(")\n");
}

static struct structure_item structure_ubifs_sb_node[] = {
	STRUCTURE_ITEM(struct ubifs_sb_node, key_hash,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, key_fmt,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, flags,		structure_item_print_ubifs_sb_node_flags),
	STRUCTURE_ITEM(struct ubifs_sb_node, min_io_size,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, leb_size,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, leb_cnt,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, max_leb_cnt,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, max_bud_bytes,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, log_lebs,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, lpt_lebs,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, orph_lebs,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, jhead_cnt,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, fanout,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, lsave_cnt,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, fmt_version,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, default_compr,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, rp_uid,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, rp_gid,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, rp_size,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, time_gran,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, uuid,		structure_item_print_x8_array),
	STRUCTURE_ITEM(struct ubifs_sb_node, ro_compat_version,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, hmac,		structure_item_print_ubifs_hash_array),
	STRUCTURE_ITEM(struct ubifs_sb_node, hmac_wkm,		structure_item_print_ubifs_hash_array),
	STRUCTURE_ITEM(struct ubifs_sb_node, hash_algo,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_sb_node, hash_mst,		structure_item_print_ubifs_hash_array),

	STRUCTURE_ITEM_END(),
};

static struct structure_item structure_ubifs_pad_node[] = {
	STRUCTURE_ITEM(struct ubifs_pad_node, pad_len,		structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

static struct structure_item structure_ubifs_cs_node[] = {
	STRUCTURE_ITEM(struct ubifs_cs_node, cmt_no,		structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

static void structure_item_print_ubifs_branch_key(const char *print_name_fmt,
						  const char *name,
						  const void *addr, size_t sz)
{
	const uint8_t *key = addr;

	structure_print_name(print_name_fmt, name);

	/* XXX: assume the length of key is always UBIFS_SK_LEN */
	for (size_t i = 0; i < UBIFS_SK_LEN; i++)
		printf("%02x", key[i]);
	printf("\n");
}

static struct structure_item structure_ubifs_branch[] = {
	STRUCTURE_ITEM(struct ubifs_branch, lnum,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_branch, offs,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ubifs_branch, len,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_branch, key,	structure_item_print_ubifs_branch_key),
	STRUCTURE_ITEM_END(),
};

static void structure_item_print_ubifs_idx_branches(const char *print_name_fmt,
						    const char *name,
						    const void *addr, size_t sz)
{
	const struct ubifs_idx_node *node = addr
			- offsetof(struct ubifs_idx_node, branches);
	const void *p_branch = addr;

	structure_print_name(print_name_fmt, name);
	printf("\n");

	for (uint32_t i = 0; i < le16_to_cpu(node->child_cnt); i++) {
		printf(PRINT_LEVEL3 "%d\n", "---branch---", i);
		structure_print(PRINT_LEVEL3, p_branch, structure_ubifs_branch);

		p_branch += sizeof(struct ubifs_branch) + UBIFS_SK_LEN;
	}
}

static struct structure_item structure_ubifs_idx_node[] = {
	STRUCTURE_ITEM(struct ubifs_idx_node, child_cnt,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_idx_node, level,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ubifs_idx_node, branches,		structure_item_print_ubifs_idx_branches),
	STRUCTURE_ITEM_END(),
};

static uint32_t ubi_crc32(void *buf, size_t sz)
{
	uint32_t crc = UBI_CRC32_INIT ^ 0xffffffff;

	return crc32(crc, buf, sz) ^ 0xffffffff;
}

#define ubi_hdr_crc(hdr)						\
	ubi_crc32((hdr), sizeof(*(hdr)) - sizeof(__be32))

#define ubi_ec_hdr_is_good(hdr)						\
(									\
	be32_to_cpu((hdr)->magic) == UBI_EC_HDR_MAGIC			\
	&& ubi_hdr_crc(hdr) == be32_to_cpu((hdr)->hdr_crc)		\
)

#define ubi_vid_hdr_is_good(hdr)					\
(									\
	be32_to_cpu((hdr)->magic) == UBI_VID_HDR_MAGIC			\
	&& ubi_hdr_crc(hdr) == be32_to_cpu((hdr)->hdr_crc)		\
)

static int ubifs_ch_is_good(struct ubifs_ch *ch, size_t leb_size);

struct ubi_bptree_branch;

struct ubi_bptree_leaf_node {
	uint32_t			key0;
	uint32_t			key1;
	uint32_t			self_lnum;
	uint32_t			self_offs;

	struct ubi_bptree_branch	*self_branch;
};

struct ubi_bptree_branch {
	uint16_t			level;
	uint16_t			child_cnt;

	struct list_head		head;
	struct ubi_bptree_leaf_node	*child_leafs;
	struct list_head		child_branches;

	struct ubi_bptree_branch	*parent;
};

static struct ubi_bptree_branch *alloc_ubi_bptree_branch(size_t max_leafs)
{
	struct ubi_bptree_branch *branch =
			malloc(sizeof(struct ubi_bptree_branch));

	if (branch) {
		memset(branch, 0, sizeof(*branch));
		list_init(&branch->head);
		list_init(&branch->child_branches);

		branch->child_leafs =
			calloc(max_leafs, sizeof(*branch->child_leafs));

		if (!branch->child_leafs) {
			fprintf(stderr, "Error: Alloc %zu leafs failed\n",
				max_leafs);
			free(branch);
			return NULL;
		}
	}

	return branch;
}

static uint64_t make_u64(uint32_t msb, uint32_t lsb)
{
	uint64_t n = msb;

	n <<= 32;
	n |= lsb;

	return n;
}

static uint64_t ubi_bptree_branch_min_key(struct ubi_bptree_branch *b)
{
	return make_u64(b->child_leafs[0].key0, b->child_leafs[0].key1);
}

static uint64_t ubi_bptree_branch_max_key(struct ubi_bptree_branch *b)
{
	struct ubi_bptree_leaf_node *last_leaf =
			&b->child_leafs[b->child_cnt] - 1;

	if (last_leaf->self_branch)
		return ubi_bptree_branch_max_key(last_leaf->self_branch);

	return make_u64(last_leaf->key0, last_leaf->key1);
}

struct cache_peb {
	int				hit;
	uint32_t			lnum; /* leb number */
	uint32_t			pnum; /* peb number */
	void				*peb;
};

#define UBI_EDITOR_CACHE_PEB_NUMBERS	8

struct ubi_editor_private_data {
	int				fd;
	size_t				peb_size;

	size_t				leb_size;
	uint32_t			vid_hdr_offset;
	uint32_t			data_offset;

	struct cache_peb		caches[UBI_EDITOR_CACHE_PEB_NUMBERS];

	struct ubifs_sb_node		sblock;
	struct ubifs_mst_node		master;

	struct ubi_bptree_branch	*root;
};

static struct cache_peb *ubi_peb_caches_find_by_lnum(struct cache_peb *arrays,
						     void *arg)
{
	uint32_t lnum = *((uint32_t *)arg);

	for (int i = 0; i < UBI_EDITOR_CACHE_PEB_NUMBERS; i++) {
		if (arrays[i].hit > 0 && arrays[i].lnum == lnum)
			return &arrays[i];
	}

	return NULL;
}

static struct cache_peb *ubi_peb_caches_find_by_pnum(struct cache_peb *arrays,
						     void *arg)
{
	uint32_t pnum = *((uint32_t *)arg);

	for (int i = 0; i < UBI_EDITOR_CACHE_PEB_NUMBERS; i++) {
		if (arrays[i].hit > 0 && arrays[i].pnum == pnum)
			return &arrays[i];
	}

	return NULL;
}

static struct cache_peb *ubi_peb_caches_find_min_hit(struct cache_peb *arrays)
{
	struct cache_peb *cache = &arrays[0];
	int hit = cache->hit;

	for (int i = 1; i < UBI_EDITOR_CACHE_PEB_NUMBERS; i++) {
		if (arrays[i].hit < hit) {
			cache = &arrays[i];
			hit = arrays[i].hit;
		}
	}

	return cache;
}

static int ubi_read_leb(struct ubi_editor_private_data *p, uint32_t lnum,
			uint32_t *ret_peb, void *buf, size_t bufsz);
static void *ubi_alloc_read_leb(struct ubi_editor_private_data *p, uint32_t lnum,
				uint32_t *ret_peb);

/* bptree layout: B means branch and F means leaf.
 *
 *          --[B]-[F]-[F]-[F]-[F]
 *          |
 * [B]-[F]-[F]-[F]-[F]
 *      |
 *      |_[B]-[F]-[F]-[F]
 */
static int ubi_bptree_load_index_node(struct ubi_editor_private_data *p,
				      uint32_t lnum, uint32_t offs,
				      struct ubi_bptree_branch *self,
				      void *tmp_peb)
{
	unsigned int max_childs = le32_to_cpu(p->sblock.fanout);
	void *tmp_leb = tmp_peb + p->data_offset;
	struct ubifs_idx_node *p_node;
	struct ubifs_branch *p_branch;
	int ret;

	ret = ubi_read_leb(p, lnum, NULL, tmp_peb, p->peb_size);
	if (ret < 0) {
		fprintf(stderr, "Error: %s read leb %d failed\n",
			__func__, lnum);
		return ret;
	}

	if (!ubifs_ch_is_good(tmp_leb, p->leb_size)) {
		fprintf(stderr, "Error: bad ubifs_ch node at %d\n", lnum);
		return -1;
	}

	p_node = tmp_leb + offs;
	p_branch = (struct ubifs_branch *)p_node->branches;

	if (le32_to_cpu(p_node->ch.magic) != UBIFS_NODE_MAGIC ||
		p_node->ch.node_type != UBIFS_IDX_NODE) {
		fprintf(stderr, "Error: %d:%08x is not index node\n",
			lnum, offs);
		return -1;
	}

	self->child_cnt = le16_to_cpu(p_node->child_cnt);
	self->level = le16_to_cpu(p_node->level);
	if (self->child_cnt > max_childs) {
		fprintf(stderr, "Error: %d:%08x has %d child\n",
			lnum, offs, self->child_cnt);
		return -1;
	}

	for (int i = 0; i < self->child_cnt; i++) {
		__le32 *key = (__le32 *)p_branch->key;
		struct ubi_bptree_leaf_node *leaf = &self->child_leafs[i];

		leaf->key0 = le32_to_cpu(key[0]);
		leaf->key1 = le32_to_cpu(key[1]);

		leaf->self_lnum = le32_to_cpu(p_branch->lnum);
		leaf->self_offs = le32_to_cpu(p_branch->offs);

		p_branch = (void *)p_branch +
				sizeof(struct ubifs_branch) + UBIFS_SK_LEN;
	}

	return 0;
}

static int ubi_bptree_load(struct ubi_editor_private_data *p,
			   uint32_t lnum, uint32_t offs,
			   struct ubi_bptree_branch *self,
			   struct ubi_bptree_branch *parent,
			   void *tmp_peb)
{
	int ret;

	self->parent = parent;
	ret = ubi_bptree_load_index_node(p, lnum, offs, self, tmp_peb);
	if (ret < 0)
		return ret;

	/* level0 doesn't has any child branch */
	if (self->level == 0)
		return ret;

	/* load all childs */
	for (int i = 0; i < self->child_cnt; i++) {
		struct ubi_bptree_leaf_node *leaf = &self->child_leafs[i];
		struct ubi_bptree_branch *child;

		child = alloc_ubi_bptree_branch(le32_to_cpu(p->sblock.fanout));
		if (!child) {
			fprintf(stderr, "Error: alloc ubi bptree branch failed\n");
			return -1;
		}

		ret = ubi_bptree_load(p, leaf->self_lnum, leaf->self_offs,
				      child, self, tmp_peb);
		if (ret < 0)
			break;

		leaf->self_branch = child;
		list_add_tail(&child->head, &self->child_branches);
	}

	return ret;
}

static int ubi_bptree_init(struct ubi_editor_private_data *p)
{
	uint32_t lnum_sb_mst[] = { UBIFS_SB_LNUM, UBIFS_MST_LNUM };
	void *tmp_peb;
	int ret;

	tmp_peb = malloc(p->peb_size);
	if (!tmp_peb) {
		fprintf(stderr, "Error: %s malloc peb failed\n",
			__func__);
		return -1;
	}

	/* read sblock and master */
	for (int i = 0; i < 2; i++) {
		struct ubifs_ch *ch;

		ret = ubi_read_leb(p, lnum_sb_mst[i], NULL, tmp_peb, p->peb_size);
		if (ret < 0)
			return ret;

		ch = tmp_peb + p->data_offset;
		if (!ubifs_ch_is_good(ch, p->leb_size)) {
			fprintf(stderr, "Error: bad ubifs_ch on leb %d\n",
				lnum_sb_mst[i]);
			return -1;
		}

		switch (i) {
		case 0:
			memcpy(&p->sblock, ch, sizeof(p->sblock));
			break;
		case 1:
			memcpy(&p->master, ch, sizeof(p->master));
			break;
		}
	}

	p->root = alloc_ubi_bptree_branch(le16_to_cpu(p->sblock.fanout));
	if (!p->root) {
		fprintf(stderr, "Error: malloc root branch failed\n");
		ret = -1;
		goto done;
	}

	ret = ubi_bptree_load(p,
			      le32_to_cpu(p->master.root_lnum),
			      le32_to_cpu(p->master.root_offs),
			      p->root, NULL, tmp_peb);
done:
	free(tmp_peb);
	return ret;
}

static void ubi_snprint_key(char *buf, size_t bufsz, uint32_t key0, uint32_t key1)
{
	uint32_t keytype = (key1 >> UBIFS_S_KEY_BLOCK_BITS) & 0x7;

	buf[0] = '\0';

	key1 &= UBIFS_S_KEY_BLOCK_MASK;
	switch (keytype) {
	case UBIFS_INO_KEY:
		/* ref: ubifs-utils: ino_key_init */
		snprintf(buf, bufsz, "INO  %8d", key0);
		break;
	case UBIFS_DATA_KEY:
		/* ref: ubifs-utils: data_key_init
		 * key0: inode number
		 * key1: block number
		 */
		snprintf(buf, bufsz, "DATA %8d block: %08d", key0, key1);
		break;
	case UBIFS_DENT_KEY:
		/* ref: ubifs-utils: dent_key_init
		 * key0: parent inode number
		 */
		snprintf(buf, bufsz, "DENT ->%06d hash:  0x%08x", key0, key1);
		break;
	case UBIFS_XENT_KEY:
		/* ref: ubifs-utils: xent_key_init */
		snprintf(buf, bufsz, "XENT %8d hash:  0x%08x", key0, key1);
		break;
	default:
		snprintf(buf, bufsz, "0x%08x 0x%08x", key0, key1);
		break;
	}
}

static uint64_t ubi_leaf_make_key(uint8_t keytype, uint32_t key0, uint32_t hash)
{
	uint64_t key = key0;

	hash &= UBIFS_S_KEY_BLOCK_MASK;
	keytype &= 0x7;

	key <<= 32;
	key |= ((keytype << UBIFS_S_KEY_BLOCK_BITS) | hash); /* key1 */

	return key;
}

/* print file related(inode, dent, data) node */
static void ubi_print_file_node(struct ubi_editor_private_data *p,
				uint32_t lnum, uint32_t offs)
{
	struct ubifs_ino_node *ino = NULL;
	struct ubifs_dent_node *dent = NULL;
	struct ubifs_data_node *data = NULL;
	struct ubifs_ch *ch;
	void *peb, *leb;
	int max_print_sz;

	peb = ubi_alloc_read_leb(p, lnum, NULL);
	if (!peb)
		return;

	leb = peb + p->data_offset;
	ch = leb + offs;
	if (ubifs_ch_is_good(ch, p->leb_size)) {
		switch (ch->node_type) {
		case UBIFS_INO_NODE:
			ino = (struct ubifs_ino_node *)ch;
			printf("[%o]", le32_to_cpu(ino->mode));
			break;
		case UBIFS_DENT_NODE:
			dent = (struct ubifs_dent_node *)ch;
			printf("[inode=%-4" PRIu64 "%s %s]",
				le64_to_cpu(dent->inum),
				dent->type < UBIFS_ITYPES_CNT
					? ubifs_itype_short_names[dent->type]
					: "??",
				dent->name);
			break;
		case UBIFS_DATA_NODE:
			data = (struct ubifs_data_node *)ch;
			max_print_sz = le32_to_cpu(data->size);
			if (max_print_sz > 8)
				max_print_sz = 8;

			printf("[size=%-5d ", le32_to_cpu(data->size));
			for (int loop = 0; loop < 2; loop++) {
				for (int i = 0; i < max_print_sz; i++) {
					switch (loop) {
					case 0:
						printf("%02x", data->data[i]);
						break;
					default:
						if (isprint(data->data[i]))
							putchar(data->data[i]);
						else
							putchar('.');
						break;
					}
				}
				printf("%s", loop == 0 ? "(" : ")");
			}
			printf("]");
			break;
		}
	}

	free(peb);
}

static void ubi_print_leaf_node(struct ubi_editor_private_data *p,
				int show_filenode,
				struct ubi_bptree_leaf_node *leaf)
{
	char buf[128];

	if (leaf->self_branch) /* it is a branch, not leaf node */
		return;

	ubi_snprint_key(buf, sizeof(buf), leaf->key0, leaf->key1);
	printf("%-32s ", buf);

	if (show_filenode)
		ubi_print_file_node(p, leaf->self_lnum, leaf->self_offs);
}

static void ubi_bptree_print_branch(struct ubi_editor_private_data *p,
				    int show_filenode,
				    struct ubi_bptree_branch *branch, int level)
{
	int leaf_idx = 0;

	for (int i = 0; i < branch->child_cnt; i++) {
		struct ubi_bptree_leaf_node *leaf = &branch->child_leafs[i];

		for (int i = 0; i < level; i++) {
			int split = ' ';

			if (i == level - 1)
				split = '-';

			for (int j = 0; j < 11; j++)
				putchar(split);
			if (i == level - 1) {
				printf("%02d/%02d",
					++leaf_idx, branch->child_cnt);
			} else {
				for (size_t j = 0; j < strlen("01/02"); j++)
					putchar(split);
			}
			for (int j = 0; j < 11; j++)
				putchar(split);

			printf("|");
		}
		printf("%04d:%05x:%08x%08x",
			leaf->self_lnum, leaf->self_offs,
			leaf->key0, leaf->key1);

		if (!leaf->self_branch) {
			putchar(' ');
			ubi_print_leaf_node(p, show_filenode, leaf);
		}
		printf("\n");

		if (leaf->self_branch)
			ubi_bptree_print_branch(p, show_filenode, leaf->self_branch, level + 1);
	}
}

static int ubi_do_bptree(struct ubi_editor_private_data *p, int argc, char **argv)
{
	struct ubi_bptree_branch *root = p->root;
	int show_filenode = 0;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--show-filenode"))
			show_filenode = 1;
	}

	ubi_bptree_print_branch(p, show_filenode, root, 1);
	return 0;
}

static void ubi_bptree_free_branch(struct ubi_bptree_branch *branch)
{
	if (list_empty(&branch->child_branches)) { /* no child branch, free self */
		free(branch->child_leafs);
#if 0
		printf("free branch level %d with %d childs\n",
			branch->level, branch->child_cnt);
#endif
		/* remove self from parent's link */
		list_del(&branch->head);
		free(branch);
	} else {
		struct ubi_bptree_branch *b, *next;

		/* free all the child branch */
		list_for_each_entry_safe(b, next, &branch->child_branches,
					 head, struct ubi_bptree_branch)
			ubi_bptree_free_branch(b);
	}
}

static int ubi_editor_alloc_cache_pebs(struct ubi_editor_private_data *p)
{
	for (int i = 0; i < UBI_EDITOR_CACHE_PEB_NUMBERS; i++) {
		struct cache_peb *cache = &p->caches[i];

		memset(cache, 0, sizeof(*cache));
		cache->peb = malloc(p->peb_size);
		if (!cache->peb)
			return -1;
	}

	return 0;
}

static int ubi_editor_init(void *private_data)
{
	struct ubi_editor_private_data *p = private_data;

	for (int i = 0; i < UBI_EDITOR_CACHE_PEB_NUMBERS; i++)
		memset(&p->caches[i], 0, sizeof(struct cache_peb));

	return 0;
}

static int ubi_detect(void *private_data, int force_type, int fd)
{
	size_t peb_size_auto_detect[] = {
			SIZE_KB(128),	SIZE_KB(256),	SIZE_KB(512),
			SIZE_MB(1),	SIZE_MB(2),	SIZE_MB(4),
			0 };
	struct ubi_editor_private_data *p = private_data;
	struct ubi_ec_hdr ec_hdr;
	int ret;

	static_assert(sizeof(struct ubi_ec_hdr) == 64, "sizeof(ec_hdr)");
	static_assert(sizeof(struct ubi_vid_hdr) == 64, "sizeof(vid_hdr)");

	lseek64(fd, 0, SEEK_SET);
	ret = read(fd, &ec_hdr, sizeof(ec_hdr));
	if (ret < 0)
		return ret;

	if (!ubi_ec_hdr_is_good(&ec_hdr)) {
		fprintf_if_force_type("Error: EC_HDR from PEB0 is bad\n");
		return -1;
	}

	/* auto detect the peb size, ubi_ec_hdr is always the first data
	 * structure in PEB.
	 */
	p->peb_size = 0;
	for (size_t i = 0; peb_size_auto_detect[i] > 0; i++) {
		lseek64(fd, peb_size_auto_detect[i], SEEK_SET);

		ret = read(fd, &ec_hdr, sizeof(ec_hdr));
		if (ret < 0)
			return ret;

		if (ret != (int)sizeof(ec_hdr)) {
			fprintf_if_force_type("Error: read EC_HDR from offset "
					      "%08lx failed\n",
					      (unsigned long)peb_size_auto_detect[i]);
			return -1;
		}

		if (ubi_ec_hdr_is_good(&ec_hdr)) {
			p->peb_size = peb_size_auto_detect[i];
			break;
		}
	}

	if (p->peb_size == 0) {
		fprintf_if_force_type("Error: can't detect the peb size\n");
		return -1;
	}

	ret = ubi_editor_alloc_cache_pebs(p);
	if (ret < 0)
		return ret;

	p->vid_hdr_offset = be32_to_cpu(ec_hdr.vid_hdr_offset);
	p->data_offset = be32_to_cpu(ec_hdr.data_offset);
	p->leb_size = p->peb_size - p->data_offset;
	p->fd = fd;

	return ubi_bptree_init(p);
}

static void ubi_editor_exit(void *private_data)
{
	struct ubi_editor_private_data *p = private_data;
	struct ubi_bptree_branch *root = p->root;

	if (root)
		ubi_bptree_free_branch(root);

	for (int i = 0; i < UBI_EDITOR_CACHE_PEB_NUMBERS; i++)
		free(p->caches[i].peb);
}

static int ubi_peb_is_empty(void *buf, size_t bufsz)
{
	uint8_t *p = buf;

	for (size_t i = 0; i < bufsz; i++) {
		if (p[i] != 0xff)
			return 0; /* false */
	}

	return 1; /* yes, it is empty */
}

#define UBI_READ_PEB_FAILED		-1
#define UBI_READ_PEB_INCOMPLETE		-2
#define UBI_READ_PEB_FF			-3
#define UBI_READ_PEB_BAD_ECHDR		-4

static int ubi_read_peb(struct ubi_editor_private_data *p, uint32_t peb,
			void *buf, size_t bufsz)
{
	struct cache_peb *cache = ubi_peb_caches_find_by_pnum(p->caches, &peb);
	int ret, fd = p->fd;

	if (!cache) {
		struct ubi_ec_hdr *ec_hdr;
		struct ubi_vid_hdr *vid_hdr;

		cache = ubi_peb_caches_find_min_hit(p->caches);

		/* this cache is changed, clean the hit number */
		cache->hit = 0;

		lseek64(fd, peb * p->peb_size, SEEK_SET);

		ret = read(fd, cache->peb, p->peb_size);
		if (ret < 0)
			return UBI_READ_PEB_FAILED;

		if (ret != (int)p->peb_size)
			return UBI_READ_PEB_INCOMPLETE;

		if (ubi_peb_is_empty(cache->peb, p->peb_size))
			return UBI_READ_PEB_FF;

		ec_hdr = cache->peb;
		vid_hdr = cache->peb + p->vid_hdr_offset;

		if (!ubi_ec_hdr_is_good(ec_hdr))
			return UBI_READ_PEB_BAD_ECHDR;

		cache->pnum = peb;
		cache->lnum = be32_to_cpu(vid_hdr->lnum);
		++cache->hit;
	} else {
		++cache->hit;
	}

	memcpy(buf, cache->peb, bufsz);
	return 0;
}

static int ubi_read_leb(struct ubi_editor_private_data *p, uint32_t lnum,
			uint32_t *ret_peb, void *buf, size_t bufsz)
{
	struct cache_peb *cache;
	uint32_t peb = 2; /* PEB0 abd PEB1 is used for vtbl, skip them */
	struct ubi_vid_hdr *vid_hdr;
	struct ubi_ec_hdr *ec_hdr;
	int ret;

	/* try the cached first */
	cache = ubi_peb_caches_find_by_lnum(p->caches, &lnum);
	if (cache) {
		if (ret_peb)
			*ret_peb = cache->pnum;
		memcpy(buf, cache->peb, bufsz);
		return 0;
	}

	do {
		/* read the ec_hdr only to speedup */
		ret = ubi_read_peb(p, peb, buf, sizeof(*ec_hdr));
		switch (ret) {
		case UBI_READ_PEB_FAILED:
		case UBI_READ_PEB_INCOMPLETE:
			/* not found, exit */
			break;
		case UBI_READ_PEB_FF:
		case UBI_READ_PEB_BAD_ECHDR:
			/* continue search */
			ret = 0;
			break;
		case 0:
			/* read the whole block */
			ret = ubi_read_peb(p, peb, buf, bufsz);

			ec_hdr = buf;
			vid_hdr = buf + be32_to_cpu(ec_hdr->vid_hdr_offset);
			if (be32_to_cpu(vid_hdr->lnum) == lnum) {
				if (ret_peb)
					*ret_peb = peb;
				return ret;
			}
			break;
		}

		peb++;
	} while (ret == 0);

	return -1; /* the matched PEB is not found */
}

static void *ubi_alloc_read_peb(struct ubi_editor_private_data *p, uint32_t peb,
				size_t n_peb)
{
	size_t bufsz = n_peb * p->peb_size;
	void *buf;

	buf = malloc(bufsz);
	if (!buf)
		return buf;

	if (ubi_read_peb(p, peb, buf, bufsz) < 0) {
		free(buf);
		return NULL;
	}

	return buf;
}

static void *ubi_alloc_read_leb(struct ubi_editor_private_data *p, uint32_t lnum,
				uint32_t *ret_peb)
{
	void *buf = malloc(p->peb_size);

	if (!buf)
		return buf;

	if (ubi_read_leb(p, lnum, ret_peb, buf, p->peb_size) < 0) {
		free(buf);
		return NULL;
	}

	return buf;
}

static struct ubifs_ch *ubi_alloc_read_ch(struct ubi_editor_private_data *p,
					  uint32_t lnum, uint32_t offset,
					  void **ret_peb)
{
	struct ubifs_ch *ch;
	void *peb, *leb;

	peb = ubi_alloc_read_leb(p, lnum, NULL);
	if (!peb) {
		fprintf(stderr, "Error: read leb %u failed\n", lnum);
		return NULL;
	}

	leb = peb + p->data_offset;
	ch = leb + offset;

	if (!ubifs_ch_is_good(ch, p->leb_size)) {
		fprintf(stderr, "Error: ubifs ch %u:%u is bad\n", lnum, offset);
		free(peb);
		return NULL;
	}

	*ret_peb = peb;
	return ch;
}

static void ubi_dump_peb_hdr(uint32_t pebno, void *peb)
{
	struct ubi_vid_hdr *vid_hdr;
	struct ubi_ec_hdr *ec_hdr;

	ec_hdr = peb;
	vid_hdr = peb + be32_to_cpu(ec_hdr->vid_hdr_offset);

	printf("EC_HDR of PEB#%d:\n", pebno);
	structure_print(PRINT_LEVEL1, ec_hdr, structure_ubi_ec_hdr);

	printf("VID_HDR of PEB#%d:\n", pebno);
	structure_print(PRINT_LEVEL1, vid_hdr, structure_ubi_vid_hdr);
}

static int ubi_do_peb(struct ubi_editor_private_data *p, int argc, char **argv)
{
	int pebno = -1;
	void *peb;

	if (argc > 1)
		pebno = (int)strtol(argv[1], NULL, 0);

	if (pebno < 0) {
		fprintf(stderr, "Usage: ubi peb #peb\n");
		return -1;
	}

	peb = ubi_alloc_read_peb(p, pebno, 1);
	if (!peb)
		return -1;

	ubi_dump_peb_hdr(pebno, peb);
	hexdump(peb + p->data_offset, p->leb_size,
		pebno * p->peb_size + p->data_offset);
	free(peb);
	return 0;
}

static int ubi_do_leb(struct ubi_editor_private_data *p, int argc, char **argv)
{
	uint32_t pebno = 0;
	int lnum = -1, offset = 0, size = p->leb_size;
	void *peb;

	if (argc > 1)
		lnum = (int)strtol(argv[1], NULL, 0);

	if (lnum < 0) {
	usage:
		fprintf(stderr, "Usage: ubi leb #lnum [leb_offset size]\n");
		return -1;
	}

	if (argc > 2) {
		offset = strtol(argv[2], NULL, 0); /* auto detect the offset */
		if (offset < 0)
			goto usage;

		if (offset >= (int)p->leb_size) {
			fprintf(stderr, "Error: the leb offset overflow\n");
			return -1;
		}
	}

	if (argc > 3) {
		size = strtol(argv[3], NULL, 0); /* auto detect the size */
		if (size < 0)
			goto usage;

		if (offset + size >= (int)p->leb_size) {
			fprintf(stderr, "Error: the leb size overflow\n");
			return -1;
		}
	} else {
		size = p->leb_size - offset;
	}

	peb = ubi_alloc_read_leb(p, lnum, &pebno);
	if (!peb) {
		fprintf(stderr, "Error: PEB is not found\n");
		return -1;
	}

	ubi_dump_peb_hdr(pebno, peb);
	hexdump(peb + p->data_offset + offset, size, offset);

	free(peb);
	return 0;
}

static int ubi_do_vtbl(struct ubi_editor_private_data *p, int argc, char **argv)
{
	size_t max_record;
	struct ubi_ec_hdr *ec_hdr;
	struct ubi_vtbl_record *vtbls;
	void *peb;
	int ret = 0;

	peb = ubi_alloc_read_peb(p, 0, 1); /* vtbl_record always in PEB0 */
	if (!peb)
		return -1;

	ec_hdr = peb;
	vtbls = peb + be32_to_cpu(ec_hdr->data_offset);

	max_record = (p->peb_size - be32_to_cpu(ec_hdr->data_offset))
			/ sizeof(*vtbls);
	if (max_record > UBI_MAX_VOLUMES)
		max_record = UBI_MAX_VOLUMES;

	for (size_t i = 0; i < max_record; i++) {
		struct ubi_vtbl_record *vtbl = &vtbls[i];

		if (ubi_hdr_crc(vtbl) != be32_to_cpu(vtbl->crc)) {
			fprintf(stderr, "Error: vtbl %zu has bad CRC\n", i);
			ret = -1;
			break;
		}

		/* skip empty vtbl */
		if (be32_to_cpu(vtbl->reserved_pebs) == 0
			&& be32_to_cpu(vtbl->alignment) == 0
			&& be32_to_cpu(vtbl->data_pad) == 0
			&& be16_to_cpu(vtbl->name_len) == 0)
			continue;

		printf("volume table record #%zu:\n", i);
		structure_print(PRINT_LEVEL1, vtbl, structure_ubi_vtbl_record);
	}

	free(peb);
	return ret;
}

static int ubifs_ch_is_good(struct ubifs_ch *ch, size_t leb_size)
{
	if (le32_to_cpu(ch->magic) != UBIFS_NODE_MAGIC)
		return 0; /* false */

	if (le32_to_cpu(ch->len) + sizeof(*ch) > leb_size)
		return 0; /* false */

	return le32_to_cpu(ch->crc) ==
		ubi_crc32((void *)ch + 8, le32_to_cpu(ch->len) - 8);
}

static int ubi_print_node_verbose(struct ubifs_ch *ch, unsigned long leb_offset, int idx)
{
	printf(PRINT_LEVEL0 "0x%08lx\n", "Node offset", leb_offset);
	structure_print(PRINT_LEVEL0, ch, structure_ubifs_ch);

	switch (ch->node_type) {
	case UBIFS_MST_NODE:
		structure_print(PRINT_LEVEL1, ch, structure_ubifs_mst_node);
		break;
	case UBIFS_PAD_NODE:
		structure_print(PRINT_LEVEL1, ch, structure_ubifs_pad_node);
		break;
	case UBIFS_CS_NODE:
		structure_print(PRINT_LEVEL1, ch, structure_ubifs_cs_node);
		break;
	case UBIFS_IDX_NODE:
		structure_print(PRINT_LEVEL1, ch, structure_ubifs_idx_node);
		break;
	case UBIFS_SB_NODE:
		structure_print(PRINT_LEVEL1, ch, structure_ubifs_sb_node);
		break;
	case UBIFS_DATA_NODE:
		structure_print(PRINT_LEVEL1, ch, structure_ubifs_data_node);
		break;
	case UBIFS_INO_NODE:
		structure_print(PRINT_LEVEL1, ch, structure_ubifs_ino_node);

		if (get_verbose_level() >= 0) { /* always print */
			struct ubifs_ino_node *inode =
				(struct ubifs_ino_node *)ch;
			size_t data_len = (size_t)le64_to_cpu(inode->data_len);

			if (data_len)
				hexdump(inode->data, data_len, 0);
		}
		break;
	case UBIFS_DENT_NODE:
		structure_print(PRINT_LEVEL1, ch, structure_ubifs_dent_node);
		break;
	default:
		fprintf(stderr, "Error: unsupported node type 0x%02x\n",
			ch->node_type);
		return -1;
	}

	return 0;
}

static int ubi_print_node_brief(struct ubifs_ch *ch, unsigned long leb_offset, int idx)
{
	if (!(ch->node_type < UBIFS_NODE_TYPES_CNT)) {
		fprintf(stderr, "Error: unknown node type 0x%02x\n",
			ch->node_type);
		return -1;
	}

	printf("[%-12s %8" PRIu64 "]: leb_offs: 0x%08lx ",
		ubifs_node_short_names[ch->node_type], le64_to_cpu(ch->sqnum),
		leb_offset);

	switch (ch->node_type) {
	case UBIFS_MST_NODE: {
		struct ubifs_mst_node *mst = (struct ubifs_mst_node *)ch;

		printf("\troot_lnum: %-8d root_offs: 0x%08x root_len: %-8d\n",
			le32_to_cpu(mst->root_lnum),
			le32_to_cpu(mst->root_offs),
			le32_to_cpu(mst->root_len));
	}
		break;
	case UBIFS_PAD_NODE: {
		struct ubifs_pad_node *pad = (struct ubifs_pad_node *)ch;

		printf("\tpad_len: %-8d\n", le32_to_cpu(pad->pad_len));
	}
		break;
	case UBIFS_CS_NODE: {
		struct ubifs_cs_node *cs = (struct ubifs_cs_node *)ch;

		printf("\tcmt_no: %-8d\n", le32_to_cpu(cs->cmt_no));
	}
		break;
	case UBIFS_IDX_NODE: {
		struct ubifs_idx_node *idx = (struct ubifs_idx_node *)ch;
		void *p_branch = idx->branches;

		printf("\tchild_cnt: %-2d level: %-4d\n",
			le16_to_cpu(idx->child_cnt), le16_to_cpu(idx->level));

		for (int i = 0; i < le16_to_cpu(idx->child_cnt); i++) {
			struct ubifs_branch *branch = p_branch;
			const __le32 *key = (const __le32 *)branch->key;
			char buf[128];

			/* the sizeof(branch->key) is UBIFS_SK_LEN
			 * ref: ubifs-utils: ino_key_init, dent_key_init
			 */
			uint32_t key0 = le32_to_cpu(key[0]), key1 = le32_to_cpu(key[1]);

			printf("\t[%02d] lnum: %-8d offs: 0x%08x len: %-8d key: ",
				i,
				le32_to_cpu(branch->lnum),
				le32_to_cpu(branch->offs),
				le32_to_cpu(branch->len)
				);

			ubi_snprint_key(buf, sizeof(buf), key0, key1);
			printf("%s\n", buf);
			p_branch += sizeof(struct ubifs_branch) + UBIFS_SK_LEN;
		}
	}
		break;
	case UBIFS_SB_NODE: {
		struct ubifs_sb_node *sb = (struct ubifs_sb_node *)ch;

		printf("\tmin_io_size: %d leb_size: %d leb_cnt: %d\n",
			le32_to_cpu(sb->min_io_size),
			le32_to_cpu(sb->leb_size),
			le32_to_cpu(sb->leb_cnt));
	}
		break;
	case UBIFS_INO_NODE: {
		struct ubifs_ino_node *ino = (struct ubifs_ino_node *)ch;

		printf("\tmode: %05o size: %" PRIu64 "\n",
			le32_to_cpu(ino->mode),
			le64_to_cpu(ino->size));
	};
		break;
	case UBIFS_DENT_NODE: {
		struct ubifs_dent_node *dent = (struct ubifs_dent_node *)ch;

		printf("\t%.*s type: %s\n",
			le16_to_cpu(dent->nlen),
			dent->name,
			ubifs_itype_names[dent->type]);
	}
		break;
	case UBIFS_DATA_NODE: {
		struct ubifs_data_node *data = (struct ubifs_data_node *)ch;

		printf("\tdata size: %-8d compr_size: %-8d (%scompressed)\n",
			le32_to_cpu(data->size),
			le16_to_cpu(data->compr_size),
			le16_to_cpu(data->compr_type) == UBIFS_COMPR_NONE ? "un" : "");
	}
		break;
	default:
		printf("\tunsupported now\n");
		break;
	}

	return 0;
}

static int _ubi_do_node(struct ubi_editor_private_data *p, int lnum)
{
	void *peb = ubi_alloc_read_leb(p, lnum, NULL);
	void *leb = peb + p->data_offset;
	void *node = leb;
	int idx = 0, ret = 0;

	if (!peb)
		return -1;

	if (!ubifs_ch_is_good(node, p->leb_size)) {
		fprintf(stderr, "Error: LEB %d doesn't contain any node\n",
			lnum);
		ret = -1;
		goto done;
	}

	while (1) {
		struct ubifs_ch *ch = node;

		if (le32_to_cpu(ch->magic) != UBIFS_NODE_MAGIC)
			break;

		if (!ubifs_ch_is_good(ch, p->leb_size)) {
			fprintf(stderr, "Error: ubifs_ch is bad\n");
			ret = -1;
			break;
		}

		if (get_verbose_level() > 0) {
			if (idx != 0)
				printf("\n");
			ret = ubi_print_node_verbose(ch, node - leb, idx);
		} else {
			ret = ubi_print_node_brief(ch, node - leb, idx);
		}

		if (ret < 0)
			break;

		node += aligned_length(le32_to_cpu(ch->len), 8);
		idx++;
	}

done:
	free(peb);
	return ret;
}

static int ubi_do_node(struct ubi_editor_private_data *p, int argc, char **argv)
{
	int lnum = -1;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "sblock"))
			lnum = UBIFS_SB_LNUM;
		else if (!strcmp(argv[i], "master"))
			lnum = UBIFS_MST_LNUM;
		else if (!strcmp(argv[i], "log"))
			lnum = UBIFS_LOG_LNUM;
		else
			lnum = (int)strtol(argv[i], NULL, 0);
	}

	if (lnum < 0) {
		fprintf(stderr, "Usage: ubi node #lnum\n");
		return -1;
	}

	return _ubi_do_node(p, lnum);
}

static struct ubi_bptree_leaf_node *
ubi_bptree_branch_find_leaf(struct ubi_bptree_branch *b, uint64_t key_min,
			    uint64_t key_max)
{
	for (int i = 0; i < b->child_cnt; i++) {
		struct ubi_bptree_leaf_node *leaf = &b->child_leafs[i];
		uint64_t key = make_u64(leaf->key0, leaf->key1);

		if (key_min == key_max && key_min == key)
			return leaf;

		if (key >= key_min && key <= key_max)
			return leaf;
	}

	return NULL;
}

static struct ubi_bptree_leaf_node *
ubi_bptree_find(struct ubi_bptree_branch *root, uint8_t keytype, uint32_t key0,
		uint32_t hash_min, uint32_t hash_max)
{
	uint64_t key_min = ubi_leaf_make_key(keytype, key0, hash_min);
	uint64_t key_max = ubi_leaf_make_key(keytype, key0, hash_max);
	struct ubi_bptree_branch *b;

	if (list_empty(&root->child_branches))
		return ubi_bptree_branch_find_leaf(root, key_min, key_max);

	list_for_each_entry(b, &root->child_branches, head,
			    struct ubi_bptree_branch) {
		struct ubi_bptree_leaf_node *leaf;
		uint64_t branch_min, branch_max;

		branch_min = ubi_bptree_branch_min_key(b);
		branch_max = ubi_bptree_branch_max_key(b);

		/* clear hash/block part */
		branch_min &= ~UBIFS_S_KEY_HASH_MASK;

		if (branch_min > key_min)
			return NULL; /* not found */
		if (branch_max < key_min)
			continue; /* search next */

		if (!list_empty(&b->child_branches)) {
			return ubi_bptree_find(b, keytype, key0,
					       hash_min, hash_max);
		}

		leaf = ubi_bptree_branch_find_leaf(b, key_min, key_max);
		if (leaf)
			return leaf;
	}

	return NULL;
}

static struct ubifs_ino_node *
	ubi_alloc_read_inode(struct ubi_editor_private_data *p, uint32_t ino,
			     void **ret_peb)
{
	struct ubi_bptree_leaf_node *leaf;
	struct ubifs_ch *ch;

	leaf = ubi_bptree_find(p->root, UBIFS_INO_KEY, ino, 0, 0);
	if (!leaf) {
		fprintf(stderr, "Error: can not found UBIFS_INO_NODE %u\n",
			ino);
		return NULL;
	}

	ch = ubi_alloc_read_ch(p, leaf->self_lnum, leaf->self_offs, ret_peb);
	if (!ch)
		return NULL;

	if (ch->node_type != UBIFS_INO_NODE) {
		fprintf(stderr, "Error: node %u:%u is not INO_NODE\n",
			leaf->self_lnum, leaf->self_offs);
		free(*ret_peb);
		return NULL;
	}

	return (struct ubifs_ino_node *)ch;
}

/*
 * Read a data node where the @min_blk_idx <= blk_idx <= @max_blk_idx,
 * And save it's blk_idx to @ret_blk_idx.
 */
static struct ubifs_data_node *
	ubi_alloc_read_data_node(struct ubi_editor_private_data *p,
				 uint32_t ino, uint32_t min_blk_idx,
				 uint32_t max_blk_idx,
				 uint32_t *ret_blk_idx,
				 void **ret_peb)
{
	struct ubi_bptree_leaf_node *leaf;
	struct ubifs_ch *ch;

	leaf = ubi_bptree_find(p->root, UBIFS_DATA_KEY, ino, min_blk_idx,
				max_blk_idx);
	if (!leaf)
		return NULL;

	*ret_blk_idx = leaf->key1 & UBIFS_S_KEY_BLOCK_MASK;

	ch = ubi_alloc_read_ch(p, leaf->self_lnum, leaf->self_offs, ret_peb);
	if (!ch)
		return NULL;

	if (ch->node_type != UBIFS_DATA_NODE) {
		fprintf(stderr, "Error: node %u:%u is not DATA_NODE\n",
			leaf->self_lnum, leaf->self_offs);
		free(*ret_peb);
		return NULL;
	}

	return (struct ubifs_data_node *)ch;
}

static ssize_t ubi_data_node_decompress_none(struct ubifs_data_node *dnode,
					     uint8_t *decompress_buf,
					     size_t decompress_buf_sz)
{
	size_t decompress_sz = le32_to_cpu(dnode->size);

	if (decompress_sz > decompress_buf_sz)
		decompress_sz = decompress_buf_sz;

	memcpy(decompress_buf, dnode->data, decompress_sz);
	return decompress_sz;
}

static ssize_t ubi_data_node_decompress_lzo(struct ubifs_data_node *dnode,
					    uint8_t *decompress_buf,
					    size_t decompress_buf_sz)
{
	lzo_uint decompress_sz = decompress_buf_sz;
	lzo_uint compress_sz;
	int ret;

	compress_sz = le32_to_cpu(dnode->ch.len) - UBIFS_DATA_NODE_SZ;

	ret = lzo1x_decompress_safe(dnode->data, compress_sz,
				    decompress_buf, &decompress_sz,
				    NULL);
	if (ret < 0)
		return ret;

	return decompress_sz;
}

/*
 * Decompress ubifs data node to @decompress_buf, and the @decompress_buf_sz
 * should at least UBIFS_BLOCK_SIZE.
 *
 * Return error code or the actually decompress size.
 */
static ssize_t ubi_data_node_decompress(struct ubifs_data_node *dnode,
					uint8_t *decompress_buf,
					size_t decompress_buf_sz)
{
	switch (le16_to_cpu(dnode->compr_type)) {
	case UBIFS_COMPR_NONE:
		return ubi_data_node_decompress_none(dnode, decompress_buf,
						     decompress_buf_sz);
	case UBIFS_COMPR_LZO:
		return ubi_data_node_decompress_lzo(dnode, decompress_buf,
						    decompress_buf_sz);
	default:
		fprintf(stderr, "Error: unsupported compress type: %d\n",
			le16_to_cpu(dnode->compr_type));
		break;
	}

	return -1;
}

static int ubi_list_file(struct ubi_editor_private_data *p, uint32_t ino)
{
	struct ubifs_ino_node *inode;
	void *peb;

	inode = ubi_alloc_read_inode(p, ino, &peb);
	if (!inode)
		return -1;

	printf(" %" PRIu64 " bytes\n", le64_to_cpu(inode->size));
	free(peb);
	return 0;
}

static int ubi_list_link(struct ubi_editor_private_data *p, uint32_t ino)
{
	struct ubifs_ino_node *inode;
	void *peb;

	inode = ubi_alloc_read_inode(p, ino, &peb);
	if (!inode)
		return -1;

	printf(" -> %.*s\n",
		(int)le64_to_cpu(inode->data_len),
		(const char *)inode->data);
	free(peb);
	return 0;
}

static int ubi_list_dent(struct ubi_editor_private_data *p, uint32_t ino,
			 int indent)
{
	const char *ubifs_itype_list_names[UBIFS_ITYPES_CNT] = {
		[UBIFS_ITYPE_REG]	= "File",
		[UBIFS_ITYPE_DIR]	= "Directory",
		[UBIFS_ITYPE_BLK]	= "Block",
		[UBIFS_ITYPE_CHR]	= "Char",
		[UBIFS_ITYPE_FIFO]	= "FIFO",
		[UBIFS_ITYPE_SOCK]	= "Socket",
		[UBIFS_ITYPE_LNK]	= "Link",
	};
	struct ubi_bptree_leaf_node *leaf;
	uint32_t hash_min = 0;

	while (1) {
		struct ubifs_dent_node *dent = NULL;
		struct ubifs_ch *ch;
		uint32_t inum;
		void *peb;
		int ret = -1;

		leaf = ubi_bptree_find(p->root, UBIFS_DENT_KEY, ino,
				       hash_min, 0xffffffff);
		if (!leaf)
			break;

		ch = ubi_alloc_read_ch(p, leaf->self_lnum, leaf->self_offs, &peb);
		if (!ch)
			return ret;

		if (ch->node_type != UBIFS_DENT_NODE) {
			fprintf(stderr, "Error: UBI node is not dent (%u:%u)\n",
				leaf->self_lnum, leaf->self_offs);
			free(peb);
			return ret;
		}

		dent = (struct ubifs_dent_node *)ch;
		if (dent->type >= UBIFS_ITYPES_CNT) {
			fprintf(stderr, "Error: DENT node has bad type(%u:%u)\n",
				leaf->self_lnum, leaf->self_offs);
			free(peb);
			return ret;
		}

		inum = (uint32_t)le64_to_cpu(dent->inum);

		for (int i = 0; i < indent; i++)
			printf("|   ");
		printf("|-- #%08d:", inum);
		printf(" %-10s", ubifs_itype_list_names[dent->type]);
		printf(" %s", dent->name);

		if ((leaf->key1 & UBIFS_S_KEY_BLOCK_MASK) == UBIFS_S_KEY_BLOCK_MASK)
			hash_min = UBIFS_S_KEY_BLOCK_MASK;
		else
			hash_min = (leaf->key1 + 1) & UBIFS_S_KEY_BLOCK_MASK;

		switch (dent->type) {
		case UBIFS_ITYPE_DIR:
			printf("\n");
			ret = ubi_list_dent(p, inum, indent + 1);
			break;
		case UBIFS_ITYPE_REG:
			ret = ubi_list_file(p, inum);
			break;
		case UBIFS_ITYPE_LNK:
			ret = ubi_list_link(p, inum);
			break;
		default:
			printf("\n");
			ret = 0;
			break;
		}

		/* peb need free after we do not touch dent anymore */
		free(peb);

		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ubi_list(struct ubi_editor_private_data *p)
{
	return ubi_list_dent(p, 1 /* root inode */, 0);
}

static int ubi_do_inode(struct ubi_editor_private_data *p, int argc, char **argv)
{
	int ino = (int)strtol(argv[1], NULL, 0);
	struct ubifs_ino_node *inode;
	void *peb, *leb;

	if (ino <= 0) {
		fprintf(stderr, "Usage: ubi inode #ino\n");
		return -1;
	}

	inode = ubi_alloc_read_inode(p, ino, &peb);
	if (!inode)
		return -1;

	leb = peb + p->data_offset;
	ubi_print_node_verbose(&inode->ch, (void *)inode - leb, 0);

	free(peb);
	return 0;
}

static int ubi_do_dnode(struct ubi_editor_private_data *p, int no_decompress,
			int argc, char **argv)
{
	struct ubifs_data_node *dnode;
	uint32_t act_blkno;
	int ino, blkno;
	int ret = 0;
	void *peb;

	if (argc != 3) {
		fprintf(stderr, "Usage: ubi dnode #ino #blk\n");
		return -1;
	}

	ino = (int)strtol(argv[1], NULL, 10);
	if (ino <= 0) {
		fprintf(stderr, "Error: bad #ino number\n");
		return -1;
	}

	blkno = (int)strtol(argv[2], NULL, 10);
	if (blkno < 0) {
		fprintf(stderr, "Error: bad #blk number\n");
		return -1;
	}

	dnode = ubi_alloc_read_data_node(p, ino, blkno, blkno,
					 &act_blkno, &peb);
	if (!dnode) {
		fprintf(stderr, "Error: data node is not found\n");
		return -1;
	}

	printf("UBIFS_DATA_NODE: DATA %d block %u\n", ino, act_blkno);
	structure_print(PRINT_LEVEL0, &dnode->ch, structure_ubifs_ch);
	structure_print(PRINT_LEVEL1, dnode, structure_ubifs_data_node);

	if (no_decompress) {
		hexdump(dnode->data,
			le32_to_cpu(dnode->ch.len) - UBIFS_DATA_NODE_SZ,
			0);
	} else {
		uint8_t decompress[UBIFS_BLOCK_SIZE];
		int r;

		r = ubi_data_node_decompress(dnode, decompress,
					     sizeof(decompress));
		if (r < 0) {
			fprintf(stderr, "Error: decompress failed(%d)\n", r);
			ret = r;
			goto done;
		}

		hexdump(decompress, r, 0);
	}

done:
	free(peb);
	return ret;
}

static int ubi_main(void *private_data, int fd, int argc, char **argv)
{
	struct ubi_editor_private_data *p = private_data;

	if (argc >= 1) {
		if (!strcmp(argv[0], "peb"))
			return ubi_do_peb(p, argc, argv);
		else if (!strcmp(argv[0], "leb"))
			return ubi_do_leb(p, argc, argv);
		else if (!strcmp(argv[0], "vtbl"))
			return ubi_do_vtbl(p, argc, argv);
		else if (!strcmp(argv[0], "node"))
			return ubi_do_node(p, argc, argv);
		else if (!strcmp(argv[0], "inode"))
			return ubi_do_inode(p, argc, argv);
		else if (!strcmp(argv[0], "dnode"))
			return ubi_do_dnode(p, 0, argc, argv);
		else if (!strcmp(argv[0], "dnode.raw"))
			return ubi_do_dnode(p, 1, argc, argv);
		else if (!strcmp(argv[0], "bptree"))
			return ubi_do_bptree(p, argc, argv);
	}

	return ubi_list(p);
}

static int ubi_unpack_file(struct ubi_editor_private_data *p, uint32_t ino,
			   const char *filename)
{
	struct ubifs_ino_node *inode;
	void *inode_peb;
	uint32_t data_block = 0;
	uint64_t filesz, n = 0;
	uint8_t zero = 0;
	int ret = 0;
	int fd;

	inode = ubi_alloc_read_inode(p, ino, &inode_peb);
	if (!inode)
		return -1;

	fd = fileopen(filename, O_RDWR | O_CREAT | O_TRUNC,
			le32_to_cpu(inode->mode));
	if (fd < 0) {
		free(inode_peb);
		return -1;
	}

	filesz = le64_to_cpu(inode->size);
	free(inode_peb);
	inode_peb = NULL;

	if (filesz == 0)
		goto done;

	/* fill the file with zero */
	lseek64(fd, filesz - sizeof(zero), SEEK_SET);
	write(fd, &zero, sizeof(zero));

	while (n < filesz) {
		struct ubifs_data_node *dnode;
		void *data_peb;
		uint32_t chunk_sz, blk_idx;
		uint8_t decompress[UBIFS_BLOCK_SIZE];
		int r;

		dnode = ubi_alloc_read_data_node(p, ino, data_block, 0xffffffff,
						 &blk_idx, &data_peb);
		/* all data node are unpacked, the remain is hole */
		if (!dnode)
			break;

		r = ubi_data_node_decompress(dnode, decompress,
					     sizeof(decompress));
		if (r < 0) {
			fprintf(stderr, "Error: decompress data node DATA %u "
				"block %u failed(%d)\n",
				ino, blk_idx, r);
			ret = r;
			free(data_peb);
			break;
		} else if ((unsigned)r != le32_to_cpu(dnode->size)) {
			fprintf(stderr, "Error: decompress data node DATA %u "
				"block %u failed: decompress_sz %d != %u\n",
				ino, blk_idx, r, le32_to_cpu(dnode->size));
			ret = -1;
			free(data_peb);
			break;
		}

		chunk_sz = le32_to_cpu(dnode->size);
		n = blk_idx * UBIFS_BLOCK_SIZE;
		lseek64(fd, n, SEEK_SET);

		write(fd, decompress, chunk_sz);
		data_block = blk_idx + 1;
		n += chunk_sz;

		free(data_peb);
	}

done:
	close(fd);
	return ret;
}

static int ubi_unpack_link(struct ubi_editor_private_data *p, uint32_t ino,
			   const char *symlink_name)
{
	char target[1024] = { 0 };
	struct ubifs_ino_node *inode;
	void *peb;

	inode = ubi_alloc_read_inode(p, ino, &peb);
	if (!inode)
		return -1;

	snprintf(target, sizeof(target), "%.*s",
		(int)le32_to_cpu(inode->data_len),
		(const char *)inode->data);
	free(peb);
	inode = NULL;

	unlink(symlink_name);
	symlink(target, symlink_name);
	return 0;
}

static int ubi_unpack_dent(struct ubi_editor_private_data *p, uint32_t ino,
			   const char *parent_dir)
{
	struct ubi_bptree_leaf_node *leaf;
	uint32_t hash_min = 0;

	while (1) {
		char filename[1024] = { 0 };
		struct ubifs_dent_node *dent = NULL;
		struct ubifs_ch *ch;
		uint32_t inum;
		void *peb;
		int ret = -1;

		leaf = ubi_bptree_find(p->root, UBIFS_DENT_KEY, ino,
				       hash_min, 0xffffffff);
		if (!leaf)
			break;

		ch = ubi_alloc_read_ch(p, leaf->self_lnum, leaf->self_offs, &peb);
		if (!ch)
			return ret;

		if (ch->node_type != UBIFS_DENT_NODE) {
			fprintf(stderr, "Error: UBI node is not dent (%u:%u)\n",
				leaf->self_lnum, leaf->self_offs);
			free(peb);
			return ret;
		}

		dent = (struct ubifs_dent_node *)ch;
		if (dent->type >= UBIFS_ITYPES_CNT) {
			fprintf(stderr, "Error: DENT node has bad type(%u:%u)\n",
				leaf->self_lnum, leaf->self_offs);
			free(peb);
			return ret;
		}

		if ((leaf->key1 & UBIFS_S_KEY_BLOCK_MASK) == UBIFS_S_KEY_BLOCK_MASK)
			hash_min = UBIFS_S_KEY_BLOCK_MASK;
		else
			hash_min = (leaf->key1 + 1) & UBIFS_S_KEY_BLOCK_MASK;

		snprintf(filename, sizeof(filename), "%s/%s",
			parent_dir, (const char *)dent->name);

		inum = (uint32_t)le64_to_cpu(dent->inum);

		switch (dent->type) {
		case UBIFS_ITYPE_DIR: {
			struct ubifs_ino_node *inode;
			void *inode_peb;
			int mode;

			inode = ubi_alloc_read_inode(p, dent->inum, &inode_peb);
			if (!inode) {
				fprintf(stderr, "Error: read inode %d of dent "
					"%s failed\n", inum, filename);
				ret = -1;
				break;
			}

			mode = le32_to_cpu(inode->mode);
			free(inode_peb);

			mkdir(filename, mode);
			ret = ubi_unpack_dent(p, inum, filename);
			break;
		}
		case UBIFS_ITYPE_REG:
			ret = ubi_unpack_file(p, inum, filename);
			break;
		case UBIFS_ITYPE_LNK:
			ret = ubi_unpack_link(p, inum, filename);
			break;
		default:
			fprintf(stderr, "Error: unpack %s failed, unsupported "
				"type %s\n",
				filename,
				ubifs_itype_short_names[dent->type]);
			ret = -1;
			break;
		}

		/* peb need free after we do not touch dent anymore */
		free(peb);

		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ubi_unpack(void *private_data, int fd, const char *outdir,
		      int argc, char **argv)
{
	struct ubi_editor_private_data *p = private_data;

	return ubi_unpack_dent(p, 1 /* root ino */, outdir);
}

static struct imgeditor ubi_editor = {
	.name			= "ubi",
	.descriptor		= "ubi image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN |
				  IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= SIZE_MB(1),
	.private_data_size	= sizeof(struct ubi_editor_private_data),
	.init			= ubi_editor_init,
	.detect			= ubi_detect,
	.list			= ubi_main,
	.unpack			= ubi_unpack,
	.exit			= ubi_editor_exit,
};
REGISTER_IMGEDITOR(ubi_editor);
