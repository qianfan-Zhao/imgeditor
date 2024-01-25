大量磁盘碎片下的文件存储方式
============================

在 [multi_file](./multi_file.md) 中, 我们知道了 inode 中最多存放 4 个 ext4_extent
结构, 如果没有连续 block 的情况下, 按照一个 ext4_extent 中仅有一个 block 记录
dirent 来计算, 即使文件名长度在 8 字节之内, 4 个 block 最多存储 1024 个文件.
当文件名较长时, 可储存的文件数量更少.

下面我们来测试下大量磁盘碎片情况下存储情况.

# 碎片化磁盘

创建一个空的文件, 制作 16MiB 的 ext4 镜像, 并挂载:

```console
$ mkdir ext4leaf
$ make_ext4fs -l 16MiB leaf.ext4 ext4leaf
$ sudo mount -o loop leaf.ext4 mnt
$ sudo chown qianfan:qianfan mnt
```

使用 16K 大小的文件, 填满 16MiB 的文件系统:

```console
$ mkdir a
$ cd a
$ for i in $(seq 1 4096) ; do if ! dd if=/dev/urandom of=$i.bin bs=16K count=1 ; then break; fi; done
```

最终创建了 728 个文件, 下面是整个文件系统 inode 的信息:

```console
$ imgeditor leaf.ext4
ext2: ext2 image editor
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  a
|   |-- #00000013: File       1.bin 16384 bytes
|   |-- #00000014: File       2.bin 16384 bytes
...
|   |-- #00000739: File       727.bin 16384 bytes
|   |-- #00000740: File       728.bin 8192 bytes
```

使用 rm 踢掉一半不连续的文件, 踢掉之后可以看到文件系统还剩余 5M 左右的空间.

```console
$ for i in $(seq 1 2 1024) ; do rm -f $i.bin ; done
$ df -h
/dev/loop0       12M  5.8M  5.7M  51% /home/.../mnt
```

# 创建大量长文件名的文件

到 a 文件夹中创建大量长名字的文件, 直至用完所有的 inode:

```console
$ cd a
$ for i in $(seq 1 4096) ; do if ! touch very_lonoooooooooooooooooooooooooooooooooooooooooog_name_$i.bin ; then break; fi; done
touch: cannot touch 'very_lonoooooooooooooooooooooooooooooooooooooooooog_name_649.bin': No space left on device
```

因为我们创建的是空文件, 所以除了 dirent 结构, 并不占用文件空间. 可以看到磁盘
剩余空间没什么变化:

```console
$ df -h
/dev/loop0       12M  5.8M  5.7M  51% /home/.../mnt
```

# leaf 存储结构

查看 a 文件夹的 inode 信息:

```console
$ imgeditor leaf.ext4 -- inode 12
mode                          : 0x41ed (Directory 755)
uid                           : 1000
size                          : 61440
atime                         : 2022-11-17T19:14:15
ctime                         : 2022-11-18T11:20:55
mtime                         : 2022-11-18T11:20:55
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 2
blockcnt                      : 128
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x000006cd
eh_magic                      : 0xf30a
eh_entries                    : 1
eh_max                        : 4
eh_depth                      : 1
eh_generation                 : 0
ei_block                      : 0
ei_leaf_lo                    : 1125
ei_leaf_hi                    : 0
ei_unused                     : 0
version                       : 0xb42c837a
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

可以看到 eh_depth 变成了 1, 不再是使用之前的存储结构了. 这里扩展使用 leaf 方式
记录文件 extent 信息.

之前看到的是 indoe 直接指向 ext4_extent 结构, 现在这种方式是 inode 指向 leaf,
leaf 指向一个 block, block 里面存储的是 ext4_extent 结构. 我们看下 leaf 指向
的节点 1125:

```console
$ imgeditor leaf.ext4 -- block 1125
00465000 0a f3 07 00 54 01 00 00 00 00 00 00 00 00 00 00 |....T...........|
00465010 02 00 00 00 4e 04 00 00 02 00 00 00 01 00 00 00 |....N...........|
00465020 e4 04 00 00 03 00 00 00 04 00 00 00 54 04 00 00 |............T...|
00465030 07 00 00 00 04 00 00 00 5c 04 00 00 0b 00 00 00 |........\.......|
00465040 01 00 00 00 64 04 00 00 0c 00 00 00 02 00 00 00 |....d...........|
00465050 66 04 00 00 0e 00 00 00 01 00 00 00 6c 04 00 00 |f...........l...|
00465060 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00466000
```

f30a 是 ext4_extent_header 结构中的 magic, 表明后续的存储是 ext4_extent 结构.
这里就不再有 inode 中 60 字节的限制了, 在连续空间满足的情况下, 可以随便追加
ext4_extent.

```console
$ imgeditor leaf.ext4 -- extent 1125
eh_magic                      : 0xf30a
eh_entries                    : 7
eh_max                        : 340
eh_depth                      : 0
eh_generation                 : 0
----
ee_block                      : 0
ee_len                        : 2
ee_start_hi                   : 0
ee_start_lo                   : 1102
----
ee_block                      : 2
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1252
----
ee_block                      : 3
ee_len                        : 4
ee_start_hi                   : 0
ee_start_lo                   : 1108
----
ee_block                      : 7
ee_len                        : 4
ee_start_hi                   : 0
ee_start_lo                   : 1116
----
ee_block                      : 11
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1124
----
ee_block                      : 12
ee_len                        : 2
ee_start_hi                   : 0
ee_start_lo                   : 1126
----
ee_block                      : 14
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1132
----
```

block 1125 处记录了 7(eh_entries) 个 ext4_extent 结构, 之前也提到
ext4_extent_header 和 ext4_extent 结构都是 12 字节大小, 一个 block 可以容纳
4096 / 12 = 341 个结构, 去除 ext4_extent_header, 总共可以容纳 340(eh_max)
个 ext4_extent.

拿到 ext4_extent 之后, 剩下的就没什么特殊的了, 以 block 1124 为例, 可以看到存储
的文件信息:

```console
imgeditor leaf.ext4 -- dirent 1124
00000000: inode:     #833
00000004: direntlen: 72
00000006: namelen:   64
00000007: filetype:  1
00000008: name:      very_lonoooooooooooooooooooooooooooooooooooooooooog_name_457.bin

