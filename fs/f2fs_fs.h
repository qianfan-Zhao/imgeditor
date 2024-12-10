/*
 * f2fs filesystem structure copied from linux kernel.
 * qianfan Zhao <qianfanguijin@163.com>
 */
#ifndef IMGEDITOR_F2FS_H
#define IMGEDITOR_F2FS_H

#include <assert.h>
#include "imgeditor.h"

#define __packed			__attribute__((packed))

#define BITS_PER_BYTE			8

#define F2FS_SUPER_MAGIC		0xF2F52010 /* F2FS Magic Number */
#define F2FS_SUPER_OFFSET		1024	/* byte-size offset */
#define F2FS_BLKSIZE			4096
#define F2FS_MAX_EXTENSION		64	/* # of extension entries */
#define F2FS_EXTENSION_LEN		8	/* max size of extension */

#define F2FS_MAX_QUOTAS			3

enum {
	CURSEG_HOT_DATA	= 0,	/* directory entry blocks */
	CURSEG_WARM_DATA,	/* data blocks */
	CURSEG_COLD_DATA,	/* multimedia or GCed data blocks */
	CURSEG_HOT_NODE,	/* direct node blocks of directory files */
	CURSEG_WARM_NODE,	/* direct node blocks of normal files */
	CURSEG_COLD_NODE,	/* indirect node blocks */
	NO_CHECK_TYPE
};

#define F2FS_FEATURE_ENCRYPT		0x0001
#define F2FS_FEATURE_BLKZONED		0x0002
#define F2FS_FEATURE_ATOMIC_WRITE	0x0004
#define F2FS_FEATURE_EXTRA_ATTR		0x0008
#define F2FS_FEATURE_PRJQUOTA		0x0010
#define F2FS_FEATURE_INODE_CHKSUM	0x0020
#define F2FS_FEATURE_FLEXIBLE_INLINE_XATTR	0x0040
#define F2FS_FEATURE_QUOTA_INO		0x0080
#define F2FS_FEATURE_INODE_CRTIME	0x0100
#define F2FS_FEATURE_LOST_FOUND		0x0200
#define F2FS_FEATURE_VERITY		0x0400	/* reserved */
#define F2FS_FEATURE_SB_CHKSUM		0x0800
#define F2FS_FEATURE_CASEFOLD		0x1000
#define F2FS_FEATURE_COMPRESSION	0x2000
#define F2FS_FEATURE_RO			0x4000
#define F2FS_FEATURE_DEVICE_ALIAS	0x8000

#define VERSION_LEN			256
#define MAX_VOLUME_NAME			512
#define MAX_PATH_LEN			64
#define MAX_DEVICES			8

/*
 * For superblock
 */
struct f2fs_device {
	__u8 path[MAX_PATH_LEN];
	__le32 total_segments;
} __packed;

static_assert(sizeof(struct f2fs_device) == 68, "");

/* reason of stop_checkpoint */
enum stop_cp_reason {
	STOP_CP_REASON_SHUTDOWN,
	STOP_CP_REASON_FAULT_INJECT,
	STOP_CP_REASON_META_PAGE,
	STOP_CP_REASON_WRITE_FAIL,
	STOP_CP_REASON_CORRUPTED_SUMMARY,
	STOP_CP_REASON_UPDATE_INODE,
	STOP_CP_REASON_FLUSH_FAIL,
	STOP_CP_REASON_NO_SEGMENT,
	STOP_CP_REASON_MAX,
};

#define	MAX_STOP_REASON			32

/* detail reason for EFSCORRUPTED */
enum f2fs_error {
	ERROR_CORRUPTED_CLUSTER,
	ERROR_FAIL_DECOMPRESSION,
	ERROR_INVALID_BLKADDR,
	ERROR_CORRUPTED_DIRENT,
	ERROR_CORRUPTED_INODE,
	ERROR_INCONSISTENT_SUMMARY,
	ERROR_INCONSISTENT_FOOTER,
	ERROR_INCONSISTENT_SUM_TYPE,
	ERROR_CORRUPTED_JOURNAL,
	ERROR_INCONSISTENT_NODE_COUNT,
	ERROR_INCONSISTENT_BLOCK_COUNT,
	ERROR_INVALID_CURSEG,
	ERROR_INCONSISTENT_SIT,
	ERROR_CORRUPTED_VERITY_XATTR,
	ERROR_CORRUPTED_XATTR,
	ERROR_INVALID_NODE_REFERENCE,
	ERROR_INCONSISTENT_NAT,
	ERROR_MAX,
};

