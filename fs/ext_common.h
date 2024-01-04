#ifndef EXT_COMMON_H
#define EXT_COMMON_H

#include <stdint.h>
#include <endian.h>

#define SECTOR_SIZE			0x200
#define LOG2_SECTOR_SIZE		9

/* Magic value used to identify an ext2 filesystem.  */
#define	EXT2_MAGIC			0xEF53
/* Amount of indirect blocks in an inode.  */
#define INDIRECT_BLOCKS			12
/* Maximum lenght of a pathname.  */
#define EXT2_PATH_MAX			4096
/* Maximum nesting of symlinks, used to prevent a loop.  */
#define	EXT2_MAX_SYMLINKCNT		8

/* Filetype used in directory entry.  */
#define	FILETYPE_UNKNOWN		0
#define	FILETYPE_REG			1
#define	FILETYPE_DIRECTORY		2
#define	FILETYPE_SYMLINK		7

/* Filetype information as used in inodes.  */
#define FILETYPE_INO_MASK		0170000
#define FILETYPE_INO_REG		0100000
#define FILETYPE_INO_DIRECTORY		0040000
#define FILETYPE_INO_SYMLINK		0120000
#define EXT2_ROOT_INO			2 /* Root inode */

#define EXT2_FT_DIR	2
#define SUCCESS	1

/* Macro-instructions used to manage several block sizes  */
#define EXT2_MIN_BLOCK_LOG_SIZE		10 /* 1024 */
#define EXT2_MAX_BLOCK_LOG_SIZE		16 /* 65536 */
#define EXT2_MIN_BLOCK_SIZE		(1 << EXT2_MIN_BLOCK_LOG_SIZE)
#define EXT2_MAX_BLOCK_SIZE		(1 << EXT2_MAX_BLOCK_LOG_SIZE)

#define SUPERBLOCK_START		(2 * 512)
#define SUPERBLOCK_SIZE			1024

/* The ext2 superblock.  */
struct ext2_sblock {
	__le32 total_inodes;
	__le32 total_blocks;
	__le32 reserved_blocks;
	__le32 free_blocks;
	__le32 free_inodes;
	__le32 first_data_block;
	__le32 log2_block_size;
	__le32 log2_fragment_size;
	__le32 blocks_per_group;
	__le32 fragments_per_group;
	__le32 inodes_per_group;
	__le32 mtime;
	__le32 utime;
	__le16 mnt_count;
	__le16 max_mnt_count;
	__le16 magic;
	__le16 fs_state;
	__le16 error_handling;
	__le16 minor_revision_level;
	__le32 lastcheck;
	__le32 checkinterval;
	__le32 creator_os;
	__le32 revision_level;
	__le16 uid_reserved;
	__le16 gid_reserved;
	__le32 first_inode;
	__le16 inode_size;
	__le16 block_group_number;
	__le32 feature_compatibility;
	__le32 feature_incompat;
	__le32 feature_ro_compat;
	__le32 unique_id[4];
	char volume_name[16];
	char last_mounted_on[64];
	__le32 compression_info;
	uint8_t prealloc_blocks;
	uint8_t prealloc_dir_blocks;
	__le16 reserved_gdt_blocks;
	uint8_t journal_uuid[16];
	__le32 journal_inode;
	__le32 journal_dev;
	__le32 last_orphan;
	__le32 hash_seed[4];
	uint8_t default_hash_version;
	uint8_t journal_backup_type;
	__le16 descriptor_size;
	__le32 default_mount_options;
	__le32 first_meta_block_group;
	__le32 mkfs_time;
	__le32 journal_blocks[17];
	__le32 total_blocks_high;
	__le32 reserved_blocks_high;
	__le32 free_blocks_high;
	__le16 min_extra_inode_size;
	__le16 want_extra_inode_size;
	__le32 flags;
	__le16 raid_stride;
	__le16 mmp_interval;
	__le64 mmp_block;
	__le32 raid_stripe_width;
	uint8_t log2_groups_per_flex;
	uint8_t checksum_type;
};

struct ext2_block_group {
	__le32 block_id;	/* Blocks bitmap block */
	__le32 inode_id;	/* Inodes bitmap block */
	__le32 inode_table_id;	/* Inodes table block */
	__le16 free_blocks;	/* Free blocks count */
	__le16 free_inodes;	/* Free inodes count */
	__le16 used_dir_cnt;	/* Directories count */
	__le16 bg_flags;
	__le32 bg_exclude_bitmap;
	__le16 bg_block_id_csum;
	__le16 bg_inode_id_csum;
	__le16 bg_itable_unused; /* Unused inodes count */
	__le16 bg_checksum;	/* crc16(s_uuid+group_num+group_desc)*/
	/* following fields only exist if descriptor size is 64 */
	__le32 block_id_high;
	__le32 inode_id_high;
	__le32 inode_table_id_high;
	__le16 free_blocks_high;
	__le16 free_inodes_high;
	__le16 used_dir_cnt_high;
	__le16 bg_itable_unused_high;
	__le32 bg_exclude_bitmap_high;
	__le16 bg_block_id_csum_high;
	__le16 bg_inode_id_csum_high;
	__le32 bg_reserved;
};

