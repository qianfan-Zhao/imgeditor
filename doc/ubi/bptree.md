b+ tree
=======

ubifs 使用 b+ 树来描述文件结构.

从 master 中可以读取到 rootfs.ubi 镜像的入口地址:

```console
$ ./imgeditor tests/fs/ubi/simple_abc.ubi -v -- node master
...
    root_lnum                 : 12
    root_offs                 : 0x00000108
...
```

# node 信息

从 root_lnum:root_offs 看去, 可以看到 level 1 和 level 0 的两个分支, level 0
中指向了最终的文件数据.

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

让我们用更直观的方式绘制这颗树:

```console
$ ./imgeditor tests/fs/ubi/simple_abc.ubi -- bptree --show-filenode
-----------01/02-----------|0012:00000:0000000100000000
                           |-----------01/08-----------|0010:003d0:0000000100000000 INO         1                    [40755]
                           |-----------02/08-----------|0010:00390:00000001400042f2 DENT ->000001 hash:  0x000042f2  [inode=65  dir a]
                           |-----------03/08-----------|0010:002f0:0000004100000000 INO        65                    [40755]
                           |-----------04/08-----------|0010:001d0:00000041400043a2 DENT ->000065 hash:  0x000043a2  [inode=66  dir b]
                           |-----------05/08-----------|0010:002b0:0000004140004452 DENT ->000065 hash:  0x00004452  [inode=68  dir c]
                           |-----------06/08-----------|0010:00130:0000004200000000 INO        66                    [40755]
                           |-----------07/08-----------|0010:000f0:0000004240032408 DENT ->000066 hash:  0x00032408  [inode=67  file ab]
                           |-----------08/08-----------|0010:00050:0000004300000000 INO        67                    [100644]
-----------02/02-----------|0012:000c0:0000004320000000
                           |-----------01/02-----------|0010:00000:0000004320000000 DATA       67 block: 00000000    [size=28    66696c6570617468(filepath)]
                           |-----------02/02-----------|0010:00210:0000004400000000 INO        68
```

UBIFS 使用 1 表示文件系统的根节点.

从上面可以找到 4 个 DENT 结构, 分别是 a, b, c, ab, 并且 a(inode 65) 的父 inode 是
1, 也就是根节点. b 和 c 的父节点是 65, 也就是 a.

```console
$ ./imgeditor tests/fs/ubi/simple_abc.ubi
|-- #00000065: Directory  a
|   |-- #00000066: Directory  b
|   |   |-- #00000067: File       ab 28 bytes
|   |-- #00000068: Directory  c
```

# 文件存储方式

总结下上面的 `bptree` 信息, 可以看到存储一个文件需要三个结构. ino 存储文件的权限等信息,
dent 存储文件所属的文件夹, data 存储文件的数据. 不同种类的 node 使用不同的方法计算
b+ 树 key 值, 并插入到 b+ 树中.

这样可以快速的在文件夹中查询指定的文件.
