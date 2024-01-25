ext4文件系统中的链接
==================

创建一个 16MiB 大小的测试镜像:

```console
$ mkdir symlink
$ make_ext4fs -l 16MiB symlink.ext4 symlink
$ sudo mount -o loop symlink.ext4 mnt
$ sudo chown qianfan:qianfan mnt
```

# 软链接与硬链接

首先创建一个文件, 之后分别创建一个软连接和硬链接指向它:

```console
$ touch a
$ ln -s symlink_of_a a
$ ln a hardlink_of_a
$ sync
```

观察文件系统的 inode 信息:

```console
$ imgeditor symlink.ext4
ext2: ext filesystem image
|-- #00000011: Directory  lost+found
|-- #00000012: File       a 0 bytes
|-- #00000013: Link       symlink_of_a -> a
|-- #00000012: File       hardlink_of_a 0 bytes
```

可以看到软链接有独立的 inode, 硬链接与原始文件共享相同的 inode.

inode 信息中存在一个 nlinks 数据结构, 指示硬链接的个数.

```console
imgeditor symlink.ext4 -- inode 12
mode                          : 0x81a4 (File 644)
uid                           : 1000
size                          : 0
atime                         : 2022-11-21T14:44:39
ctime                         : 2022-11-21T14:46:22
mtime                         : 2022-11-21T14:44:39
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 2
blockcnt                      : 0
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00000001
eh_magic                      : 0xf30a
eh_entries                    : 0
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
version                       : 0xc8802429
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

当我们再次创建硬链接时, nlinks 个数会同步增加:

```console
$ ln a another_hardlink_of_a
$ sync

$ imgeditor symlink.ext4 -- inode 12
mode                          : 0x81a4 (File 644)
uid                           : 1000
size                          : 0
atime                         : 2022-11-21T14:44:39
ctime                         : 2022-11-21T14:51:43
mtime                         : 2022-11-21T14:44:39
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 3
blockcnt                      : 0
...
```

# 软链接文件名长度

查看 symlink_of_a 的 inode 信息:

```console
imgeditor symlink.ext4 -- inode 13
mode                          : 0xa1ff (Symlink 777)
uid                           : 1000
size                          : 1
atime                         : 2022-11-21T14:45:02
ctime                         : 2022-11-21T14:45:02
mtime                         : 2022-11-21T14:45:02
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 1
blockcnt                      : 0
flags                         : 0x00000080 ( EXT4_NOATIME_FL )
osd1                          : 0x00000001
b.symlink                     : a
version                       : 0xd5d0c0a2
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

可以看到链接的目的地址保存在 b.symlink 中. 这是一个 60 字节的数组. 如果链接指向
的文件文件名超过 60 字节会如何存储?

创建一个文件名为 99 字节的文件测试下:

```console
$ touch very_loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong_file
$ ln -s very_loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong_file symlink_of_long_file
$ sync
```

查看 symlink_of_long_file 的 inode 信息, 发现操作系统为其分配了一个 extent 结构
存储指向的文件名:

```console
$ imgeditor symlink.ext4 -- inode 15
mode                          : 0xa1ff (Symlink 777)
uid                           : 1000
size                          : 99
atime                         : 2022-11-21T14:58:52
ctime                         : 2022-11-21T14:58:52
mtime                         : 2022-11-21T14:58:52
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 1
blockcnt                      : 8
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00000001
eh_magic                      : 0xf30a
eh_entries                    : 1
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
ee_block                      : 0
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1102
version                       : 0x16868611
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000


$ imgeditor symlink.ext4 -- block 1102
0044e000 76 65 72 79 5f 6c 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f |very_loooooooooo|
0044e010 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f |oooooooooooooooo|
*
0044e050 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f 6f 6e 67 5f 66 |oooooooooooong_f|
0044e060 69 6c 65 00 00 00 00 00 00 00 00 00 00 00 00 00 |ile.............|
0044e070 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
0044f000
```

# 奇怪的软链接

在mount挂载的文件夹中, 创建symlink指向的目标, 如果文件名小于60字节, 可以按照inline
的方式存储. 当大于等于60字节时, 通过`EXT4_EXTENTS_FL`的方式存储. 但是我测试
make_ext4fs似乎有着不同的方式:

在Ubuntu 20.04上测试, 使用的make_ext4fs是通过apt-get安装的, 版本号如下:

