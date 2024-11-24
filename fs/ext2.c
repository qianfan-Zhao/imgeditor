/*
 * ext2 file system imaga editor (experience)
 *
 * reference: https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout
 *            https://docs.kernel.org/filesystems/ext4/globals.html
 *            https://blogs.oracle.com/linux/post/ondisk-journal-data-structures-jbd2
 *
 * qianfan Zhao
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "imgeditor.h"
#include "ext_common.h"
#include "ext4_journal.h"
#include "structure.h"
#include "libcrc.h"

struct ext2_editor_private_data;
static void *alloc_inode_file(struct ext2_editor_private_data *p, uint32_t ino,
			      uint64_t *ret_filesize);
static int ext2_whohas_blkno(struct ext2_editor_private_data *p,
			     uint32_t which_blk,
			     int *ret_ino);

static size_t aligned_block(size_t size, size_t block_size)
{
	return aligned_length(size, block_size) / block_size;
}

static void structure_item_print_ext2_unix_epoch(const char *print_name_fmt, const char *name,
						 const void *addr, size_t sz)
{
	__le32 epoch = *((__le32 *)addr);
	time_t t = le32_to_cpu(epoch);
	struct tm tm;
	char buf[64];

	structure_print_name(print_name_fmt, name);
	localtime_r(&t, &tm);
	strftime(buf, sizeof(buf), "%FT%T", &tm);
	printf("%s (%ld)\n", buf, t);
}

static void st_print_sb_feature_compat(const char *print_name_fmt,
				       const char *name,
				       const void *data, size_t sz)
{
	#define EXT_SB_FLAG_BIT(name) { EXT4_FEATURE_COMPAT_##name, #name }
	static const struct structure_bit_descriptor flags[] = {
		EXT_SB_FLAG_BIT(DIR_PREALLOC),
		EXT_SB_FLAG_BIT(IMAGIC_INODES),
		EXT_SB_FLAG_BIT(EXT_ATTR),
		EXT_SB_FLAG_BIT(RESIZE_INODE),
		EXT_SB_FLAG_BIT(DIR_INDEX),
		EXT_SB_FLAG_BIT(SPARSE_SUPER2),
		EXT_SB_FLAG_BIT(FAST_COMMIT),
		EXT_SB_FLAG_BIT(STABLE_INODES),
		EXT_SB_FLAG_BIT(ORPHAN_FILE),
		{ 0, NULL },
	};
	#undef EXT_SB_FLAG_BIT

	structure_item_print_bit_flags(print_name_fmt, name, data, sz, flags);
}

static void st_print_sb_feature_incompat(const char *print_name_fmt,
					 const char *name,
					 const void *data, size_t sz)
{
	#define EXT_SB_FLAG_BIT(name) { EXT4_FEATURE_INCOMPAT_##name, #name }
	static const struct structure_bit_descriptor flags[] = {
		EXT_SB_FLAG_BIT(COMPRESSION),
		EXT_SB_FLAG_BIT(FILETYPE),
		EXT_SB_FLAG_BIT(RECOVER),
		EXT_SB_FLAG_BIT(JOURNAL_DEV),
		EXT_SB_FLAG_BIT(META_BG),
		EXT_SB_FLAG_BIT(EXTENTS),
		EXT_SB_FLAG_BIT(64BIT),
		EXT_SB_FLAG_BIT(MMP),
		EXT_SB_FLAG_BIT(FLEX_BG),
		EXT_SB_FLAG_BIT(EA_INODE),
		EXT_SB_FLAG_BIT(DIRDATA),
		EXT_SB_FLAG_BIT(CSUM_SEED),
		EXT_SB_FLAG_BIT(LARGEDIR),
		EXT_SB_FLAG_BIT(INLINE_DATA),
		EXT_SB_FLAG_BIT(ENCRYPT),
		EXT_SB_FLAG_BIT(CASEFOLD),
		{ 0, NULL },
	};
	#undef EXT_SB_FLAG_BIT

	structure_item_print_bit_flags(print_name_fmt, name, data, sz, flags);
}

static void st_print_sb_feature_rocompat(const char *print_name_fmt,
					 const char *name,
					 const void *data, size_t sz)
{
	#define EXT_SB_FLAG_BIT(name) { EXT4_FEATURE_RO_COMPAT_##name, #name }
	static const struct structure_bit_descriptor flags[] = {
		EXT_SB_FLAG_BIT(SPARSE_SUPER),
		EXT_SB_FLAG_BIT(LARGE_FILE),
		EXT_SB_FLAG_BIT(BTREE_DIR),
		EXT_SB_FLAG_BIT(HUGE_FILE),
		EXT_SB_FLAG_BIT(GDT_CSUM),
		EXT_SB_FLAG_BIT(DIR_NLINK),
		EXT_SB_FLAG_BIT(EXTRA_ISIZE),
		EXT_SB_FLAG_BIT(QUOTA),
		EXT_SB_FLAG_BIT(BIGALLOC),
		EXT_SB_FLAG_BIT(METADATA_CSUM),
		EXT_SB_FLAG_BIT(READONLY),
		EXT_SB_FLAG_BIT(PROJECT),
		EXT_SB_FLAG_BIT(VERITY),
		EXT_SB_FLAG_BIT(ORPHAN_PRESENT),
		{ 0, NULL },
	};
	#undef EXT_SB_FLAG_BIT

	structure_item_print_bit_flags(print_name_fmt, name, data, sz, flags);
}

static const struct structure_item ext2_sblock_structure[] = {
	STRUCTURE_ITEM(struct ext2_sblock, total_inodes,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, total_blocks,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, reserved_blocks,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, free_blocks,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, free_inodes,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, first_data_block,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, log2_block_size,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, log2_fragment_size,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, blocks_per_group,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, fragments_per_group,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, inodes_per_group,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, mtime,			structure_item_print_ext2_unix_epoch),
	STRUCTURE_ITEM(struct ext2_sblock, utime,			structure_item_print_ext2_unix_epoch),
	STRUCTURE_ITEM(struct ext2_sblock, mnt_count,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, max_mnt_count,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, magic,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_sblock, fs_state,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, error_handling,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, minor_revision_level,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, lastcheck,			structure_item_print_ext2_unix_epoch),
	STRUCTURE_ITEM(struct ext2_sblock, checkinterval,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, creator_os,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, revision_level,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, uid_reserved,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, gid_reserved,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, first_inode,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, inode_size,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, block_group_number,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, feature_compatibility,	st_print_sb_feature_compat),
	STRUCTURE_ITEM(struct ext2_sblock, feature_incompat,		st_print_sb_feature_incompat),
	STRUCTURE_ITEM(struct ext2_sblock, feature_ro_compat,		st_print_sb_feature_rocompat),
	STRUCTURE_ITEM(struct ext2_sblock, unique_id,			structure_item_print_x32_array),
	STRUCTURE_ITEM(struct ext2_sblock, volume_name,			structure_item_print_string),
	STRUCTURE_ITEM(struct ext2_sblock, last_mounted_on,		structure_item_print_string),
	STRUCTURE_ITEM(struct ext2_sblock, compression_info,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, prealloc_blocks,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, prealloc_dir_blocks,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, reserved_gdt_blocks,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, journal_uuid,		structure_item_print_x8_array),
	STRUCTURE_ITEM(struct ext2_sblock, journal_inode,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, journal_dev,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, last_orphan,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, hash_seed,			structure_item_print_x32_array),
	STRUCTURE_ITEM(struct ext2_sblock, default_hash_version,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, journal_backup_type,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, descriptor_size,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, default_mount_options,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, first_meta_block_group,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, mkfs_time,			structure_item_print_ext2_unix_epoch),
	STRUCTURE_ITEM(struct ext2_sblock, journal_blocks,		structure_item_print_u32_array),
	STRUCTURE_ITEM(struct ext2_sblock, total_blocks_high,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, reserved_blocks_high,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, free_blocks_high,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, min_extra_inode_size,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, want_extra_inode_size,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, flags,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, mmp_interval,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, mmp_block,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, raid_stripe_width,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, log2_groups_per_flex,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, checksum_type,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, checksum_seed,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_sblock, checksum,			structure_item_print_xunsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item ext2_block_group_structure_32[] = {
	STRUCTURE_ITEM(struct ext2_block_group, block_id,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, inode_id,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, inode_table_id,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, free_blocks,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, free_inodes,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, used_dir_cnt,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_flags,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_exclude_bitmap,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_block_id_csum,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_inode_id_csum,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_itable_unused,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_checksum,		structure_item_print_xunsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item ext2_block_group_structure_64[] = {
	STRUCTURE_ITEM(struct ext2_block_group, block_id_high,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, inode_id_high,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, inode_table_id_high,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, free_blocks_high,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, free_inodes_high,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, used_dir_cnt_high,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_itable_unused_high,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_exclude_bitmap_high,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_block_id_csum_high,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_inode_id_csum_high,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, bg_reserved,		structure_item_print_xunsigned),
	STRUCTURE_ITEM_END(),
};

static void ext2_inode_print_mask(uint16_t mode)
{
	printf("0x%04x (", mode);

	switch (mode & INODE_MODE_S_MASK) {
	case INODE_MODE_S_FIFO:
		printf("FIFO");
		break;
	case INODE_MODE_S_IFCHR:
		printf("CHAR");
		break;
	case INODE_MODE_S_IFDIR:
		printf("Directory");
		break;
	case INODE_MODE_S_IFBLK:
		printf("Block");
		break;
	case INODE_MODE_S_IFREG:
		printf("File");
		break;
	case INODE_MODE_S_IFLINK:
		printf("Symlink");
		break;
	case INODE_MODE_S_IFSOCK:
		printf("Socket");
		break;
	}

	printf(" %o)", mode & INODE_MODE_PERMISSION_MASK);
	printf("\n");
}

static void structure_ext2_inode_print_mask(const char *print_name_fmt,
					    const char *name,
					    const void *p, size_t sz)
{
	uint16_t mode = le16_to_cpu(*(const __le16 *)p);

	structure_print_name(print_name_fmt, name);
	ext2_inode_print_mask(mode);
}

static void ext2_inode_print_flags(uint32_t flags)
{
	#define CHECK_PRINT_FLAG(f) do {	\
		if (flags & (f))		\
			printf("%s ", #f);	\
	} while (0)

	printf("0x%08x ( ", flags);

	CHECK_PRINT_FLAG(EXT4_SECRM_FL);
	CHECK_PRINT_FLAG(EXT4_UNRM_FL);
	CHECK_PRINT_FLAG(EXT4_COMPR_FL);
	CHECK_PRINT_FLAG(EXT4_SYNC_FL);
	CHECK_PRINT_FLAG(EXT4_IMMUTABLE_FL);
	CHECK_PRINT_FLAG(EXT4_APPEND_FL);
	CHECK_PRINT_FLAG(EXT4_NODUMP_FL);
	CHECK_PRINT_FLAG(EXT4_NOATIME_FL);
	CHECK_PRINT_FLAG(EXT4_DIRTY_FL);
	CHECK_PRINT_FLAG(EXT4_COMPRBLK_FL);
	CHECK_PRINT_FLAG(EXT4_NOCOMPR_FL);
	CHECK_PRINT_FLAG(EXT4_ENCRYPT_FL);
	CHECK_PRINT_FLAG(EXT4_INDEX_FL);
	CHECK_PRINT_FLAG(EXT4_IMAGIC_FL);
	CHECK_PRINT_FLAG(EXT4_JOURNAL_DATA_FL);
	CHECK_PRINT_FLAG(EXT4_NOTAIL_FL);
	CHECK_PRINT_FLAG(EXT4_DIRSYNC_FL);
	CHECK_PRINT_FLAG(EXT4_TOPDIR_FL);
	CHECK_PRINT_FLAG(EXT4_HUGE_FILE_FL);
	CHECK_PRINT_FLAG(EXT4_EXTENTS_FL);
	CHECK_PRINT_FLAG(EXT4_VERITY_FL);
	CHECK_PRINT_FLAG(EXT4_EA_INODE_FL);
	CHECK_PRINT_FLAG(EXT4_DAX_FL);
	CHECK_PRINT_FLAG(EXT4_INLINE_DATA_FL);
	CHECK_PRINT_FLAG(EXT4_PROJINHERIT_FL);
	CHECK_PRINT_FLAG(EXT4_CASEFOLD_FL);
	CHECK_PRINT_FLAG(EXT4_RESERVED_FL);

	printf(")\n");
}

static void structure_ext2_inode_print_flags(const char *print_name_fmt,
					     const char *name,
					     const void *p, size_t sz)
{
	uint32_t flags = le32_to_cpu(*(const __le32 *)p);

	structure_print_name(print_name_fmt, name);
	ext2_inode_print_flags(flags);
}

static const struct structure_item ext2_inode_structure_part1[] = {
	STRUCTURE_ITEM(struct ext2_inode, mode,				structure_ext2_inode_print_mask),
	STRUCTURE_ITEM(struct ext2_inode, uid,				structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_inode, size,				structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_inode, atime,			structure_item_print_ext2_unix_epoch),
	STRUCTURE_ITEM(struct ext2_inode, ctime,			structure_item_print_ext2_unix_epoch),
	STRUCTURE_ITEM(struct ext2_inode, mtime,			structure_item_print_ext2_unix_epoch),
	STRUCTURE_ITEM(struct ext2_inode, dtime,			structure_item_print_ext2_unix_epoch),
	STRUCTURE_ITEM(struct ext2_inode, gid,				structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_inode, nlinks,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_inode, blockcnt,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_inode, flags,			structure_ext2_inode_print_flags),
	STRUCTURE_ITEM(struct ext2_inode, osd1,				structure_item_print_xunsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item ext4_extent_header_structure[] = {
	STRUCTURE_ITEM(struct ext4_extent_header, eh_magic,		structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext4_extent_header, eh_entries,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext4_extent_header, eh_max,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext4_extent_header, eh_depth,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext4_extent_header, eh_generation,	structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item ext4_extent_structure[] = {
	STRUCTURE_ITEM(struct ext4_extent, ee_block,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext4_extent, ee_len,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext4_extent, ee_start_hi,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext4_extent, ee_start_lo,			structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item ext4_extent_idx_structure[] = {
	STRUCTURE_ITEM(struct ext4_extent_idx, ei_block,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext4_extent_idx, ei_leaf_lo,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext4_extent_idx, ei_leaf_hi,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext4_extent_idx, ei_unused,		structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item ext2_inode_structure_datablocks[] = {
	STRUCTURE_ITEM(struct ext2_inode, b.blocks.dir_blocks,		structure_item_print_u32_array),
	STRUCTURE_ITEM(struct ext2_inode, b.blocks.indir_block,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_inode, b.blocks.double_indir_block,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_inode, b.blocks.triple_indir_block,	structure_item_print_unsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item ext2_inode_structure_symlink[] = {
	STRUCTURE_ITEM(struct ext2_inode, b.symlink,			structure_item_print_string),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item ext2_inode_structure_part3[] = {
	STRUCTURE_ITEM(struct ext2_inode, version,			structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_inode, acl,				structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_inode, size_high,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_inode, fragment_addr,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_inode, osd2,				structure_item_print_x32_array),
	STRUCTURE_ITEM_END(),
};

static void structure_ext4_jsb_print_blocktype(const char *print_name_fmt,
						const char *name,
						const void *p, size_t sz)
{
	const __be32 *p_be32 = p;
	uint32_t blocktype = be32_to_cpu(*p_be32);

	structure_print_name(print_name_fmt, name);
	printf("0x%08x (", blocktype);

	switch (blocktype) {
	case EXT3_JOURNAL_DESCRIPTOR_BLOCK:
		printf("Descriptor");
		break;
	case EXT3_JOURNAL_COMMIT_BLOCK:
		printf("Commit");
		break;
	case EXT3_JOURNAL_SUPERBLOCK_V1:
	case EXT3_JOURNAL_SUPERBLOCK_V2:
		printf("Superblock, v%d",
			blocktype - EXT3_JOURNAL_SUPERBLOCK_V1 + 1);
		break;
	case EXT3_JOURNAL_REVOKE_BLOCK:
		printf("Revocation");
		break;
	default:
		printf("???");
		break;
	}

	printf(")\n");
}

static const struct structure_item ext4_journal_header_structure[] = {
	STRUCTURE_ITEM(struct journal_header_t, h_magic,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct journal_header_t, h_blocktype,		structure_ext4_jsb_print_blocktype),
	STRUCTURE_ITEM(struct journal_header_t, h_sequence,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM_END(),
};

struct bit_descriptor {
	int		value;
	const char	*descriptor;
};

static void print_bit_descriptors(uint32_t flags, const struct bit_descriptor *d)
{
	for (; d->descriptor; d++) {
		if (flags & d->value)
			printf(" %s", d->descriptor);
	}
}

static void ext4_jsb_print_features(const char *print_name_fmt, const char *name,
				    const void *p_features,
				    const struct bit_descriptor *d)
{
	const __be32 *p_be32 = p_features;
	uint32_t features = be32_to_cpu(*p_be32);

	structure_print_name(print_name_fmt, name);
	printf("0x%08x (", features);

	print_bit_descriptors(features, d);
	printf(" )\n");
}

#define JBD2_FEATURE_BIT_DESCRIPTOR(name) { name, #name }

static void structure_ext4_jsb_compat_feature(const char *print_name_fmt,
					      const char *name,
					      const void *p, size_t sz)
{
	static const struct bit_descriptor compat_features[] = {
		JBD2_FEATURE_BIT_DESCRIPTOR(JBD2_FEATURE_COMPAT_CHECKSUM),
		{ 0, NULL },
	};

	ext4_jsb_print_features(print_name_fmt, name, p, compat_features);
}

static void structure_ext4_jsb_incompat_features(const char *print_name_fmt,
						 const char *name,
						 const void *p, size_t sz)
{
	static const struct bit_descriptor incompat_features[] = {
		JBD2_FEATURE_BIT_DESCRIPTOR(JBD2_FEATURE_INCOMPAT_REVOKE),
		JBD2_FEATURE_BIT_DESCRIPTOR(JBD2_FEATURE_INCOMPAT_64BIT),
		JBD2_FEATURE_BIT_DESCRIPTOR(JBD2_FEATURE_INCOMPAT_ASYNC_COMMIT),
		JBD2_FEATURE_BIT_DESCRIPTOR(JBD2_FEATURE_INCOMPAT_CSUM_V2),
		JBD2_FEATURE_BIT_DESCRIPTOR(JBD2_FEATURE_INCOMPAT_CSUM_V3),
		JBD2_FEATURE_BIT_DESCRIPTOR(JBD2_FEATURE_INCOMPAT_FAST_COMMIT),
		{ 0, NULL },
	};

	ext4_jsb_print_features(print_name_fmt, name, p, incompat_features);
}

static const struct structure_item ext4_journal_sblock_structure[] = {
	STRUCTURE_ITEM(struct journal_superblock_t, s_blocksize,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct journal_superblock_t, s_maxlen,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct journal_superblock_t, s_first,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct journal_superblock_t, s_sequence,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct journal_superblock_t, s_start,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct journal_superblock_t, s_errno,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct journal_superblock_t, s_feature_compat,		structure_ext4_jsb_compat_feature),
	STRUCTURE_ITEM(struct journal_superblock_t, s_feature_incompat,		structure_ext4_jsb_incompat_features),
	STRUCTURE_ITEM(struct journal_superblock_t, s_feature_ro_compat,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct journal_superblock_t, s_uuid,			structure_item_print_x8_array),
	STRUCTURE_ITEM(struct journal_superblock_t, s_nr_users,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct journal_superblock_t, s_dynsuper,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct journal_superblock_t, s_max_transaction,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct journal_superblock_t, s_max_trans_data,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM_END(),
};

static void structure_print_ext4_extent(const char *print_name_fmt, struct ext4_extent_header *eh)
{
	void *p = eh;

	structure_print(print_name_fmt, p, ext4_extent_header_structure);
	p += sizeof(*eh);

	for (uint32_t i = 0; i < le16_to_cpu(eh->eh_entries); i++) {
		if (le16_to_cpu(eh->eh_entries) > 1)
			printf("----\n");

		if (eh->eh_depth == 0)
			structure_print(print_name_fmt, p, ext4_extent_structure);
		else
			structure_print(print_name_fmt, p, ext4_extent_idx_structure);

		p += sizeof(struct ext4_extent);
	}

	if (le16_to_cpu(eh->eh_entries) > 1)
		printf("----\n");
}

static void structure_print_ext2_inode(const char *print_name_fmt, struct ext2_inode *inode)
{
	structure_print(print_name_fmt, inode, ext2_inode_structure_part1);

	if (le32_to_cpu(inode->flags) & EXT4_EXTENTS_FL) {
		struct ext4_extent_header *eh =
			(struct ext4_extent_header *)&inode->b;

		structure_print_ext4_extent(print_name_fmt, eh);
	}  else {
		switch (le16_to_cpu(inode->mode) & INODE_MODE_S_MASK) {
		case INODE_MODE_S_IFREG:
		case INODE_MODE_S_IFDIR:
			structure_print(print_name_fmt, inode, ext2_inode_structure_datablocks);
			break;
		case INODE_MODE_S_IFLINK:
			/* https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout#Symbolic_Links
			 * The target of a symbolic link will be stored in this
			 * field if the target string is less than 60 bytes
			 * long. Otherwise, either extents or block maps will
			 * be used to allocate data blocks to store the
			 * link target.
			 */
			if (le32_to_cpu(inode->size) < sizeof(inode->b.symlink))
				structure_print(print_name_fmt, inode, ext2_inode_structure_symlink);
			else
				structure_print(print_name_fmt, inode, ext2_inode_structure_datablocks);
			break;
		default:
			structure_print(print_name_fmt, inode, ext2_inode_structure_datablocks);
			break;
		}
	}

	structure_print(print_name_fmt, inode, ext2_inode_structure_part3);
}