/* The ext2 inode. */
struct ext2_inode {
	__le16 mode;
	__le16 uid;
	__le32 size;
	__le32 atime;
	__le32 ctime;
	__le32 mtime;
	__le32 dtime;
	__le16 gid;
	__le16 nlinks;
	__le32 blockcnt;	/* Blocks of either 512 or block_size bytes */
	__le32 flags;
	__le32 osd1;
	union {
		struct datablocks {
			__le32 dir_blocks[INDIRECT_BLOCKS];
			__le32 indir_block;
			__le32 double_indir_block;
			__le32 triple_indir_block;
		} blocks;
		char symlink[60];
		char inline_data[60];
	} b;
	__le32 version;
	__le32 acl;
	__le32 size_high;	/* previously dir_acl, but never used */
	__le32 fragment_addr;
	__le32 osd2[3];
};

#define INODE_MODE_PERMISSION_MASK		0x0fff
#define INODE_MODE_S_MASK			0xf000

#define INODE_MODE_S_FIFO			0x1000 /* fifo */
#define INODE_MODE_S_IFCHR			0x2000 /* character device */
#define INODE_MODE_S_IFDIR			0x4000 /* directory */
#define INODE_MODE_S_IFBLK			0x6000 /* block device */
#define INODE_MODE_S_IFREG			0x8000 /* regular file */
#define INODE_MODE_S_IFLINK			0xa000 /* symbolic link */
#define INODE_MODE_S_IFSOCK			0xc000 /* socket */

/*
 * Inode flags
 */
#define	EXT4_SECRM_FL			0x00000001 /* Secure deletion */
#define	EXT4_UNRM_FL			0x00000002 /* Undelete */
#define	EXT4_COMPR_FL			0x00000004 /* Compress file */
#define EXT4_SYNC_FL			0x00000008 /* Synchronous updates */
#define EXT4_IMMUTABLE_FL		0x00000010 /* Immutable file */
#define EXT4_APPEND_FL			0x00000020 /* writes to file may only append */
#define EXT4_NODUMP_FL			0x00000040 /* do not dump file */
#define EXT4_NOATIME_FL			0x00000080 /* do not update atime */
/* Reserved for compression usage... */
#define EXT4_DIRTY_FL			0x00000100
#define EXT4_COMPRBLK_FL		0x00000200 /* One or more compressed clusters */
#define EXT4_NOCOMPR_FL			0x00000400 /* Don't compress */
	/* nb: was previously EXT2_ECOMPR_FL */
#define EXT4_ENCRYPT_FL			0x00000800 /* encrypted file */
/* End compression flags --- maybe not all used */
#define EXT4_INDEX_FL			0x00001000 /* hash-indexed directory */
#define EXT4_IMAGIC_FL			0x00002000 /* AFS directory */
#define EXT4_JOURNAL_DATA_FL		0x00004000 /* file data should be journaled */
#define EXT4_NOTAIL_FL			0x00008000 /* file tail should not be merged */
#define EXT4_DIRSYNC_FL			0x00010000 /* dirsync behaviour (directories only) */
#define EXT4_TOPDIR_FL			0x00020000 /* Top of directory hierarchies*/
#define EXT4_HUGE_FILE_FL               0x00040000 /* Set to each huge file */
#define EXT4_EXTENTS_FL			0x00080000 /* Inode uses extents */
#define EXT4_VERITY_FL			0x00100000 /* Verity protected inode */
#define EXT4_EA_INODE_FL	        0x00200000 /* Inode used for large EA */
/* 0x00400000 was formerly EXT4_EOFBLOCKS_FL */

#define EXT4_DAX_FL			0x02000000 /* Inode is DAX */

#define EXT4_INLINE_DATA_FL		0x10000000 /* Inode has inline data. */
#define EXT4_PROJINHERIT_FL		0x20000000 /* Create with parents projid */
#define EXT4_CASEFOLD_FL		0x40000000 /* Casefolded directory */
#define EXT4_RESERVED_FL		0x80000000 /* reserved for ext4 lib */

