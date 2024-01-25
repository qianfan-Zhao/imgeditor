ext4 文件系统结构布局
===================

ext4 文件系统将整个物理磁盘按照块进行切割, 默认的块大小是 4K. 将一定数量连续块
进行组合管理, 成为 group. 我们下面的讨论都从 0 开始对 block 和 group 进行计数.

其中 block0 和 block1 有特殊用途.

block0 偏移 1K 字节处存放了文件系统信息, 被称为 super block:

```console
$  imgeditor rootfs.ext4 -- block 0
ext2: ext2 image editor
00000000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00000400 00 00 01 00 00 00 04 00 00 00 00 00 24 87 02 00 |............$...|
00000410 02 ee 00 00 00 00 00 00 02 00 00 00 02 00 00 00 |................|
00000420 00 80 00 00 00 80 00 00 00 20 00 00 00 00 00 00 |......... ......|
00000430 00 00 00 00 00 00 0a 00 53 ef 01 00 02 00 00 00 |........S.......|
00000440 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 |................|
00000450 00 00 00 00 0b 00 00 00 00 01 00 00 1c 00 00 00 |................|
00000460 42 00 00 00 13 00 00 00 57 f8 f4 bc ab f4 65 5f |B.......W.....e_|
00000470 bf 67 94 6f c0 f9 f2 5b 00 00 00 00 00 00 00 00 |.g.o...[........|
00000480 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
000004c0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 3f 00 |..............?.|
000004d0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
000004e0 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
000004f0 00 00 00 00 00 00 00 00 00 00 00 00 02 01 20 00 |.............. .|
00000500 00 00 00 00 00 00 00 00 00 00 00 00 0a f3 01 00 |................|
00000510 03 00 00 00 00 00 00 00 00 00 00 00 00 10 00 00 |................|
00000520 43 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |C...............|
00000530 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00000550 00 00 00 00 00 00 00 00 00 00 00 00 1c 00 1c 00 |................|
00000560 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
00000570 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00001000
```

block1 存放了所有 group 的描述符. 单个描述符的大小是 supber_block->descriptor_size.

```console
$ imgeditor rootfs.ext4 -- block 1
ext2: ext2 image editor
00001000 41 00 00 00 42 00 00 00 43 00 00 00 02 00 02 0e |A...B...C.......|
00001010 8c 01 00 00 00 00 00 00 00 00 00 00 00 00 5d 3a |..............]:|
00001020 41 80 00 00 42 80 00 00 43 80 00 00 bd 7d 00 20 |A...B...C....}. |
00001030 00 00 01 00 00 00 00 00 00 00 00 00 00 00 95 de |................|
00001040 00 00 01 00 01 00 01 00 02 00 01 00 fe 7d 00 20 |.............}. |
00001050 00 00 01 00 00 00 00 00 00 00 00 00 00 00 98 e9 |................|
00001060 41 80 01 00 42 80 01 00 43 80 01 00 02 00 00 20 |A...B...C...... |
00001070 00 00 01 00 00 00 00 00 00 00 00 00 00 00 53 38 |..............S8|
00001080 00 00 02 00 01 00 02 00 02 00 02 00 fe 7d 00 20 |.............}. |
00001090 00 00 01 00 00 00 00 00 00 00 00 00 00 00 3e 0b |..............>.|
000010a0 41 80 02 00 42 80 02 00 43 80 02 00 ac 11 00 20 |A...B...C...... |
000010b0 00 00 01 00 00 00 00 00 00 00 00 00 00 00 e9 46 |...............F|
000010c0 00 00 03 00 01 00 03 00 02 00 03 00 fe 7d 00 20 |.............}. |
000010d0 00 00 01 00 00 00 00 00 00 00 00 00 00 00 5c 55 |..............\U|
000010e0 41 80 03 00 42 80 03 00 43 80 03 00 bd 7d 00 20 |A...B...C....}. |
000010f0 00 00 01 00 00 00 00 00 00 00 00 00 00 00 33 3c |..............3<|
00001100 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00002000
```

列出文件系统的基本信息:

从 super block 信息中可以看到共有 262144 个 block, 每个 block 的大小是 4K, 因此
文件系统总共 1GB 大小. blocks_per_group 是 32768, 一个 group 共有 32K block,
共计 128M, 1GB 的空间共分为 8 个 group.

1GB 的文件系统中最多可以分配 65536(total_inodes) 个 inode, 平分给 8 个 group,
每个 group 有 8192 个 inode. 需要注意的是 inode 的计数是从 1 开始的, 因此
inode#1 ~ inode#8192 位于 group0 中, inode#8193 位于 group1 中,
以此类推.