enum disk_layout_type {
	DISK_LAYOUT_SUPBER_BLOCK,
	DISK_LAYOUT_GROUP_DESCRIPTOR,
	DISK_LAYOUT_RESERVED_GDT,
	DISK_LAYOUT_DATA_BLOCK_BITMAP,
	DISK_LAYOUT_INODE_BITMAP,
	DISK_LAYOUT_INODE_TABLE,

	DISK_LAYOUT_MAX,
};

struct disk_layout {
	enum disk_layout_type		major_type;
	uint16_t			group;
	uint32_t			start_block;
	uint32_t			len_block;
};

struct ext2_editor_private_data {
	int				fd;

	struct ext2_sblock		sblock;
	struct ext2_block_group		*block_groups;
	struct ext2_inode		root_inode;

	uint32_t			block_size; /* in bytes */
	uint32_t			fragment_size; /* in bytes */
	uint32_t			inode_size; /* in bytes */
	uint32_t			n_block_group;

	uint32_t			csum_seed;
	struct libcrc32			crc32c_le;
	struct libcrc16			crc16;

	/* 1T partition has 8192 groups, one group need at no more than 6 item:
	 * data bitmap, inode bitmap, inode table,
	 * backup super block, backup gdt, reserved gdt
	 */
#define EXT2_MAX_DISK_LAYOUT_ARRAYS	(8192 * 6)
	struct disk_layout		layouts[EXT2_MAX_DISK_LAYOUT_ARRAYS];
	size_t				n_layout;
};

