/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#ifndef __XFS_FORMAT_H__
#define __XFS_FORMAT_H__

#include "xfs_internal.h"

/*
 * Super block
 * Fits into a sector-sized buffer at address 0 of each allocation group.
 * Only the first of these is ever updated except during growfs.
 */
#define	XFS_SB_MAGIC		0x58465342	/* 'XFSB' */
#define	XFS_SB_VERSION_1	1		/* 5.3, 6.0.1, 6.1 */
#define	XFS_SB_VERSION_2	2		/* 6.2 - attributes */
#define	XFS_SB_VERSION_3	3		/* 6.2 - new inode version */
#define	XFS_SB_VERSION_4	4		/* 6.2+ - bitmask version */
#define	XFS_SB_VERSION_5	5		/* CRC enabled filesystem */
#define	XFS_SB_VERSION_NUMBITS		0x000f
#define	XFS_SB_VERSION_ALLFBITS		0xfff0
#define	XFS_SB_VERSION_ATTRBIT		0x0010
#define	XFS_SB_VERSION_NLINKBIT		0x0020
#define	XFS_SB_VERSION_QUOTABIT		0x0040
#define	XFS_SB_VERSION_ALIGNBIT		0x0080
#define	XFS_SB_VERSION_DALIGNBIT	0x0100
#define	XFS_SB_VERSION_SHAREDBIT	0x0200
#define XFS_SB_VERSION_LOGV2BIT		0x0400
#define XFS_SB_VERSION_SECTORBIT	0x0800
#define	XFS_SB_VERSION_EXTFLGBIT	0x1000
#define	XFS_SB_VERSION_DIRV2BIT		0x2000
#define	XFS_SB_VERSION_BORGBIT		0x4000	/* ASCII only case-insens. */
#define	XFS_SB_VERSION_MOREBITSBIT	0x8000

/*
 * There are two words to hold XFS "feature" bits: the original
 * word, sb_versionnum, and sb_features2.  Whenever a bit is set in
 * sb_features2, the feature bit XFS_SB_VERSION_MOREBITSBIT must be set.
 *
 * These defines represent bits in sb_features2.
 */
#define XFS_SB_VERSION2_RESERVED1BIT	0x00000001
#define XFS_SB_VERSION2_LAZYSBCOUNTBIT	0x00000002	/* Superblk counters */
#define XFS_SB_VERSION2_RESERVED4BIT	0x00000004
#define XFS_SB_VERSION2_ATTR2BIT	0x00000008	/* Inline attr rework */
#define XFS_SB_VERSION2_PARENTBIT	0x00000010	/* parent pointers */
#define XFS_SB_VERSION2_PROJID32BIT	0x00000080	/* 32 bit project id */
#define XFS_SB_VERSION2_CRCBIT		0x00000100	/* metadata CRCs */
#define XFS_SB_VERSION2_FTYPE		0x00000200	/* inode type in dir */

#define XFS_SB_FEAT_RO_COMPAT_FINOBT   (1 << 0)		/* free inode btree */
#define XFS_SB_FEAT_RO_COMPAT_RMAPBT   (1 << 1)		/* reverse map btree */
#define XFS_SB_FEAT_RO_COMPAT_REFLINK  (1 << 2)		/* reflinked files */
#define XFS_SB_FEAT_RO_COMPAT_INOBTCNT (1 << 3)		/* inobt block counts */

#define XFS_SB_FEAT_INCOMPAT_FTYPE	(1 << 0)  /* filetype in dirent */
#define XFS_SB_FEAT_INCOMPAT_SPINODES	(1 << 1)  /* sparse inode chunks */
#define XFS_SB_FEAT_INCOMPAT_META_UUID	(1 << 2)  /* metadata UUID */
#define XFS_SB_FEAT_INCOMPAT_BIGTIME	(1 << 3)  /* large timestamps */
#define XFS_SB_FEAT_INCOMPAT_NEEDSREPAIR (1 << 4) /* needs xfs_repair */
#define XFS_SB_FEAT_INCOMPAT_NREXT64	(1 << 5)  /* large extent counters */
#define XFS_SB_FEAT_INCOMPAT_EXCHRANGE	(1 << 6)  /* exchangerange supported */
#define XFS_SB_FEAT_INCOMPAT_PARENT	(1 << 7)  /* parent pointers */

#define XFS_SB_FEAT_INCOMPAT_LOG_XATTRS   (1 << 0)	/* Delayed Attributes */

/* Maximum size of the xfs filesystem label, no terminating NULL */
#define XFSLABEL_MAX			12