```console
$ imgeditor rootfs.ext4 -- sblock
ext2: ext2 image editor
total_inodes                      : 65536
total_blocks                      : 262144
reserved_blocks                   : 0
free_blocks                       : 165668
free_inodes                       : 60930
first_data_block                  : 0
log2_block_size                   : 2
log2_fragment_size                : 2
blocks_per_group                  : 32768
fragments_per_group               : 32768
inodes_per_group                  : 8192
mtime                             : 1970-01-01T08:00:00
utime                             : 1970-01-01T08:00:00
mnt_count                         : 0
max_mnt_count                     : 10
magic                             : 0xef53
fs_state                          : 1
error_handling                    : 2
minor_revision_level              : 0
lastcheck                         : 0
checkinterval                     : 0
creator_os                        : 0
revision_level                    : 1
uid_reserved                      : 0
gid_reserved                      : 0
first_inode                       : 11
inode_size                        : 256
block_group_number                : 0
feature_compatibility             : 28
feature_incompat                  : 66
feature_ro_compat                 : 19
unique_id                         : 0xbcf4f857 0x5f65f4ab 0x6f9467bf 0x5bf2f9c0
volume_name                       :
last_mounted_on                   :
compression_info                  : 0
prealloc_blocks                   : 0
prealloc_dir_blocks               : 0
reserved_gdt_blocks               : 63
journal_uuid                      : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
journal_inode                     : 8
journal_dev                       : 0
last_orphan                       : 0
hash_seed                         : 0x00000000 0x00000000 0x00000000 0x00000000
default_hash_version              : 2
journal_backup_type               : 1
descriptor_size                   : 32
default_mount_options             : 0
first_meta_block_group            : 0
mkfs_time                         : 1970-01-01T08:00:00
journal_blocks                    : 127754 3 0 0 4096 579 0 0 0 0 0 0 0 0 0 0 0
total_blocks_high                 : 0
reserved_blocks_high              : 0
free_blocks_high                  : 0
min_extra_inode_size              : 28
want_extra_inode_size             : 28
flags                             : 2
mmp_interval                      : 0
mmp_block                         : 0
raid_stripe_width                 : 0
log2_groups_per_flex              : 0
checksum_type                     : 0

Group  0:
    block_id                      : 65
    inode_id                      : 66
    inode_table_id                : 67
    free_blocks                   : 2
    free_inodes                   : 3586
    used_dir_cnt                  : 396
    bg_flags                      : 0x0000
    bg_exclude_bitmap             : 0x00000000
    bg_block_id_csum              : 0
    bg_inode_id_csum              : 0
    bg_itable_unused              : 0
    bg_checksum                   : 0x3a5d

Group  1:
    block_id                      : 32833
    inode_id                      : 32834
    inode_table_id                : 32835
    free_blocks                   : 32189
    free_inodes                   : 8192
    used_dir_cnt                  : 0
    bg_flags                      : 0x0001
    bg_exclude_bitmap             : 0x00000000
    bg_block_id_csum              : 0
    bg_inode_id_csum              : 0
    bg_itable_unused              : 0
    bg_checksum                   : 0xde95

Group  2:
    block_id                      : 65536
    inode_id                      : 65537
    inode_table_id                : 65538
    free_blocks                   : 32254
    free_inodes                   : 8192
    used_dir_cnt                  : 0
    bg_flags                      : 0x0001
    bg_exclude_bitmap             : 0x00000000
    bg_block_id_csum              : 0
    bg_inode_id_csum              : 0
    bg_itable_unused              : 0
    bg_checksum                   : 0xe998

Group  3:
    block_id                      : 98369
    inode_id                      : 98370
    inode_table_id                : 98371
    free_blocks                   : 2
    free_inodes                   : 8192
    used_dir_cnt                  : 0
    bg_flags                      : 0x0001
    bg_exclude_bitmap             : 0x00000000
    bg_block_id_csum              : 0
    bg_inode_id_csum              : 0
    bg_itable_unused              : 0
    bg_checksum                   : 0x3853

Group  4:
    block_id                      : 131072
    inode_id                      : 131073
    inode_table_id                : 131074
    free_blocks                   : 32254
    free_inodes                   : 8192
    used_dir_cnt                  : 0
    bg_flags                      : 0x0001
    bg_exclude_bitmap             : 0x00000000
    bg_block_id_csum              : 0
    bg_inode_id_csum              : 0
    bg_itable_unused              : 0
    bg_checksum                   : 0x0b3e

Group  5:
    block_id                      : 163905
    inode_id                      : 163906
    inode_table_id                : 163907
    free_blocks                   : 4524
    free_inodes                   : 8192
    used_dir_cnt                  : 0
    bg_flags                      : 0x0001
    bg_exclude_bitmap             : 0x00000000
    bg_block_id_csum              : 0
    bg_inode_id_csum              : 0
    bg_itable_unused              : 0
    bg_checksum                   : 0x46e9

Group  6:
    block_id                      : 196608
    inode_id                      : 196609
    inode_table_id                : 196610
    free_blocks                   : 32254
    free_inodes                   : 8192
    used_dir_cnt                  : 0
    bg_flags                      : 0x0001
    bg_exclude_bitmap             : 0x00000000
    bg_block_id_csum              : 0
    bg_inode_id_csum              : 0
    bg_itable_unused              : 0
    bg_checksum                   : 0x555c

Group  7:
    block_id                      : 229441
    inode_id                      : 229442
    inode_table_id                : 229443
    free_blocks                   : 32189
    free_inodes                   : 8192
    used_dir_cnt                  : 0
    bg_flags                      : 0x0001
    bg_exclude_bitmap             : 0x00000000
    bg_block_id_csum              : 0
    bg_inode_id_csum              : 0
    bg_itable_unused              : 0
    bg_checksum                   : 0x3c33
```