static int ext2_editor_register_layout(struct ext2_editor_private_data *p,
				       enum disk_layout_type major,
				       uint16_t group,
				       uint32_t start_block,
				       uint32_t len_block)
{
	if (p->n_layout < EXT2_MAX_DISK_LAYOUT_ARRAYS) {
		struct disk_layout *layout = &p->layouts[p->n_layout];

		++p->n_layout;
		layout->major_type = major;
		layout->group = group;
		layout->start_block = start_block;
		layout->len_block = len_block;

		return 0;
	}

	return -1;
}

static const struct disk_layout *
	ext2_editor_disk_layout_match_block(struct ext2_editor_private_data *p,
					    uint32_t blkno)
{
	for (size_t i = 0; i < p->n_layout; i++) {
		struct disk_layout *layout = &p->layouts[i];

		if (blkno >= layout->start_block &&
			blkno < layout->start_block + layout->len_block)
			return layout;
	}

	return NULL;
}

static char *format_disk_layout(const struct disk_layout *layout,
				char *buf, size_t bufsz)
{
	static const char *string_types[DISK_LAYOUT_MAX] = {
		[DISK_LAYOUT_SUPBER_BLOCK]	= "super block",
		[DISK_LAYOUT_GROUP_DESCRIPTOR]	= "group descriptor",
		[DISK_LAYOUT_RESERVED_GDT]	= "group descriptor(rev)",
		[DISK_LAYOUT_DATA_BLOCK_BITMAP] = "data bitmap",
		[DISK_LAYOUT_INODE_BITMAP]	= "inode bitmap",
		[DISK_LAYOUT_INODE_TABLE]	= "inode table",
	};

	if (!layout)
		return NULL;

	switch (layout->major_type) {
	case DISK_LAYOUT_DATA_BLOCK_BITMAP:
	case DISK_LAYOUT_INODE_BITMAP:
	case DISK_LAYOUT_INODE_TABLE:
	case DISK_LAYOUT_RESERVED_GDT:
		snprintf(buf, bufsz, "%-22s for group %d",
			 string_types[layout->major_type],
			 layout->group);
		break;
	case DISK_LAYOUT_SUPBER_BLOCK:
	case DISK_LAYOUT_GROUP_DESCRIPTOR:
		if (layout->group != 0)
			snprintf(buf, bufsz, "%-22s  in group %d (backup)",
				string_types[layout->major_type],
				layout->group);
		else
			snprintf(buf, bufsz, "%s",
				 string_types[layout->major_type]);
		break;
	default:
		return NULL;
	}

	return buf;
}

static int ext2_editor_init(void *private_data)
{
	struct ext2_editor_private_data *p = private_data;
	struct libcrc32 *crc32 = &p->crc32c_le;
	struct libcrc16 *crc16 = &p->crc16;

	memset(crc32, 0, sizeof(*crc32));
	crc32->poly = 0x1edc6f41;
	crc32->seed = 0xffffffff;
	crc32->refin = true;
	crc32->refout = true;

	memset(crc16, 0, sizeof(*crc16));
	crc16->poly = 0x8005;
	crc16->seed = 0xffff;
	crc16->refin = true;
	crc16->refout = true;

	return 0;
}

static void ext2_editor_exit(void *private_data)
{
	struct ext2_editor_private_data *p = private_data;

	if (p->block_groups) {
		free(p->block_groups);
		p->block_groups = NULL;
	}
}

static int ext2_has_ro_compat_feature(struct ext2_sblock *sb, unsigned int flags)
{
	return le32_to_cpu(sb->feature_ro_compat) & flags;
}

static int ext2_has_sblock_csum(struct ext2_sblock *sblock)
{
	return ext2_has_ro_compat_feature(sblock,
			EXT4_FEATURE_RO_COMPAT_METADATA_CSUM);
}

static int ext2_has_block_group_csum(struct ext2_sblock *sblock)
{
	return ext2_has_ro_compat_feature(sblock,
			EXT4_FEATURE_RO_COMPAT_METADATA_CSUM |
			EXT4_FEATURE_RO_COMPAT_GDT_CSUM);
}

static int ext2_check_sblock(struct libcrc32 *crc, int force_type,
			     struct ext2_sblock *sblock)
{
	uint32_t sum;

	if (le16_to_cpu(sblock->magic) != EXT2_MAGIC) {
		fprintf_if_force_type("Error: magic doesn't match\n");
		return -1;
	}

#define check_sblock_not(type, value) do {				\
	if (le32_to_cpu(sblock->type) == (value)) {			\
		fprintf_if_force_type("Error: sblock.%s == %d\n",	\
					#type, value);			\
		return -1;						\
	}								\
} while (0)

	check_sblock_not(total_inodes, 0);
	check_sblock_not(total_blocks, 0);
	check_sblock_not(blocks_per_group, 0);
	check_sblock_not(fragments_per_group, 0);
	check_sblock_not(inodes_per_group, 0);

	if (ext2_has_sblock_csum(sblock) == 0)
		return 0;

	libcrc32_init_seed(crc, 0xffffffff);
	libcrc32_update(crc, sblock, offsetof(struct ext2_sblock, checksum));
	sum = libcrc32_finish(crc);
	if (sum != le32_to_cpu(sblock->checksum)) {
		fprintf_if_force_type("Error: bad checksum in super block "
				      "%x != %x\n",
				      sum,
				      le32_to_cpu(sblock->checksum));
		return -1;
	}

	return 0;
}

static uint32_t ext2_sblock_log2_size_to_bytes(__le32 log2_size)
{
	return 1 << (le32_to_cpu(log2_size) + EXT2_MIN_BLOCK_LOG_SIZE);
}

static int ext2_inode_block_number(struct ext2_editor_private_data *p, int ino,
				   uint32_t *blkno, uint32_t *blk_offset)
{
	struct ext2_sblock *sblock = &p->sblock;
	struct ext2_block_group *blkgrp = NULL;
	uint32_t inodes_per_block;
	int idx;

	--ino; /* inode number are started from 1 */
	inodes_per_block = p->block_size / p->inode_size;

	idx = ino / le32_to_cpu(sblock->inodes_per_group);
	if (!(idx < (int)p->n_block_group))
		return -1;

	blkgrp = &p->block_groups[idx];
	*blkno = le32_to_cpu(blkgrp->inode_table_id) +
		(ino % le32_to_cpu(sblock->inodes_per_group)) / inodes_per_block;
	*blk_offset = (ino % inodes_per_block) * p->inode_size;

	return 0;
}

static int _ext2_read_inode(struct ext2_editor_private_data *p, int ino,
			    struct ext2_inode *inode,
			    uint32_t *ret_blkno, uint32_t *ret_blk_offset)
{
	uint32_t blkno, blk_offset;
	int ret;

	ret = ext2_inode_block_number(p, ino, &blkno, &blk_offset);
	if (ret < 0)
		return ret;

	fileseek(p->fd, blkno * p->block_size + blk_offset);
	ret = read(p->fd, inode, sizeof(*inode));
	if (ret != (int)sizeof(*inode)) {
		fprintf(stderr, "Error: read inode #%d failed(%d)\n",
			ino, ret);
		return -1;
	}

	if (ret_blkno)
		*ret_blkno = blkno;
	if (ret_blk_offset)
		*ret_blk_offset = blk_offset;

	return 0;
}

static int ext2_read_inode(struct ext2_editor_private_data *p, int ino,
			   struct ext2_inode *inode)
{
	return _ext2_read_inode(p, ino, inode, NULL, NULL);
}

static int ext2_read_blocks(struct ext2_editor_private_data *p,
			    unsigned long blkno, uint32_t nblks,
			    void *buf, size_t bufsz)
{
	size_t sz = p->block_size * nblks;
	int n;

	if (sz > bufsz)
		sz = bufsz;

	fileseek(p->fd, blkno * p->block_size);
	n = read(p->fd, buf, sz);
	if (n <= 0 || (size_t)n != sz) {
		fprintf(stderr, "Error: read %d blocks from #%lu failed\n",
			nblks, blkno);
		return -1;
	}

	return 0;
}

static void *ext2_alloc_read_block(struct ext2_editor_private_data *p,
				   unsigned long blkno)
{
	uint8_t *blk = malloc(p->block_size);

	if (blk) {
		if (ext2_read_blocks(p, blkno, 1, blk, p->block_size) < 0) {
			free(blk);
			blk = NULL;
		}
	}

	return blk;
}

struct ext2_inode_blocks {
	uint32_t		*blocks;
	size_t			total;

	/* private data */
	size_t			__maxsize;
};

static int ext2_inode_blocks_push(struct ext2_editor_private_data *p,
				  struct ext2_inode_blocks *b, uint32_t blkno)
{
	if (b->__maxsize == 0) {
		b->total = 0;
		b->__maxsize = INDIRECT_BLOCKS;
		b->blocks = calloc(b->__maxsize, sizeof(*b->blocks));
	} else if (b->total + 1 > b->__maxsize) {
		b->__maxsize *= 2;
		b->blocks = realloc(b->blocks,
				    b->__maxsize * sizeof(*b->blocks));
	}

	if (!b->blocks) {
		fprintf(stderr, "Error: Alloc %zu inode indir blocks failed\n",
			b->__maxsize);
		return -1;
	}

	b->blocks[b->total] = blkno;
	b->total++;

	return 0;
}