/*
 * Superblock - on disk version.
 * Must be padded to 64 bit alignment.
 */
struct xfs_dsb {
	__be32		sb_magicnum;	/* magic number == XFS_SB_MAGIC */
	__be32		sb_blocksize;	/* logical block size, bytes */
	__be64		sb_dblocks;	/* number of data blocks */
	__be64		sb_rblocks;	/* number of realtime blocks */
	__be64		sb_rextents;	/* number of realtime extents */
	uuid_t		sb_uuid;	/* user-visible file system unique id */
	__be64		sb_logstart;	/* starting block of log if internal */
	__be64		sb_rootino;	/* root inode number */
	__be64		sb_rbmino;	/* bitmap inode for realtime extents */
	__be64		sb_rsumino;	/* summary inode for rt bitmap */
	__be32		sb_rextsize;	/* realtime extent size, blocks */
	__be32		sb_agblocks;	/* size of an allocation group */
	__be32		sb_agcount;	/* number of allocation groups */
	__be32		sb_rbmblocks;	/* number of rt bitmap blocks */
	__be32		sb_logblocks;	/* number of log blocks */
	__be16		sb_versionnum;	/* header version == XFS_SB_VERSION */
	__be16		sb_sectsize;	/* volume sector size, bytes */
	__be16		sb_inodesize;	/* inode size, bytes */
	__be16		sb_inopblock;	/* inodes per block */
	char		sb_fname[XFSLABEL_MAX]; /* file system name */
	__u8		sb_blocklog;	/* log2 of sb_blocksize */
	__u8		sb_sectlog;	/* log2 of sb_sectsize */
	__u8		sb_inodelog;	/* log2 of sb_inodesize */
	__u8		sb_inopblog;	/* log2 of sb_inopblock */
	__u8		sb_agblklog;	/* log2 of sb_agblocks (rounded up) */
	__u8		sb_rextslog;	/* log2 of sb_rextents */
	__u8		sb_inprogress;	/* mkfs is in progress, don't mount */
	__u8		sb_imax_pct;	/* max % of fs for inode space */
					/* statistics */
	/*
	 * These fields must remain contiguous.  If you really
	 * want to change their layout, make sure you fix the
	 * code in xfs_trans_apply_sb_deltas().
	 */
	__be64		sb_icount;	/* allocated inodes */
	__be64		sb_ifree;	/* free inodes */
	__be64		sb_fdblocks;	/* free data blocks */
	__be64		sb_frextents;	/* free realtime extents */
	/*
	 * End contiguous fields.
	 */
	__be64		sb_uquotino;	/* user quota inode */
	__be64		sb_gquotino;	/* group quota inode */
	__be16		sb_qflags;	/* quota flags */
	__u8		sb_flags;	/* misc. flags */
	__u8		sb_shared_vn;	/* shared version number */
	__be32		sb_inoalignmt;	/* inode chunk alignment, fsblocks */
	__be32		sb_unit;	/* stripe or raid unit */
	__be32		sb_width;	/* stripe or raid width */
	__u8		sb_dirblklog;	/* log2 of dir block size (fsbs) */
	__u8		sb_logsectlog;	/* log2 of the log sector size */
	__be16		sb_logsectsize;	/* sector size for the log, bytes */
	__be32		sb_logsunit;	/* stripe unit size for the log */
	__be32		sb_features2;	/* additional feature bits */
	/*
	 * bad features2 field as a result of failing to pad the sb
	 * structure to 64 bits. Some machines will be using this field
	 * for features2 bits. Easiest just to mark it bad and not use
	 * it for anything else.
	 */
	__be32		sb_bad_features2;

	/* version 5 superblock fields start here */

	/* feature masks */
	__be32		sb_features_compat;
	__be32		sb_features_ro_compat;
	__be32		sb_features_incompat;
	__be32		sb_features_log_incompat;

	__le32		sb_crc;		/* superblock crc */
	__be32		sb_spino_align;	/* sparse inode chunk alignment */

	__be64		sb_pquotino;	/* project quota inode */
	__be64		sb_lsn;		/* last write sequence */
	uuid_t		sb_meta_uuid;	/* metadata file system unique id */

	/* must be padded to 64 bit alignment */
};

#define XFS_SB_CRC_OFF		offsetof(struct xfs_dsb, sb_crc)

/*
 * Misc. Flags - warning - these will be cleared by xfs_repair unless
 * a feature bit is set when the flag is used.
 */
#define XFS_SBF_NOFLAGS		0x00	/* no flags set */
#define XFS_SBF_READONLY	0x01	/* only read-only mounts allowed */

