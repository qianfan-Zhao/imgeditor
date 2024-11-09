squashfs
========

squashfs 是一个只读的, 压缩的文件系统, 好理解一些就是将一个压缩文件增加一些其他属性使
操作系统能够将其当做文件系统挂载. 它的二进制格式可以参考:
https://github.com/AgentD/squashfs-tools-ng/blob/master/doc/format.adoc

在 tests 中做了一些例子, 可以通过 `make test` 生成这些例子.

在 `imgeditor` 的实现中, 不像 `ext4` 和 `ubi` 那样增加了文件系统的解析, 在这里只是
探索了 `squashfs` 的二进制结构, 并且不支持压缩格式. 文件系统数据的解包和打包是依赖
外部的 `mksquashfs` 和 `unsquashfs` 实现的.

既然是压缩的, 那自然是移除了一些没用的对齐, squashfs 的不同数据段之间是紧密结合的,
一个字节挨着一个字节的.

# sblock

每一个文件系统都少不了 supber block 和 inode, 先看下 sblock:

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -- sblock
inodes                        : 7
mkfs_time                     : 2024-11-12T15:26:43
block_size                    : 131072
fragments                     : 1
compression                   : 1 (ZLIB)
block_log                     : 17
flags                         : 0x01cb (NOI NOD NOF DUPLICATE EXPORT)
no_ids                        : 1
s_major                       : 4
s_minor                       : 0
root_inode                    : 192
bytes_used                    : 589
id_table_start                : 581
xattr_id_table_start          : 0xffffffffffffffff
inode_table_start             : 150
directory_table_start         : 376
fragment_table_start          : 501
lookup_table_start            : 567
```

超级块中描述的信息比较少, 关键的信息就是那几个 start 的地址, 这几个地址的含义不一样,
有的是一维指针, 有的是二维指针.

# metadata

`squashfs` 中引入了一个 metadata 的结构, 说白了就是在一个数据块(大小不超过8K)的前面
增加 16bit (小端序) 的文件头, 最高位(bit15) 为 0 表示数据是压缩状态, 非零表示未压缩.
其他的位表示后面数据块的长度. 参考下面的例子:

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -- hexdump 0x023f 16
0000023f 04 80 e8 03 00 00 3f 02 00 00 00 00 00 00 00 00 |......?.........|
0000024f
```

文件头是 0x8004 表示未压缩, 并且后面的数据长度是 4 字节 (e8, 03, 00, 00).

# inode

`inode_table_start` 是一个一维指针, 指向了一个 `union squashfs_inode` 结构的数组.
数组的中的数据是使用 metadata 的格式存储的.

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -- hexdump 150 256
00000096 e0 80 02 00 a4 01 00 00 00 00 32 03 33 67 02 00 |..........2.3g..|
000000a6 00 00 00 00 00 00 00 00 00 00 00 00 00 00 12 00 |................|
000000b6 00 00 01 00 ed 01 00 00 00 00 32 03 33 67 01 00 |..........2.3g..|
000000c6 00 00 00 00 00 00 02 00 00 00 19 00 00 00 07 00 |................|
000000d6 00 00 02 00 a4 01 00 00 00 00 32 03 33 67 04 00 |..........2.3g..|
000000e6 00 00 00 00 00 00 00 00 00 00 12 00 00 00 12 00 |................|
000000f6 00 00 01 00 ed 01 00 00 00 00 32 03 33 67 03 00 |..........2.3g..|
00000106 00 00 00 00 00 00 02 00 00 00 19 00 16 00 07 00 |................|
00000116 00 00 02 00 a4 01 00 00 00 00 33 03 33 67 06 00 |..........3.3g..|
00000126 00 00 00 00 00 00 00 00 00 00 24 00 00 00 12 00 |..........$.....|
00000136 00 00 01 00 ed 01 00 00 00 00 33 03 33 67 05 00 |..........3.3g..|
00000146 00 00 00 00 00 00 02 00 00 00 19 00 2c 00 07 00 |............,...|
00000156 00 00 01 00 ed 01 00 00 00 00 33 03 33 67 07 00 |..........3.3g..|
00000166 00 00 00 00 00 00 05 00 00 00 2a 00 42 00 08 00 |..........*.B...|
00000176 00 00 69 80 00 00 00 00 00 00 00 00 02 00 00 00 |..i.............|
00000186 00 00 00 00 02 00 01 00 61 61 00 00 00 00 00 00 |........aa......|
00000196
```

对上面的数据进行解析可以看到描述的 inode 信息:

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -v -- inodes
00000000 02 00 a4 01 00 00 00 00 32 03 33 67 02 00 00 00 |........2.3g....|
00000010 00 00 00 00 00 00 00 00 00 00 00 00 12 00 00 00 |................|
00000020 01 00 ed 01 00 00 00 00 32 03 33 67 01 00 00 00 |........2.3g....|
00000030 00 00 00 00 02 00 00 00 19 00 00 00 07 00 00 00 |................|
00000040 02 00 a4 01 00 00 00 00 32 03 33 67 04 00 00 00 |........2.3g....|
00000050 00 00 00 00 00 00 00 00 12 00 00 00 12 00 00 00 |................|
00000060 01 00 ed 01 00 00 00 00 32 03 33 67 03 00 00 00 |........2.3g....|
00000070 00 00 00 00 02 00 00 00 19 00 16 00 07 00 00 00 |................|
00000080 02 00 a4 01 00 00 00 00 33 03 33 67 06 00 00 00 |........3.3g....|
00000090 00 00 00 00 00 00 00 00 24 00 00 00 12 00 00 00 |........$.......|
000000a0 01 00 ed 01 00 00 00 00 33 03 33 67 05 00 00 00 |........3.3g....|
000000b0 00 00 00 00 02 00 00 00 19 00 2c 00 07 00 00 00 |..........,.....|
000000c0 01 00 ed 01 00 00 00 00 33 03 33 67 07 00 00 00 |........3.3g....|
000000d0 00 00 00 00 05 00 00 00 2a 00 42 00 08 00 00 00 |........*.B.....|
000000e0
         type     mode  uid  gid inode
00000000 REG       644 1000 1000     2 offset=0x0000 block=0 fragment=0 filesize=18
00000020 DIR       755 1000 1000     1 offset=0x0000 block=0 nlink=2 filesize=25 parent=7
00000040 REG       644 1000 1000     4 offset=0x0012 block=0 fragment=0 filesize=18
00000060 DIR       755 1000 1000     3 offset=0x0016 block=0 nlink=2 filesize=25 parent=7
00000080 REG       644 1000 1000     6 offset=0x0024 block=0 fragment=0 filesize=18
000000a0 DIR       755 1000 1000     5 offset=0x002c block=0 nlink=2 filesize=25 parent=7
000000c0 DIR       755 1000 1000     7 offset=0x0042 block=0 nlink=5 filesize=42 parent=8
```