#define MAX_F2FS_ERRORS			16

struct f2fs_super_block {
	__le32 magic;			/* Magic Number */
	__le16 major_ver;		/* Major Version */
	__le16 minor_ver;		/* Minor Version */
	__le32 log_sectorsize;		/* log2 sector size in bytes */
	__le32 log_sectors_per_block;	/* log2 # of sectors per block */
	__le32 log_blocksize;		/* log2 block size in bytes */
	__le32 log_blocks_per_seg;	/* log2 # of blocks per segment */
	__le32 segs_per_sec;		/* # of segments per section */
	__le32 secs_per_zone;		/* # of sections per zone */
	__le32 checksum_offset;		/* checksum offset inside super block */
	__le64 block_count;		/* total # of user blocks */
	__le32 section_count;		/* total # of sections */
	__le32 segment_count;		/* total # of segments */
	__le32 segment_count_ckpt;	/* # of segments for checkpoint */
	__le32 segment_count_sit;	/* # of segments for SIT */
	__le32 segment_count_nat;	/* # of segments for NAT */
	__le32 segment_count_ssa;	/* # of segments for SSA */
	__le32 segment_count_main;	/* # of segments for main area */
	__le32 segment0_blkaddr;	/* start block address of segment 0 */
	__le32 cp_blkaddr;		/* start block address of checkpoint */
	__le32 sit_blkaddr;		/* start block address of SIT */
	__le32 nat_blkaddr;		/* start block address of NAT */
	__le32 ssa_blkaddr;		/* start block address of SSA */
	__le32 main_blkaddr;		/* start block address of main area */
	__le32 root_ino;		/* root inode number */
	__le32 node_ino;		/* node inode number */
	__le32 meta_ino;		/* meta inode number */
	__u8 uuid[16];			/* 128-bit uuid for volume */
	__le16 volume_name[MAX_VOLUME_NAME];	/* volume name */
	__le32 extension_count;		/* # of extensions below */
	__u8 extension_list[F2FS_MAX_EXTENSION][F2FS_EXTENSION_LEN];/* extension array */
	__le32 cp_payload;
	__u8 version[VERSION_LEN];	/* the kernel version */
	__u8 init_version[VERSION_LEN];	/* the initial kernel version */
	__le32 feature;			/* defined features */
	__u8 encryption_level;		/* versioning level for encryption */
	__u8 encrypt_pw_salt[16];	/* Salt used for string2key algorithm */
	struct f2fs_device devs[MAX_DEVICES];	/* device list */
	__le32 qf_ino[F2FS_MAX_QUOTAS];	/* quota inode numbers */
	__u8 hot_ext_count;		/* # of hot file extension */
	__le16  s_encoding;		/* Filename charset encoding */
	__le16  s_encoding_flags;	/* Filename charset encoding flags */
	__u8 s_stop_reason[MAX_STOP_REASON];	/* stop checkpoint reason */
	__u8 s_errors[MAX_F2FS_ERRORS];		/* reason of image corrupts */
	__u8 reserved[258];		/* valid reserved region */
	__le32 crc;			/* checksum of superblock */
} __packed;

static_assert(sizeof(struct f2fs_super_block) == 3072, "");

/*
 * For further optimization on multi-head logs, on-disk layout supports maximum
 * 16 logs by default. The number, 16, is expected to cover all the cases
 * enoughly. The implementaion currently uses no more than 6 logs.
 * Half the logs are used for nodes, and the other half are used for data.
 */
#define MAX_ACTIVE_LOGS			16
#define MAX_ACTIVE_NODE_LOGS		8
#define MAX_ACTIVE_DATA_LOGS		8

