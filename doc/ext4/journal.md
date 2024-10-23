journal
=======

在 ext4 文件系统的介绍中, 都表示它是一个日志文件系统, 关于这个日志是什么, 之前一直不
清楚. 这几天有个现场的问题, 出现了 ext4 fsck 失败的问题, 正好借着这个机会分析了一下
日志系统.

# inode 8

ext4 系统中使用 inode 来描述文件, 日志 (journal) 也是一个特殊的文件, 它的 inode 的
编号为 8, 写在 super block 中:

```console
$ imgeditor simple_abc.ext4 -- sblock | grep journal
journal_uuid                      : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
journal_inode                     : 8
journal_dev                       : 0
journal_backup_type               : 1
journal_blocks                    : 258826 4 0 0 10 9 10 15 20 25 999 164 0 0 0 0 4194304
```

在我现在使用的这个 16MiB 大小的镜像中, journal 占用了 4MiB 大小的空间. 既然 jouranl
是一个文件, 那么它的存储方式自然与系统中的其他文件没有差别.

```console
$ imgeditor simple_abc.ext4 -- inode 8
inode #8 location on blk #35 + 0x0380
mode                          : 0x8180 (File 600)
uid                           : 0
size                          : 4194304
atime                         : 2024-10-22T11:08:31
ctime                         : 2024-10-22T11:08:31
mtime                         : 2024-10-22T11:08:31
dtime                         : 1970-01-01T08:00:00
gid                           : 0
nlinks                        : 1
blockcnt                      : 8192
flags                         : 0x00080000 ( EXT4_EXTENTS_FL )
osd1                          : 0x00000000
eh_magic                      : 0xf30a
eh_entries                    : 3
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
----
ee_block                      : 0
ee_len                        : 10
ee_start_hi                   : 0
ee_start_lo                   : 9
----
ee_block                      : 10
ee_len                        : 15
ee_start_hi                   : 0
ee_start_lo                   : 20
----
ee_block                      : 25
ee_len                        : 999
ee_start_hi                   : 0
ee_start_lo                   : 164
----
version                       : 0x00000000
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00002540
```

# journal super block

可以将 journal 理解成一个独立的文件系统, 只不过它被集成到 ext4 文件系统中. journal
也有它自己的 super block 和文件结构:

```console
$ imgeditor simple_abc.ext4 -- journal sblock
h_magic                       0xc03b3998
h_blocktype                   0x00000004 (Superblock, v2)
h_sequence                    0
s_blocksize                   4096
s_maxlen                      1024
s_first                       0x00000001
s_sequence                    2
s_start                       0x00000001
s_errno                       0x00000000
s_feature_compat              0x00000000 ( )
s_feature_incompat            0x00000012 ( JBD2_FEATURE_INCOMPAT_64BIT JBD2_FEATURE_INCOMPAT_CSUM_V3 )
s_feature_ro_compat           0x00000000
s_uuid                        46 c8 7e 72 c3 64 4f 88 ad 03 c1 0f 15 57 cb d7
s_nr_users                    0x00000001
s_dynsuper                    0x00000000
s_max_transaction             0x00000000
s_max_trans_data              0x00000000
```

journal 文件划分成 s_blocksize 大小的块来管理, 可以通过命令查看 journal 的块的具体
内容:

```console
$ imgeditor simple_abc.ext4 -- journal block 0
00000000 c0 3b 39 98 00 00 00 04 00 00 00 00 00 00 10 00 |.;9.............|
00000010 00 00 04 00 00 00 00 01 00 00 00 02 00 00 00 01 |................|
00000020 00 00 00 00 00 00 00 00 00 00 00 12 00 00 00 00 |................|
00000030 46 c8 7e 72 c3 64 4f 88 ad 03 c1 0f 15 57 cb d7 |F.~r.dO......W..|
00000040 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 |................|
00000050 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
00000060 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
000000f0 00 00 00 00 00 00 00 00 00 00 00 00 f3 cf 76 af |..............v.|
00000100 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00001000
```

# 文件更新过程

以 `make test` 测试中生成的 simple_abc.ext4 为例子, 演示 journal 的工作流程:

挂载文件系统, 并在根目录下创建一个名为 'd' 的文件:

```console
$ cp tests/fs/ext4/simple_abc.ext4 .
$ sudo mount simple_abc.ext4 mnt -o data=journal
$ sudo sh -c 'echo abcd > mnt/d'
$ sync
$ imgeditor simple_abc.ext4
ext2: ext filesystem image
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  a
|   |-- #00000013: Directory  b
|   |-- #00000015: Directory  c
|-- #00000016: File       d 5 bytes
```

此时能看到 jouranl 中有一些提交记录, 我们先不关注这次的提交记录的具体内容, 我们重点
关注下 `d` 文件变化之后 journal 中的变化.

