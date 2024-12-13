/*
 * xfs filesystem image editor
 *
 * Reference: https://cdn.kernel.org/pub/linux/utils/fs/xfs/docs/xfs_filesystem_structure.pdf
 *
 * qianfan Zhao <qianfanguijin@163.com>
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "xfs_format.h"
#include "xfs_da_format.h"
#include "structure.h"
#include "bitmask.h"
#include "android_sparse.h"

#define PRINT_LEVEL0					"%-30s: "
#define PRINT_LEVEL1					"    %-26s: "
#define PRINT_LEVEL3					"        %-22s: "

static void xfs_dsb_print_versionnum(const char *print_name_fmt, const char *name,
				     const void *addr, size_t sz)
{
	#define VERSION_BIT(v) { XFS_SB_VERSION_##v##BIT, #v }
	static const struct structure_bit_descriptor version_bits[] = {
		VERSION_BIT(ATTR),
		VERSION_BIT(NLINK),
		VERSION_BIT(QUOTA),
		VERSION_BIT(ALIGN),
		VERSION_BIT(DALIGN),
		VERSION_BIT(SHARED),
		VERSION_BIT(LOGV2),
		VERSION_BIT(SECTOR),
		VERSION_BIT(EXTFLG),
		VERSION_BIT(DIRV2),
		VERSION_BIT(BORG),
		VERSION_BIT(MOREBITS),
		{ 0, NULL },
	};
	#undef VERSION_BIT

	structure_item_print_be_bit_flags(print_name_fmt, name, addr, sz,
					  version_bits);
}

static void xfs_dsb_print_features2(const char *print_name_fmt, const char *name,
				     const void *addr, size_t sz)
{
	#define VERSION_BIT(v) { XFS_SB_VERSION2_##v##BIT, #v }
	static const struct structure_bit_descriptor version_bits[] = {
		VERSION_BIT(RESERVED1),
		VERSION_BIT(LAZYSBCOUNT),
		VERSION_BIT(RESERVED4),
		VERSION_BIT(ATTR2),
		VERSION_BIT(PARENT),
		VERSION_BIT(PROJID32),
		VERSION_BIT(CRC),
		{ XFS_SB_VERSION2_FTYPE, "FTYPE" },
		{ 0, NULL },
	};
	#undef VERSION_BIT

	structure_item_print_be_bit_flags(print_name_fmt, name, addr, sz,
					  version_bits);
}

static void xfs_dsb_print_features_ro_compat(const char *print_name_fmt,
					     const char *name,
					     const void *addr, size_t sz)
{
	#define FEATURE(v) { XFS_SB_FEAT_RO_COMPAT_##v, #v }
	static const struct structure_bit_descriptor features[] = {
		FEATURE(FINOBT),
		FEATURE(RMAPBT),
		FEATURE(REFLINK),
		{ 0, NULL },
	};
	#undef FEATURE

	structure_item_print_be_bit_flags(print_name_fmt, name, addr, sz,
					  features);
}

static void xfs_dsb_print_features_incompat(const char *print_name_fmt,
					    const char *name,
					    const void *addr, size_t sz)
{
	#define FEATURE(v) { XFS_SB_FEAT_INCOMPAT_##v, #v }
	static const struct structure_bit_descriptor features[] = {
		FEATURE(FTYPE),
		FEATURE(SPINODES),
		FEATURE(META_UUID),
		FEATURE(BIGTIME),
		FEATURE(NEEDSREPAIR),
		FEATURE(NREXT64),
		FEATURE(EXCHRANGE),
		FEATURE(PARENT),
		{ 0, NULL },
	};
	#undef FEATURE

	structure_item_print_be_bit_flags(print_name_fmt, name, addr, sz,
					  features);
}

static void xfs_dsb_print_features_log_incompat(const char *print_name_fmt,
						const char *name,
						const void *addr, size_t sz)
{
	#define FEATURE(v) { XFS_SB_FEAT_INCOMPAT_LOG_##v, #v }
	static const struct structure_bit_descriptor features[] = {
		FEATURE(XATTRS),
		{ 0, NULL },
	};
	#undef FEATURE

	structure_item_print_be_bit_flags(print_name_fmt, name, addr, sz,
					  features);
}

static void xfs_dsb_print_ino(const char *print_name_fmt, const char *name,
			      const void *addr, size_t sz)
{
	size_t count_0 = 0, count_ff = 0;
	const uint8_t *p = addr;

	for (size_t i = 0; i < sz; i++) {
		switch (p[i]) {
		case 0:
			count_0++;
			break;
		case 0xff:
			count_ff++;
			break;
		}
	}

	if (count_0 == sz || count_ff == sz)
		structure_item_print_be_xunsigned(print_name_fmt, name, addr, sz);
	else
		structure_item_print_be_unsigned(print_name_fmt, name, addr, sz);
}

static const struct structure_item st_xfs_dsb[] = {
	STRUCTURE_ITEM(struct xfs_dsb, sb_magicnum,			structure_item_print_string),
	STRUCTURE_ITEM(struct xfs_dsb, sb_blocksize,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_dblocks,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_rblocks,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_rextents,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_uuid,				structure_item_print_x8_array),
	STRUCTURE_ITEM(struct xfs_dsb, sb_logstart,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_rootino,			xfs_dsb_print_ino),
	STRUCTURE_ITEM(struct xfs_dsb, sb_rbmino,			xfs_dsb_print_ino),
	STRUCTURE_ITEM(struct xfs_dsb, sb_rsumino,			xfs_dsb_print_ino),
	STRUCTURE_ITEM(struct xfs_dsb, sb_rextsize,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_agblocks,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_agcount,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_rbmblocks,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_logblocks,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_versionnum,			xfs_dsb_print_versionnum),
	STRUCTURE_ITEM(struct xfs_dsb, sb_sectsize,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_inodesize,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_inopblock,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_fname,			structure_item_print_string),
	STRUCTURE_ITEM(struct xfs_dsb, sb_blocklog,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_sectlog,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_inodelog,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_inopblog,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_agblklog,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_rextslog,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_inprogress,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_imax_pct,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_icount,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_ifree,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_fdblocks,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_frextents,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_uquotino,			xfs_dsb_print_ino),
	STRUCTURE_ITEM(struct xfs_dsb, sb_gquotino,			xfs_dsb_print_ino),
	STRUCTURE_ITEM(struct xfs_dsb, sb_qflags,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_flags,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_shared_vn,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_inoalignmt,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_unit,				structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_width,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_dirblklog,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_logsectlog,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_logsectsize,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_logsunit,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_features2,			xfs_dsb_print_features2),
	STRUCTURE_ITEM(struct xfs_dsb, sb_bad_features2,		xfs_dsb_print_features2),
	STRUCTURE_ITEM(struct xfs_dsb, sb_features_compat,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_features_ro_compat,		xfs_dsb_print_features_ro_compat),
	STRUCTURE_ITEM(struct xfs_dsb, sb_features_incompat,		xfs_dsb_print_features_incompat),
	STRUCTURE_ITEM(struct xfs_dsb, sb_features_log_incompat,	xfs_dsb_print_features_log_incompat),
	STRUCTURE_ITEM(struct xfs_dsb, sb_crc,				structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_spino_align,			structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_pquotino,			xfs_dsb_print_ino),
	STRUCTURE_ITEM(struct xfs_dsb, sb_lsn,				structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_dsb, sb_meta_uuid,			structure_item_print_x8_array),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item st_xfs_agf[] = {
	STRUCTURE_ITEM(struct xfs_agf, agf_magicnum,		structure_item_print_string),
	STRUCTURE_ITEM(struct xfs_agf, agf_versionnum,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_seqno,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_length,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_bno_root,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_cnt_root,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_rmap_root,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_bno_level,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_cnt_level,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_rmap_level,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_flfirst,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_fllast,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_flcount,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_freeblks,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_longest,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_btreeblks,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_uuid,		structure_item_print_x8_array),
	STRUCTURE_ITEM(struct xfs_agf, agf_rmap_blocks,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_refcount_blocks,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_refcount_root,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_refcount_level,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_spare64,		structure_item_print_hexdump),
	STRUCTURE_ITEM(struct xfs_agf, agf_lsn,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_crc,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_agf, agf_spare2,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item st_xfs_agi[] = {
	STRUCTURE_ITEM(struct xfs_agi, agi_magicnum,		structure_item_print_string),
	STRUCTURE_ITEM(struct xfs_agi, agi_versionnum,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_seqno,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_length,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_count,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_root,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_level,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_freecount,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_newino,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_dirino,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_unlinked,		structure_item_print_hexdump),
	STRUCTURE_ITEM(struct xfs_agi, agi_uuid,		structure_item_print_x8_array),
	STRUCTURE_ITEM(struct xfs_agi, agi_crc,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_pad32,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_lsn,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_free_root,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_free_level,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_iblocks,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agi, agi_fblocks,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item st_xfs_agfl[] = {
	STRUCTURE_ITEM(struct xfs_agfl, agfl_magicnum,		structure_item_print_string),
	STRUCTURE_ITEM(struct xfs_agfl, agfl_seqno,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_agfl, agfl_uuid,		structure_item_print_x8_array),
	STRUCTURE_ITEM(struct xfs_agfl, agfl_lsn,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_agfl, agfl_crc,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item st_xfs_btree_block_head[] = {
	STRUCTURE_ITEM(struct xfs_btree_block, bb_magic,	structure_item_print_string),
	STRUCTURE_ITEM(struct xfs_btree_block, bb_level,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_btree_block, bb_numrecs,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item st_xfs_btree_block_shdr[] = {
	STRUCTURE_ITEM(struct xfs_btree_block_shdr, bb_leftsib,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_btree_block_shdr, bb_rightsib,structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_btree_block_shdr, bb_blkno,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_btree_block_shdr, bb_lsn,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_btree_block_shdr, bb_uuid,	structure_item_print_x8_array),
	STRUCTURE_ITEM(struct xfs_btree_block_shdr, bb_owner,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_btree_block_shdr, bb_crc,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item st_xfs_inobt_rec[] = {
	STRUCTURE_ITEM(struct xfs_inobt_rec, ir_startino,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_inobt_rec, ir_u.sp.ir_holemask,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_inobt_rec, ir_u.sp.ir_count,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_inobt_rec, ir_u.sp.ir_freecount,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_inobt_rec, ir_free,			structure_item_print_be_xunsigned),
	STRUCTURE_ITEM_END(),
};

static const struct structure_item st_xfs_alloc_rec[] = {
	STRUCTURE_ITEM(struct xfs_alloc_rec, ar_startblock,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_alloc_rec, ar_blockcount,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM_END(),
};

static void xfs_dir2_sf_hdr_print_parent(const char *print_name_fmt,
					 const char *name,
					 const void *addr, size_t sz)
{
	const struct xfs_dir2_sf_hdr *hdr =
		container_of(addr, struct xfs_dir2_sf_hdr, parent);

	/* i8count: number of direcoty entries requiring 64-bit entries,
	 *          if any inode numbers require 64-bits.
	 *          Zero otherwise.
	 */
	structure_item_print_be_unsigned(print_name_fmt, name, addr,
					 hdr->i8count ? 8 : 4);
}

