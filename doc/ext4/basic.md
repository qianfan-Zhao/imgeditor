ext4文件系统底层数据存储结构
============================

创建测试用的文件夹环境:

```console
$ mkdir a
$ mkdir -p a/b
$ mkdir -p a/c
$ echo 'filepath: a/b, filename: ab' > a/b/ab

$ tree a
a
├── b
│   └── ab
└── c

2 directories, 1 file
```

使用 make_ext4fs 创建一个测试用的镜像:

```console
$ make_ext4fs -l 16MiB -L qianfan qianfan.ext4 a
Creating filesystem with parameters:
    Size: 16777216
    Block size: 4096
    Blocks per group: 32768
    Inodes per group: 1024
    Inode size: 256
    Journal blocks: 1024
    Label: qianfan
    Blocks: 4096
    Block groups: 1
    Reserved block group size: 7
Created filesystem with 14/1024 inodes and 1105/4096 blocks
```

# 基本信息

```console
$ imgeditor qianfan.ext4 -- sblock
ext2: ext2 image editor
total_inodes                      : 1024
total_blocks                      : 4096
reserved_blocks                   : 0
free_blocks                       : 2991
free_inodes                       : 1010
first_data_block                  : 0
log2_block_size                   : 2
log2_fragment_size                : 2
blocks_per_group                  : 32768
fragments_per_group               : 32768
inodes_per_group                  : 1024
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
unique_id                         : 0xd6e85da8 0x55af1687 0x8732beb0 0xcaa6e472
volume_name                       : qianfan
last_mounted_on                   :
compression_info                  : 0
prealloc_blocks                   : 0
prealloc_dir_blocks               : 0
reserved_gdt_blocks               : 7
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
journal_blocks                    : 127754 3 0 0 1024 75 0 0 0 0 0 0 0 0 0 0 0
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
    block_id                      : 9
    inode_id                      : 10
    inode_table_id                : 11
    free_blocks                   : 2991
    free_inodes                   : 1010
    used_dir_cnt                  : 4
    bg_flags                      : 0x0000
    bg_exclude_bitmap             : 0x00000000
    bg_block_id_csum              : 0
    bg_inode_id_csum              : 0
    bg_itable_unused              : 0
    bg_checksum                   : 0x2bb8

$ imgeditor qianfan.ext4
ext2: ext2 image editor
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  b
|   |-- #00000013: File       ab 28 bytes
|-- #00000014: Directory  c
```

# root inode

linux 使用 inode 描述文件/文件夹的信息, 有一个特殊的 inode 编号是 2, 用来表示
文件系统的根目录信息. 下面查看我们上面创建的文件系统根目录的信息:

```console
$ imgeditor qianfan.ext4 -- inode 2
ext2: ext2 image editor
mode                          : 0x41ed (Directory 755)
uid                           : 0
size                          : 4096
atime                         : 1970-01-01T08:00:00
ctime                         : 1970-01-01T08:00:00
mtime                         : 1970-01-01T08:00:00
dtime                         : 1970-01-01T08:00:00
gid                           : 0
nlinks                        : 5
blockcnt                      : 8
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00000000
eh_magic                      : 0xf30a
eh_entries                    : 1
eh_max                        : 3
eh_depth                      : 0
eh_generation                 : 0
ee_block                      : 0
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1100
version                       : 0x00000000
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

mode 表述了文件的类型和权限, uid/gid 描述文件的所有者. size 描述文件或者文件夹
的大小, 对于文件夹而言, 大小固定是 4K.

ee_start_lo/ee_start_hi 的单位是 block, 对于文件而言, 指向了存储文件内容的块号,
对于文件夹而言, 指向了文件夹描述符 (extent_entry).

根目录的文件夹描述符位于 1100 block的位置, 查看其内容:

```console
$ imgeditor qianfan.ext4 -- block 1100
ext2: ext2 image editor
0044c000 02 00 00 00 0c 00 01 02 2e 00 00 00 02 00 00 00 |................|
0044c010 0c 00 02 02 2e 2e 00 00 0b 00 00 00 14 00 0a 02 |................|
0044c020 6c 6f 73 74 2b 66 6f 75 6e 64 00 00 0c 00 00 00 |lost+found......|
0044c030 0c 00 01 02 62 00 00 00 0e 00 00 00 c8 0f 01 02 |....b...........|
0044c040 63 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |c...............|
0044c050 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
0044d000

$ imgeditor qianfan.ext4 -- dirent 1100
ext2: ext2 image editor
0044c000: inode:     #2
0044c004: direntlen: 12
0044c006: namelen:   1
0044c007: filetype:  2
0044c008: name:      .

0044c00c: inode:     #2
0044c010: direntlen: 12
0044c012: namelen:   2
0044c013: filetype:  2
0044c014: name:      ..

0044c018: inode:     #11
0044c01c: direntlen: 20
0044c01e: namelen:   10
0044c01f: filetype:  2
0044c020: name:      lost+found

0044c02c: inode:     #12
0044c030: direntlen: 12
0044c032: namelen:   1
0044c033: filetype:  2
0044c034: name:      b

0044c038: inode:     #14
0044c03c: direntlen: 4040
0044c03e: namelen:   1
0044c03f: filetype:  2
0044c040: name:      c
```

# 挂载

挂载之后通过 tree 查看文件及 inode 编号:

```console
$ mkdir qianfan
$ sudo mount -o loop qianfan.ext4 qianfan
$ tree qianfan --inodes
qianfan
├── [     12]  b
│   └── [     13]  ab
├── [     14]  c
└── [     11]  lost+found [error opening dir]