/* User modifiable flags */
#define EXT4_FL_USER_MODIFIABLE		(EXT4_SECRM_FL | \
					 EXT4_UNRM_FL | \
					 EXT4_COMPR_FL | \
					 EXT4_SYNC_FL | \
					 EXT4_IMMUTABLE_FL | \
					 EXT4_APPEND_FL | \
					 EXT4_NODUMP_FL | \
					 EXT4_NOATIME_FL | \
					 EXT4_JOURNAL_DATA_FL | \
					 EXT4_NOTAIL_FL | \
					 EXT4_DIRSYNC_FL | \
					 EXT4_TOPDIR_FL | \
					 EXT4_EXTENTS_FL | \
					 0x00400000 /* EXT4_EOFBLOCKS_FL */ | \
					 EXT4_DAX_FL | \
					 EXT4_PROJINHERIT_FL | \
					 EXT4_CASEFOLD_FL)

/* User visible flags */
#define EXT4_FL_USER_VISIBLE		(EXT4_FL_USER_MODIFIABLE | \
					 EXT4_DIRTY_FL | \
					 EXT4_COMPRBLK_FL | \
					 EXT4_NOCOMPR_FL | \
					 EXT4_ENCRYPT_FL | \
					 EXT4_INDEX_FL | \
					 EXT4_VERITY_FL | \
					 EXT4_INLINE_DATA_FL)

/* Flags that should be inherited by new inodes from their parent. */
#define EXT4_FL_INHERITED (EXT4_SECRM_FL | EXT4_UNRM_FL | EXT4_COMPR_FL |\
			   EXT4_SYNC_FL | EXT4_NODUMP_FL | EXT4_NOATIME_FL |\
			   EXT4_NOCOMPR_FL | EXT4_JOURNAL_DATA_FL |\
			   EXT4_NOTAIL_FL | EXT4_DIRSYNC_FL |\
			   EXT4_PROJINHERIT_FL | EXT4_CASEFOLD_FL |\
			   EXT4_DAX_FL)

/* Flags that are appropriate for regular files (all but dir-specific ones). */
#define EXT4_REG_FLMASK (~(EXT4_DIRSYNC_FL | EXT4_TOPDIR_FL | EXT4_CASEFOLD_FL |\
			   EXT4_PROJINHERIT_FL))

/* Flags that are appropriate for non-directories/regular files. */
#define EXT4_OTHER_FLMASK (EXT4_NODUMP_FL | EXT4_NOATIME_FL)

/* The only flags that should be swapped */
#define EXT4_FL_SHOULD_SWAP (EXT4_HUGE_FILE_FL | EXT4_EXTENTS_FL)

/* Flags which are mutually exclusive to DAX */
#define EXT4_DAX_MUT_EXCL (EXT4_VERITY_FL | EXT4_ENCRYPT_FL |\
			   EXT4_JOURNAL_DATA_FL | EXT4_INLINE_DATA_FL)


/* The header of an ext2 directory entry. */
struct ext2_dirent {
	__le32 inode;
	__le16 direntlen;
	__u8 namelen;
	__u8 filetype;
};

/*
 * Ext4 directory file types.  Only the low 3 bits are used.  The
 * other bits are reserved for now.
 */
#define EXT4_FT_UNKNOWN		0
#define EXT4_FT_REG_FILE	1
#define EXT4_FT_DIR		2
#define EXT4_FT_CHRDEV		3
#define EXT4_FT_BLKDEV		4
#define EXT4_FT_FIFO		5
#define EXT4_FT_SOCK		6
#define EXT4_FT_SYMLINK		7

/*
 * ext4_inode has i_block array (60 bytes total).
 * The first 12 bytes store ext4_extent_header;
 * the remainder stores an array of ext4_extent.
 */

/*
 * This is the extent on-disk structure.
 * It's used at the bottom of the tree.
 */
struct ext4_extent {
	__le32	ee_block;	/* first logical block extent covers */
	__le16	ee_len;		/* number of blocks covered by extent */
	__le16	ee_start_hi;	/* high 16 bits of physical block */
	__le32	ee_start_lo;	/* low 32 bits of physical block */
};

/*
 * This is index on-disk structure.
 * It's used at all the levels except the bottom.
 */
struct ext4_extent_idx {
	__le32	ei_block;	/* index covers logical blocks from 'block' */
	__le32	ei_leaf_lo;	/* pointer to the physical block of the next *
				 * level. leaf or next index could be there */
	__le16	ei_leaf_hi;	/* high 16 bits of physical block */
	__le16	ei_unused;
};

/* Each block (leaves and indexes), even inode-stored has header. */
struct ext4_extent_header {
	__le16	eh_magic;	/* probably will support different formats */
	__le16	eh_entries;	/* number of valid entries */
	__le16	eh_max;		/* capacity of store in entries */
	__le16	eh_depth;	/* has tree real underlying blocks? */
	__le32	eh_generation;	/* generation of the tree */
};

#define EXT4_EXT_MAGIC			0xf30a

#endif