#define CP_RESIZEFS_FLAG                0x00004000
#define CP_DISABLED_FLAG		0x00001000
#define CP_QUOTA_NEED_FSCK_FLAG		0x00000800
#define CP_LARGE_NAT_BITMAP_FLAG	0x00000400
#define CP_NOCRC_RECOVERY_FLAG		0x00000200
#define CP_TRIMMED_FLAG			0x00000100
#define CP_NAT_BITS_FLAG		0x00000080
#define CP_CRC_RECOVERY_FLAG		0x00000040
#define CP_FASTBOOT_FLAG		0x00000020
#define CP_FSCK_FLAG			0x00000010
#define CP_ERROR_FLAG			0x00000008
#define CP_COMPACT_SUM_FLAG		0x00000004
#define CP_ORPHAN_PRESENT_FLAG		0x00000002
#define CP_UMOUNT_FLAG			0x00000001

struct f2fs_checkpoint {
	__le64 checkpoint_ver;		/* checkpoint block version number */
	__le64 user_block_count;	/* # of user blocks */
	__le64 valid_block_count;	/* # of valid blocks in main area */
	__le32 rsvd_segment_count;	/* # of reserved segments for gc */
	__le32 overprov_segment_count;	/* # of overprovision segments */
	__le32 free_segment_count;	/* # of free segments in main area */

	/* information of current node segments */
	__le32 cur_node_segno[MAX_ACTIVE_NODE_LOGS];
	__le16 cur_node_blkoff[MAX_ACTIVE_NODE_LOGS];
	/* information of current data segments */
	__le32 cur_data_segno[MAX_ACTIVE_DATA_LOGS];
	__le16 cur_data_blkoff[MAX_ACTIVE_DATA_LOGS];
	__le32 ckpt_flags;		/* Flags : umount and journal_present */
	__le32 cp_pack_total_block_count;	/* total # of one cp pack */
	__le32 cp_pack_start_sum;	/* start block number of data summary */
	__le32 valid_node_count;	/* Total number of valid nodes */
	__le32 valid_inode_count;	/* Total number of valid inodes */
	__le32 next_free_nid;		/* Next free node number */
	__le32 sit_ver_bitmap_bytesize;	/* Default value 64 */
	__le32 nat_ver_bitmap_bytesize; /* Default value 256 */
	__le32 checksum_offset;		/* checksum offset inside cp block */
	__le64 elapsed_time;		/* mounted time */
	/* allocation type of current segment */
	unsigned char alloc_type[MAX_ACTIVE_LOGS];

	/* SIT and NAT version bitmap */
	unsigned char sit_nat_version_bitmap[];
} __packed;

static_assert(sizeof(struct f2fs_checkpoint) == 192, "");

/*
 * For NODE structure
 */
struct f2fs_extent {
	__le32 fofs;		/* start file offset of the extent */
	__le32 blk;		/* start block address of the extent */
	__le32 len;		/* length of the extent */
} __packed;

struct node_footer {
	__le32 nid;		/* node id */
	__le32 ino;		/* inode number */
	__le32 flag;		/* include cold/fsync/dentry marks and offset */
	__le64 cp_ver;		/* checkpoint version */
	__le32 next_blkaddr;	/* next node page block address */
} __packed;

#define F2FS_INLINE_XATTR		0x01	/* file inline xattr flag */
#define F2FS_INLINE_DATA		0x02	/* file inline data flag */
#define F2FS_INLINE_DENTRY		0x04	/* file inline dentry flag */
#define F2FS_DATA_EXIST			0x08	/* file inline data exist flag */
#define F2FS_INLINE_DOTS		0x10	/* file having implicit dot dentries */
#define F2FS_EXTRA_ATTR			0x20	/* file having extra attribute */
#define F2FS_PIN_FILE			0x40	/* file should not be gced */
#define F2FS_COMPRESS_RELEASED		0x80	/* file released compressed blocks */

#define F2FS_NAME_LEN			255
#define OFFSET_OF_END_OF_I_EXT		360
#define SIZE_OF_I_NID			20

/* Address Pointers in an Inode */
#define DEF_ADDRS_PER_INODE	((F2FS_BLKSIZE - OFFSET_OF_END_OF_I_EXT	\
					- SIZE_OF_I_NID	\
					- sizeof(struct node_footer)) / sizeof(__le32))
#define DEF_NIDS_PER_INODE	5	/* Node IDs in an Inode */
/* Address Pointers in a Direct Block */
#define DEF_ADDRS_PER_BLOCK	((F2FS_BLKSIZE - sizeof(struct node_footer)) / sizeof(__le32))
/* Node IDs in an Indirect Block */
#define NIDS_PER_BLOCK		((F2FS_BLKSIZE - sizeof(struct node_footer)) / sizeof(__le32))