/*
 * define max. shared version we can interoperate with
 */
#define XFS_SB_MAX_SHARED_VN	0

/*
 * Allocation group header
 *
 * This is divided into three structures, placed in sequential 512-byte
 * buffers after a copy of the superblock (also in a 512-byte buffer).
 */
#define	XFS_AGF_MAGIC	0x58414746	/* 'XAGF' */
#define	XFS_AGI_MAGIC	0x58414749	/* 'XAGI' */
#define	XFS_AGFL_MAGIC	0x5841464c	/* 'XAFL' */
#define	XFS_AGF_VERSION	1
#define	XFS_AGI_VERSION	1

/*
 * agf_cnt_level in the first AGF overlaps the EFS superblock's magic number.
 * Since the magic numbers valid for EFS are > 64k, our value cannot be confused
 * for an EFS superblock.
 */

typedef struct xfs_agf {
	/*
	 * Common allocation group header information
	 */
	__be32		agf_magicnum;	/* magic number == XFS_AGF_MAGIC */
	__be32		agf_versionnum;	/* header version == XFS_AGF_VERSION */
	__be32		agf_seqno;	/* sequence # starting from 0 */
	__be32		agf_length;	/* size in blocks of a.g. */
	/*
	 * Freespace and rmap information
	 */
	__be32		agf_bno_root;	/* bnobt root block */
	__be32		agf_cnt_root;	/* cntbt root block */
	__be32		agf_rmap_root;	/* rmapbt root block */

	__be32		agf_bno_level;	/* bnobt btree levels */
	__be32		agf_cnt_level;	/* cntbt btree levels */
	__be32		agf_rmap_level;	/* rmapbt btree levels */

	__be32		agf_flfirst;	/* first freelist block's index */
	__be32		agf_fllast;	/* last freelist block's index */
	__be32		agf_flcount;	/* count of blocks in freelist */
	__be32		agf_freeblks;	/* total free blocks */

	__be32		agf_longest;	/* longest free space */
	__be32		agf_btreeblks;	/* # of blocks held in AGF btrees */
	uuid_t		agf_uuid;	/* uuid of filesystem */

	__be32		agf_rmap_blocks;	/* rmapbt blocks used */
	__be32		agf_refcount_blocks;	/* refcountbt blocks used */

	__be32		agf_refcount_root;	/* refcount tree root block */
	__be32		agf_refcount_level;	/* refcount btree levels */

	/*
	 * reserve some contiguous space for future logged fields before we add
	 * the unlogged fields. This makes the range logging via flags and
	 * structure offsets much simpler.
	 */
	__be64		agf_spare64[14];

	/* unlogged fields, written during buffer writeback. */
	__be64		agf_lsn;	/* last write sequence */
	__be32		agf_crc;	/* crc of agf sector */
	__be32		agf_spare2;

	/* structure must be padded to 64 bit alignment */
} xfs_agf_t;

#define XFS_AGF_CRC_OFF		offsetof(struct xfs_agf, agf_crc)

#define	XFS_AGF_MAGICNUM	(1u << 0)
#define	XFS_AGF_VERSIONNUM	(1u << 1)
#define	XFS_AGF_SEQNO		(1u << 2)
#define	XFS_AGF_LENGTH		(1u << 3)
#define	XFS_AGF_ROOTS		(1u << 4)
#define	XFS_AGF_LEVELS		(1u << 5)
#define	XFS_AGF_FLFIRST		(1u << 6)
#define	XFS_AGF_FLLAST		(1u << 7)
#define	XFS_AGF_FLCOUNT		(1u << 8)
#define	XFS_AGF_FREEBLKS	(1u << 9)
#define	XFS_AGF_LONGEST		(1u << 10)
#define	XFS_AGF_BTREEBLKS	(1u << 11)
#define	XFS_AGF_UUID		(1u << 12)
#define	XFS_AGF_RMAP_BLOCKS	(1u << 13)
#define	XFS_AGF_REFCOUNT_BLOCKS	(1u << 14)
#define	XFS_AGF_REFCOUNT_ROOT	(1u << 15)
#define	XFS_AGF_REFCOUNT_LEVEL	(1u << 16)
#define	XFS_AGF_SPARE64		(1u << 17)
#define	XFS_AGF_NUM_BITS	18
#define	XFS_AGF_ALL_BITS	((1u << XFS_AGF_NUM_BITS) - 1)

/*
 * Size of the unlinked inode hash table in the agi.
 */
#define	XFS_AGI_UNLINKED_BUCKETS	64