#define ext2_inode_read_block_define(name, todo)			\
static int								\
ext2_inode_##name##_blocks_push(struct ext2_editor_private_data *p,	\
				struct ext2_inode_blocks *b,		\
				uint32_t blkno)				\
{									\
	__le32 *blkbuf = ext2_alloc_read_block(p, blkno);		\
	size_t maxcount = p->block_size / sizeof(__le32);		\
	int ret = 0;							\
									\
	if (!blkbuf) {							\
		fprintf(stderr, "Error: read %s blk #%d failed\n",	\
			#name, blkno);					\
		return -1;						\
	}								\
									\
	for (size_t i = 0; i < maxcount; i++) {				\
		uint32_t n = le32_to_cpu(blkbuf[i]);			\
									\
		if (n == 0)						\
			break;						\
									\
		ret = todo(p, b, n);					\
		if (ret < 0)						\
			return ret;					\
	}								\
									\
	return ret;							\
}

ext2_inode_read_block_define(indir, ext2_inode_blocks_push);
ext2_inode_read_block_define(double_indir, ext2_inode_indir_blocks_push);
ext2_inode_read_block_define(triple_indir, ext2_inode_double_indir_blocks_push);

static int ext2_inode_blocks_read(struct ext2_editor_private_data *p,
				  struct ext2_inode_blocks *b,
				  struct ext2_inode *inode,
				  uint32_t ino)
{
	uint32_t blkno;
	int ret = 0;

	for (int i = 0; i < INDIRECT_BLOCKS; i++) {
		blkno = le32_to_cpu(inode->b.blocks.dir_blocks[i]);
		if (blkno == 0)
			return ret;

		ret = ext2_inode_blocks_push(p, b, blkno);
		if (ret < 0)
			return ret;
	}

	blkno = le32_to_cpu(inode->b.blocks.indir_block);
	if (blkno) {
		ret = ext2_inode_indir_blocks_push(p, b, blkno);
		if (ret < 0)
			return ret;
	}

	blkno = le32_to_cpu(inode->b.blocks.double_indir_block);
	if (blkno) {
		ret = ext2_inode_double_indir_blocks_push(p, b, blkno);
		if (ret < 0)
			return ret;
	}

	blkno = le32_to_cpu(inode->b.blocks.triple_indir_block);
	if (blkno) {
		ret = ext2_inode_triple_indir_blocks_push(p, b, blkno);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static uint64_t ext4_extent_start_block(struct ext4_extent *ee)
{
	uint64_t ee_start = le16_to_cpu(ee->ee_start_hi);

	ee_start = ee_start << 32;
	ee_start |= le32_to_cpu(ee->ee_start_lo);

	return ee_start;
}

static uint64_t ext4_extent_idx_leaf_block(struct ext4_extent_idx *ei)
{
	uint64_t leaf_start = le16_to_cpu(ei->ei_leaf_hi);

	leaf_start = leaf_start << 32;
	leaf_start |= le32_to_cpu(ei->ei_leaf_lo);

	return leaf_start;
}

struct extent_iterator {
	struct ext2_inode	inode;
	struct ext4_extent	*ee;
	struct ext4_extent	*parent;
	size_t			total_entries;
};

static int extent_iterator_appending(struct ext2_editor_private_data *p,
				     struct extent_iterator *it,
				     struct ext4_extent_header *eh)
{
	int eh_depth = le16_to_cpu(eh->eh_depth);
	struct ext4_extent_idx *ei;
	uint8_t *blk;
	int ret = 0;

	blk = malloc(p->block_size);
	if (!blk) {
		fprintf(stderr, "Error: alloc one block failed\n");
		return -1;
	}

	if (le16_to_cpu(eh->eh_depth) > 0) {
		ei = (struct ext4_extent_idx *)(eh + 1);
		for (int i = 0; i < le16_to_cpu(eh->eh_entries); i++, ei++) {
			uint64_t blkno = ext4_extent_idx_leaf_block(ei);
			struct ext4_extent_header *eh_child;

			ret = ext2_read_blocks(p, blkno, 1,
						blk, p->block_size);
			if (ret < 0)
				goto done;

			eh_child = (struct ext4_extent_header *)blk;
			if (le32_to_cpu(eh_child->eh_magic) != EXT4_EXT_MAGIC) {
				fprintf(stderr, "Error: EXT4_EXT_MAGIC doesn't match at "
						"block #0x%" PRIu64 "\n",
					blkno);
				ret = -1;
				goto done;
			} else if (le16_to_cpu(eh_child->eh_depth) != eh_depth - 1) {
				fprintf(stderr, "Error: eh_depth doesn't match at "
						"block #0x%" PRIu64
						"it should be %d\n",
					blkno, eh_depth - 1);
				ret = -1;
				goto done;
			}

			ret = extent_iterator_appending(p, it, eh_child);
			if (ret < 0)
				goto done;
		}
	} else { /* eh_depth == 0 */
		size_t loaded_entries = it->total_entries;

		it->total_entries += le16_to_cpu(eh->eh_entries);
		it->parent = realloc(it->parent,
				sizeof(struct ext4_extent) * it->total_entries);
		if (!it->parent) {
			fprintf(stderr, "Error: alloc %zu ext4_extent failed\n",
				it->total_entries);
			ret = -1;
			goto done;
		}

		/* appending ext4_entry*/
		memcpy(&it->parent[loaded_entries],
			eh + 1,
			le16_to_cpu(eh->eh_entries) * sizeof(struct ext4_extent));
	}

done:
	if (ret < 0)
		free(it->parent);
	free(blk);

	return ret;
}

static int extent_iterator_init(struct ext2_editor_private_data *p,
				struct extent_iterator *it,
				uint32_t ino)
{
	struct ext4_extent_header *eh;
	uint16_t eh_depth;
	int ret;

	memset(it, 0, sizeof(*it));

	ret = ext2_read_inode(p, ino, &it->inode);
	if (ret < 0)
		return ret;

	if (!(le32_to_cpu(it->inode.flags) & EXT4_EXTENTS_FL)) {
		fprintf(stderr, "Error: inode #%d doesn't have EXT4_EXTENTS_FL\n",
			ino);
		return -1;
	}

	eh = (struct ext4_extent_header *)&it->inode.b;
	eh_depth = le16_to_cpu(eh->eh_depth);

	if (le32_to_cpu(eh->eh_magic) != EXT4_EXT_MAGIC) {
		fprintf(stderr, "Error: bad EXT4_EXT_MAGIC on inode #%d\n",
			ino);
		return -1;
	}

	if (eh_depth > 5) {
		fprintf(stderr, "Error: bad eh_depth number at inode #%d\n", ino);
		 return -1;
	}

	return extent_iterator_appending(p, it, eh);
}

static void extent_iterator_exit(struct extent_iterator *it)
{
	free(it->parent);
	it->total_entries = 0;
	it->parent = NULL;
	it->ee = NULL;
}

#define extent_list_foreach(it)						\
	for ((it)->ee = (it)->parent;					\
	     (unsigned)((it)->ee - (it)->parent) < (it)->total_entries;	\
	     (it)->ee = (it)->ee + 1)

/*
 * @dir: a temp variable that used when foreach.
 * @parent: where the ext2_dirent arrays actually loaded location.
 * @dirent_size: total bytes of loaded @parent.
 */
struct dirent_iterator {
	struct ext2_inode	inode;

	struct ext2_dirent	*dir;
	struct ext2_dirent	*parent;
	uint32_t		dirent_size;
};

static int dirent_iterator_init_dir_blocks(struct ext2_editor_private_data *p,
					   struct dirent_iterator *it,
					   uint32_t ino)
{
	struct ext2_inode_blocks b = { .blocks = NULL };
	int ret;

	ret = ext2_inode_blocks_read(p, &b, &it->inode, ino);
	if (ret < 0)
		return ret;

	if (b.total == 0) {
		fprintf(stderr, "Error: no dir_blocks defined in inode #%d\n",
			ino);
		return -1;
	}

	it->dirent_size = b.total * p->block_size;
	it->parent = calloc(b.total, p->block_size);
	if (!it->parent) {
		fprintf(stderr, "Error: %s alloc %zu blocks for inode #%d "
			"failed\n", __func__, b.total, ino);
		it->dirent_size = 0;
		ret = -1;
		goto done;
	}

	for (size_t i = 0; i < b.total; i++) {
		uint32_t blkno = b.blocks[i];
		int ret;

		ret = ext2_read_blocks(p, blkno, 1,
				       (void *)it->parent + i * p->block_size,
				       p->block_size);
		if (ret < 0) {
			fprintf(stderr, "Error: read block #%d failed\n",
				blkno);
			free(it->parent);
			it->parent = NULL;
			it->dirent_size = 0;
			break;
		}
	}

done:
	free(b.blocks);
	return ret;
}

static int dirent_iterator_init(struct ext2_editor_private_data *p,
				struct dirent_iterator *it,
				uint32_t ino)
{
	uint32_t total_ee_len = 0;
	struct extent_iterator eit;
	int ret;

	memset(it, 0, sizeof(*it));
	ret = ext2_read_inode(p, ino, &it->inode);
	if (ret < 0)
		return ret;

	if (!(le32_to_cpu(it->inode.mode) & INODE_MODE_S_IFDIR)) {
		fprintf(stderr, "Error: %s can not work due to inode #%d is not a dir\n",
			__func__, ino);
		return -1;
	}

	if (!(le32_to_cpu(it->inode.flags) & EXT4_EXTENTS_FL)) {
		/* the ext2 doesn't has ext4_extent and it will save data
		 * to inode->b.blocks
		 */
		return dirent_iterator_init_dir_blocks(p, it, ino);
	}

	ret = extent_iterator_init(p, &eit, ino);
	if (ret < 0)
		return ret;

	extent_list_foreach(&eit) {
		struct ext4_extent *ee = eit.ee;

		total_ee_len += le16_to_cpu(ee->ee_len);
	}

	it->dirent_size = total_ee_len * p->block_size;
	it->parent = malloc(it->dirent_size);
	if (!it->parent) {
		fprintf(stderr, "Error: malloc %d bytes dirent failed\n",
			it->dirent_size);
		ret = -1;
		goto done;
	}

	memset(it->parent, 0, it->dirent_size);
	extent_list_foreach(&eit) {
		struct ext4_extent *ee = eit.ee;

		ret = ext2_read_blocks(p, ext4_extent_start_block(ee),
				       le16_to_cpu(ee->ee_len),
				       (void *)(it->parent) +
				       le16_to_cpu(ee->ee_block) * p->block_size,
				       le16_to_cpu(ee->ee_len) * p->block_size);
		if (ret < 0)
			goto done;
	}

done:
	extent_iterator_exit(&eit);
	return ret;
}

static void dirent_iterator_exit(struct dirent_iterator *it)
{
	free(it->parent);
	it->dirent_size = 0;
	it->parent = NULL;
	it->dir = NULL;
}

#define dirent_iterator_next(it)					\
	((void *)((it)->dir) + le16_to_cpu((it)->dir->direntlen))
#define dirent_iterator_next_offset(it)					\
	(dirent_iterator_offset(it) + le16_to_cpu((it)->dir->direntlen))
#define dirent_iterator_offset(it)					\
	((unsigned long)(((void *)((it)->dir) - (void *)((it)->parent))))

#define dirent_list_foreach(it)						\
	for ((it)->dir = (it)->parent;					\
	       dirent_iterator_offset(it) < (it)->dirent_size &&	\
	       dirent_iterator_next_offset(it) <= (it)->dirent_size;	\
	       (it)->dir = dirent_iterator_next(it))			\

static int symlink_inode_get_target(struct ext2_editor_private_data *p,
				    struct ext2_inode *inode,
				    char *target, size_t sz)
{
	size_t target_sz = le32_to_cpu(inode->size);
	unsigned long blkno;

	/* is symlink? */
	if ((le16_to_cpu(inode->mode) & INODE_MODE_S_MASK) != INODE_MODE_S_IFLINK)
		return -1;
	/* assume the filename of symlink target is less than block_size */
	if (target_sz > p->block_size || target_sz >= sz)
		return -1;

	if (target_sz < sizeof(inode->b.symlink)) {
		snprintf(target, sz, "%s", inode->b.symlink);
		return 0;
	} else if (!(le32_to_cpu(inode->flags) & EXT4_EXTENTS_FL)) {
		blkno = inode->b.blocks.dir_blocks[0];
	} else {
		struct ext4_extent *ee = (void *)inode->b.inline_data
			+ sizeof(struct ext4_extent_header);

		blkno = le32_to_cpu(ee->ee_start_lo);
	}

	return ext2_read_blocks(p, blkno, 1, target, target_sz);
}

static int print_file(struct ext2_editor_private_data *p, uint32_t ino)
{
	char symlink_target[4096] = { 0 };
	struct ext2_inode inode;
	int ret;

	ret = ext2_read_inode(p, ino, &inode);
	if (ret < 0)
		return ret;

	switch (le16_to_cpu(inode.mode) & INODE_MODE_S_MASK) {
	case INODE_MODE_S_IFREG:
		printf(" %d bytes\n", le32_to_cpu(inode.size));;
		break;
	case INODE_MODE_S_IFLINK:
		ret = symlink_inode_get_target(p, &inode, symlink_target,
					       sizeof(symlink_target) - 1);
		if (ret < 0) {
			fflush(stdout);
			fprintf(stderr, "Error: read symlink target of inode %d failed\n",
				ino);
			return ret;
		}

		printf(" -> %s\n", symlink_target);
		break;
	default:
		printf("\n");
	}

	return 0;
}

static int print_dirent(struct ext2_editor_private_data *p, uint32_t ino,
		        int indent, int max_depth)
{
	struct dirent_iterator it;
	int ret;

	if (max_depth > 0 && indent + 1 > max_depth)
		return 0;

	ret = dirent_iterator_init(p, &it, ino);
	if (ret < 0)
		return ret;

	dirent_list_foreach(&it) {
		struct ext2_dirent *dir = it.dir;
		const char *filename = (const char *)(dir + 1);
		const char *filetype = NULL;

		if (le16_to_cpu(dir->direntlen) <= (int)sizeof(struct ext2_dirent)) {
			fprintf(stderr, "direntlen is too short\n");
			return -1;
		} else if (le32_to_cpu(dir->inode) == 0) {
			continue;
		}

		switch (dir->filetype) {
		case EXT4_FT_UNKNOWN:
			filetype = "unknown";
			break;
		case EXT4_FT_REG_FILE:
			filetype = "File";
			break;
		case EXT4_FT_DIR:
			filetype = "Directory";
			break;
		case EXT4_FT_CHRDEV:
			filetype = "Char";
			break;
		case EXT4_FT_BLKDEV:
			filetype = "Block";
			break;
		case EXT4_FT_FIFO:
			filetype = "FIFO";
			break;
		case EXT4_FT_SOCK:
			filetype = "Socket";
			break;
		case EXT4_FT_SYMLINK:
			filetype = "Link";
			break;
		default:
			fprintf(stderr, "unknown filetype 0x%x\n", dir->filetype);
			return -1;
		}

		if (dir->filetype == EXT4_FT_DIR) {
			/* skip '.' and '..' */
			if (dir->namelen == 1 && !strncmp(filename, ".", 1))
				continue;
			if (dir->namelen == 2 && !strncmp(filename, "..", 2))
				continue;
		}

		for (int i = 0; i < indent; i++)
			printf("|   ");
		printf("|-- #%08d: %-10s %.*s", le32_to_cpu(dir->inode),
			filetype, dir->namelen, filename);

		if (dir->filetype == EXT4_FT_DIR) {
			printf("\n");
			ret = print_dirent(p, le32_to_cpu(dir->inode),
					   indent + 1, max_depth);
			if (ret < 0)
				break;
		} else {
			ret = print_file(p, le32_to_cpu(dir->inode));
			if (ret < 0)
				break;
		}
	}

	dirent_iterator_exit(&it);
	return ret;
}

static void ext2_init_csum_seed(struct ext2_editor_private_data *p)
{
	struct ext2_sblock *sblock = &p->sblock;

	if (le32_to_cpu(sblock->feature_incompat) & EXT4_FEATURE_INCOMPAT_CSUM_SEED) {
		p->csum_seed = le32_to_cpu(sblock->checksum_seed);
		return;
	}

	libcrc32_init_seed(&p->crc32c_le, 0xffffffff);
	libcrc32_update(&p->crc32c_le, sblock->unique_id, sizeof(sblock->unique_id));
	p->csum_seed = libcrc32_finish(&p->crc32c_le);
}

static int qsort_compare_layout(const void *a, const void *b)
{
	const struct disk_layout *la = a, *lb = b;

	return la->start_block - lb->start_block;
}

static bool is_power_of(int a, int b)
{
	while (a > b) {
		if (a % b)
			return 0;
		a /= b;
	}

	return (a == b);
}

static bool ext2_block_group_has_backup_sb(struct ext2_sblock *sb, size_t group)
{
	if (ext2_has_ro_compat_feature(sb, EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER)) {
		uint8_t power_bases[] = {7, 5, 3};

		/* the sequence of powers of 3, 5, and 7:
		 * 1, 3, 5, 7, 9, 25, 27, 49, 81, ...
		 */
		if (group == 1)
			return true;

		for (size_t i = 0; i < sizeof(power_bases); i++) {
			if (is_power_of(group, power_bases[i]))
				return true;
		}

		return false;
	}

	return true;
}

static int ext2_editor_init_layouts(struct ext2_editor_private_data *p)
{
	struct ext2_sblock *sblock = &p->sblock;
	uint16_t descriptor_size;

	descriptor_size = le16_to_cpu(sblock->descriptor_size);
	/* sblock->descriptor_size maybe zero on some ext2 filesystem */
	if (descriptor_size == 0)
		descriptor_size = 32;

	/* inode tables */
	for (size_t group = 0; group < p->n_block_group; group++) {
		struct ext2_block_group *bgrp = &p->block_groups[group];
		uint32_t block_start, block_len;

		block_start = group * le32_to_cpu(sblock->blocks_per_group);

		/* super block */
		if (group == 0 || ext2_block_group_has_backup_sb(sblock, group)) {
			ext2_editor_register_layout(p, DISK_LAYOUT_SUPBER_BLOCK,
						    group,
						    block_start, 1);

			/* block descriptor */
			block_start += 1;
			block_len = aligned_block(p->n_block_group * descriptor_size,
						p->block_size);
			ext2_editor_register_layout(p, DISK_LAYOUT_GROUP_DESCRIPTOR,
						group, block_start, block_len);
			block_start += block_len;

			/* Reserved GDT */
			block_len = le16_to_cpu(sblock->reserved_gdt_blocks);
			if (block_len > 0) {
				ext2_editor_register_layout(p, DISK_LAYOUT_RESERVED_GDT,
							group, block_start, block_len);
				block_start += block_len;
			}
		}

		ext2_editor_register_layout(p, DISK_LAYOUT_INODE_BITMAP, group,
					    le32_to_cpu(bgrp->inode_id),
					    1);

		ext2_editor_register_layout(p, DISK_LAYOUT_DATA_BLOCK_BITMAP,
					    group,
					    le32_to_cpu(bgrp->block_id),
					    1);

		block_len = aligned_block(
			p->inode_size * le32_to_cpu(sblock->inodes_per_group),
			p->block_size);

		ext2_editor_register_layout(p, DISK_LAYOUT_INODE_TABLE, group,
					    le32_to_cpu(bgrp->inode_table_id),
					    block_len);
	}

	/* sort it */
	qsort(p->layouts, p->n_layout, sizeof(p->layouts[0]),
		qsort_compare_layout);

	return 0;
}

static int64_t ext2_total_size(void *private_data, int fd);

static int ext2_detect(void *private_data, int force_type, int fd)
{
	struct ext2_editor_private_data *p = private_data;
	struct ext2_sblock *sblock = &p->sblock;
	uint16_t descriptor_size;
	int ret;

	/* save fd to private_data */
	p->fd = fd;

	fileseek(fd, SUPERBLOCK_START);
	ret = fileread(fd, sblock, sizeof(*sblock));
	if (ret < 0)
		return ret;

	/* NOTE:
	 *
	 * The block group descriptor is layouting after super block.
	 * We need check the block group descriptor's checksum to detect
	 * this image since we have many backup supber blocks but only one
	 * block group descriptor.
	 */
	if (imgeditor_in_search_mode() && ext2_has_block_group_csum(sblock) == 0)
		return -1;

	ret = ext2_check_sblock(&p->crc32c_le, force_type, sblock);
	if (ret < 0)
		return ret;

	ext2_init_csum_seed(p);

	p->block_size = ext2_sblock_log2_size_to_bytes(sblock->log2_block_size);
	p->fragment_size = ext2_sblock_log2_size_to_bytes(sblock->log2_fragment_size);
	p->n_block_group =
		aligned_length((sblock->total_blocks - sblock->first_data_block),
				sblock->blocks_per_group) / sblock->blocks_per_group;

	/* The default blocks_per_group is 32K,
	 * one group = 32K * 4K = 128M,
	 * for a 1T partition it need 1T / 128M = 1024 * 1024 / 128 = 8192
	 */
	if (p->n_block_group > 8192) {
		fprintf_if_force_type("Error: too many block groups(%d)\n",
					p->n_block_group);
		return -1;
	}

	descriptor_size = le16_to_cpu(sblock->descriptor_size);
	/* sblock->descriptor_size maybe zero on some ext2 filesystem */
	if (descriptor_size == 0)
		descriptor_size = 32;

	if (descriptor_size > sizeof(struct ext2_block_group)) {
		fprintf_if_force_type("Error: too large block group "
					"descriptor size %d\n",
					descriptor_size);
		return -1;
	}

	if (le32_to_cpu(sblock->revision_level) == 0)
		p->inode_size = 128;
	else
		p->inode_size = le16_to_cpu(sblock->inode_size);

	if (filelength(fd) < ext2_total_size(private_data, fd)) {
		fprintf_if_force_type("Error: file is incompleted\n");
		return -1;
	}

	p->block_groups = calloc(p->n_block_group,
				 sizeof(struct ext2_block_group));
	if (!p->block_groups) {
		fprintf(stderr, "Error: alloc %d ext2_block_groups failed\n",
			p->n_block_group);
		return -1;
	}

	/* loading all block groups */
	fileseek(fd, p->block_size);
	for (uint32_t i = 0; i < p->n_block_group; i++) {
		struct ext2_block_group *group = &p->block_groups[i];

		read(fd, group, descriptor_size);

		if (ext2_has_block_group_csum(sblock)) {
			uint32_t sum;

			if (ext2_has_ro_compat_feature(sblock,
				EXT4_FEATURE_RO_COMPAT_METADATA_CSUM)) {
				uint32_t oldcrc;

				oldcrc = group->bg_checksum;
				group->bg_checksum = 0;

				libcrc32_init_seed(&p->crc32c_le, p->csum_seed);
				libcrc32_update(&p->crc32c_le, &i, sizeof(i));
				libcrc32_update(&p->crc32c_le, group, descriptor_size);
				sum = libcrc32_finish(&p->crc32c_le);

				group->bg_checksum = oldcrc;
			} else {
				size_t offset = offsetof(struct ext2_block_group, bg_checksum);

				libcrc16_init_seed(&p->crc16, 0xffff);
				libcrc16_update(&p->crc16, sblock->unique_id,
						sizeof(sblock->unique_id));
				libcrc16_update(&p->crc16, &i, sizeof(i));
				libcrc16_update(&p->crc16, group, offset);

				offset += sizeof(group->bg_checksum);
				if (offset < descriptor_size)
					libcrc16_update(&p->crc16,
							(void *)group + offset,
							descriptor_size - offset);
				sum = libcrc16_finish(&p->crc16);
			}

			if ((sum & 0xffff) != le32_to_cpu(group->bg_checksum)) {
				fprintf_if_force_type("Error: bad bg_checksum on "
					"group descriptor %d (%08x != %08x)\n",
					i, sum, le32_to_cpu(group->bg_checksum));
				ext2_editor_exit(p);
				return -1;
			}
		}
	}

	ret = ext2_read_inode(p, EXT2_ROOT_INO, &p->root_inode);
	if (ret < 0) {
		fprintf_if_force_type("Error: read root inode failed\n");
		ext2_editor_exit(p);
		return ret;
	}

	ext2_editor_init_layouts(p);
	return 0;
}

static int64_t ext2_total_size(void *private_data, int fd)
{
	struct ext2_editor_private_data *p = private_data;
	int64_t sz = le32_to_cpu(p->sblock.total_blocks);

	return sz * p->block_size;
}

static int ext2_summary(void *private_data, int fd, char *buf, size_t bufsz)
{
	struct ext2_editor_private_data *p = private_data;
	int disk_mb = ext2_total_size(private_data, fd) >> 20;

	snprintf(buf, bufsz,
		"%dMiB on %.*s",
		disk_mb,
		(int)sizeof(p->sblock.last_mounted_on),
		p->sblock.last_mounted_on);
	return 0;
}

static int ext2_print_header_info(struct ext2_editor_private_data *p)
{
	struct ext2_sblock *sblock = &p->sblock;

	structure_print("%-34s: ", sblock, ext2_sblock_structure);

	for (uint32_t i = 0; i < p->n_block_group; i++) {
		struct ext2_block_group *group = &p->block_groups[i];

		printf("\n");
		printf("Group %2d: \n", i);
		structure_print("    %-30s: ", group, ext2_block_group_structure_32);
		if (le32_to_cpu(sblock->descriptor_size) == 64) {
			structure_print("    %-30s: ", &group,
					ext2_block_group_structure_64);
		}
	}

	return 0;
}

static int ext2_do_sblock(void *private_data, int fd, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;

	ext2_print_header_info(p);
	return 0;
}

/* show inode informations */
static int ext2_do_inode(void *private_data, int fd, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;
	uint32_t blkno, blk_offset;
	struct ext2_inode inode;
	int ino = -1;
	int ret;

	if (argc > 1)
		ino = (int)strtol(argv[1], NULL, 0); /* auto detect dec or hex */

	if (ino < 0) {
		fprintf(stderr, "Usage: ext2 inode #ino\n");
		return -1;
	}

	ret = _ext2_read_inode(p, ino, &inode, &blkno, &blk_offset);
	if (ret < 0)
		return ret;

	printf("inode #%d location on blk #%d + 0x%04x\n",
		ino, blkno, blk_offset);
	structure_print_ext2_inode("%-30s: ", &inode);
	return 0;
}

/* show block raw data */
static int ext2_do_block(void *private_data, int fd, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;
	int blkno = -1;
	uint8_t *blk;

	if (argc > 1)
		blkno = (int)strtol(argv[1], NULL, 0);

	if (blkno < 0) {
		fprintf(stderr, "Usage: ext2 block #blkno\n");
		return -1;
	}

	blk = ext2_alloc_read_block(p, blkno);
	if (!blk)
		return -1;

	hexdump(blk, p->block_size, blkno * p->block_size);

	free(blk);
	return 0;
}

/* show dirent in block address */
static int ext2_do_dirent(void *private_data, int fd, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;
	int ret = 0, blkno = -1;
	unsigned long offset = 0;
	uint8_t *blk;

	if (argc > 1)
		blkno = (int)strtol(argv[1], NULL, 0);

	if (blkno < 0) {
		fprintf(stderr, "Usage: ext2 dirent #blkno\n");
		return -1;
	}

	blk = ext2_alloc_read_block(p, blkno);
	if (!blk)
		return -1;

	while (1) {
		struct ext2_dirent *dir = (struct ext2_dirent *)(blk + offset);

		if (offset >= p->block_size)
			break;
		if (offset + le16_to_cpu(dir->direntlen) > p->block_size)
			break;
		if (le32_to_cpu(dir->inode) == 0)
			goto next;

		if (le16_to_cpu(dir->direntlen) <= (int)sizeof(*dir)) {
			fprintf(stderr, "Error: bad dirent struct on block #%ld\n",
				blkno * p->block_size + offset);
			ret = -1;
			break;
		}

		printf("%08lx: inode:     #%d\n",
			offset + offsetof(struct ext2_dirent, inode),
			le32_to_cpu(dir->inode));
		printf("%08lx: direntlen: %d\n",
			offset + offsetof(struct ext2_dirent, direntlen),
			le16_to_cpu(dir->direntlen));
		printf("%08lx: namelen:   %d\n",
			offset + offsetof(struct ext2_dirent, namelen),
			dir->namelen);
		printf("%08lx: filetype:  %d\n",
			offset + offsetof(struct ext2_dirent, filetype),
			dir->filetype);
		printf("%08lx: name:      %.*s\n",
			offset + sizeof(struct ext2_dirent),
			dir->namelen, (const char *)(dir + 1));
		printf("\n");

	next:
		offset += le16_to_cpu(dir->direntlen);
	}

	free(blk);
	return ret;
}

/* show extent in block address */
static int ext2_do_extent(void *private_data, int fd, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;
	struct ext4_extent_header *eh;
	int blkno = -1;
	uint8_t *blk;

	if (argc > 1)
		blkno = (int)strtol(argv[1], NULL, 0);

	if (blkno < 0) {
		fprintf(stderr, "Usage: ext2 extent #blkno\n");
		return -1;
	}

	blk = ext2_alloc_read_block(p, blkno);
	if (!blk)
		return -1;

	eh = (struct ext4_extent_header *)blk;
	if (le16_to_cpu(eh->eh_magic) != EXT4_EXT_MAGIC) {
		fprintf(stderr, "Error: magic doesn't match\n");
		free(blk);
		return -1;
	}

	structure_print_ext4_extent("%-30s: ", eh);
	free(blk);
	return 0;
}

static int ext2_do_list(void *private_data, int fd, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;
	int depth = 2;

	printf("ext2: ext filesystem image\n");

	for (int i = 0; i < argc; i++) {
		const char *opt = argv[i];

		if (!strcmp(opt, "--depth")) {
			if (++i < argc)
				depth = atoi(argv[i]);
		}
	}

	return print_dirent(p, EXT2_ROOT_INO, 0, depth);
}

static size_t journal_descriptor_blocktag_size(uint32_t incompat_features)
{
	size_t sz;

	/* the size is 16 (without UUID) or 32 bytes when CSUM_V3 */
	if (incompat_features & JBD2_FEATURE_INCOMPAT_CSUM_V3)
		return sizeof(struct journal_block_tag3_s);

	/* IF CSUM_V3 is not set, teh size is 8, 12, 24, or 28 bytes */
	sz = sizeof(struct journal_block_tag_s);

	if (!(incompat_features & JBD2_FEATURE_INCOMPAT_64BIT))
		sz -= sizeof(__be32); /* t_blocknr_high */

	return sz;
}

struct journal_foreach_arg {
	struct ext2_editor_private_data		*ext;
	void					*journal_buf;
	size_t					journal_size;

	struct journal_superblock_t		*sb;
	int					break_next;

	int (*class_handler)(struct journal_foreach_arg *arg,
			     struct journal_header_t *this, void *data,
			     uint32_t journal_blkno);

	int (*desc_handler)(struct journal_foreach_arg *arg,
			    struct journal_header_t *this,
			    uint64_t fs_blocknr, uint64_t journal_blocknr,
			    uint32_t flags);
};

static int ext2_journal_print_class(struct journal_foreach_arg *arg,
				    struct journal_header_t *h,
				    void *data,
				    uint32_t journal_blkno)
{
	struct commit_header *commit = data;
	struct journal_revoke_header_t *revoke = data;

	if (get_verbose_level() == 0) {
		uint32_t seq = be32_to_cpu(arg->sb->s_sequence);
		uint32_t thisseq = be32_to_cpu(h->h_sequence);

		if (thisseq < seq)
			return 0;
	}

	switch (be32_to_cpu(h->h_blocktype)) {
	case EXT3_JOURNAL_DESCRIPTOR_BLOCK:
	case EXT3_JOURNAL_COMMIT_BLOCK:
	case EXT3_JOURNAL_SUPERBLOCK_V1:
	case EXT3_JOURNAL_SUPERBLOCK_V2:
	case EXT3_JOURNAL_REVOKE_BLOCK:
		printf("Journal %4d on blk #%-5d ",
			be32_to_cpu(h->h_sequence), journal_blkno);
		break;
	default:
		return -1;
	}

	switch (be32_to_cpu(h->h_blocktype)) {
	case EXT3_JOURNAL_SUPERBLOCK_V1:
	case EXT3_JOURNAL_SUPERBLOCK_V2:
		printf("%-13s sequence=%d", "Superblock",
			be32_to_cpu(arg->sb->s_sequence));
		break;
	case EXT3_JOURNAL_DESCRIPTOR_BLOCK:
		printf("%-13s ", "Descriptor");
		break;
	case EXT3_JOURNAL_COMMIT_BLOCK:
		{
			char buf[64];
			struct tm tm = { 0 };
			time_t t = be64_to_cpu(commit->h_commit_sec);

			printf("%-13s", "Commit");

			localtime_r(&t, &tm);
			strftime(buf, sizeof(buf), "%FT%T", &tm);
			printf(" %s.%03d",
				buf,
				be32_to_cpu(commit->h_commit_nsec) / 1000000);
		}
		break;
	case EXT3_JOURNAL_REVOKE_BLOCK:
		printf("%-13s", "Revocation");
		printf(" count=%d", be32_to_cpu(revoke->r_count) / 4);
		for (unsigned int i = 0;
			i < be32_to_cpu(revoke->r_count); i += 4) {
			__be32 *p_revoke_blk = (void *)(revoke + 1) + i;

			printf(" %d", be32_to_cpu(*p_revoke_blk));
		}
		break;
	}

	printf("\n");
	return 0;
}

static int ext2_journal_print_desc(struct journal_foreach_arg *arg,
				   struct journal_header_t *this,
				   uint64_t fs_blocknr, uint64_t journal_blocknr,
				   uint32_t flags)
{

	char *fs_blocknr_info = NULL, s[256];

	if (get_verbose_level() == 0) {
		const struct disk_layout *layout;
		uint32_t seq = be32_to_cpu(arg->sb->s_sequence);
		uint32_t thisseq = be32_to_cpu(this->h_sequence);

		if (thisseq < seq)
			return 0;

		layout = ext2_editor_disk_layout_match_block(arg->ext,
							     fs_blocknr);
		if (!layout) {
			int ino = -1;

			if (!ext2_whohas_blkno(arg->ext, fs_blocknr, &ino)) {
				snprintf(s, sizeof(s), "inode #%d", ino);
				fs_blocknr_info = s;
			}
		} else {
			fs_blocknr_info =
				format_disk_layout(layout, s, sizeof(s));
		}
	}

	printf("%27sFS blocknr %-8" PRIu64
		"logged at journal block %-5" PRIu64 " with flags 0x%x",
		"", fs_blocknr, journal_blocknr, flags);

	if (fs_blocknr_info)
		printf(" (%s)", fs_blocknr_info);

	printf("\n");

	return 0;
}

static int ext2_journal_foreach_descriptor(struct journal_foreach_arg *arg,
					  struct journal_header_t *this,
					  void *data,
					  uint32_t journal_start_blkno,
					  uint32_t *ret_data_blks)
{
	uint32_t incompat_features = be32_to_cpu(arg->sb->s_feature_incompat);
	size_t tagsize = journal_descriptor_blocktag_size(incompat_features);
	size_t blocksize = be32_to_cpu(arg->sb->s_blocksize);
	size_t tailsize = 0;
	uint32_t data_blks = 0;
	void *p = data;

	if (incompat_features & JBD2_FEATURE_INCOMPAT_CSUM_V2_3)
		tailsize = sizeof(struct jbd2_journal_block_tail);

	while ((size_t)(p - (void *)this) <= blocksize - tagsize - tailsize) {
		struct journal_block_tag3_s *tag3 = p;
		struct journal_block_tag_s *tag = p;
		uint64_t blocknr = 0;
		uint32_t flags;

		++data_blks;

		if (incompat_features & JBD2_FEATURE_INCOMPAT_CSUM_V3) {
			blocknr = be32_to_cpu(tag3->t_blocknr_high);
			blocknr <<= 32;
			blocknr |= be32_to_cpu(tag3->t_blocknr);
			flags = be32_to_cpu(tag3->t_flags);
		} else {
			if (incompat_features & JBD2_FEATURE_INCOMPAT_64BIT)
				blocknr = be32_to_cpu(tag->t_blocknr_high);
			blocknr <<= 32;
			blocknr |= be32_to_cpu(tag->t_blocknr);
			flags = be16_to_cpu(tag->t_flags);
		}

		if (arg->desc_handler) {
			arg->desc_handler(arg, this, blocknr,
					 journal_start_blkno + data_blks,
					 flags);
		}

		if (flags & EXT3_JOURNAL_FLAG_LAST_TAG)
			break;

		p += tagsize;
		if (!(flags & EXT3_JOURNAL_FLAG_SAME_UUID))
			p += 16;
	}

	*ret_data_blks = data_blks;
	return 0;
}

static int ext2_foreach_journal(struct journal_foreach_arg *arg)
{
	void *p = arg->journal_buf;
	uint32_t journal_blkno = 0;

	arg->break_next = 0;
	arg->sb = arg->journal_buf + sizeof(struct journal_header_t);

	while (p - arg->journal_buf < (int)arg->journal_size) {
		struct journal_header_t *h = p;
		uint32_t blocktype = be32_to_cpu(h->h_blocktype);
		uint32_t skip_size = be32_to_cpu(arg->sb->s_blocksize);

		if (be32_to_cpu(h->h_magic) != EXT3_JOURNAL_MAGIC_NUMBER)
			goto next;

		switch (blocktype) {
		case EXT3_JOURNAL_DESCRIPTOR_BLOCK:
		case EXT3_JOURNAL_COMMIT_BLOCK:
		case EXT3_JOURNAL_SUPERBLOCK_V1:
		case EXT3_JOURNAL_SUPERBLOCK_V2:
		case EXT3_JOURNAL_REVOKE_BLOCK:
			if (arg->class_handler) {
				arg->class_handler(arg, h, h + 1, journal_blkno);
				if (arg->break_next)
					return 0;
			}
			break;
		default:
			goto next;
		}

		switch (blocktype) {
		case EXT3_JOURNAL_DESCRIPTOR_BLOCK:
			{
				uint32_t n_data_blk = 0;

				/* In general, the dta blocks being written to
				 * disk through the journal are written
				 * verbatim into the journal file after the
				 * descriptor block.
				 */
				ext2_journal_foreach_descriptor(arg, h, h + 1,
								journal_blkno,
								&n_data_blk);

				journal_blkno += n_data_blk;
				skip_size +=
					(n_data_blk * be32_to_cpu(arg->sb->s_blocksize));
			}
			break;
		}

	next:
		p += skip_size;
		journal_blkno++;
	}

	return 0;
}

static int ext2_do_journal_list(struct ext2_editor_private_data *ext2,
				void *journal_buf, size_t journal_size)
{
	struct journal_foreach_arg arg = {
		.ext			= ext2,
		.journal_buf		= journal_buf,
		.journal_size		= journal_size,
		.class_handler		= ext2_journal_print_class,
		.desc_handler		= ext2_journal_print_desc,
	};

	return ext2_foreach_journal(&arg);
}

static int ext2_do_journal_superblock(void *journal_buf, size_t journal_size)
{
	structure_print("%-30s", journal_buf, ext4_journal_header_structure);

	structure_print("%-30s",
			journal_buf + sizeof(struct journal_header_t),
			ext4_journal_sblock_structure);
	return 0;
}

static int ext2_do_journal_block(int argc, char **argv,
				 void *journal_buf, size_t journal_size)
{
	struct journal_superblock_t *sb = journal_buf + sizeof(struct journal_header_t);
	uint32_t blksize = be32_to_cpu(sb->s_blocksize);
	uint32_t blkno;

	if (argc != 2) {
		fprintf(stderr, "Usage: journal block #blkno\n");
		return -1;
	}

	blkno = (uint32_t)strtol(argv[1], NULL, 10);
	if (blkno * blksize >= journal_size) {
		fprintf(stderr, "Error: journal blk #%d overrange\n", blkno);
		return -1;
	}

	hexdump(journal_buf + blkno * blksize, blksize, 0);
	return 0;
}

static int ext2_do_journal(void *private_data, int fd, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;
	uint64_t journal_size;
	void *journal_buf;
	int ret = -1;

	journal_buf = alloc_inode_file(p, le32_to_cpu(p->sblock.journal_inode),
				       &journal_size);
	if (!journal_buf)
		return -1;

	if (argc <= 1)
		ret = ext2_do_journal_list(p, journal_buf, journal_size);
	else if (!strcmp(argv[1], "sblock"))
		ret = ext2_do_journal_superblock(journal_buf, journal_size);
	else if (!strcmp(argv[1], "block"))
		ret = ext2_do_journal_block(argc - 1, argv + 1, journal_buf, journal_size);

	free(journal_buf);
	return ret;
}

static int ext2_do_orphan(void *private_data, int fd, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;
	uint32_t total_inodes = le32_to_cpu(p->sblock.total_inodes);

	for (uint32_t i = 1; i < total_inodes; i++) {
		struct ext2_inode inode;
		uint32_t dtime;
		int ret;

		ret = ext2_read_inode(p, i, &inode);
		if (ret < 0) {
			fprintf(stderr, "Warnning: scan not completed. "
					"%d/%d\n",
				i, total_inodes);
			break;
		}

		if (be16_to_cpu(inode.nlinks)) /* inode was alive */
			continue;

		dtime = le32_to_cpu(inode.dtime);
		if (dtime && dtime < total_inodes)
			printf("inode #%-6d in orphan list\n", i);
	}

	return 0;
}

static int ext2_do_layout(void *private_data, int fd, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;

	printf("%8s %8s %8s\n", "start", "end", "blocks");
	for (size_t i = 0; i < p->n_layout; i++) {
		struct disk_layout *layout = &p->layouts[i];
		char s[256];

		if (layout->major_type < 0
			|| layout->major_type >= DISK_LAYOUT_MAX)
			return -1;

		printf("%8d %8d %8d ",
			layout->start_block,
			layout->start_block + layout->len_block - 1,
			layout->len_block);

		format_disk_layout(layout, s, sizeof(s));
		printf("%s", s);

		printf("\n");
	}

	return 0;
}

/* show extent in block address */
static int ext2_do_whohas(void *private_data, int fd, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;
	const struct disk_layout *layout;
	int ino = -1, blkno = -1;
	int ret;

	if (argc > 1)
		blkno = (int)strtol(argv[1], NULL, 0);

	if (blkno < 0) {
		fprintf(stderr, "Usage: ext2 whohas #blkno\n");
		return -1;
	}

	layout = ext2_editor_disk_layout_match_block(p, blkno);
	if (layout) {
		char s[256];

		format_disk_layout(layout, s, sizeof(s));
		printf("%s\n", s);
		return 0;
	}

	ret = ext2_whohas_blkno(p, blkno, &ino);
	if (!ret) {
		printf("inode #%d\n", ino);
		return ret;
	}

	return -1;
}

static int ext2_main(void *private_data, int fd, int argc, char **argv)
{
	if (argc >= 1) {
		if (!strcmp(argv[0], "list"))
			return ext2_do_list(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "sblock"))
			return ext2_do_sblock(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "inode"))
			return ext2_do_inode(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "block"))
			return ext2_do_block(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "dirent"))
			return ext2_do_dirent(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "extent"))
			return ext2_do_extent(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "journal"))
			return ext2_do_journal(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "orphan"))
			return ext2_do_orphan(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "layout"))
			return ext2_do_layout(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "whohas"))
			return ext2_do_whohas(private_data, fd, argc, argv);
	}

	return ext2_do_list(private_data, fd, argc, argv);
}

