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

struct ext2_editor_private_data;
static void *alloc_inode_file(struct ext2_editor_private_data *p, uint32_t ino,
			      uint64_t *ret_filesize);

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
	STRUCTURE_ITEM(struct ext2_sblock, lastcheck,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, checkinterval,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, creator_os,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, revision_level,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, uid_reserved,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, gid_reserved,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, first_inode,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, inode_size,			structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, block_group_number,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, feature_compatibility,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, feature_incompat,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_sblock, feature_ro_compat,		structure_item_print_unsigned),
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
	STRUCTURE_ITEM_END(),
};

static const struct structure_item ext2_block_group_structure_32[] = {
	STRUCTURE_ITEM(struct ext2_block_group, block_id,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, inode_id,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, inode_table_id,		structure_item_print_unsigned),
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
	STRUCTURE_ITEM(struct ext2_block_group, block_id_high,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, inode_id_high,		structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, inode_table_id_high,	structure_item_print_unsigned),
	STRUCTURE_ITEM(struct ext2_block_group, free_blocks_high,	structure_item_print_xunsigned),
	STRUCTURE_ITEM(struct ext2_block_group, free_inodes_high,	structure_item_print_unsigned),
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

struct ext2_editor_private_data {
	int				fd;

	struct ext2_sblock		sblock;
	struct ext2_block_group		*block_groups;
	struct ext2_inode		root_inode;

	uint32_t			block_size; /* in bytes */
	uint32_t			fragment_size; /* in bytes */
	uint32_t			inode_size; /* in bytes */
	uint32_t			n_block_group;
};

static int ext2_check_sblock(int force_type, struct ext2_sblock *sblock)
{
	if (sblock->magic != EXT2_MAGIC) {
		fprintf_if_force_type("Error: magic doesn't match\n");
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

	lseek(p->fd, blkno * p->block_size + blk_offset, SEEK_SET);
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

	lseek(p->fd, blkno * p->block_size, SEEK_SET);
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

static int ext2_detect(void *private_data, int force_type, int fd)
{
	struct ext2_editor_private_data *p = private_data;
	struct ext2_sblock *sblock = &p->sblock;
	int ret;

	/* save fd to private_data */
	p->fd = fd;

	lseek(fd, SUPERBLOCK_START, SEEK_SET);
	ret = read(fd, sblock, sizeof(*sblock));
	if (ret < 0)
		return ret;

	ret = ext2_check_sblock(force_type, sblock);
	if (ret < 0)
		return ret;

	p->block_size = ext2_sblock_log2_size_to_bytes(sblock->log2_block_size);
	p->fragment_size = ext2_sblock_log2_size_to_bytes(sblock->log2_fragment_size);
	p->n_block_group =
		aligned_length((sblock->total_blocks - sblock->first_data_block),
				sblock->blocks_per_group) / sblock->blocks_per_group;

	if (le32_to_cpu(sblock->revision_level) == 0)
		p->inode_size = 128;
	else
		p->inode_size = le16_to_cpu(sblock->inode_size);

	p->block_groups = calloc(p->n_block_group,
				 sizeof(struct ext2_block_group));
	if (!p->block_groups) {
		fprintf(stderr, "Error: alloc %d ext2_block_groups failed\n",
			p->n_block_group);
		return -1;
	}

	/* loading all block groups */
	lseek(fd, p->block_size, SEEK_SET);
	for (uint32_t i = 0; i < p->n_block_group; i++) {
		uint16_t descriptor_size = le16_to_cpu(sblock->descriptor_size);
		struct ext2_block_group *group = &p->block_groups[i];

		/* sblock->descriptor_size maybe zero on some ext2 filesystem */
		if (descriptor_size == 0)
			descriptor_size = 32;

		read(fd, group, descriptor_size);
	}

	ret = ext2_read_inode(p, EXT2_ROOT_INO, &p->root_inode);
	if (ret < 0) {
		fprintf_if_force_type("Error: read root inode failed\n");
		return ret;
	}

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

static int ext2_journal_list_descriptor(struct journal_superblock_t *sb,
					uint32_t journal_blkno,
					uint32_t *ret_data_blks,
					void *blockbuf, size_t blocksize)
{
	uint32_t incompat_features = be32_to_cpu(sb->s_feature_incompat);
	size_t tagsize = journal_descriptor_blocktag_size(incompat_features);
	void *p = blockbuf + sizeof(struct journal_header_t);
	uint32_t data_blks = 0;
	size_t tailsize = 0;

	if (incompat_features & JBD2_FEATURE_INCOMPAT_CSUM_V2_3)
		tailsize = sizeof(struct jbd2_journal_block_tail);

	while ((size_t)(p - blockbuf) <= blocksize - tagsize - tailsize) {
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

		printf("\n%27sFS blocknr %-8" PRIu64
		       "logged at journal block %-5d with flags 0x%x",
			"", blocknr, journal_blkno + data_blks, flags);

		if (flags & EXT3_JOURNAL_FLAG_LAST_TAG)
			break;

		p += tagsize;
		if (!(flags & EXT3_JOURNAL_FLAG_SAME_UUID))
			p += 16;
	}

	*ret_data_blks = data_blks;
	return 0;
}

static int ext2_do_journal_list(void *journal_buf, size_t journal_size)
{
	struct journal_superblock_t *sb = journal_buf + sizeof(struct journal_header_t);
	struct journal_revoke_header_t *revoke;
	struct commit_header *commit;
	void *p = journal_buf;
	uint32_t journal_blkno = 0;

	while (p - journal_buf < (int)journal_size) {
		struct journal_header_t *h = p;
		uint32_t blocktype = be32_to_cpu(h->h_blocktype);
		uint32_t skip_size = be32_to_cpu(sb->s_blocksize);

		if (be32_to_cpu(h->h_magic) != EXT3_JOURNAL_MAGIC_NUMBER)
			goto next;

		switch (blocktype) {
		case EXT3_JOURNAL_DESCRIPTOR_BLOCK:
		case EXT3_JOURNAL_COMMIT_BLOCK:
		case EXT3_JOURNAL_SUPERBLOCK_V1:
		case EXT3_JOURNAL_SUPERBLOCK_V2:
		case EXT3_JOURNAL_REVOKE_BLOCK:
			printf("Journal %4d on blk #%-5d ",
				be32_to_cpu(h->h_sequence), journal_blkno);
			break;
		default:
			goto next;
		}

		switch (blocktype) {
		case EXT3_JOURNAL_SUPERBLOCK_V1:
		case EXT3_JOURNAL_SUPERBLOCK_V2:
			printf("%-13s sequence=%d", "Superblock",
				be32_to_cpu(sb->s_sequence));
			break;
		case EXT3_JOURNAL_DESCRIPTOR_BLOCK:
			printf("%-13s ", "Descriptor");
			{
				uint32_t n_data_blk = 0;

				/* In general, the dta blocks being written to
				 * disk through the journal are written
				 * verbatim into the journal file after the
				 * descriptor block.
				 */
				ext2_journal_list_descriptor(sb, journal_blkno,
					&n_data_blk,
					p, be32_to_cpu(sb->s_blocksize));

				journal_blkno += n_data_blk;
				skip_size +=
					(n_data_blk * be32_to_cpu(sb->s_blocksize));
			}
			break;
		case EXT3_JOURNAL_COMMIT_BLOCK:
			commit = p + sizeof(*h);
			printf("%-13s", "Commit");

			{
				char buf[64];
				struct tm tm = { 0 };
				time_t t = be64_to_cpu(commit->h_commit_sec);

				localtime_r(&t, &tm);
				strftime(buf, sizeof(buf), "%FT%T", &tm);
				printf(" %s.%03d",
					buf,
					be32_to_cpu(commit->h_commit_nsec) / 1000000);
			}
			break;
		case EXT3_JOURNAL_REVOKE_BLOCK:
			revoke = p + sizeof(*h);
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
	next:
		p += skip_size;
		journal_blkno++;
	}

	return 0;
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
		ret = ext2_do_journal_list(journal_buf, journal_size);
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

		lseek64(p->fd, b.blocks[i] * p->block_size, SEEK_SET);
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

		lseek64(p->fd,
			ext4_extent_start_block(ee) * p->block_size,
			SEEK_SET);

		read(p->fd,
		     buf + le32_to_cpu(ee->ee_block) * p->block_size,
		     bytes);

		filesz -= bytes;
	}
	extent_iterator_exit(&it);

	return buf;
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

static struct imgeditor ext2_editor = {
	.name			= "ext2",
	.descriptor		= "ext2 image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN |
				  IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= SUPERBLOCK_START + SUPERBLOCK_SIZE,
	.private_data_size	= sizeof(struct ext2_editor_private_data),
	.detect			= ext2_detect,
	.list			= ext2_main,
	.unpack			= ext2_unpack,
};
REGISTER_IMGEDITOR(ext2_editor);