```console
$ imgeditor simple_abc.ext4 -- journal
Journal    0 on blk #0     Superblock    sequence=2
Journal    2 on blk #1     Descriptor
                           FS blocknr 19      logged at journal block 2     with flags 0x0
                           FS blocknr 1       logged at journal block 3     with flags 0x2
                           FS blocknr 35      logged at journal block 4     with flags 0x2
                           FS blocknr 4       logged at journal block 5     with flags 0x2
                           FS blocknr 0       logged at journal block 6     with flags 0x2
                           FS blocknr 3       logged at journal block 7     with flags 0x2
                           FS blocknr 1167    logged at journal block 8     with flags 0xa
Journal    2 on blk #9     Commit        2024-10-23T16:00:57.224
```

更新 'd' 文件的内容:

```console
$ sudo sh -c 'echo abcd > mnt/d'
$ sync
$ imgeditor simple_abc.ext4 -- journal
Journal    0 on blk #0     Superblock    sequence=2
Journal    2 on blk #1     Descriptor
                           FS blocknr 19      logged at journal block 2     with flags 0x0
                           FS blocknr 1       logged at journal block 3     with flags 0x2
                           FS blocknr 35      logged at journal block 4     with flags 0x2
                           FS blocknr 4       logged at journal block 5     with flags 0x2
                           FS blocknr 0       logged at journal block 6     with flags 0x2
                           FS blocknr 3       logged at journal block 7     with flags 0x2
                           FS blocknr 1167    logged at journal block 8     with flags 0xa
Journal    2 on blk #9     Commit        2024-10-23T16:00:57.224
Journal    3 on blk #10    Descriptor
                           FS blocknr 0       logged at journal block 11    with flags 0x0
                           FS blocknr 35      logged at journal block 12    with flags 0x2
                           FS blocknr 3       logged at journal block 13    with flags 0x2
                           FS blocknr 1       logged at journal block 14    with flags 0x2
                           FS blocknr 1679    logged at journal block 15    with flags 0xa
Journal    3 on blk #16    Commit        2024-10-23T16:03:02.204
```

可以看到在文件更新之后, 增加了一条提交内容. 里面有一条的内容是这样的:

`FS blocknr 1679    logged at journal block 15    with flags 0xa`

提到的两个 `block` 的含义是不一样的, 第一个 1679 是在整个文件系统中的块号, 第二个 15
是在 journal 这个文件 (inode 8) 中的块号, 块号都是从 0 开始计数的.

```console
$ imgeditor simple_abc.ext4 -- inode 16
inode #16 location on blk #35 + 0x0780
...
ee_start_hi                   : 0
ee_start_lo                   : 1679
...
```

1679 这个块是 'd' 这个文件在 ext4 中的真实位置:

```console
$ imgeditor simple_abc.ext4 -- block 1679
0068f000 61 62 63 64 0a 00 00 00 00 00 00 00 00 00 00 00 |abcd............|
0068f010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00690000
```

journal block 15 是在日志文件 (inode 8) 中的位置, 跟上面的 1679 是两个不同的物理
位置.

```console
$ imgeditor simple_abc.ext4 -- journal block 15
00000000 61 62 63 64 0a 00 00 00 00 00 00 00 00 00 00 00 |abcd............|
00000010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00001000
```

从这里也能看出 journal 的逻辑, 先在 journal 文件中更新, 更新完毕之后再更新实际文件,
当两者都更新完毕之后, 在写入一条 commit.

# fsck

将此时的 simple_abc.ext4 文件复制一份后打开二进制编辑工具, 将 'd' 文件的内容
(block 1679) 修改成为 '????', 命名为 simple_abc_chagne_d.ext4, 修改完可以确认下:

```console
$ imgeditor simple_abc_change_d.ext4 -- block 1679
0068f000 3f 3f 3f 3f 0a 00 00 00 00 00 00 00 00 00 00 00 |????............|
0068f010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00690000
```

写一个辅助脚本 fsck.sh 来测试下 fsck 在不同参数下的修复能力:

```shell
#!/bin/sh

cp simple_abc_change_d.ext4 1.ext4
fsck.ext4 "$@" 1.ext4
echo $?
imgeditor 1.ext4 -- block 1679
```

可以使用这样的参数来进行测试:

```console
$ ./fsck.sh
e2fsck 1.45.5 (07-Jan-2020)
1.ext4: recovering journal
Setting free inodes count to 4080 (was 4081)
Setting free blocks count to 2928 (was 2929)
1.ext4: clean, 16/4096 files, 1168/4096 blocks
0
0068f000 61 62 63 64 0a 00 00 00 00 00 00 00 00 00 00 00 |abcd............|
0068f010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00690000
```

整理了不同参数下的修复结果:

| 修复参数 | 结果     | fsck返回值 |
| -------- | -------- | ---------- |
| 无参数   | 成功修复 | 0          |
| -p       | 成功修复 | 0          |
| -y       | 成功修复 | 0          |
| -n       | 未修复   | 0          |

除了 `-n` 参数, 都能够修复.

另外还可以测试不修复直接挂载, 在我的系统上能到看到文件在 mount 的时候自动被修复了.

```console
$ cp simple_abc_change_d.ext4 1.ext4
$ sudo mount 1.ext4 mnt
$ cat mnt/d
abcd
```