static const struct structure_item st_xfs_dir2_sf_hdr[] = {
	STRUCTURE_ITEM(struct xfs_dir2_sf_hdr, count,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dir2_sf_hdr, i8count,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_dir2_sf_hdr, parent,		xfs_dir2_sf_hdr_print_parent),
	STRUCTURE_ITEM_END(),
};

static void xfs_dinode_print_mode(const char *print_name_fmt, const char *name,
				  const void *p, size_t sz)
{
	uint16_t mode = be16_to_cpu(*(const __le16 *)p);

	structure_print_name(print_name_fmt, name);
	printf("0x%04x (", mode);

	switch (mode & S_IFMT) {
	case S_IFIFO:
		printf("FIFO");
		break;
	case S_IFCHR:
		printf("CHAR");
		break;
	case S_IFDIR:
		printf("Directory");
		break;
	case S_IFBLK:
		printf("Block");
		break;
	case S_IFREG:
		printf("File");
		break;
	case S_IFLNK:
		printf("Symlink");
		break;
	case S_IFSOCK:
		printf("Socket");
		break;
	}

	printf(" %o)", mode & ~S_IFMT);
	printf("\n");
}

static void xfs_dinode_print_format(const char *print_name_fmt, const char *name,
				    const void *addr, size_t sz)
{
	uint8_t di_format = *(const uint8_t *)addr;
	const char *names[] = {
		[XFS_DINODE_FMT_DEV] = "dev",
		[XFS_DINODE_FMT_LOCAL] = "local",
		[XFS_DINODE_FMT_EXTENTS] = "extent",
		[XFS_DINODE_FMT_BTREE] = "btree",
		[XFS_DINODE_FMT_UUID] =	"uuid",
	};

	structure_print_name(print_name_fmt, name);
	printf("%d ", di_format);
	if (di_format < sizeof(names) / sizeof(names[0]))
		printf("(%s)", names[di_format]);
	printf("\n");
}

static void xfs_dinode_print_ts(const char *print_name_fmt, const char *name,
				const void *addr, size_t sz)
{
	const struct xfs_legacy_timestamp *ts = addr;
	time_t t = be32_to_cpu(ts->t_sec);
	struct tm tm;
	char buf[64];

	structure_print_name(print_name_fmt, name);
	localtime_r(&t, &tm);
	strftime(buf, sizeof(buf), "%FT%T", &tm);
	printf("%s.%06d (%ld, %d)\n",
		buf, be32_to_cpu(ts->t_nsec) / 1000,
		t, be32_to_cpu(ts->t_nsec));
}

static void xfs_dinode_print_flag(const char *print_name_fmt, const char *name,
				  const void *addr, size_t sz)
{
	#define FLAG(v) { XFS_DIFLAG_##v, #v }
	static const struct structure_bit_descriptor flags[] = {
		FLAG(REALTIME),
		FLAG(PREALLOC),
		FLAG(NEWRTBM),
		FLAG(IMMUTABLE),
		FLAG(APPEND),
		FLAG(SYNC),
		FLAG(NOATIME),
		FLAG(NODUMP),
		FLAG(RTINHERIT),
		FLAG(PROJINHERIT),
		FLAG(NOSYMLINKS),
		FLAG(EXTSIZE),
		FLAG(EXTSZINHERIT),
		FLAG(NODEFRAG),
		FLAG(FILESTREAM),
		{ 0, NULL },
	};
	#undef FLAG

	structure_item_print_be_bit_flags(print_name_fmt, name, addr, sz,
					  flags);
}

