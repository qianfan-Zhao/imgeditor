ubi node
========

ubi 文件系统 LEB 中的数据使用 ubifs_x_node 来描述, 他们都是继承自
ubifs_ch (common head) 结构.

```c
/**
 * struct ubifs_ch - common header node.
 * @magic: UBIFS node magic number (%UBIFS_NODE_MAGIC)
 * @crc: CRC-32 checksum of the node header
 * @sqnum: sequence number
 * @len: full node length
 * @node_type: node type
 * @group_type: node group type
 * @padding: reserved for future, zeroes
 *
 * Every UBIFS node starts with this common part. If the node has a key, the
 * key always goes next.
 */
struct ubifs_ch {
	__le32 magic;
	__le32 crc;
	__le64 sqnum;
	__le32 len;
	__u8 node_type;
	__u8 group_type;
	__u8 padding[2];
} __attribute__ ((packed));

struct ubifs_dent_node {
	struct ubifs_ch ch;
	__u8 key[UBIFS_MAX_KEY_LEN];
	__le64 inum;
	__u8 padding1;
	__u8 type;
	__le16 nlen;
	__le32 cookie;
	__u8 name[0];
} __attribute__ ((packed));
```

ubifs_ch.node_type 字段表示了后续数据是何种类型. ubi 定义了大量的数据结构, 表示不同
的 node:

| node_type 数值     | 对应结构体             |
| ------------------ | ---------------------- |
| UBIFS_INO_NODE(0)  | struct ubifs_ino_node  |
| UBIFS_DATA_NODE(1) | struct ubifs_data_node |
| UBIFS_DENT_NODE(2) | struct ubifs_dent_node |
| ...                | ...                    |

一个 LEB 中可以顺序存放多个 node, 当前 node 占用的字节大小可以通过 ubifs_ch.len
获取. 当多个 node 顺序存储时, 下一个 node 存储的地址总是按照 `8 字节` 对齐的.

例如 LEB12 中存储的 node (删减了无用的信息), seq 30 起始地址是0, 大小是 188 (0xBC),
seq 31 的起始地址应该是 aligned(0 + 188, 8) = 0xC0.

```console
$ imgeditor rootfs.ubi -v -- node 12
Node offset                   : 0x00000000
magic                         : 0x06101831(UBI NODE)
sqnum                         : 30
len                           : 188
node_type                     : 0x09 (index node)

Node offset                   : 0x000000c0
magic                         : 0x06101831(UBI NODE)
sqnum                         : 31
len                           : 188
node_type                     : 0x09 (index node)

Node offset                   : 0x00000180
magic                         : 0x06101831(UBI NODE)
sqnum                         : 32
len                           : 128
node_type                     : 0x09 (index node)
```

# UBIFS_SB_NODE (struct ubifs_sb_node)

sb 的全称是 `super block`, 用于记录文件系统的信息, 他固定存储在 LEB 0 中.
sb node 中保存了文件系统最小 io 大小(页大小), leb 大小, 数量等信息.

```console
$ ./imgeditor tests/fs/ubi/simple_abc.ubi -v -- node sblock
Node offset                   : 0x00000000
magic                         : 0x06101831(UBI NODE)
crc                           : 0x93c7e973
sqnum                         : 19
len                           : 4096
node_type                     : 0x06 (superblock node)
group_type                    : 0x00 (this node is not part of a group)
    key_hash                  : 0
    key_fmt                   : 0
    flags                     : 0x00000004 ( SPACE_FIXUP )
    min_io_size               : 2048
    leb_size                  : 126976
    leb_cnt                   : 13
    max_leb_cnt               : 100
    max_bud_bytes             : 1396736
    log_lebs                  : 4
    lpt_lebs                  : 2
    orph_lebs                 : 1
    jhead_cnt                 : 1
    fanout                    : 8
    lsave_cnt                 : 256
    fmt_version               : 4
    default_compr             : 1
    rp_uid                    : 0
    rp_gid                    : 0
    rp_size                   : 0
    time_gran                 : 1000000000
    uuid                      : b6 6b 77 47 91 07 48 b7 85 df 17 60 1d 9f ef b7
    ro_compat_version         : 0
    hmac                      : 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    hmac_wkm                  : 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    hash_algo                 : 0
    hash_mst                  : 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
```

