inode 描述文件夹中的多文件
==========================

ext 文件系统使用 ext2_dirent 结构描述文件, 结构体的最后保存文件名, 文件名长度存储在
namelen, 并且整个结构是按照 4 字节对齐. 假设文件名是 8 字节, 那么 ext2_dirent
结构是 16 字节长度, 一个 block 可以存储 4096 / 16 = 256 个文件.

linux 中使用 "." 和 ".." 表示当前文件夹和上一及文件夹, 这两个特殊文件同普通文件一起
存储. 因此我们上面计算的 256 要减去这两个文件, 也就是假设文件名长度是 8 字节的情况
下, 一个 block 最多可以存储 254 个文件.

```c
struct ext2_dirent {
	__le32 inode;
	__le16 direntlen;
	__u8 namelen;
	__u8 filetype;
	__u8 filename[0];
};
```

让我们做一个实验来验证下:

在一个文件夹中创建 254 个文件, 文件名对齐到 8 字节.

```console
$ cd mnt
$ mkdir a b c
$ cd a
$ for i in $(seq 1 254) ; do if ! dd if=/dev/urandom of=$i.bin bs=1K count=1 ; then break; fi; done
```

查看文件系统的 inode 信息, a 文件夹 inode 编号是 12.

```console
$ imgeditor total_inodes.ext4
ext2: ext filesystem image
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  a
|   |-- #00000015: File       1.bin 1024 bytes
|   |-- #00000016: File       2.bin 1024 bytes
...
|   |-- #00000267: File       253.bin 1024 bytes
|   |-- #00000268: File       254.bin 1024 bytes
|-- #00000013: Directory  b
|-- #00000014: Directory  c
```

查看 inode 12 的基本信息:

```console
$ imgeditor total_inodes.ext4 -- inode 12
mode                          : 0x41ed (Directory 755)
uid                           : 1000
size                          : 4096
atime                         : 2022-11-18T10:33:54
ctime                         : 2022-11-18T10:34:15
mtime                         : 2022-11-18T10:34:15
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 2
blockcnt                      : 8
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x000000ff
eh_magic                      : 0xf30a
eh_entries                    : 1
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
ee_block                      : 0
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1362
version                       : 0x72c8561f
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

显示只有一个 eh_entries 结构, 并且位于 block 1362, 查看 1362 存储的 ext2_dirent
结构, 可以看到前两个文件是 "." 和 "..", 存储自身的 inode 编号和 上一级目录的
inode 编号. 后续文件是我们创建的 1.bin ~ 254.bin

共计 256 个文件信息刚好填满一个 block.

```console
imgeditor total_inodes.ext4 -- dirent 1362
00000000: inode:     #12
00000004: direntlen: 12
00000006: namelen:   1
00000007: filetype:  2
00000008: name:      .

0000000c: inode:     #2
00000010: direntlen: 12
00000012: namelen:   2
00000013: filetype:  2
00000014: name:      ..

00000018: inode:     #15
0000001c: direntlen: 16
0000001e: namelen:   5
0000001f: filetype:  1
00000020: name:      1.bin

00000028: inode:     #16
0000002c: direntlen: 16
0000002e: namelen:   5
0000002f: filetype:  1
00000030: name:      2.bin

...

00000fd8: inode:     #267
00000fdc: direntlen: 16
00000fde: namelen:   7
00000fdf: filetype:  1
00000fe0: name:      253.bin

00000fe8: inode:     #268
00000fec: direntlen: 24
00000fee: namelen:   7
00000fef: filetype:  1
00000ff0: name:      254.bin
```

让我们在 a 文件夹下再创建一个文件, 看 a 的 inode 信息会发生什么变化.

```console
$ touch 255.bin
$ sync

$ imgeditor total_inodes.ext4 -- inode 12
mode                          : 0x41ed (Directory 755)
uid                           : 1000
size                          : 8192
atime                         : 2022-11-18T10:33:54
ctime                         : 2022-11-18T10:42:03
mtime                         : 2022-11-18T10:42:03
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 2
blockcnt                      : 16
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00000100
eh_magic                      : 0xf30a
eh_entries                    : 2
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
----
ee_block                      : 0
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1362
----
ee_block                      : 1
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1231
----
version                       : 0x72c8561f
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

可以看到文件系统中 eh_entries 变成了 2, 并追加了 block 1231 存储新建文件:

```console
$ imgeditor total_inodes.ext4 -- dirent 1231
00000000: inode:     #269
00000004: direntlen: 4096
00000006: namelen:   7
00000007: filetype:  1
00000008: name:      255.bin
```

继续往 a 文件夹中追加文件, 可以看到新文件的 dirent 结构追加到 block 1231 中.

```console
imgeditor total_inodes.ext4 -- dirent 1231
00000000: inode:     #269
00000004: direntlen: 16
00000006: namelen:   7
00000007: filetype:  1
00000008: name:      255.bin

00000010: inode:     #270
00000014: direntlen: 4080
00000016: namelen:   7
00000017: filetype:  1
00000018: name:      256.bin
```

inode 中共留了 60 字节的空间保存文件夹结构信息, 组成方式是
ext4_extent_header + ext4_extent[N].

根据定义, ext4_extent_header 和 ext4_extent 的大小都是 12 字节, 因此这 60 字节
的空间最多可以塞上一个 ext4_extent_header 和 4 个 ext4_extent.
因此 inode 信息中的 eh_max 是 4.

当存在连续可用的空间时, ee_len 可以占据多个连续的 block 存储 dirent 结构.
例如:

```console
$ mkdir mfile
$ make_ext4fs -l 512MiB mfile.ext4 mfile
$ sudo mount -o loop mfile.ext4 mnt
$ sudo chown qianfan:qianfan mnt
$ cd mnt
$ mkdir a b c
$ cd a
$ for i in $(seq 1 4096) ; do if ! dd if=/dev/urandom of=$i.bin bs=1K count=1 ; then break; fi; done
$ sync
```

可以看到 a 的 inode 是 16385, 查看 inode 信息可以看到在 block 66053 处存在
连续 16 个 block 记录文件结构信息.

```console
$ imgeditor mfile.ext4 -- --depth 1
ext2: ext filesystem image
|-- #00000011: Directory  lost+found
|-- #00016385: Directory  a
|-- #00008193: Directory  b
|-- #00024577: Directory  c
$ imgeditor mfile.ext4 -- inode 16385
mode                          : 0x41ed (Directory 755)
uid                           : 1000
size                          : 69632
atime                         : 2022-11-18T11:03:08
ctime                         : 2022-11-18T11:03:18
mtime                         : 2022-11-18T11:03:18
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 2
blockcnt                      : 136
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00001001
eh_magic                      : 0xf30a
eh_entries                    : 2
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
----
ee_block                      : 0
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 66050
----
ee_block                      : 1
ee_len                        : 16
ee_start_hi                   : 0
ee_start_lo                   : 66053
----
version                       : 0x398d8fc6
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```