static void xfs_dinode_print_flag2(const char *print_name_fmt, const char *name,
				   const void *addr, size_t sz)
{
	#define FLAG(v) { XFS_DIFLAG2_##v, #v }
	static const struct structure_bit_descriptor flags[] = {
		FLAG(DAX),
		FLAG(REFLINK),
		FLAG(COWEXTSIZE),
		FLAG(BIGTIME),
		FLAG(NREXT64),
		{ 0, NULL },
	};
	#undef FLAG

	structure_item_print_be_bit_flags(print_name_fmt, name, addr, sz,
					  flags);
}

static const struct structure_item st_xfs_dinode[] = {
	STRUCTURE_ITEM(struct xfs_dinode, di_magic,		structure_item_print_string),
	STRUCTURE_ITEM(struct xfs_dinode, di_mode,		xfs_dinode_print_mode),
	STRUCTURE_ITEM(struct xfs_dinode, di_version,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_format,		xfs_dinode_print_format),
	STRUCTURE_ITEM(struct xfs_dinode, di_onlink,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_uid,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_gid,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_nlink,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_projid_lo,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_projid_hi,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_big_nextents,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_atime,		xfs_dinode_print_ts),
	STRUCTURE_ITEM(struct xfs_dinode, di_mtime,		xfs_dinode_print_ts),
	STRUCTURE_ITEM(struct xfs_dinode, di_ctime,		xfs_dinode_print_ts),
	STRUCTURE_ITEM(struct xfs_dinode, di_size,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_nblocks,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_extsize,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_nextents,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_anextents,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_forkoff,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_aformat,		xfs_dinode_print_format),
	STRUCTURE_ITEM(struct xfs_dinode, di_dmevmask,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_dmstate,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_flags,		xfs_dinode_print_flag),
	STRUCTURE_ITEM(struct xfs_dinode, di_gen,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_next_unlinked,	structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_crc,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_changecount,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_lsn,		structure_item_print_be_xunsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_flags2,		xfs_dinode_print_flag2),
	STRUCTURE_ITEM(struct xfs_dinode, di_cowextsize,	structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_crtime,		xfs_dinode_print_ts),
	STRUCTURE_ITEM(struct xfs_dinode, di_ino,		structure_item_print_be_unsigned),
	STRUCTURE_ITEM(struct xfs_dinode, di_uuid,		structure_item_print_x8_array),
	STRUCTURE_ITEM_END(),
};

struct xfs_bmbt_irec {
	uint64_t			br_startoff;
	uint64_t			br_startblock;
	uint32_t			br_blockcount;
	int				br_state;
};

/* Table 19.1 Extent record format */
static void xfs_bmbt_rec_unpack(const struct xfs_bmbt_rec *rec,
				struct xfs_bmbt_irec *irec)
{
	uint64_t l0 = be64_to_cpu(rec->l0), l1 = be64_to_cpu(rec->l1);

	irec->br_state = !!(l0 & (1ull << 63));
	irec->br_startoff = (l0 >> 9) & BMBT_STARTOFF_MASK;

	irec->br_startblock = (l0 & 0x1f);
	irec->br_startblock <<= 43;
	irec->br_startblock |= (l1 >> 21);

	irec->br_blockcount = l1 & BMBT_BLOCKCOUNT_MASK;
}

struct xfs_ag {
	#define XFS_AG_SECTOR_BUFS	4
	#define XFS_AG_BLOCK_BUFS	5

	union {
		struct {
			struct xfs_dsb	*sb;
			struct xfs_agf	*agf;
			struct xfs_agi	*agi;
			struct xfs_agfl	*agfl;
		};
		void			*sector_bufs[XFS_AG_SECTOR_BUFS];
	};

	void				*bnobt;
	void				*cntbt;
	void				*inobt;
	void				*finobt;
	void				*refcountbt;
};

struct xfs_editor {
	int				fd;

	struct xfs_ag			*ags;
	uint32_t			ag_count;
	uint32_t			ag_blocks;

	uint32_t			block_size;
	uint16_t			sector_size;
	uint16_t			inode_per_block;
	uint16_t			inode_size;
};

static void xfs_editor_exit(void *private_data)
{
	struct xfs_editor *xfs = private_data;

	if (xfs->ags && xfs->ag_count) {
		for (uint32_t i = 0; i < xfs->ag_count; i++) {
			struct xfs_ag *ag = &xfs->ags[i];

			for (int i = 0; i < XFS_AG_SECTOR_BUFS; i++)
				free(ag->sector_bufs[i]);

			/* free null is ok */
			free(ag->bnobt);
			free(ag->cntbt);
			free(ag->inobt);
			free(ag->finobt);
			free(ag->refcountbt);
		}
		free(xfs->ags);
		xfs->ags = NULL;
	}
}

static int xfs_read_blocks(struct xfs_editor *xfs,
			    uint64_t blkno, uint32_t nblks,
			    void *buf, size_t bufsz)
{
	size_t sz = xfs->block_size * nblks;
	int n;

	if (sz > bufsz)
		sz = bufsz;

	fileseek(xfs->fd, blkno * xfs->block_size);
	n = fileread(xfs->fd, buf, sz);
	if (n < 0) {
		fprintf(stderr, "Error: read %d blocks from #%" PRIu64 "failed\n",
			nblks, blkno);
		return -1;
	}

	return 0;
}

static void *xfs_alloc_read_blocks(struct xfs_editor *xfs,
				    uint64_t blkno, uint32_t nblks)
{
	size_t sz = xfs->block_size * nblks;
	void *buf = malloc(sz);

	if (buf) {
		if (xfs_read_blocks(xfs, blkno, nblks, buf, sz) < 0) {
			free(buf);
			buf = NULL;
		}
	}

	return buf;
}

static int xfs_init_alloc_ags(struct xfs_editor *xfs, struct xfs_dsb *primary_sb)
{
	int ret = 0;

	xfs->sector_size = be16_to_cpu(primary_sb->sb_sectsize);
	xfs->block_size = be32_to_cpu(primary_sb->sb_blocksize);
	xfs->inode_size = be16_to_cpu(primary_sb->sb_inodesize);
	xfs->inode_per_block = be16_to_cpu(primary_sb->sb_inopblock);
	xfs->ag_count = be32_to_cpu(primary_sb->sb_agcount);
	xfs->ag_blocks = be32_to_cpu(primary_sb->sb_agblocks);

	xfs->ags = calloc(xfs->ag_count, sizeof(*xfs->ags));
	if (!xfs->ags)
		return -1;

	for (uint32_t i = 0; i < xfs->ag_count; i++) {
		struct xfs_ag *ag = &xfs->ags[i];

		for (int i = 0; i < XFS_AG_SECTOR_BUFS; i++) {
			ag->sector_bufs[i] = calloc(xfs->sector_size, 1);
			if (!ag->sector_bufs[i]) {
				ret = -1;
				break;
			}
		}
	}

	if (ret < 0)
		xfs_editor_exit(xfs);

	return ret;
}

static int xfs_check_sb(struct xfs_dsb *sb, int force_type)
{
	if (be32_to_cpu(sb->sb_magicnum) != XFS_SB_MAGIC) {
		fprintf_if_force_type("Error: bad magic\n");
		return -1;
	}

#define sb_check_logsize(sb, bits, name, logname) do {			\
	if (be##bits##_to_cpu((sb)->name) != (1ul << (sb)->logname)) {	\
		fprintf_if_force_type(					\
			"Error: %s size %u doesn't match %s %d\n",	\
			#name, be32_to_cpu((sb)->name),			\
			#logname, (sb)->logname);			\
		return -1;						\
	}								\
} while (0)

	sb_check_logsize(sb, 32, sb_blocksize, sb_blocklog);
	sb_check_logsize(sb, 16, sb_sectsize, sb_sectlog);
	sb_check_logsize(sb, 16, sb_inodesize, sb_inodelog);
	sb_check_logsize(sb, 16, sb_inopblock, sb_inopblog);

	/* sb_agblocks maybe not matched with sb_agblocklog
	 * Error: sb_agblocks size 32000 doesn't match sb_agblklog 15
	 */
	#undef sb_check_logsize

	return 0;
}