# UBI_MST_NODE (struct ubi_mst_node)

master node 固定存储在 LEB1 和 LEB2 中, 互为备份. 存储了文件系统中目录树的地址
(root_lnum:root_offs)和一些其他的信息.

```console
$ ./imgeditor tests/fs/ubi/simple_abc.ubi -v -- node master
Node offset                   : 0x00000000
magic                         : 0x06101831(UBI NODE)
crc                           : 0x91b244d2
sqnum                         : 20
len                           : 512
node_type                     : 0x07 (master node)
group_type                    : 0x00 (this node is not part of a group)
    highest_inum              : 68
    cmt_no                    : 0
    flags                     : 2
    log_lnum                  : 3
    root_lnum                 : 12
    root_offs                 : 0x00000108
    root_len                  : 68
    gc_lnum                   : 11
    ihead_lnum                : 12
    ihead_offs                : 0x00000800
    index_size                : 336
    total_free                : 376832
    total_dirty               : 2624
    total_used                : 1136
    total_dead                : 0
    total_dark                : 12288
    lpt_lnum                  : 7
    lpt_offs                  : 0x00000029
    ltab_offs                 : 53
    lsave_lnum                : 0
    lsave_offs                : 0x00000000
    lscan_lnum                : 10
    empty_lebs                : 1
    leb_cnt                   : 13
    hash_root_idx             : 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    hash_lpt                  : 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    hmac                      : 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

Node offset                   : 0x00000200
magic                         : 0x06101831(UBI NODE)
crc                           : 0xee0edeef
sqnum                         : 0
len                           : 28
node_type                     : 0x05 (padding node)
group_type                    : 0x00 (this node is not part of a group)
    pad_len                   : 1508
```

# UBI_IDX_NODE (struct ubi_idx_node)

ubifs 使用 b+ 树存储文件数据, 更深层的数据结构在这里不讨论, 仅介绍 index node.
上面的 master node 中记录了 `root_lnum:root_offs`, 其中存储的 node 结构就是
index node.

```console
$ ./imgeditor tests/fs/ubi/simple_abc.ubi -- node 12
[index node         16]: leb_offs: 0x00000000   child_cnt: 8  level: 0
        [00] lnum: 10       offs: 0x000003d0 len: 160      key: INO         1
        [01] lnum: 10       offs: 0x00000390 len: 58       key: DENT ->000001 hash:  0x000042f2
        [02] lnum: 10       offs: 0x000002f0 len: 160      key: INO        65
        [03] lnum: 10       offs: 0x000001d0 len: 58       key: DENT ->000065 hash:  0x000043a2
        [04] lnum: 10       offs: 0x000002b0 len: 58       key: DENT ->000065 hash:  0x00004452
        [05] lnum: 10       offs: 0x00000130 len: 160      key: INO        66
        [06] lnum: 10       offs: 0x000000f0 len: 59       key: DENT ->000066 hash:  0x00032408
        [07] lnum: 10       offs: 0x00000050 len: 160      key: INO        67
[index node         17]: leb_offs: 0x000000c0   child_cnt: 2  level: 0
        [00] lnum: 10       offs: 0x00000000 len: 76       key: DATA       67 block: 00000000
        [01] lnum: 10       offs: 0x00000210 len: 160      key: INO        68
[index node         18]: leb_offs: 0x00000108   child_cnt: 2  level: 1
        [00] lnum: 12       offs: 0x00000000 len: 188      key: INO         1
        [01] lnum: 12       offs: 0x000000c0 len: 68       key: DATA       67 block: 00000000
[pad node            0]: leb_offs: 0x00000150   pad_len: 1684
```

# 文件相关

对于文件和文件夹, ubifs 使用 ubifs_ino_node, ubifs_dent_node, ubifs_data_node
来描述. 后续探讨 b+ 树时再讨论.