typedef struct xfs_agi {
	/*
	 * Common allocation group header information
	 */
	__be32		agi_magicnum;	/* magic number == XFS_AGI_MAGIC */
	__be32		agi_versionnum;	/* header version == XFS_AGI_VERSION */
	__be32		agi_seqno;	/* sequence # starting from 0 */
	__be32		agi_length;	/* size in blocks of a.g. */
	/*
	 * Inode information
	 * Inodes are mapped by interpreting the inode number, so no
	 * mapping data is needed here.
	 */
	__be32		agi_count;	/* count of allocated inodes */
	__be32		agi_root;	/* root of inode btree */
	__be32		agi_level;	/* levels in inode btree */
	__be32		agi_freecount;	/* number of free inodes */

	__be32		agi_newino;	/* new inode just allocated */
	__be32		agi_dirino;	/* last directory inode chunk */
	/*
	 * Hash table of inodes which have been unlinked but are
	 * still being referenced.
	 */
	__be32		agi_unlinked[XFS_AGI_UNLINKED_BUCKETS];
	/*
	 * This marks the end of logging region 1 and start of logging region 2.
	 */
	uuid_t		agi_uuid;	/* uuid of filesystem */
	__be32		agi_crc;	/* crc of agi sector */
	__be32		agi_pad32;
	__be64		agi_lsn;	/* last write sequence */

	__be32		agi_free_root; /* root of the free inode btree */
	__be32		agi_free_level;/* levels in free inode btree */

	__be32		agi_iblocks;	/* inobt blocks used */
	__be32		agi_fblocks;	/* finobt blocks used */

	/* structure must be padded to 64 bit alignment */
} xfs_agi_t;

#define XFS_AGI_CRC_OFF		offsetof(struct xfs_agi, agi_crc)

#define	XFS_AGI_MAGICNUM	(1u << 0)
#define	XFS_AGI_VERSIONNUM	(1u << 1)
#define	XFS_AGI_SEQNO		(1u << 2)
#define	XFS_AGI_LENGTH		(1u << 3)
#define	XFS_AGI_COUNT		(1u << 4)
#define	XFS_AGI_ROOT		(1u << 5)
#define	XFS_AGI_LEVEL		(1u << 6)
#define	XFS_AGI_FREECOUNT	(1u << 7)
#define	XFS_AGI_NEWINO		(1u << 8)
#define	XFS_AGI_DIRINO		(1u << 9)
#define	XFS_AGI_UNLINKED	(1u << 10)
#define	XFS_AGI_NUM_BITS_R1	11	/* end of the 1st agi logging region */
#define	XFS_AGI_ALL_BITS_R1	((1u << XFS_AGI_NUM_BITS_R1) - 1)
#define	XFS_AGI_FREE_ROOT	(1u << 11)
#define	XFS_AGI_FREE_LEVEL	(1u << 12)
#define	XFS_AGI_IBLOCKS		(1u << 13) /* both inobt/finobt block counters */
#define	XFS_AGI_NUM_BITS_R2	14

struct xfs_agfl {
	__be32		agfl_magicnum;
	__be32		agfl_seqno;
	uuid_t		agfl_uuid;
	__be64		agfl_lsn;
	__be32		agfl_crc;
} __attribute__((packed));

#define XFS_AGFL_CRC_OFF	offsetof(struct xfs_agfl, agfl_crc)

/*
 * XFS Timestamps
 * ==============
 *
 * Traditional ondisk inode timestamps consist of signed 32-bit counters for
 * seconds and nanoseconds; time zero is the Unix epoch, Jan  1 00:00:00 UTC
 * 1970, which means that the timestamp epoch is the same as the Unix epoch.
 * Therefore, the ondisk min and max defined here can be used directly to
 * constrain the incore timestamps on a Unix system.  Note that we actually
 * encode a __be64 value on disk.
 *
 * When the bigtime feature is enabled, ondisk inode timestamps become an
 * unsigned 64-bit nanoseconds counter.  This means that the bigtime inode
 * timestamp epoch is the start of the classic timestamp range, which is
 * Dec 13 20:45:52 UTC 1901.  Because the epochs are not the same, callers
 * /must/ use the bigtime conversion functions when encoding and decoding raw
 * timestamps.
 */
typedef __be64 xfs_timestamp_t;

/* Legacy timestamp encoding format. */
struct xfs_legacy_timestamp {
	__be32		t_sec;		/* timestamp seconds */
	__be32		t_nsec;		/* timestamp nanoseconds */
};