static void *xfs_alloc_read_bt(struct xfs_editor *xfs, int ag_idx,
			       uint32_t blkoff,
			       uint32_t magic1, uint32_t magic2,
			       int force_type)
{
	#define is_good_magic(m, expected) (expected != 0 && m == expected)
	unsigned long blk = ag_idx * xfs->ag_blocks + blkoff;
	uint32_t magic;
	void *blkbuf;

	fileseek(xfs->fd, blk * xfs->block_size);
	blkbuf = xfs_alloc_read_blocks(xfs, blk, 1);
	if (!blkbuf)
		return blkbuf;

	magic = be32_to_cpu(*(__be32 *)blkbuf);
	if (is_good_magic(magic, magic1) || is_good_magic(magic, magic2))
		return blkbuf;

	fprintf_if_force_type("Error: ag%u block %u magic 0x%08x doesn't "
			      "match 0x%08x\n",
			      ag_idx, blkoff, magic, magic1);
	free(blkbuf);
	return NULL;
}

static int xfs_load_ags(struct xfs_editor *xfs, int force_type)
{
	void *blkbuf = malloc(xfs->block_size);
	int ret = 0;

	if (!blkbuf)
		return -1;

	for (uint32_t i = 0; i < xfs->ag_count; i++) {
		static const uint32_t magics[XFS_AG_SECTOR_BUFS] = {
			XFS_SB_MAGIC,
			XFS_AGF_MAGIC,
			XFS_AGI_MAGIC,
			XFS_AGFL_MAGIC,
		};

		uint64_t offset = i * xfs->ag_blocks * xfs->block_size;
		struct xfs_ag *ag = &xfs->ags[i];
		uint32_t magic;

		/* SB, AGF, AGI, AGFL saved in the first block */
		fileseek(xfs->fd, offset);
		for (int sector = 0; sector < XFS_AG_SECTOR_BUFS; sector++) {
			ret = fileread(xfs->fd, ag->sector_bufs[sector],
				       xfs->sector_size);
			if (ret < 0) {
				fprintf_if_force_type("Error: read ag%u sector"
						" %u failed\n", i, sector);
				goto done;
			}

			switch (sector) {
			case 0:
				ret = xfs_check_sb(ag->sb, force_type);
				if (ret < 0)
					goto done;
				break;
			default:
				magic = be32_to_cpu(*(__be32 *)ag->sector_bufs[sector]);
				if (magic != magics[sector]) {
					fprintf_if_force_type(
						"Error: ag%u sector %u magic 0x%08x"
						" doesn't match 0x%08x\n",
						i, sector, magic, magics[sector]);
					goto done;
				}
				break;
			}
		}

		#define ag_try_load_bt(root, target, magic1, magic2) do {	\
			if (ag->root) {						\
				ag->target = xfs_alloc_read_bt(xfs, i,		\
					be32_to_cpu(ag->root),			\
					magic1, magic2,				\
					force_type);				\
				if (!ag->target) {				\
					ret = -1;				\
					goto done;				\
				}						\
			}							\
		} while (0)

		ag_try_load_bt(agf->agf_bno_root, bnobt, XFS_ABTB_MAGIC, XFS_ABTB_CRC_MAGIC);
		ag_try_load_bt(agf->agf_cnt_root, cntbt, XFS_ABTC_MAGIC, XFS_ABTC_CRC_MAGIC);
		ag_try_load_bt(agf->agf_refcount_root, refcountbt, XFS_REFC_CRC_MAGIC, 0);
		ag_try_load_bt(agi->agi_root, inobt, XFS_IBT_MAGIC, XFS_IBT_CRC_MAGIC);
		ag_try_load_bt(agi->agi_free_root, finobt, XFS_FIBT_MAGIC, XFS_FIBT_CRC_MAGIC);
		#undef ag_try_load_bt
	}

done:
	free(blkbuf);
	return ret;
}

static int xfs_detect(void *private_data, int force_type, int fd)
{
	struct xfs_editor *xfs = private_data;
	struct xfs_dsb primary_sb;
	int ret;

	ret = fileread(fd, &primary_sb, sizeof(primary_sb));
	if (ret < 0)
		return ret;

	ret = xfs_check_sb(&primary_sb, force_type);
	if (ret < 0)
		return ret;

	xfs->fd = fd;
	ret = xfs_init_alloc_ags(xfs, &primary_sb);
	if (ret < 0)
		return ret;

	ret = xfs_load_ags(xfs, force_type);
	if (ret < 0) {
		xfs_editor_exit(xfs);
		return ret;
	}

	return 0;
}

static struct xfs_dinode *_xfs_alloc_read_inode(struct xfs_editor *xfs, int ino,
						uint32_t *ret_blkno,
						uint32_t *ret_blkoffset)


{
	struct xfs_dsb *sb = xfs->ags[0].sb;
	uint32_t blkno, blkoffset;
	struct xfs_dinode *inode;
	char reason[128] = { 0 };
	int ret;

	if (ino < (int)be64_to_cpu(sb->sb_rootino)) {
		fprintf(stderr, "Error: read ino #%u failed"
			" (less than root ino %u)\n",
			ino, (unsigned int)be64_to_cpu(sb->sb_rootino));

		return NULL;
	}

	blkno = ino / xfs->inode_per_block;
	blkoffset = (ino % xfs->inode_per_block) * xfs->inode_size;
	if (ret_blkno)
		*ret_blkno = blkno;
	if (ret_blkoffset)
		*ret_blkoffset = blkoffset;

	fileseek(xfs->fd, blkno * xfs->block_size + blkoffset);
	inode = malloc(xfs->inode_size);
	if (!inode)
		return inode;

	ret = fileread(xfs->fd, inode, xfs->inode_size);
	if (ret < 0) {
		snprintf(reason, sizeof(reason), "io fault");
	} else if (be16_to_cpu(inode->di_magic) != XFS_DINODE_MAGIC) {
		snprintf(reason, sizeof(reason), "bad magic 0x%08x",
			 be16_to_cpu(inode->di_magic));
		ret = -1;
	} else if (be64_to_cpu(inode->di_ino) != (uint64_t)ino) {
		snprintf(reason, sizeof(reason), "bad ino %u != %" PRIu64 ")",
			ino, be64_to_cpu(inode->di_ino));
		ret = -1;
	}

	if (ret < 0) {
		fprintf(stderr, "Error: read inode #%u from block %u+%u"
				" failed (%s)\n",
				ino, blkno, blkoffset, reason);
		free(inode);
		return NULL;
	}

	return inode;
}

struct xfs_structure {
	const char				*name;
	uint32_t				magic;
	size_t					magic_size;
	const struct structure_item		*layout;
	void (*print)(struct xfs_editor *xfs, const void *buf, size_t bufsz);
	const char				*comment;
};

#define XFSST_MAGIC32(_name, _magic32, _layout, ...)	{	\
	.name = _name,						\
	.magic = _magic32,					\
	.magic_size = 4,					\
	.layout = _layout,					\
	##__VA_ARGS__						\
}