static int unpack_symlink(struct ext2_editor_private_data *p, uint32_t ino,
			  const char *link)
{
	char symlink_target[4096] = { 0 };
	struct ext2_inode inode;
	int ret;

	ret = ext2_read_inode(p, ino, &inode);
	if (ret < 0)
		return ret;

	if ((le16_to_cpu(inode.mode) & INODE_MODE_S_IFLINK) == 0) {
		fprintf(stderr, "Error: unpack symlink %s failed"
				"(ino #%d is not symlink)\n",
			inode.b.symlink, ino);
		return -1;
	}

	ret = symlink_inode_get_target(p, &inode, symlink_target,
				       sizeof(symlink_target) - 1);
	if (ret < 0) {
		fprintf(stderr, "Error: get symlink target of inode %d failed\n",
			ino);
		return ret;
	}

	unlink(link);
	ret = symlink(symlink_target, link);
	if (ret < 0) {
		fprintf(stderr, "Error: Create symlink %s point to %s"
				"(ino #%d) failed: %m\n",
			link, symlink_target, ino);
		return ret;
	}

	return 0;
}

static int unpack_file_from_dir_blocks(struct ext2_editor_private_data *p,
				       struct ext2_inode *inode, uint32_t ino,
				       const char *filename, int fd)
{
	struct ext2_inode_blocks b = { .blocks = NULL };
	uint32_t filesize = le32_to_cpu(inode->size);
	size_t copied = 0;
	int ret;

	ret = ext2_inode_blocks_read(p, &b, inode, ino);
	if (ret < 0)
		return ret;

	if (filesize > b.total * p->block_size) {
		fprintf(stderr, "Error: unpack %s failed(filesize %u and "
			"dir_blocks %zu doesn't match)\n",
			filename, filesize, b.total);
		ret = -1;
		goto done;
	}

	for (size_t i = 0; i < b.total && filesize > 0; i++) {
		uint32_t chunk_sz = filesize;

		if (chunk_sz > p->block_size)
			chunk_sz = p->block_size;

		dd(p->fd, fd, b.blocks[i] * p->block_size, copied, chunk_sz,
		   NULL, NULL);

		filesize -= chunk_sz;
		copied += chunk_sz;
	}

done:
	free(b.blocks);
	return ret;
}