每个 inode 都有自己的描述符, 被称为 ext2_inode, 占据的大小是 super_block->inode_size,
在上面查询的信息中, 这个数值是 256 字节. 因此一个 block 可以存储 4K / 256 = 16
个 inode 描述符. 一个 group 有 8K inode, 共需要 512 个块 (2M Bytes) 存储.

inode 描述符是存储在对应 group 中的, 按照上面的信息, inode#1 ~ inode#8192 存储在
group0 中, inode#8193 存储在 group1 中...

group 描述符中的 inode_table_id 是一个指针, 指向了自身 inode 描述符的存储块序号.
按照上面的信息, group0 的 inode 描述符表存在第 67 个块中, group7 的 inode 描述符表
存储在 229443 块中.

```console
$ imgeditor rootfs.ext4 -- block 67
ext2: ext2 image editor
00043000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00043100 ed 41 00 00 00 10 00 00 00 00 00 00 00 00 00 00 |.A..............|
00043110 00 00 00 00 00 00 00 00 00 00 18 00 08 00 00 00 |................|
00043120 80 00 08 00 00 00 00 00 0a f3 01 00 03 00 00 00 |................|
00043130 00 00 00 00 00 00 00 00 01 00 00 00 44 12 00 00 |............D...|
00043140 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00043600 80 81 00 00 00 f0 c0 0f 00 00 00 00 00 00 00 00 |................|
00043610 00 00 00 00 00 00 00 00 00 00 01 00 e0 09 00 00 |................|
00043620 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00043650 00 00 00 00 00 00 00 00 00 00 00 00 43 12 00 00 |............C...|
00043660 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00043700 80 81 00 00 00 00 00 01 00 00 00 00 00 00 00 00 |................|
00043710 00 00 00 00 00 00 00 00 00 00 01 00 00 80 00 00 |................|
00043720 00 00 08 00 00 00 00 00 0a f3 01 00 03 00 00 00 |................|
00043730 00 00 00 00 00 00 00 00 00 10 00 00 43 02 00 00 |............C...|
00043740 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00043a00 fd 41 00 00 00 10 00 00 6a 01 fe 62 6a 01 fe 62 |.A......j..bj..b|
00043a10 6a 01 fe 62 00 00 00 00 00 00 03 00 08 00 00 00 |j..b............|
00043a20 80 00 08 00 00 00 00 00 0a f3 01 00 03 00 00 00 |................|
00043a30 00 00 00 00 00 00 00 00 01 00 00 00 45 12 00 00 |............E...|
00043a40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00043b00 ed 41 00 00 00 10 00 00 2d 81 0c 63 2d 81 0c 63 |.A......-..c-..c|
00043b10 2d 81 0c 63 00 00 00 00 00 00 02 00 08 00 00 00 |-..c............|
00043b20 80 00 08 00 00 00 00 00 0a f3 01 00 03 00 00 00 |................|
00043b30 00 00 00 00 00 00 00 00 01 00 00 00 46 12 00 00 |............F...|
00043b40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00043c00 e4 81 00 00 3f 49 3f 00 8d bd 1e 63 8d bd 1e 63 |....?I?....c...c|
00043c10 8d bd 1e 63 00 00 00 00 00 00 01 00 a8 1f 00 00 |...c............|
00043c20 80 00 08 00 00 00 00 00 0a f3 01 00 03 00 00 00 |................|
00043c30 00 00 00 00 00 00 00 00 f5 03 00 00 47 12 00 00 |............G...|
00043c40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00043d00 e4 81 00 00 18 06 00 00 8d bd 1e 63 8d bd 1e 63 |...........c...c|
00043d10 8d bd 1e 63 00 00 00 00 00 00 01 00 08 00 00 00 |...c............|
00043d20 80 00 08 00 00 00 00 00 0a f3 01 00 03 00 00 00 |................|
00043d30 00 00 00 00 00 00 00 00 01 00 00 00 3c 16 00 00 |............<...|
00043d40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00043e00 ff a1 00 00 07 00 00 00 66 7e 08 63 66 7e 08 63 |........f~.cf~.c|
00043e10 66 7e 08 63 00 00 00 00 00 00 01 00 00 00 00 00 |f~.c............|
00043e20 80 00 00 00 00 00 00 00 75 73 72 2f 62 69 6e 00 |........usr/bin.|
00043e30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00043f00 fd 41 00 00 00 10 00 00 cf 20 f7 62 cf 20 f7 62 |.A....... .b. .b|
00043f10 cf 20 f7 62 00 00 00 00 00 00 02 00 08 00 00 00 |. .b............|
00043f20 80 00 08 00 00 00 00 00 0a f3 01 00 03 00 00 00 |................|
00043f30 00 00 00 00 00 00 00 00 01 00 00 00 3d 16 00 00 |............=...|
00043f40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
```