```console
$ apt-cache show android-sdk-ext4-utils
Package: android-sdk-ext4-utils
Architecture: amd64
Version: 8.1.0+r23-2
Priority: optional
Section: universe/utils
Source: android-platform-system-extras
Origin: Ubuntu
Maintainer: Ubuntu Developers <ubuntu-devel-discuss@lists.ubuntu.com>
Original-Maintainer: Android Tools Maintainers <android-tools-devel@lists.alioth.debian.org>
Bugs: https://bugs.launchpad.net/ubuntu/+filebug
Installed-Size: 54
Depends: e2fsprogs, android-libcutils, android-libext4-utils, android-libselinux, libc6 (>= 2.7)
Breaks: android-tools-fsutils
Replaces: android-tools-fsutils
Filename: pool/universe/a/android-platform-system-extras/android-sdk-ext4-utils_8.1.0+r23-2_amd64.deb
Size: 8992
MD5sum: c5c1f6e495a77b193d9882a76ccb866b
SHA1: cd03eab754b9f418a4e8cad39d288dc053de343f
SHA256: 42c3a7ff72afa429dba22f9410ad80108f36fb06eec412dc9d0a469796e51331
Homepage: https://android.googlesource.com/platform/system/extras
Description-en: Android ext4-utils tools
 Command line tools to make sparse images from ext4 file system images
 and android images(.img) with ext4 file systems.
 .
 This package contains tools like mkuserimg, ext4fixup and
 make_ext4fs tools.
Description-md5: 025c99dab7f0ef5f26b92ba9ea3bf801
```

通过如下方式分别创建一个指向60字节文件名的链接a和一个指向61字节的链接a61:

```console
$ mkdir symlink_of_60
$ touch symlink_of_60/123456789012345678901234567890123456789012345678901234567890
$ touch symlink_of_60/123456789012345678901234567890123456789012345678901234567890a

$ cd symlink_of_60
$ ln -s 123456789012345678901234567890123456789012345678901234567890 a60
$ ln -s 123456789012345678901234567890123456789012345678901234567890a a61

$ cd ..
$ make_ext4fs -l 16MiB symlink.ext4 symlink_of_60
Creating filesystem with parameters:
    Size: 16777216
    Block size: 4096
    Blocks per group: 32768
    Inodes per group: 1024
    Inode size: 256
    Journal blocks: 1024
    Label:
    Blocks: 4096
    Block groups: 1
    Reserved block group size: 7
Created filesystem with 15/1024 inodes and 1104/4096 blocks
```

之后使用imgeditor查看, 奇怪的问题出现了:

inode #14和#15都没有使用 `EXT4_EXTENTS_FL` 存储. 但是挂载之后还能够正确显示链接文件.

```console
$ imgeditor symlink.ext4
ext2: ext filesystem image
|-- #00000011: Directory  lost+found
|-- #00000012: File       123456789012345678901234567890123456789012345678901234567890 0 bytes
|-- #00000013: File       123456789012345678901234567890123456789012345678901234567890a 0 bytes
|-- #00000014: Link       a60 -> N
|-- #00000015: Link       a61 -> O

$ imgeditor symlink.ext4 -- inode 14
mode                          : 0xa1ff (Symlink 777)
uid                           : 0
size                          : 60
atime                         : 2023-01-13T14:57:26
ctime                         : 2023-01-13T14:57:26
mtime                         : 2023-01-13T14:57:26
dtime                         : 1970-01-01T08:00:00
gid                           : 0
nlinks                        : 1
blockcnt                      : 8
flags                         : 0x00000080 ( EXT4_NOATIME_FL )
osd1                          : 0x00000000
b.symlink                     : N
version                       : 0x00000000
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000

$ imgeditor symlink.ext4 -- inode 15
mode                          : 0xa1ff (Symlink 777)
uid                           : 0
size                          : 61
atime                         : 2023-01-13T14:57:30
ctime                         : 2023-01-13T14:57:30
mtime                         : 2023-01-13T14:57:30
dtime                         : 1970-01-01T08:00:00
gid                           : 0
nlinks                        : 1
blockcnt                      : 8
flags                         : 0x00000080 ( EXT4_NOATIME_FL )
osd1                          : 0x00000000
b.symlink                     : O
version                       : 0x00000000
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

关于这个问题, 可以查看内核代码中的`ext4_inode_is_fast_symlink`:

```c
/*
 * Test whether an inode is a fast symlink.
 * A fast symlink has its symlink data stored in ext4_inode_info->i_data.
 */
int ext4_inode_is_fast_symlink(struct inode *inode)
{
	if (!(EXT4_I(inode)->i_flags & EXT4_EA_INODE_FL)) {
		int ea_blocks = EXT4_I(inode)->i_file_acl ?
				EXT4_CLUSTER_SIZE(inode->i_sb) >> 9 : 0;

		if (ext4_has_inline_data(inode))
			return 0;

		return (S_ISLNK(inode->i_mode) && inode->i_blocks - ea_blocks == 0);
	}
	return S_ISLNK(inode->i_mode) && inode->i_size &&
	       (inode->i_size < EXT4_N_BLOCKS * 4);
}
```

小于60字节的时候, 是fast_symlink, 大于等于60字节的话就不能按照b.symlink这种方式处理
了.