static int unpack_file(struct ext2_editor_private_data *p, uint32_t ino,
		       const char *filename)
{
	struct ext2_inode inode;
	struct extent_iterator it;
	uint64_t filesz;
	int fd, mode;
	int ret;

	ret = ext2_read_inode(p, ino, &inode);
	if (ret < 0)
		return ret;

	if ((le16_to_cpu(inode.mode) & INODE_MODE_S_IFLINK) == 0) {
		fprintf(stderr, "Error: unpack file %s failed"
				"(ino #%d is not a file)\n",
			filename, ino);
		return -1;
	}

	filesz = le32_to_cpu(inode.size_high);
	filesz = filesz << 32;
	filesz |= le32_to_cpu(inode.size);
	mode = le16_to_cpu(inode.mode) & INODE_MODE_PERMISSION_MASK;

	fd = fileopen(filename, O_RDWR | O_CREAT | O_TRUNC, mode);
	if (fd < 0)
		return fd;

	if (filesz == 0) /* create a empty file */
		goto done;

	if (!(le32_to_cpu(inode.flags) & EXT4_EXTENTS_FL)) {
		/* this is ext2 based filesystem */
		ret = unpack_file_from_dir_blocks(p, &inode, ino, filename, fd);
		goto done;
	}

	ret = extent_iterator_init(p, &it, ino);
	if (ret < 0)
		goto done;

	extent_list_foreach(&it) {
		struct ext4_extent *ee = it.ee;
		size_t sz, bytes = p->block_size * le16_to_cpu(ee->ee_len);

		if (bytes > filesz)
			bytes = filesz;

		sz = dd(p->fd, fd,
			ext4_extent_start_block(ee) * p->block_size,
			le32_to_cpu(ee->ee_block) * p->block_size,
			bytes,
			NULL, NULL);
		if (sz != bytes) {
			fprintf(stderr, "Error: saving %s failed\n",
				filename);
			ret = -1;
			break;
		}

		filesz -= bytes;
	}
	extent_iterator_exit(&it);

done:
	close(fd);
	/* this file maybe already created, the file mode passwd by open
	 * doesn't work. let's fix it's mode now
	 */
	chmod(filename, mode);

	return ret;
}