#define XFSST_MAGIC16(_name, _magic16, _layout, ...)	{	\
	.name = _name,						\
	.magic = _magic16,					\
	.magic_size = 2,					\
	.layout = _layout,					\
	##__VA_ARGS__						\
}

#define XFSST_MAGIC32_PRINT(_name, _magic32, _print, ...) {	\
	.name = _name,						\
	.magic = _magic32,					\
	.magic_size = 4,					\
	.print = _print,					\
	##__VA_ARGS__						\
}

#define XFSST_MAGIC16_PRINT(_name, _magic16, _print, ...) {	\
	.name = _name,						\
	.magic = _magic16,					\
	.magic_size = 2,					\
	.print = _print,					\
	##__VA_ARGS__						\
}

static void xfs_structure_print(struct xfs_editor *xfs,
				const struct xfs_structure *s,
				const void *buf, size_t bufsz)
{
	if (s->print)
		s->print(xfs, buf, bufsz);
	else if (s->layout)
		structure_print(PRINT_LEVEL0, buf, s->layout);
	else
		structure_item_print_hexdump(PRINT_LEVEL0, "rawdata", buf, bufsz);
}

static void print_structure_and_remain(struct xfs_editor *xfs,
				       const void *buf, size_t bufsz,
				       size_t one_structure_size,
				       const struct structure_item *layout)
{
	int remain = (int)bufsz - (int)one_structure_size;

	structure_print(PRINT_LEVEL0, buf, layout);

	if (remain > 0) {
		printf("\n");
		structure_item_print_hexdump(PRINT_LEVEL0, "remain-data",
					     buf + one_structure_size,
					     remain);
	}
}