/*
 * On-disk inode structure.
 *
 * This is just the header or "dinode core", the inode is expanded to fill a
 * variable size the leftover area split into a data and an attribute fork.
 * The format of the data and attribute fork depends on the format of the
 * inode as indicated by di_format and di_aformat.  To access the data and
 * attribute use the XFS_DFORK_DPTR, XFS_DFORK_APTR, and XFS_DFORK_PTR macros
 * below.
 *
 * There is a very similar struct xfs_log_dinode which matches the layout of
 * this structure, but is kept in native format instead of big endian.
 *
 * Note: di_flushiter is only used by v1/2 inodes - it's effectively a zeroed
 * padding field for v3 inodes.
 */
#define	XFS_DINODE_MAGIC		0x494e	/* 'IN' */
struct xfs_dinode {
	__be16		di_magic;	/* inode magic # = XFS_DINODE_MAGIC */
	__be16		di_mode;	/* mode and type of file */
	__u8		di_version;	/* inode version */
	__u8		di_format;	/* format of di_c data */
	__be16		di_onlink;	/* old number of links to file */
	__be32		di_uid;		/* owner's user id */
	__be32		di_gid;		/* owner's group id */
	__be32		di_nlink;	/* number of links to file */
	__be16		di_projid_lo;	/* lower part of owner's project id */
	__be16		di_projid_hi;	/* higher part owner's project id */
	union {
		/* Number of data fork extents if NREXT64 is set */
		__be64	di_big_nextents;

		/* Padding for V3 inodes without NREXT64 set. */
		__be64	di_v3_pad;

		/* Padding and inode flush counter for V2 inodes. */
		struct {
			__u8	di_v2_pad[6];
			__be16	di_flushiter;
		};
	};
	xfs_timestamp_t	di_atime;	/* time last accessed */
	xfs_timestamp_t	di_mtime;	/* time last modified */
	xfs_timestamp_t	di_ctime;	/* time created/inode modified */
	__be64		di_size;	/* number of bytes in file */
	__be64		di_nblocks;	/* # of direct & btree blocks used */
	__be32		di_extsize;	/* basic/minimum extent size for file */
	union {
		/*
		 * For V2 inodes and V3 inodes without NREXT64 set, this
		 * is the number of data and attr fork extents.
		 */
		struct {
			__be32	di_nextents;
			__be16	di_anextents;
		} __packed;

		/* Number of attr fork extents if NREXT64 is set. */
		struct {
			__be32	di_big_anextents;
			__be16	di_nrext64_pad;
		} __packed;
	} __packed;
	__u8		di_forkoff;	/* attr fork offs, <<3 for 64b align */
	__s8		di_aformat;	/* format of attr fork's data */
	__be32		di_dmevmask;	/* DMIG event mask */
	__be16		di_dmstate;	/* DMIG state info */
	__be16		di_flags;	/* random flags, XFS_DIFLAG_... */
	__be32		di_gen;		/* generation number */

	/* di_next_unlinked is the only non-core field in the old dinode */
	__be32		di_next_unlinked;/* agi unlinked list ptr */

	/* start of the extended dinode, writable fields */
	__le32		di_crc;		/* CRC of the inode */
	__be64		di_changecount;	/* number of attribute changes */
	__be64		di_lsn;		/* flush sequence */
	__be64		di_flags2;	/* more random flags */
	__be32		di_cowextsize;	/* basic cow extent size for file */
	__u8		di_pad2[12];	/* more padding for future expansion */

	/* fields only written to during inode creation */
	xfs_timestamp_t	di_crtime;	/* time created */
	__be64		di_ino;		/* inode number */
	uuid_t		di_uuid;	/* UUID of the filesystem */

	/* structure must be padded to 64 bit alignment */
};

#define XFS_DINODE_CRC_OFF	offsetof(struct xfs_dinode, di_crc)

#define DI_MAX_FLUSH 0xffff

/*
 * Size of the core inode on disk.  Version 1 and 2 inodes have
 * the same size, but version 3 has grown a few additional fields.
 */
static inline uint xfs_dinode_size(int version)
{
	if (version == 3)
		return sizeof(struct xfs_dinode);
	return offsetof(struct xfs_dinode, di_crc);
}

/*
 * Values for di_format
 *
 * This enum is used in string mapping in xfs_trace.h; please keep the
 * TRACE_DEFINE_ENUMs for it up to date.
 */