static int load_file_from_dir_blocks(struct ext2_editor_private_data *p,
				       struct ext2_inode *inode, uint32_t ino,
				       void *buf, uint64_t bufsz)
{
	struct ext2_inode_blocks b = { .blocks = NULL };
	size_t copied = 0;
	int ret;

	ret = ext2_inode_blocks_read(p, &b, inode, ino);
	if (ret < 0)
		return ret;

	if (bufsz > b.total * p->block_size) {
		fprintf(stderr, "Error: load inode #%d failed(bufsize %ld and "
			"dir_blocks size %zu doesn't match)\n",
			ino, (long)bufsz, b.total);
		ret = -1;
		goto done;
	}

	for (size_t i = 0; i < b.total && bufsz > 0; i++) {
		uint32_t chunk_sz = bufsz;

		if (chunk_sz > p->block_size)
			chunk_sz = p->block_size;

		fileseek(p->fd, b.blocks[i] * p->block_size);
		read(p->fd, buf + copied, chunk_sz);

		copied += chunk_sz;
		bufsz -= chunk_sz;
	}

done:
	free(b.blocks);
	return ret;
}

static void *alloc_inode_file(struct ext2_editor_private_data *p, uint32_t ino,
			      uint64_t *ret_filesize)
{
	struct ext2_inode inode;
	struct extent_iterator it;
	void *buf = NULL;
	uint64_t filesz;
	int ret;

	ret = ext2_read_inode(p, ino, &inode);
	if (ret < 0)
		return buf;

	if ((le16_to_cpu(inode.mode) & INODE_MODE_S_IFLINK) == 0) {
		fprintf(stderr, "Error: alloc inode file #%d failed"
				"(not a file)\n", ino);
		return buf;
	}

	filesz = le32_to_cpu(inode.size_high);
	filesz = filesz << 32;
	filesz |= le32_to_cpu(inode.size);
	*ret_filesize = filesz;

	buf = calloc(1, filesz);
	if (!buf)
		return buf;

	if (filesz == 0) /* empty file */
		return buf;

	if (!(le32_to_cpu(inode.flags) & EXT4_EXTENTS_FL)) {
		/* this is ext2 based filesystem */
		ret = load_file_from_dir_blocks(p, &inode, ino, buf, filesz);
		if (ret < 0) {
			free(buf);
			return NULL;
		}

		return buf;
	}

	ret = extent_iterator_init(p, &it, ino);
	if (ret < 0) {
		free(buf);
		return NULL;
	}

	extent_list_foreach(&it) {
		struct ext4_extent *ee = it.ee;
		size_t bytes = p->block_size * le16_to_cpu(ee->ee_len);

		if (bytes > filesz)
			bytes = filesz;

		fileseek(p->fd,
			ext4_extent_start_block(ee) * p->block_size);

		read(p->fd,
		     buf + le32_to_cpu(ee->ee_block) * p->block_size,
		     bytes);

		filesz -= bytes;
	}
	extent_iterator_exit(&it);

	return buf;
}