3 directories, 1 file
```

通过 `imgeditor qianfan.ext4 -- inode 13` 可以找到存储普通文件 ab 文件内容的
block 编号:

```console
$ imgeditor qianfan.ext4 -- inode 13
ext2: ext2 image editor
mode                          : 0x81a4 (File 644)
uid                           : 0
size                          : 28
atime                         : 2022-11-14T14:25:21
ctime                         : 2022-11-14T14:25:21
mtime                         : 2022-11-14T14:25:21
dtime                         : 1970-01-01T08:00:00
gid                           : 0
nlinks                        : 1
blockcnt                      : 8
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00000000
eh_magic                      : 0xf30a
eh_entries                    : 1
eh_max                        : 3
eh_depth                      : 0
eh_generation                 : 0
ee_block                      : 0
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1103
version                       : 0x00000000
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

block #1103 位置存放的内容就是文件 ab 的内容, 通过 inode 可以得知文件的大小
是 28 字节:

```console
$ imgeditor qianfan.ext4 -- block 1103
ext2: ext2 image editor
0044f000 66 69 6c 65 70 61 74 68 3a 20 61 2f 62 2c 20 66 |filepath: a/b, f|
0044f010 69 6c 65 6e 61 6d 65 3a 20 61 62 0a 00 00 00 00 |ilename: ab.....|
0044f020 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00450000
```

# 文件内容追加

追加内容到文件中, 并刷新磁盘.

```console
$ sudo sh -c 'echo "appending appending appending" >> qianfan/b/ab'
$ sync
```

之后再次查看 block #1103 地址处的内容:

```console
$ imgeditor qianfan.ext4 -- block 1103
ext2: ext2 image editor
0044f000 66 69 6c 65 70 61 74 68 3a 20 61 2f 62 2c 20 66 |filepath: a/b, f|
0044f010 69 6c 65 6e 61 6d 65 3a 20 61 62 0a 61 70 70 65 |ilename: ab.appe|
0044f020 6e 64 69 6e 67 20 61 70 70 65 6e 64 69 6e 67 20 |nding appending |
0044f030 61 70 70 65 6e 64 69 6e 67 0a 00 00 00 00 00 00 |appending.......|
0044f040 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00450000
```

# 文件移动

对比移动前和移动后的 inode 信息: (左侧为移动前)

```
$ imgeditor qianfan.ext4                      $ imgeditor qianfan.ext4
ext2: ext2 image editor                       ext2: ext2 image editor
|-- #00000011: Directory  lost+found          |-- #00000011: Directory  lost+found
|-- #00000012: Directory  b                   |-- #00000012: Directory  b
|   |-- #00000013: File       ab 58 bytes     |   |-- #00000013: File       new_ab 58 bytes
|-- #00000014: Directory  c                   |-- #00000014: Directory  c
```

可以看到被移动文件的 inode 编号保持不变, 其父文件夹的 dirent 结构被更新.

# 文件删除

删除 new_ab 文件, 查看 inode 发生的变化:

```console
$ sudo rm qianfan/b/new_ab
$ sync
```

可以看到 b 文件夹 (inode 编号 #12) dirent 结构中的 new_ab 已经被清除.

```console
$ imgeditor qianfan.ext4
ext2: ext2 image editor
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  b
|-- #00000014: Directory  c
```

但是存储 new_ab 文件内容的 block 中的信息还是存在的:

```console
$ imgeditor qianfan.ext4 -- block 1103
ext2: ext2 image editor
0044f000 66 69 6c 65 70 61 74 68 3a 20 61 2f 62 2c 20 66 |filepath: a/b, f|
0044f010 69 6c 65 6e 61 6d 65 3a 20 61 62 0a 61 70 70 65 |ilename: ab.appe|
0044f020 6e 64 69 6e 67 20 61 70 70 65 6e 64 69 6e 67 20 |nding appending |
0044f030 61 70 70 65 6e 64 69 6e 67 0a 00 00 00 00 00 00 |appending.......|
0044f040 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00450000
```

# 创建空文件

当再次创建文件的时候, 之前删掉文件的 inode 编号 13 被继续使用.

```console
$ sudo touch qianfan/b/new_ab
$ sync
$ imgeditor qianfan.ext4
ext2: ext2 image editor
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  b
|   |-- #00000013: File       new_ab 0 bytes
|-- #00000014: Directory  c
```

查看 inode #13 的信息, 可以看到操作系统并没有给大小为 0 的文件分配 block 存储区.

```console
$ imgeditor qianfan.ext4 -- inode 13
ext2: ext2 image editor
mode                          : 0x81a4 (File 644)
uid                           : 0
size                          : 0
atime                         : 2022-11-14T15:47:40
ctime                         : 2022-11-14T15:47:40
mtime                         : 2022-11-14T15:47:40
dtime                         : 1970-01-01T08:00:00
gid                           : 0
nlinks                        : 1
blockcnt                      : 0
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00000001
eh_magic                      : 0xf30a
eh_entries                    : 0
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
ee_block                      : 0
ee_len                        : 0
ee_start_hi                   : 0
ee_start_lo                   : 0
version                       : 0xd6118bf6
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```