struct f2fs_inode {
	__le16 i_mode;			/* file mode */
	__u8 i_advise;			/* file hints */
	__u8 i_inline;			/* file inline flags */
	__le32 i_uid;			/* user ID */
	__le32 i_gid;			/* group ID */
	__le32 i_links;			/* links count */
	__le64 i_size;			/* file size in bytes */
	__le64 i_blocks;		/* file size in blocks */
	__le64 i_atime;			/* access time */
	__le64 i_ctime;			/* change time */
	__le64 i_mtime;			/* modification time */
	__le32 i_atime_nsec;		/* access time in nano scale */
	__le32 i_ctime_nsec;		/* change time in nano scale */
	__le32 i_mtime_nsec;		/* modification time in nano scale */
	__le32 i_generation;		/* file version (for NFS) */
	union {
		__le32 i_current_depth;	/* only for directory depth */
		__le16 i_gc_failures;	/*
					 * # of gc failures on pinned file.
					 * only for regular files.
					 */
	};
	__le32 i_xattr_nid;		/* nid to save xattr */
	__le32 i_flags;			/* file attributes */
	__le32 i_pino;			/* parent inode number */
	__le32 i_namelen;		/* file name length */
	__u8 i_name[F2FS_NAME_LEN];	/* file name for SPOR */
	__u8 i_dir_level;		/* dentry_level for large dir */

	struct f2fs_extent i_ext;	/* caching a largest extent */

	union {
		struct {
			__le16 i_extra_isize;	/* extra inode attribute size */
			__le16 i_inline_xattr_size;	/* inline xattr size, unit: 4 bytes */
			__le32 i_projid;	/* project id */
			__le32 i_inode_checksum;/* inode meta checksum */
			__le64 i_crtime;	/* creation time */
			__le32 i_crtime_nsec;	/* creation time in nano scale */
			__le64 i_compr_blocks;	/* # of compressed blocks */
			__u8 i_compress_algorithm;	/* compress algorithm */
			__u8 i_log_cluster_size;	/* log of cluster size */
			__le16 i_compress_flag;		/* compress flag */
						/* 0 bit: chksum flag
						 * [8,15] bits: compress level
						 */
			__le32 i_extra_end[0];	/* for attribute size calculation */
		} __packed;
		__le32 i_addr[DEF_ADDRS_PER_INODE];	/* Pointers to data blocks */
	};
	__le32 i_nid[DEF_NIDS_PER_INODE];	/* direct(2), indirect(2),
						double_indirect(1) node id */
} __packed;

static_assert(offsetof(struct f2fs_inode, i_extra_end) -
	      offsetof(struct f2fs_inode, i_extra_isize) == 36, "");

struct direct_node {
	__le32 addr[DEF_ADDRS_PER_BLOCK];	/* array of data block address */
} __packed;

struct indirect_node {
	__le32 nid[NIDS_PER_BLOCK];	/* array of data block address */
} __packed;

struct f2fs_node {
	/* can be one of three types: inode, direct, and indirect types */
	union {
		struct f2fs_inode i;
		struct direct_node dn;
		struct indirect_node in;
	};
	struct node_footer footer;
} __packed;

/* One directory entry slot covers 8bytes-long file name */
#define F2FS_SLOT_LEN		8

/*
 * space utilization of regular dentry and inline dentry (w/o extra reservation)
 * when block size is 4KB.
 *		regular dentry		inline dentry (def)	inline dentry (min)
 * bitmap	1 * 27 = 27		1 * 23 = 23		1 * 1 = 1
 * reserved	1 * 3 = 3		1 * 7 = 7		1 * 1 = 1
 * dentry	11 * 214 = 2354		11 * 182 = 2002		11 * 2 = 22
 * filename	8 * 214 = 1712		8 * 182 = 1456		8 * 2 = 16
 * total	4096			3488			40
 *
 * Note: there are more reserved space in inline dentry than in regular
 * dentry, when converting inline dentry we should handle this carefully.
 */

/* the number of dentry in a block */
#define NR_DENTRY_IN_BLOCK	((BITS_PER_BYTE * F2FS_BLKSIZE) / \
					((SIZE_OF_DIR_ENTRY + F2FS_SLOT_LEN) * BITS_PER_BYTE + 1))