# directory

同 `inode` 的存储一样, 文件夹结构的存储也是一个一维数组, 指针位置记录在
`directory_table_start`. 结构的内容是 `struct squashfs_dir_header`
和 `struct squashfs_dir_entry`.

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -v -- dirs
00000000 00 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 |................|
00000010 02 00 01 00 61 61 00 00 00 00 00 00 00 00 04 00 |....aa..........|
00000020 00 00 40 00 00 00 02 00 01 00 62 62 00 00 00 00 |..@.......bb....|
00000030 00 00 00 00 06 00 00 00 80 00 00 00 02 00 01 00 |................|
00000040 63 63 02 00 00 00 00 00 00 00 01 00 00 00 20 00 |cc............ .|
00000050 00 00 01 00 00 00 61 60 00 02 00 01 00 00 00 62 |......a`.......b|
00000060 a0 00 04 00 01 00 00 00 63                      |........c       |
00000070
              type     inode off    name
00000000+000c REG      2     0x0000 aa
00000016+000c REG      4     0x0040 bb
0000002c+000c REG      6     0x0080 cc
00000042+000c DIR      1     0x0020 a
00000042+0015 DIR      3     0x0060 b
00000042+001e DIR      5     0x00a0 c
```

# fragment

`fragment` 是用于存储文件的数据内容的结构, 起始地址记录在 `fragment_table_start`,
但它是一个二维数组. 一维数组的个数存储在 `fragments` 中.

从上面列出的超级块信息中可以看到 `fragment_table_start=501`, `fragments=1`:

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -- hexdump 501 16
000001f5 e3 01 00 00 00 00 00 00 38 80 20 00 00 00 00 00 |........8. .....|
00000205
```

从 501 这个一维地址处可以读到二维地址, squashfs 中定义了指针的长度是 8 字节,
并且按照小端存储的. 所以从上面的数据中可以得到二维指针地址是 0x00000000000001e3
之后按照这个地址可以读到使用 metadata 存储的 fragment 的信息:

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -- hexdump 0x1e3 64
000001e3 10 80 60 00 00 00 00 00 00 00 36 00 00 01 00 00 |..`.......6.....|
000001f3 00 00 e3 01 00 00 00 00 00 00 38 80 20 00 00 00 |..........8. ...|
00000203 00 00 00 00 00 00 00 00 00 00 00 00 60 00 00 00 |............`...|
00000213 00 00 00 00 40 00 00 00 00 00 00 00 a0 00 00 00 |....@...........|
00000223
```

里面存储的结构是 `struct squashfs_fragment_entry`. 经过解析之后是:

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -- fragments
fragment 1/1:
            start size
00000000       96   54
```

通过 fragment_entry 的地址能够读到记录的内容:

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -- hexdump 96 54
00000060 66 69 6c 65 20 6e 61 6d 65 20 61 61 20 69 6e 20 |file name aa in |
00000070 61 0a 66 69 6c 65 20 6e 61 6d 65 20 62 62 20 69 |a.file name bb i|
00000080 6e 20 62 0a 66 69 6c 65 20 6e 61 6d 65 20 63 63 |n b.file name cc|
00000090 20 69 6e 20 63 0a                               | in c.          |
000000a0
```

inode 中会记录文件数据在哪一个 fragment 中的哪个偏移地址处:

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -- inodes | grep REG
00000000 REG       644 1000 1000     2 offset=0x0000 block=0 fragment=0 filesize=18
00000040 REG       644 1000 1000     4 offset=0x0012 block=0 fragment=0 filesize=18
00000080 REG       644 1000 1000     6 offset=0x0024 block=0 fragment=0 filesize=18
```

与 inode=4 的文件为例, 他的数据内容存在 fragment 0 的偏移 0x12 字节处, 大小是 18 字节,
通过之前的读取知道 fragment 0 的起始地址是 96, 因此总偏移是 96 + 0x12 = 114

```console
$ ./imgeditor tests/fs/squashfs_nocomp/simple_abc.squashfs -- hexdump 114 18
00000072 66 69 6c 65 20 6e 61 6d 65 20 62 62 20 69 6e 20 |file name bb in |
00000082 62 0a                                           |b.              |
00000092
```