enum xfs_dinode_fmt {
	XFS_DINODE_FMT_DEV,		/* xfs_dev_t */
	XFS_DINODE_FMT_LOCAL,		/* bulk data */
	XFS_DINODE_FMT_EXTENTS,		/* struct xfs_bmbt_rec */
	XFS_DINODE_FMT_BTREE,		/* struct xfs_bmdr_block */
	XFS_DINODE_FMT_UUID		/* added long ago, but never used */
};

#define XFS_INODE_FORMAT_STR \
	{ XFS_DINODE_FMT_DEV,		"dev" }, \
	{ XFS_DINODE_FMT_LOCAL,		"local" }, \
	{ XFS_DINODE_FMT_EXTENTS,	"extent" }, \
	{ XFS_DINODE_FMT_BTREE,		"btree" }, \
	{ XFS_DINODE_FMT_UUID,		"uuid" }

/*
 * Values for di_flags
 */
#define XFS_DIFLAG_REALTIME_BIT  0	/* file's blocks come from rt area */
#define XFS_DIFLAG_PREALLOC_BIT  1	/* file space has been preallocated */
#define XFS_DIFLAG_NEWRTBM_BIT   2	/* for rtbitmap inode, new format */
#define XFS_DIFLAG_IMMUTABLE_BIT 3	/* inode is immutable */
#define XFS_DIFLAG_APPEND_BIT    4	/* inode is append-only */
#define XFS_DIFLAG_SYNC_BIT      5	/* inode is written synchronously */
#define XFS_DIFLAG_NOATIME_BIT   6	/* do not update atime */
#define XFS_DIFLAG_NODUMP_BIT    7	/* do not dump */
#define XFS_DIFLAG_RTINHERIT_BIT 8	/* create with realtime bit set */
#define XFS_DIFLAG_PROJINHERIT_BIT   9	/* create with parents projid */
#define XFS_DIFLAG_NOSYMLINKS_BIT   10	/* disallow symlink creation */
#define XFS_DIFLAG_EXTSIZE_BIT      11	/* inode extent size allocator hint */
#define XFS_DIFLAG_EXTSZINHERIT_BIT 12	/* inherit inode extent size */
#define XFS_DIFLAG_NODEFRAG_BIT     13	/* do not reorganize/defragment */
#define XFS_DIFLAG_FILESTREAM_BIT   14  /* use filestream allocator */
/* Do not use bit 15, di_flags is legacy and unchanging now */

#define XFS_DIFLAG_REALTIME      (1 << XFS_DIFLAG_REALTIME_BIT)
#define XFS_DIFLAG_PREALLOC      (1 << XFS_DIFLAG_PREALLOC_BIT)
#define XFS_DIFLAG_NEWRTBM       (1 << XFS_DIFLAG_NEWRTBM_BIT)
#define XFS_DIFLAG_IMMUTABLE     (1 << XFS_DIFLAG_IMMUTABLE_BIT)
#define XFS_DIFLAG_APPEND        (1 << XFS_DIFLAG_APPEND_BIT)
#define XFS_DIFLAG_SYNC          (1 << XFS_DIFLAG_SYNC_BIT)
#define XFS_DIFLAG_NOATIME       (1 << XFS_DIFLAG_NOATIME_BIT)
#define XFS_DIFLAG_NODUMP        (1 << XFS_DIFLAG_NODUMP_BIT)
#define XFS_DIFLAG_RTINHERIT     (1 << XFS_DIFLAG_RTINHERIT_BIT)
#define XFS_DIFLAG_PROJINHERIT   (1 << XFS_DIFLAG_PROJINHERIT_BIT)
#define XFS_DIFLAG_NOSYMLINKS    (1 << XFS_DIFLAG_NOSYMLINKS_BIT)
#define XFS_DIFLAG_EXTSIZE       (1 << XFS_DIFLAG_EXTSIZE_BIT)
#define XFS_DIFLAG_EXTSZINHERIT  (1 << XFS_DIFLAG_EXTSZINHERIT_BIT)
#define XFS_DIFLAG_NODEFRAG      (1 << XFS_DIFLAG_NODEFRAG_BIT)
#define XFS_DIFLAG_FILESTREAM    (1 << XFS_DIFLAG_FILESTREAM_BIT)

/*
 * Values for di_flags2 These start by being exposed to userspace in the upper
 * 16 bits of the XFS_XFLAG_s range.
 */
#define XFS_DIFLAG2_DAX_BIT	0	/* use DAX for this inode */
#define XFS_DIFLAG2_REFLINK_BIT	1	/* file's blocks may be shared */
#define XFS_DIFLAG2_COWEXTSIZE_BIT   2  /* copy on write extent size hint */
#define XFS_DIFLAG2_BIGTIME_BIT	3	/* big timestamps */
#define XFS_DIFLAG2_NREXT64_BIT 4	/* large extent counters */