#define DEFINE_PRINT_STRUCTURE_AND_REMAIN(name)					\
static void print_##name(struct xfs_editor *xfs, const void *buf, size_t bufsz)	\
{										\
	print_structure_and_remain(xfs, buf, bufsz,				\
				   sizeof(struct name), st_##name);		\
}

DEFINE_PRINT_STRUCTURE_AND_REMAIN(xfs_dsb);
DEFINE_PRINT_STRUCTURE_AND_REMAIN(xfs_agf);
DEFINE_PRINT_STRUCTURE_AND_REMAIN(xfs_agi);

static void print_xfs_agfl(struct xfs_editor *xfs, const void *buf, size_t bufsz)
{
	size_t point_count = (bufsz - sizeof(struct xfs_agfl)) / sizeof(__be32);
	const __be32 *point_array = buf + sizeof(struct xfs_agfl);

	structure_print(PRINT_LEVEL0, buf, st_xfs_agfl);

	printf("\nArray of free block numbers:\n");
	for (size_t i = 0; i < point_count; i++) {
		uint32_t point = be32_to_cpu(point_array[i]);

		if (point == 0xffffffff)
			continue;

		printf("[%03zu] = %u\n", i, point);
	}
}

/* Return the address of shdr or lhdr, and save the hdr size to @ret_hdr_size */
static const void *xfs_btree_block_get_hdr(struct xfs_editor *xfs,
					   const struct xfs_btree_block *b,
					   size_t *ret_hdr_size)
{
	/* FIXME: auto detect shdr and lhdr */
	if (ret_hdr_size)
		*ret_hdr_size = sizeof(b->bb_u.s);

	return &b->bb_u;
}

static const void *xfs_btree_block_get_data(struct xfs_editor *xfs,
					    const struct xfs_btree_block *b,
					    size_t total_buf_size,
					    size_t *ret_data_size)
{
	size_t bb_hdr_size = 0;
	const void *bb_hdr;

	bb_hdr = xfs_btree_block_get_hdr(xfs, b, &bb_hdr_size);
	if (ret_data_size)
		*ret_data_size = total_buf_size - bb_hdr_size;

	return bb_hdr + bb_hdr_size;
}

static void _print_btree_block(struct xfs_editor *xfs,
			       const void *buf, size_t bufsz,
			       size_t one_structure_size,
			       const struct structure_item *st_remain_data)
{
	const struct xfs_btree_block *b = buf;
	size_t bb_hdr_size = 0, remain_data_size = 0;
	const void *bb_hdr, *remain_data;

	bb_hdr = xfs_btree_block_get_hdr(xfs, b, &bb_hdr_size);
	remain_data = xfs_btree_block_get_data(xfs, b, bufsz, &remain_data_size);

	structure_print(PRINT_LEVEL0, buf, st_xfs_btree_block_head);

	switch (bb_hdr_size) {
	case sizeof(struct xfs_btree_block_shdr):
		structure_print(PRINT_LEVEL0, bb_hdr, st_xfs_btree_block_shdr);
		break;
	case sizeof(struct xfs_btree_block_lhdr):
		/* TODO */
		break;
	}

	printf("\n");

	if (st_remain_data) {
		for (int i = 0; i < be16_to_cpu(b->bb_numrecs); i++) {
			printf("\n");
			structure_print(PRINT_LEVEL0, remain_data, st_remain_data);
			remain_data += one_structure_size;
		}
	} else {
		structure_item_print_hexdump(PRINT_LEVEL0, "remain-data",
					     remain_data, remain_data_size);
	}
}

static void print_btree_block_default(struct xfs_editor *xfs,
				      const void *buf, size_t bufsz)
{
	_print_btree_block(xfs, buf, bufsz, 0, NULL);
}

#define DEFINE_PRINT_BTREE_BLOCK_FUNCTION(name, struct_name)			\
static void print_btree_block_##name(struct xfs_editor *xfs, const void *buf,	\
				    size_t bufsz)				\
{										\
	_print_btree_block(xfs, buf, bufsz, sizeof(struct struct_name),		\
			   st_##struct_name);					\
}

DEFINE_PRINT_BTREE_BLOCK_FUNCTION(alloc_rec, xfs_alloc_rec);
DEFINE_PRINT_BTREE_BLOCK_FUNCTION(inobt_rec, xfs_inobt_rec);

static void print_xfs_dir2_sf(struct xfs_editor *xfs,
			     const void *buf, size_t bufsz)
{
	const struct xfs_dir2_sf_hdr *hdr = buf;
	size_t entry_offset = xfs_dir2_sf_hdr_size(hdr->i8count);

	structure_print(PRINT_LEVEL0, hdr, st_xfs_dir2_sf_hdr);
	printf("\n");

	for (int i = 0; i < hdr->count; i++) {
		const struct xfs_dir2_sf_entry *entry = buf + entry_offset;
		static const struct ksmap ftmaps[] = {
			XFS_DIR3_FTYPE_STR,
			{ 0, NULL }
		};
		const struct ksmap *ft;
		uint8_t ftype;

		printf("offset=0x%02x%02x \"%.*s\" is",
			entry->offset[0], entry->offset[1],
			entry->namelen, entry->name);

		entry_offset += sizeof(*entry);
		entry_offset += entry->namelen;

		ftype = ((const uint8_t *)buf)[entry_offset];
		ft = ksmap_find(ftmaps, ftype);
		if (ft)
			printf(" %s", ft->str);
		else
			printf(" ftype=%u", ftype);

		entry_offset += sizeof(ftype);

		printf(" and inode=#");
		if (hdr->i8count) {
			uint64_t ino = be64_to_cpu(*(__be64 *)(buf + entry_offset));

			printf("%" PRIu64, ino);
			entry_offset += sizeof(ino);
		} else {
			uint32_t ino = be32_to_cpu(*(__be32 *)(buf + entry_offset));

			printf("%u", ino);
			entry_offset += sizeof(ino);
		}
		printf("\n");
	}
}

static void xfs_bmbt_rec_print(struct xfs_editor *xfs,
			       const struct xfs_dinode *inode,
			       const void *buf, size_t bufsz)
{
	const struct xfs_bmbt_rec *rec = buf;
	struct xfs_bmbt_irec irec;

	xfs_bmbt_rec_unpack(rec, &irec);

	printf("0x%016" PRIx64 "%016" PRIx64 "\n",
		be64_to_cpu(rec->l0), be64_to_cpu(rec->l1));

	printf( "flag is %d,"
		" logical file block offset #%" PRIu64
		", absolute block number #%" PRIu64
		", total %u blocks\n",
		irec.br_state,
		irec.br_startoff,
		irec.br_startblock,
		irec.br_blockcount);
}

#define dinode_glue_format_mode(format, mode) (((format) << 16) | ((mode) & S_IFMT))

static void print_xfs_dinode(struct xfs_editor *xfs,
			     const void *buf, size_t bufsz)
{
	const struct xfs_dinode *inode = buf;
	const void *remainbuf = buf + xfs_dinode_size(inode->di_version);
	int print_hexdump = 0;
	int remain;

	if (bufsz > xfs->inode_size)
		bufsz = xfs->inode_size;

	remain = (int)bufsz - xfs_dinode_size(inode->di_version);
	structure_print(PRINT_LEVEL0, buf, st_xfs_dinode);
	if (remain <= 0)
		return;

	/* 18.3.1 Regular Files(S_IFREG)
	 *    XFS_DINODE_FMT_EXTENTS (xfs_bmbt_rec)
	 *    XFS_DINODE_FMT_BTREE   (xfs_bmdr_block)
	 *
	 * 18.3.2 Direcroties(S_IFDIR)
	 *    XFS_DINODE_FMT_LOCAL   (xfs_dir2_sf)
	 *    XFS_DINODE_FMT_EXTENTS (xfs_bmbt_rec)
	 *    XFS_DINODE_FMT_BTREE   (xfs_bmdr_block)
	 *
	 * 18.3.3 Symbolic Links(S_IFLNK)
	 *    XFS_DINODE_FMT_LOCAL   (char *)
	 *    XFS_DINODE_FMT_EXTENTS (xfs_bmbt_rec)
	 *
	 * 18.3.4 Other File Types(S_IFCHR and S_IFBLK)
	 *                            (xfs_dev)
	 */
	switch (dinode_glue_format_mode(inode->di_format, be16_to_cpu(inode->di_mode))) {
	case dinode_glue_format_mode(XFS_DINODE_FMT_EXTENTS, S_IFREG):
	case dinode_glue_format_mode(XFS_DINODE_FMT_EXTENTS, S_IFDIR):
	case dinode_glue_format_mode(XFS_DINODE_FMT_EXTENTS, S_IFLNK):
		printf("\nxfs_bmbt_rec_t:\n");
		xfs_bmbt_rec_print(xfs, inode, remainbuf, remain);
		break;
	case dinode_glue_format_mode(XFS_DINODE_FMT_BTREE, S_IFREG):
	case dinode_glue_format_mode(XFS_DINODE_FMT_BTREE, S_IFDIR):
		printf("\nxfs_bmdr_block_t:\n");
		structure_item_print_hexdump(PRINT_LEVEL0, "", remainbuf, remain);
		break;
	case dinode_glue_format_mode(XFS_DINODE_FMT_LOCAL, S_IFDIR):
		printf("\nxfs_dir2_sf_t:\n");
		print_xfs_dir2_sf(xfs, remainbuf, remain);
		break;
	case dinode_glue_format_mode(XFS_DINODE_FMT_LOCAL, S_IFLNK):
		printf("\nchar *:\n");
		structure_item_print_hexdump(PRINT_LEVEL0, "", remainbuf, remain);
		break;
	default:
		print_hexdump = 1;
		break;
	}

	if (print_hexdump && remain > 0) {
		printf("\n");
		structure_item_print_hexdump(PRINT_LEVEL0, "remain-data",
					     remainbuf, remain);
	}
}

#define XLOG_HEADER_MAGIC_NUM	0xFEEDbabe	/* Invalid cycle number */

static const struct xfs_structure xfs_structures[] = {
	XFSST_MAGIC32_PRINT("sblock",		XFS_SB_MAGIC, 		print_xfs_dsb,			.comment = "Superblock"),
	XFSST_MAGIC32_PRINT("agf",		XFS_AGF_MAGIC,		print_xfs_agf,			.comment = "Free space"),
	XFSST_MAGIC32_PRINT("agi",		XFS_AGI_MAGIC,		print_xfs_agi,			.comment = "Inode information"),
	XFSST_MAGIC32_PRINT("agfl",		XFS_AGFL_MAGIC,		print_xfs_agfl,			.comment = "Free space list"),

	XFSST_MAGIC32_PRINT("bnobt",		XFS_ABTB_MAGIC,		print_btree_block_alloc_rec,	.comment = "Free space sorted by block B+tree"),
	XFSST_MAGIC32_PRINT("bnobt-crc",	XFS_ABTB_CRC_MAGIC,	print_btree_block_alloc_rec,	.comment = "Free space sorted by block B+tree"),
	XFSST_MAGIC32_PRINT("cntbt",		XFS_ABTC_MAGIC,		print_btree_block_alloc_rec,	.comment = "Free space sorted by size B+tree"),
	XFSST_MAGIC32_PRINT("cntbt-crc",	XFS_ABTC_CRC_MAGIC,	print_btree_block_alloc_rec,	.comment = "Free space sorted by size B+tree"),
	XFSST_MAGIC32_PRINT("inobt",		XFS_IBT_MAGIC,		print_btree_block_inobt_rec,	.comment = "Inode B+tree"),
	XFSST_MAGIC32_PRINT("inobt-crc",	XFS_IBT_CRC_MAGIC,	print_btree_block_inobt_rec,	.comment = "Inode B+tree"),
	XFSST_MAGIC32_PRINT("finobt",		XFS_FIBT_MAGIC,		print_btree_block_inobt_rec,	.comment = "Free inode B+tree"),
	XFSST_MAGIC32_PRINT("finobt-crc",	XFS_FIBT_CRC_MAGIC,	print_btree_block_inobt_rec,	.comment = "Free inode B+tree"),
	XFSST_MAGIC32_PRINT("refcountbt-crc",	XFS_REFC_CRC_MAGIC,	print_btree_block_default,	.comment = "Reference count B+tree"),
	XFSST_MAGIC32_PRINT("rmapbt",		XFS_RMAP_CRC_MAGIC,	print_btree_block_default,	.comment = "Reverse Mapping B+tree"),
	XFSST_MAGIC32_PRINT("btbt",		XFS_BMAP_MAGIC,		print_btree_block_default,	.comment = "B+Tree Extent list"),
	XFSST_MAGIC32_PRINT("btbt-crc",		XFS_BMAP_CRC_MAGIC,	print_btree_block_default,	.comment = "B+Tree Extent list"),

	XFSST_MAGIC16_PRINT("dinode",		XFS_DINODE_MAGIC,	print_xfs_dinode),
	XFSST_MAGIC16("dquot",			XFS_DQUOT_MAGIC,	NULL),

	XFSST_MAGIC32("xlog-header",		XLOG_HEADER_MAGIC_NUM,	NULL),

	{ NULL },
};

static const struct xfs_structure *xfs_structure_find_byname(const char *name)
{
	for (const struct xfs_structure *s = xfs_structures; s->name; s++) {
		if (!strcmp(name, s->name))
			return s;
	}

	return NULL;
}

static const struct xfs_structure *
	xfs_structure_find_bymagic(const void *magic_buf)
{
	uint32_t magic32 = be32_to_cpu(*(__be32 *)magic_buf);
	uint16_t magic16 = be16_to_cpu(*(__be16 *)magic_buf);

	for (const struct xfs_structure *s = xfs_structures; s->name; s++) {
		if (s->magic_size == 4 && s->magic == magic32)
			return s;
		if (s->magic_size == 2 && s->magic == magic16)
			return s;
	}

	return NULL;
}

/* show block raw data */
static int xfs_do_block(void *private_data, int fd, int argc, char **argv)
{
	struct xfs_editor *xfs = private_data;
	const char *asas = NULL;
	int ret = 0, blkno = -1, sector = 0;
	uint8_t *blk = NULL, *buf = NULL;
	size_t bufsz = xfs->block_size;
	uint32_t magic;

	/* block #blkno as inode */
	if (argc > 1) {
		char *endp;

		blkno = (int)strtol(argv[1], &endp, 0);
		if (*endp == '.') {
			sector = (int)strtol(endp + 1, &endp, 0);
			if (*endp != '\0')
				goto usage;

			bufsz = xfs->sector_size;
		} else if (*endp != '\0') {
			goto usage;
		}

		if ((unsigned)sector >= xfs->block_size / xfs->sector_size) {
			fprintf(stderr, "Error: sector %d out of range\n",
				sector);
			return -1;
		}
	}

	if (argc >= 3) {
		if (strcmp(argv[2], "as") || argc < 4)
			goto usage;
		asas = argv[3];
	}

	if (blkno < 0) {
	usage:
		fprintf(stderr, "Usage: xfs block #blkno[.#sector] ");
		fprintf(stderr, "[ as sblock ]\n");
		return -1;
	}

	blk = xfs_alloc_read_blocks(xfs, blkno, 1);
	if (!blk)
		return -1;

	buf = blk + xfs->sector_size * sector;
	magic = be32_to_cpu(*(__be32 *)buf);

	if (!asas) {
		unsigned long baseaddr = blkno * xfs->block_size
					+ xfs->sector_size * sector;
		hexdump(buf, bufsz, baseaddr);
		goto done;
	}

	if (!strcmp(asas, "auto")) {
		const struct xfs_structure *s = xfs_structure_find_bymagic(buf);
		char magic_string[32] = { 0 };

		if (!s) {
			/* The first 4 bytes is magic number */
			memcpy(magic_string, buf, 4);

			for (int i = 0; i < 4; i++) {
				if (!isprint(magic_string[i])) {
					snprintf(magic_string,
						 sizeof(magic_string),
						 "0x%08x", magic);
					break;
				}
			}

			fprintf(stderr, "Error: unknown magic %s\n",
				magic_string);
			ret = -1;
			goto done;
		}

		printf("Auto detected xfs %s", s->name);
		if (s->comment)
			printf(" (%s)", s->comment);
		printf("\n\n");
		xfs_structure_print(xfs, s, buf, bufsz);
	} else {
		const struct xfs_structure *s = xfs_structure_find_byname(asas);

		if (!s) {
			fprintf(stderr, "Error: unknown structure %s\n", asas);
			ret = -1;
			goto done;
		}

		if (s->magic_size == 2)
			magic >>= 16;

		if (magic != s->magic) {
			fprintf(stderr, "Error: magic %08x != %s magic %08x\n",
				magic, asas, s->magic);
			ret = -1;
			goto done;
		}

		xfs_structure_print(xfs, s, buf, bufsz);
	}

done:
	free(blk);
	return ret;
}

struct do_magic_arg {
	uint32_t		count;
	uint32_t		block_offset;
};

static int xfs_do_magic(void *private_data, int fd, int argc, char **argv)
{
	struct do_magic_arg argbuffer[sizeof(xfs_structures) / sizeof(xfs_structures[0])];
	struct xfs_editor *xfs = private_data;
	uint32_t xlog_start = 0, xlog_blks = 0;
	int continue_xlog_stoped = 0;
	void *blkbuf;
	int aligned = xfs->sector_size;
	int ret = 0;

	if (argc > 1) {
		aligned = atoi(argv[1]);
		if (aligned <= 0) {
			fprintf(stderr, "Error: invalid aligned size %s\n",
				argv[1]);
			return -1;
		}
	}

	blkbuf = malloc(xfs->block_size);
	if (!blkbuf) {
		fprintf(stderr, "Error: malloc failed\n");
		return -1;
	}

	for (size_t blkno = 0; blkno < be64_to_cpu(xfs->ags[0].sb->sb_dblocks); blkno++) {
		uint32_t offset = 0;

		ret = xfs_read_blocks(xfs, blkno, 1, blkbuf, xfs->block_size);
		if (ret < 0)
			break;

		memset(argbuffer, 0, sizeof(argbuffer));

		while (offset < xfs->block_size) {
			uint8_t *mem = blkbuf + offset;
			const struct xfs_structure *s = xfs_structure_find_bymagic(mem);

			if (!s)
				goto next;

			if (s->magic == XLOG_HEADER_MAGIC_NUM) {
				if (xlog_blks == 0) {
					xlog_start = blkno;
					xlog_blks = 1;
					goto next;
				}

				/* another sector in this block */
				if (offset != 0)
					goto next;

				if (xlog_start + xlog_blks == blkno) {
					xlog_blks++;
					goto next;
				}
			}

			if (xlog_blks > 0)
				continue_xlog_stoped++;

			argbuffer[s - xfs_structures].count++;
			argbuffer[s - xfs_structures].block_offset = offset;

		next:
			offset += aligned;
		}

		if (continue_xlog_stoped) {
			printf("block %8u ~ %u total %u blocks for xlog headers\n",
				xlog_start, xlog_start + xlog_blks - 1,
				xlog_blks);

			xlog_start = xlog_blks = continue_xlog_stoped = 0;
		}

		for (const struct xfs_structure *s = xfs_structures; s->name; s++) {
			uint32_t count = argbuffer[s - xfs_structures].count;
			uint32_t offset = argbuffer[s - xfs_structures].block_offset;
			char tmp[32] = { 0 };

			if (count == 0)
				continue;

			if (count > 1)
				snprintf(tmp, sizeof(tmp), " has %u", count);
			else if (offset % xfs->sector_size)
				snprintf(tmp, sizeof(tmp), "+%4u", offset);
			else if (offset != 0)
				snprintf(tmp, sizeof(tmp), ".%u", offset / xfs->sector_size);

			printf("block %8zu", blkno);
			printf("%-6s", tmp);
			printf(" %s", s->name);
			if (s->comment)
				printf(" (%s)", s->comment);
			printf("\n");
		}
	}

	free(blkbuf);
	return ret;
}

static int xfs_do_ag_cmd(void *private_data, int fd, int argc, char **argv)
{
	const struct xfs_structure *s = NULL;
	struct xfs_editor *xfs = private_data;
	const void *buf = NULL;
	size_t bufsz = 0;
	int ag = 0;

	if (argc > 1) {
		ag = strtol(argv[1], NULL, 0);
		if (ag < 0) {
			fprintf(stderr, "Error: invalid ag %s\n", argv[1]);
			return -1;
		}

		if (ag > (int)xfs->ag_count - 1) {
			fprintf(stderr, "Error: ag %d out of range\n", ag);
			return -1;
		}
	}

	if (!strcmp(argv[0], "sblock")) {
		buf = xfs->ags[ag].sb;
		bufsz = xfs->sector_size;
	} else if (!strcmp(argv[0], "agi")) {
		buf = xfs->ags[ag].agi;
		bufsz = xfs->sector_size;
	} else if (!strcmp(argv[0], "agf")) {
		buf = xfs->ags[ag].agf;
		bufsz = xfs->sector_size;
	} else if (!strcmp(argv[0], "agfl")) {
		buf = xfs->ags[ag].agfl;
		bufsz = xfs->sector_size;
	} else if (!strcmp(argv[0], "bnobt")) {
		buf = xfs->ags[ag].bnobt;
		bufsz = xfs->block_size;
	} else if (!strcmp(argv[0], "cntbt")) {
		buf = xfs->ags[ag].cntbt;
		bufsz = xfs->block_size;
	} else if (!strcmp(argv[0], "inobt")) {
		buf = xfs->ags[ag].inobt;
		bufsz = xfs->block_size;
	} else if (!strcmp(argv[0], "finobt")) {
		buf = xfs->ags[ag].finobt;
		bufsz = xfs->block_size;
	} else if (!strcmp(argv[0], "refcountbt")) {
		buf = xfs->ags[ag].refcountbt;
		bufsz = xfs->block_size;
	}

	if (!buf || !bufsz) {
		fprintf(stderr, "Error: %s not found in ag%d\n", argv[0], ag);
		return -1;
	}

	s = xfs_structure_find_bymagic(buf);
	if (!s)
		return -1;

	xfs_structure_print(xfs, s, buf, bufsz);
	return 0;
}

static int xfs_do_inode(void *private_data, int fd, int argc, char **argv)
{
	struct xfs_editor *xfs = private_data;
	uint32_t blkno, blkoffset;
	struct xfs_dinode *inode;
	int ino = -1;

	if (argc > 1)
		ino = (int)strtol(argv[1], NULL, 0); /* auto detect dec or hex */

	if (ino < 0) {
		fprintf(stderr, "Usage: xfs inode #ino\n");
		return -1;
	}

	inode = _xfs_alloc_read_inode(xfs, ino, &blkno, &blkoffset);
	if (!inode)
		return -1;

	printf("inode #%d location on blk #%u+%u\n", ino, blkno, blkoffset);
	print_xfs_dinode(xfs, inode, xfs->inode_size);
	free(inode);

	return 0;
}

#ifdef CONFIG_ENABLE_ANDROID
static int xfs_write_sparse_chunk(int fd_target,
				   struct android_sparse_chunk *chunk,
				   void *private_data)
{
	off64_t target_offset = lseek64(fd_target, 0, SEEK_CUR);
	struct xfs_editor *xfs = private_data;

	dd64(xfs->fd, fd_target,
		chunk->fs_blocknr * xfs->block_size,
		target_offset,
		chunk->count * xfs->block_size,
		NULL, NULL);
	return 0;
}

static int xfs_do_sparse(void *private_data, int fd, int argc, char **argv)
{
	struct xfs_editor *xfs = private_data;
	struct xfs_dsb *sb = xfs->ags[0].sb;
	struct android_sparse_input input;
	int ret = -1, fd_target;
	struct bitmask *b = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage: sparse output.simg\n");
		return -1;
	}

	b = alloc_bitmask(be64_to_cpu(sb->sb_dblocks));
	if (!b)
		return -1;

	fd_target = fileopen(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0664);
	if (fd_target < 0)
		goto done;

	android_sparse_init(&input, xfs->block_size, be64_to_cpu(sb->sb_dblocks));

	for (uint32_t ag_idx = 0; ag_idx < xfs->ag_count; ag_idx++) {
		struct xfs_ag *ag = &xfs->ags[ag_idx];
		struct xfs_agf *agf = ag->agf;
		struct xfs_btree_block *bnobt = ag->bnobt;
		const struct xfs_alloc_rec *ar;
		uint32_t blk_base = ag_idx * xfs->ag_blocks;
		uint32_t freeblks = 0;

		/* xfs save unused blocks in bnobt */
		if (!bnobt)
			continue;

		ar = xfs_btree_block_get_data(xfs, bnobt, xfs->block_size, NULL);

		for (int i = 0; i < be16_to_cpu(bnobt->bb_numrecs); i++) {
			uint32_t abs_blk = blk_base +
					be32_to_cpu(ar[i].ar_startblock);

			ret = bitmask_set_bits(b, abs_blk,
					be32_to_cpu(ar[i].ar_blockcount));
			if (ret < 0) {
				fprintf(stderr, "Error: set bitmask based for "
						"ag%u's dnobt failed\n",
						ag_idx);
				goto done;
			}

			freeblks += be32_to_cpu(ar[i].ar_blockcount);
		}

		if (get_verbose_level())
			printf("AG%u has %u free blocks\n", ag_idx, freeblks);

		if (agf && freeblks != be32_to_cpu(agf->agf_freeblks)) {
			fprintf(stderr, "Error: we find %u freeblks on AG%u, "
				"but agf mark it has %u freeblks\n",
				freeblks, ag_idx,
				be32_to_cpu(agf->agf_freeblks));
			ret = -1;
			goto done;
		}
	}

	/* bitmask mark all unused blocks, but we need used blocks */
	bitmask_not(b);
	bitmask_foreach_continue(it, b) {
		if (get_verbose_level() > 0)
			printf("add chunk block started at %u total %u\n",
				it.start, it.bits);

		android_sparse_add_chunk(&input, it.start, it.bits);
	}

	if (input.error) {
		fprintf(stderr, "Error: add sparse chunks failed. "
				"(no enough memory?)\n");
		goto done;
	}

	ret = android_sparse_finish(&input, fd_target,
				    xfs_write_sparse_chunk, xfs);
done:
	if (b)
		bitmask_free(b);
	close(fd_target);
	return ret;
}
#endif