static int ext2_whohas_blkno(struct ext2_editor_private_data *p,
			     uint32_t which_blk,
			     int *ret_ino)
{
	struct ext2_sblock *sb = &p->sblock;
	int inodes_used = le32_to_cpu(sb->total_inodes)
			- le32_to_cpu(sb->free_inodes);

	for (int ino = 1; ino <= inodes_used; ino++) {
		struct ext2_inode inode;
		uint64_t filesz;
		uint32_t type;
		int ret;

		ret = ext2_read_inode(p, ino, &inode);
		if (ret < 0)
			continue;

		filesz = le32_to_cpu(inode.size_high);
		filesz = filesz << 32;
		filesz |= le32_to_cpu(inode.size);
		if (filesz == 0)
			continue;

		/* empty inode */
		if (le32_to_cpu(inode.ctime) == 0)
			continue;

		/* search in regular file and directory only */
		type = le32_to_cpu(inode.mode) & INODE_MODE_S_MASK;
		if (!(type == INODE_MODE_S_IFREG || type == INODE_MODE_S_IFDIR))
			continue;

		if (!(le32_to_cpu(inode.flags) & EXT4_EXTENTS_FL)) {
			/* this is ext2 based filesystem */
			struct ext2_inode_blocks b = { .blocks = NULL };

			ret = ext2_inode_blocks_read(p, &b, &inode, ino);
			if (ret < 0)
				continue;

			for (size_t i = 0; i < b.total; i++) {
				if (b.blocks[i] == which_blk) {
					free(b.blocks);
					*ret_ino = ino;
					return 0;
				}
			}

			free(b.blocks);
		} else {
			struct extent_iterator it;

			ret = extent_iterator_init(p, &it, ino);
			if (ret < 0)
				continue;

			extent_list_foreach(&it) {
				struct ext4_extent *ee = it.ee;
				uint64_t first = ext4_extent_start_block(ee);
				uint64_t last = first +
						le16_to_cpu(ee->ee_len) - 1;

				if (which_blk >= first && which_blk <= last) {
					extent_iterator_exit(&it);
					*ret_ino = ino;
					return 0;
				}
			}
			extent_iterator_exit(&it);
		}
	}

	return -1;
}

static int unpack_dirent(struct ext2_editor_private_data *p, uint32_t ino,
			 const char *parent_dir)
{
	struct dirent_iterator it;
	char child_name[1024];
	int ret;

	ret = dirent_iterator_init(p, &it, ino);
	if (ret < 0)
		return ret;

	if (ino != EXT2_ROOT_INO) {
		int mode;

		mode = le16_to_cpu(it.inode.mode) & INODE_MODE_PERMISSION_MASK;
		ret = mkdir(parent_dir, mode);
		if (ret < 0) {
			switch (errno) {
			case EEXIST:
				ret = 0;
				break;
			default:
				fprintf(stderr, "Error: create dir %s with mode %o failed: %m\n",
					parent_dir, mode);
				goto done;
			}
		}
	}

	dirent_list_foreach(&it) {
		struct ext2_dirent *dir = it.dir;
		const char *filename = (const char *)(dir + 1);

		if (le16_to_cpu(dir->direntlen) <= (int)sizeof(struct ext2_dirent)) {
			fprintf(stderr, "direntlen is too short\n");
			return -1;
		} else if (le32_to_cpu(dir->inode) == 0) {
			continue;
		}

		snprintf(child_name, sizeof(child_name), "%s/%.*s",
			 parent_dir, dir->namelen, filename);

		switch (dir->filetype) {
		case EXT4_FT_REG_FILE:
			ret = unpack_file(p, le32_to_cpu(dir->inode), child_name);
			break;
		case EXT4_FT_DIR:
			/* skip '.' and '..' */
			if (dir->namelen == 1 && !strncmp(filename, ".", 1))
				break;
			if (dir->namelen == 2 && !strncmp(filename, "..", 2))
				break;

			ret = unpack_dirent(p, le32_to_cpu(dir->inode), child_name);
			break;
		case EXT4_FT_SYMLINK:
			ret = unpack_symlink(p, le32_to_cpu(dir->inode), child_name);
			break;
		case EXT4_FT_CHRDEV:
		case EXT4_FT_BLKDEV:
		case EXT4_FT_FIFO:
		case EXT4_FT_SOCK:
			printf("Warning: ignore %s(ino #%d): "
				"file type %d is not impl now\n",
				filename, le32_to_cpu(dir->inode), dir->filetype);
			break;
		case EXT4_FT_UNKNOWN:
		default:
			fprintf(stderr, "unknown filetype 0x%x\n", dir->filetype);
			ret = -1;
			break;
		}

		if (ret < 0)
			break;
	}

done:
	dirent_iterator_exit(&it);
	return ret;
}

static int ext2_unpack(void *private_data, int fd, const char *dirout, int argc, char **argv)
{
	struct ext2_editor_private_data *p = private_data;

	return unpack_dirent(p, EXT2_ROOT_INO, dirout);
}

static const uint8_t ext2_disk_magic[2] = {
	(EXT2_MAGIC >> 0) & 0xff,
	(EXT2_MAGIC >> 8) & 0xff,
};

static struct imgeditor ext2_editor = {
	.name			= "ext2",
	.descriptor		= "ext2 image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN |
				  IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= SUPERBLOCK_START + SUPERBLOCK_SIZE,
	.private_data_size	= sizeof(struct ext2_editor_private_data),
	.init			= ext2_editor_init,
	.exit			= ext2_editor_exit,
	.detect			= ext2_detect,
	.list			= ext2_main,
	.unpack			= ext2_unpack,
	.total_size		= ext2_total_size,
	.summary		= ext2_summary,

	.search_magic		= {
		.magic		= ext2_disk_magic,
		.magic_sz	= sizeof(ext2_disk_magic),
		.magic_offset	= offsetof(struct ext2_sblock, magic) + SUPERBLOCK_START,
	}
};
REGISTER_IMGEDITOR(ext2_editor);