#define XFS_DIFLAG2_DAX		(1 << XFS_DIFLAG2_DAX_BIT)
#define XFS_DIFLAG2_REFLINK     (1 << XFS_DIFLAG2_REFLINK_BIT)
#define XFS_DIFLAG2_COWEXTSIZE  (1 << XFS_DIFLAG2_COWEXTSIZE_BIT)
#define XFS_DIFLAG2_BIGTIME	(1 << XFS_DIFLAG2_BIGTIME_BIT)
#define XFS_DIFLAG2_NREXT64	(1 << XFS_DIFLAG2_NREXT64_BIT)

/*
 * Dquot and dquot block format definitions
 */
#define XFS_DQUOT_MAGIC		0x4451		/* 'DQ' */
#define XFS_DQUOT_VERSION	(uint8_t)0x01	/* latest version number */

#define XFS_DQTYPE_USER		(1u << 0)	/* user dquot record */
#define XFS_DQTYPE_PROJ		(1u << 1)	/* project dquot record */
#define XFS_DQTYPE_GROUP	(1u << 2)	/* group dquot record */
#define XFS_DQTYPE_BIGTIME	(1u << 7)	/* large expiry timestamps */

/*
 * This is the main portion of the on-disk representation of quota information
 * for a user.  We pad this with some more expansion room to construct the on
 * disk structure.
 */
struct xfs_disk_dquot {
	__be16		d_magic;	/* dquot magic = XFS_DQUOT_MAGIC */
	__u8		d_version;	/* dquot version */
	__u8		d_type;		/* XFS_DQTYPE_USER/PROJ/GROUP */
	__be32		d_id;		/* user,project,group id */
	__be64		d_blk_hardlimit;/* absolute limit on disk blks */
	__be64		d_blk_softlimit;/* preferred limit on disk blks */
	__be64		d_ino_hardlimit;/* maximum # allocated inodes */
	__be64		d_ino_softlimit;/* preferred inode limit */
	__be64		d_bcount;	/* disk blocks owned by the user */
	__be64		d_icount;	/* inodes owned by the user */
	__be32		d_itimer;	/* zero if within inode limits if not,
					   this is when we refuse service */
	__be32		d_btimer;	/* similar to above; for disk blocks */
	__be16		d_iwarns;	/* warnings issued wrt num inodes */
	__be16		d_bwarns;	/* warnings issued wrt disk blocks */
	__be32		d_pad0;		/* 64 bit align */
	__be64		d_rtb_hardlimit;/* absolute limit on realtime blks */
	__be64		d_rtb_softlimit;/* preferred limit on RT disk blks */
	__be64		d_rtbcount;	/* realtime blocks owned */
	__be32		d_rtbtimer;	/* similar to above; for RT disk blocks */
	__be16		d_rtbwarns;	/* warnings issued wrt RT disk blocks */
	__be16		d_pad;
};

/*
 * This is what goes on disk. This is separated from the xfs_disk_dquot because
 * carrying the unnecessary padding would be a waste of memory.
 */
struct xfs_dqblk {
	struct xfs_disk_dquot	dd_diskdq; /* portion living incore as well */
	char			dd_fill[4];/* filling for posterity */

	/*
	 * These two are only present on filesystems with the CRC bits set.
	 */
	__be32		  dd_crc;	/* checksum */
	__be64		  dd_lsn;	/* last modification in log */
	uuid_t		  dd_uuid;	/* location information */
};

/*
 * Allocation Btree format definitions
 *
 * There are two on-disk btrees, one sorted by blockno and one sorted
 * by blockcount and blockno.  All blocks look the same to make the code
 * simpler; if we have time later, we'll make the optimizations.
 */
#define	XFS_ABTB_MAGIC		0x41425442	/* 'ABTB' for bno tree */
#define	XFS_ABTB_CRC_MAGIC	0x41423342	/* 'AB3B' */
#define	XFS_ABTC_MAGIC		0x41425443	/* 'ABTC' for cnt tree */
#define	XFS_ABTC_CRC_MAGIC	0x41423343	/* 'AB3C' */

/*
 * Data record/key structure
 */
typedef struct xfs_alloc_rec {
	__be32		ar_startblock;	/* starting block number */
	__be32		ar_blockcount;	/* count of free blocks */
} xfs_alloc_rec_t, xfs_alloc_key_t;

/*
 * Inode Allocation Btree format definitions
 *
 * There is a btree for the inode map per allocation group.
 */