static int xfs_list_main(void *private_data, int fd, int argc, char **argv)
{
	if (argc >= 1) {
		static const char *ag_cmds[] = {
			"sblock",
			"agf",
			"agi",
			"agfl",
			"bnobt",
			"cntbt",
			"inobt",
			"finobt",
			"refcountbt",
			NULL,
		};

		for (size_t i = 0; ag_cmds[i]; i++) {
			if (!strcmp(ag_cmds[i], argv[0]))
				return xfs_do_ag_cmd(private_data, fd, argc, argv);
		}

		if (!strcmp(argv[0], "block"))
			return xfs_do_block(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "magic"))
			return xfs_do_magic(private_data, fd, argc, argv);
		else if (!strcmp(argv[0], "inode"))
			return xfs_do_inode(private_data, fd, argc, argv);
	#if CONFIG_ENABLE_ANDROID > 0
		else if (!strcmp(argv[0], "sparse"))
			return xfs_do_sparse(private_data, fd, argc, argv);
	#endif

		fprintf(stderr, "Error: unknown command %s\n", argv[0]);
		return -1;
	}

	return -1;
}

static struct imgeditor xfs_editor = {
	.name			= "xfs",
	.descriptor		= "xfs image editor",
	.flags			= IMGEDITOR_FLAG_CONTAIN_MULTI_BIN |
				  IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.header_size		= SIZE_MB(1),
	.private_data_size	= sizeof(struct xfs_editor),
	.exit			= xfs_editor_exit,
	.detect			= xfs_detect,
	.list			= xfs_list_main,
};
REGISTER_IMGEDITOR(xfs_editor);