#define SIZE_OF_DIR_ENTRY	11	/* by byte */
#define SIZE_OF_DENTRY_BITMAP	((NR_DENTRY_IN_BLOCK + BITS_PER_BYTE - 1) / \
					BITS_PER_BYTE)
#define SIZE_OF_RESERVED	(F2FS_BLKSIZE - ((SIZE_OF_DIR_ENTRY + \
				F2FS_SLOT_LEN) * \
				NR_DENTRY_IN_BLOCK + SIZE_OF_DENTRY_BITMAP))
#define MIN_INLINE_DENTRY_SIZE		40	/* just include '.' and '..' entries */

/* One directory entry slot representing F2FS_SLOT_LEN-sized file name */
struct f2fs_dir_entry {
	__le32 hash_code;	/* hash code of file name */
	__le32 ino;		/* inode number */
	__le16 name_len;	/* length of file name */
	__u8 file_type;		/* file type */
} __packed;

/* Block-sized directory entry block */
struct f2fs_dentry_block {
	/* validity bitmap for directory entries in each block */
	__u8 dentry_bitmap[SIZE_OF_DENTRY_BITMAP];
	__u8 reserved[SIZE_OF_RESERVED];
	struct f2fs_dir_entry dentry[NR_DENTRY_IN_BLOCK];
	__u8 filename[NR_DENTRY_IN_BLOCK][F2FS_SLOT_LEN];
} __packed;

enum {
	F2FS_FT_UNKNOWN,
	F2FS_FT_REG_FILE,
	F2FS_FT_DIR,
	F2FS_FT_CHRDEV,
	F2FS_FT_BLKDEV,
	F2FS_FT_FIFO,
	F2FS_FT_SOCK,
	F2FS_FT_SYMLINK,
	F2FS_FT_MAX
};

/*
 * For SIT entries
 *
 * A validity bitmap of 64 bytes covers 512 blocks of area. For a 4K page size,
 * this results in a segment size of 2MB. For 16k pages, the default segment size
 * is 8MB.
 * Not allow to change this.
 */
#define SIT_VBLOCK_MAP_SIZE 64
#define SIT_ENTRY_PER_BLOCK (F2FS_BLKSIZE / sizeof(struct f2fs_sit_entry))

/*
 * F2FS uses 4 bytes to represent block address. As a result, supported size of
 * disk is 16 TB for a 4K page size and 64 TB for a 16K page size and it equals
 * to 16 * 1024 * 1024 / 2 segments.
 */
#define F2FS_MAX_SEGMENT       ((16 * 1024 * 1024) / 2)

/*
 * Note that f2fs_sit_entry->vblocks has the following bit-field information.
 * [15:10] : allocation type such as CURSEG_XXXX_TYPE
 * [9:0] : valid block count
 */
#define SIT_VBLOCKS_SHIFT	10
#define SIT_VBLOCKS_MASK	((1 << SIT_VBLOCKS_SHIFT) - 1)
#define GET_SIT_VBLOCKS(raw_sit)				\
	(le16_to_cpu((raw_sit)->vblocks) & SIT_VBLOCKS_MASK)
#define GET_SIT_TYPE(raw_sit)					\
	((le16_to_cpu((raw_sit)->vblocks) & ~SIT_VBLOCKS_MASK)	\
	 >> SIT_VBLOCKS_SHIFT)

struct f2fs_sit_entry {
	__le16 vblocks;				/* reference above */
	__u8 valid_map[SIT_VBLOCK_MAP_SIZE];	/* bitmap for valid blocks */
	__le64 mtime;				/* segment age for cleaning */
} __packed;

struct f2fs_sit_block {
	struct f2fs_sit_entry entries[SIT_ENTRY_PER_BLOCK];
} __packed;

/*
 * For NAT entries
 */
#define NAT_ENTRY_PER_BLOCK (F2FS_BLKSIZE / sizeof(struct f2fs_nat_entry))

struct f2fs_nat_entry {
	__u8 version;		/* latest version of cached nat entry */
	__le32 ino;		/* inode number */
	__le32 block_addr;	/* block address */
} __packed;

struct f2fs_nat_block {
	struct f2fs_nat_entry entries[NAT_ENTRY_PER_BLOCK];
} __packed;

#endif