#define	XFS_IBT_MAGIC		0x49414254	/* 'IABT' */
#define	XFS_IBT_CRC_MAGIC	0x49414233	/* 'IAB3' */
#define	XFS_FIBT_MAGIC		0x46494254	/* 'FIBT' */
#define	XFS_FIBT_CRC_MAGIC	0x46494233	/* 'FIB3' */

/*
 * The on-disk inode record structure has two formats. The original "full"
 * format uses a 4-byte freecount. The "sparse" format uses a 1-byte freecount
 * and replaces the 3 high-order freecount bytes wth the holemask and inode
 * count.
 *
 * The holemask of the sparse record format allows an inode chunk to have holes
 * that refer to blocks not owned by the inode record. This facilitates inode
 * allocation in the event of severe free space fragmentation.
 */
typedef struct xfs_inobt_rec {
	__be32		ir_startino;	/* starting inode number */
	union {
		struct {
			__be32	ir_freecount;	/* count of free inodes */
		} f;
		struct {
			__be16	ir_holemask;/* hole mask for sparse chunks */
			__u8	ir_count;	/* total inode count */
			__u8	ir_freecount;	/* count of free inodes */
		} sp;
	} ir_u;
	__be64		ir_free;	/* free inode mask */
} xfs_inobt_rec_t;

/*
 * Reverse mapping btree format definitions
 *
 * There is a btree for the reverse map per allocation group
 */
#define	XFS_RMAP_CRC_MAGIC	0x524d4233	/* 'RMB3' */

/*
 * Reference Count Btree format definitions
 *
 */
#define	XFS_REFC_CRC_MAGIC	0x52334643	/* 'R3FC' */

/*
 * Bmap btree record and extent descriptor.
 *  l0:63 is an extent flag (value 1 indicates non-normal).
 *  l0:9-62 are startoff.
 *  l0:0-8 and l1:21-63 are startblock.
 *  l1:0-20 are blockcount.
 */
#define BMBT_EXNTFLAG_BITLEN	1
#define BMBT_STARTOFF_BITLEN	54
#define BMBT_STARTBLOCK_BITLEN	52
#define BMBT_BLOCKCOUNT_BITLEN	21

#define BMBT_STARTOFF_MASK	((1ULL << BMBT_STARTOFF_BITLEN) - 1)
#define BMBT_BLOCKCOUNT_MASK	((1ULL << BMBT_BLOCKCOUNT_BITLEN) - 1)

#define XFS_MAX_BMBT_EXTLEN	((xfs_extlen_t)(BMBT_BLOCKCOUNT_MASK))

typedef struct xfs_bmbt_rec {
	__be64			l0, l1;
} xfs_bmbt_rec_t;

/*
 * BMAP Btree format definitions
 *
 * This includes both the root block definition that sits inside an inode fork
 * and the record/pointer formats for the leaf/node in the blocks.
 */
#define XFS_BMAP_MAGIC		0x424d4150	/* 'BMAP' */
#define XFS_BMAP_CRC_MAGIC	0x424d4133	/* 'BMA3' */

/*
 * Generic Btree block format definitions
 *
 * This is a combination of the actual format used on disk for short and long
 * format btrees.  The first three fields are shared by both format, but the
 * pointers are different and should be used with care.
 *
 * To get the size of the actual short or long form headers please use the size
 * macros below.  Never use sizeof(xfs_btree_block).
 *
 * The blkno, crc, lsn, owner and uuid fields are only available in filesystems
 * with the crc feature bit, and all accesses to them must be conditional on
 * that flag.
 */
/* short form block header */
struct xfs_btree_block_shdr {
	__be32		bb_leftsib;
	__be32		bb_rightsib;

	__be64		bb_blkno;
	__be64		bb_lsn;
	uuid_t		bb_uuid;
	__be32		bb_owner;
	__le32		bb_crc;
};

/* long form block header */
struct xfs_btree_block_lhdr {
	__be64		bb_leftsib;
	__be64		bb_rightsib;

	__be64		bb_blkno;
	__be64		bb_lsn;
	uuid_t		bb_uuid;
	__be64		bb_owner;
	__le32		bb_crc;
	__be32		bb_pad; /* padding for alignment */
};

struct xfs_btree_block {
	__be32		bb_magic;	/* magic number for block type */
	__be16		bb_level;	/* 0 is a leaf */
	__be16		bb_numrecs;	/* current # of data records */
	union {
		struct xfs_btree_block_shdr s;
		struct xfs_btree_block_lhdr l;
	} bb_u;				/* rest */
};

#endif