00000048: inode:     #834
0000004c: direntlen: 72
0000004e: namelen:   64
0000004f: filetype:  1
00000050: name:      very_lonoooooooooooooooooooooooooooooooooooooooooog_name_458.bin
...
```

eh_depth 为 0 表示 eh 后续跟随的数据结构是 ext4_extent, 当 eh_depth 大于 0
时, 表示后续跟随的数据结构是 leaf. leaf 可以多级级联, 级联的深度用 eh_depth
指示.

# 充满磁盘碎片情况下的大文件存储

随便删掉一个文件, 空出一个 inode, 之后在根目录下创建一个 5M 大小的文件, 观察
文件 5M.bin 的 inode 信息:

```console
$ dd if=/dev/zero of=5M.bin bs=5M count=1
1+0 records in
1+0 records out
5242880 bytes (5.2 MB, 5.0 MiB) copied, 0.0071676 s, 731 MB/s
$ sync
$ ls -i 5M.bin
14 5M.bin
$ imgeditor leaf.ext4 -- inode 14
mode                          : 0x81a4 (File 644)
uid                           : 1000
size                          : 5242880
atime                         : 2022-11-18T13:41:08
ctime                         : 2022-11-18T13:41:08
mtime                         : 2022-11-18T13:41:08
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 1
blockcnt                      : 10272
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00000001
eh_magic                      : 0xf30a
eh_entries                    : 4
eh_max                        : 4
eh_depth                      : 1
eh_generation                 : 0
----
ei_block                      : 0
ei_leaf_lo                    : 1133
ei_leaf_hi                    : 0
ei_unused                     : 0
----
ei_block                      : 340
ei_leaf_lo                    : 1134
ei_leaf_hi                    : 0
ei_unused                     : 0
----
ei_block                      : 680
ei_leaf_lo                    : 1135
ei_leaf_hi                    : 0
ei_unused                     : 0
----
ei_block                      : 1020
ei_leaf_lo                    : 1523
ei_leaf_hi                    : 0
ei_unused                     : 0
----
version                       : 0x2a5461b3
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

leaf 结构与 ext4_extent 有着相同的大小, 因此 inode 中最多也只能塞进 4 个 leaf,
同样的可以看到 eh_max 是 4.

上面的信息中, 可以看到其中一个 eh_depth 为 1 的 leaf 指向了 block 1523,
1523 处存放的应该是 eh_depth 为 0 的 extent 结构:

```console
$ imgeditor leaf.ext4 -- extent 1523
eh_magic                      : 0xf30a
eh_entries                    : 65
eh_max                        : 340
eh_depth                      : 0
eh_generation                 : 0
----
ee_block                      : 1020
ee_len                        : 4
ee_start_hi                   : 0
ee_start_lo                   : 2664
----
ee_block                      : 1024
ee_len                        : 4
ee_start_hi                   : 0
ee_start_lo                   : 2672
----
ee_block                      : 1028
ee_len                        : 4
ee_start_hi                   : 0
ee_start_lo                   : 2684
...
```

从 extent 1523 可以看到 leaf.ext4 镜像与 5M.bin 文件的映射关系:

```
leaf.ext4                5M.bin
2664(+4)                 1020(+4)
2672(+4)                 1024(+4)
2684(+4)                 1028(+4)
...
```

# 文件夹与文件存储的区别

总结下文件夹与文件 leaf 存储的区别:

```
文件夹: inode -> leaf -> extent -> dirent
文件:   inode -> leaf -> extent -> 文件的实际内容
```

前面的都是相同的. 文件夹的 extent 中指向的是存储 dirent 数组的块, dirent 中记录
了文件夹中文件的具体信息. 而文件的 extent 中指向的是文件的内容.
