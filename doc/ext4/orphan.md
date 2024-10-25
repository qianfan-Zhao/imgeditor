orphan
======

从 simple_abc 中复制一个镜像, 然后在里面创建几个文件.

```console
$ cp tests/fs/ext4/simple_abc.ext4 .
$ sudo mount simple_abc.ext4 mnt
$ sudo chown qianfan:qianfan mnt
$ for f in d e f g h ; do echo $f > mnt/$f; done
$ sync
```

查看镜像的 inode 信息, 创建的 5 个文件的 inode 从 16 开始.

```console
$ imgeditor simple_abc.ext4
ext2: ext filesystem image
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  a
|   |-- #00000013: Directory  b
|   |-- #00000015: Directory  c
|-- #00000016: File       d 2 bytes
|-- #00000017: File       e 2 bytes
|-- #00000018: File       f 2 bytes
|-- #00000019: File       g 2 bytes
|-- #00000020: File       h 2 bytes
```

以文件 'd' 为例, 查看 'd' 的信息:

```console
$ imgeditor simple_abc.ext4 -- inode 16
inode #16 location on blk #35 + 0x0780
mode                          : 0x81a4 (File 644)
uid                           : 1000
size                          : 2
atime                         : 2024-10-25T09:25:44 (1729819544)
ctime                         : 2024-10-25T09:25:44 (1729819544)
mtime                         : 2024-10-25T09:25:44 (1729819544)
dtime                         : 1970-01-01T08:00:00 (0)
gid                           : 1000
nlinks                        : 1
blockcnt                      : 8
flags                         : 0x00080000 ( EXT4_EXTENTS_FL )
osd1                          : 0x00000001
eh_magic                      : 0xf30a
eh_entries                    : 1
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
ee_block                      : 0
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1167
version                       : 0xfceae40a
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x0000eede
$ imgeditor simple_abc.ext4 -- block 1167
0048f000 64 0a 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |d...............|
0048f010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
*
00490000
```

现在 inode 中保留了文件 'd' 的基本信息, `nlinks` 表示文件的引用计数, 大于 0 的数字
表示文件正在被引用, `dtime` 表示文件的删除时间, 因为文件并未被删除, 所以 `dtime` 这个
数字是 0.

当我们将 'd' 这个文件从磁盘上删除之后, 在重新查看这个 inode, 可以看到 inode 中文件长度
和布局的信息被抹除, 同时也更新了 `dtime` 为删除时的时间, 并且 `nlinks` 更新为 0.

```console
$ imgeditor simple_abc.ext4 -- inode 16
inode #16 location on blk #35 + 0x0780
mode                          : 0x81a4 (File 644)
uid                           : 1000
size                          : 0
atime                         : 2024-10-25T09:25:44 (1729819544)
ctime                         : 2024-10-25T09:30:10 (1729819810)
mtime                         : 2024-10-25T09:30:10 (1729819810)
dtime                         : 2024-10-25T09:30:10 (1729819810)
gid                           : 1000
nlinks                        : 0
blockcnt                      : 0
flags                         : 0x00080000 ( EXT4_EXTENTS_FL )
osd1                          : 0x00000001
eh_magic                      : 0xf30a
eh_entries                    : 0
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
version                       : 0xfceae40a
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x000069b0
```

当我们再次创建一个文件时, 可以看到 ext4 文件系统重新启用了上一次删掉的 inode:

```console
$ echo ddd > mnt/ddd
$ sync
$ imgeditor simple_abc.ext4
ext2: ext filesystem image
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  a
|   |-- #00000013: Directory  b
|   |-- #00000015: Directory  c
|-- #00000016: File       ddd 4 bytes
|-- #00000017: File       e 2 bytes
|-- #00000018: File       f 2 bytes
|-- #00000019: File       g 2 bytes
|-- #00000020: File       h 2 bytes
```

并且 inode 中的信息被重新设置, `dtime` 这个字段再次清零.

```console
$ imgeditor simple_abc.ext4 -- inode 16
inode #16 location on blk #35 + 0x0780
mode                          : 0x81a4 (File 644)
uid                           : 1000
size                          : 4
atime                         : 2024-10-25T09:33:44 (1729820024)
ctime                         : 2024-10-25T09:33:44 (1729820024)
mtime                         : 2024-10-25T09:33:44 (1729820024)
dtime                         : 1970-01-01T08:00:00 (0)
gid                           : 1000
nlinks                        : 1
blockcnt                      : 8
flags                         : 0x00080000 ( EXT4_EXTENTS_FL )
osd1                          : 0x00000001
eh_magic                      : 0xf30a
eh_entries                    : 1
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
ee_block                      : 0
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1167
version                       : 0x725b81ae
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x0000e8d1
```

# 孤儿节点

考虑这样一种情况, 一个进程占用了一个文件(打开之后一直没有关闭)之后, 这个文件被其他进程
删除, 因为这个 inode 被占用, 所以当删除文件的时候并不能将这个文件抹掉. 做个实验查看下
系统是如何处理这种问题的.

写一个简单的 C 程序来占用一个文件, 并编译成 openit:

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	if (argc == 1)
		return 0;

	for (int i = 1; i < argc; i++) {
		int fd = open(argv[i], O_RDONLY);

		if (fd < 0)
			continue;

		printf("open %s as %d\n", argv[i], fd);
	}

	while (1)
		sleep(1);

	return 0;
}
```

之后使用 openit 打开磁盘上的 e, f, g, h 四个文件:

```console
$ ./openit mnt/e mnt/f mnt/g mnt/h
open mnt/e as 3
open mnt/f as 4
open mnt/g as 5
open mnt/h as 6
```

之后从磁盘上删掉 e, f, g, h 四个文件, 删掉之后通过 `ls` 这种命令已经看不到文件了.

```console
$ rm mnt/e mnt/f mnt/g mnt/h
$ sync
$ imgeditor simple_abc.ext4
ext2: ext filesystem image
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  a
|   |-- #00000013: Directory  b
|   |-- #00000015: Directory  c
|-- #00000016: File       ddd 4 bytes
```

但是这四个文件的 inode 还继续保留:

```console
$ imgeditor simple_abc.ext4 -- inode 17
inode #17 location on blk #35 + 0x0800
mode                          : 0x81a4 (File 644)
uid                           : 1000
size                          : 2
atime                         : 2024-10-25T09:25:44 (1729819544)
ctime                         : 2024-10-25T09:43:54 (1729820634)
mtime                         : 2024-10-25T09:25:44 (1729819544)
dtime                         : 1970-01-01T08:00:00 (0)
gid                           : 1000
nlinks                        : 0
blockcnt                      : 8
flags                         : 0x00080000 ( EXT4_EXTENTS_FL )
osd1                          : 0x00000001
eh_magic                      : 0xf30a
eh_entries                    : 1
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
ee_block                      : 0
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1168
version                       : 0xc7f46599
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x0000f028
```

假设机器在此时掉电, 在下次开机时文件系统如何识别出这几个被删掉的文件?
我们将此时的 simple_abc.ext4 复制一份重命名为 orphan.ext4, 下面分析下 orphan.ext4.

ext4 文件系统使用 inode 结构中的 `dtime` 作为一个单项链表, 记录孤儿节点, 这个链表
的表头是 super block 中的 last_orphan, 链表关系如下:

        last_orphan -> inode1.dtime -> inode2.dtime -> ... -> 0

下面我们来跟踪下这个链表:

```console
$ imgeditor orphan.ext4 -- sblock | grep orphan
last_orphan                       : 20
$ imgeditor orphan.ext4 -- inode 20 | grep dtime
dtime                         : 1970-01-01T08:00:19 (19)
$ imgeditor orphan.ext4 -- inode 19 | grep dtime
dtime                         : 1970-01-01T08:00:18 (18)
$ imgeditor orphan.ext4 -- inode 18 | grep dtime
dtime                         : 1970-01-01T08:00:17 (17)
$ imgeditor orphan.ext4 -- inode 17 | grep dtime
dtime                         : 1970-01-01T08:00:00 (0)
```

链表的顺序与删除的顺序相反, 分别对应 h, g, f, e 四个文件.

# mount 修复

经过测试, 在 mount 的时候, 可以自动对这种孤儿节点进行修复:

```console
$ cp orphan.ext4 1.ext4
$ sudo mount 1.ext4 mnt
$ imgeditor 1.ext4
ext2: ext filesystem image
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  a
|   |-- #00000013: Directory  b
|   |-- #00000015: Directory  c
|-- #00000016: File       ddd 4 bytes
$ imgeditor 1.ext4 -- sblock | grep orphan
last_orphan                       : 0
$ imgeditor 1.ext4 -- inode 20
inode #20 location on blk #35 + 0x0980
mode                          : 0x81a4 (File 644)
uid                           : 1000
size                          : 0
atime                         : 2024-10-25T09:25:44 (1729819544)
ctime                         : 2024-10-25T10:03:36 (1729821816)
mtime                         : 2024-10-25T10:03:36 (1729821816)
dtime                         : 2024-10-25T10:03:36 (1729821816)
gid                           : 1000
nlinks                        : 0
blockcnt                      : 0
flags                         : 0x00080000 ( EXT4_EXTENTS_FL )
osd1                          : 0x00000001
eh_magic                      : 0xf30a
eh_entries                    : 0
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
version                       : 0xc4d5ad39
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x0000dbe0
```

孤儿节点的 `dtime` 时间被更新成这次 mount 的时间.

# fsck 修复

fsck 的自动模式也能对这种简单的孤儿节点进行修复:

```console
$ cp orphan.ext4 1.ext4
$ fsck.ext4 -p 1.ext4
1.ext4: recovering journal
1.ext4: Clearing orphaned inode 20 (uid=1000, gid=1000, mode=0100644, size=2)
1.ext4: Clearing orphaned inode 19 (uid=1000, gid=1000, mode=0100644, size=2)
1.ext4: Clearing orphaned inode 18 (uid=1000, gid=1000, mode=0100644, size=2)
1.ext4: Clearing orphaned inode 17 (uid=1000, gid=1000, mode=0100644, size=2)
1.ext4: clean, 16/4096 files, 1168/4096 blocks
```

# 不在链表中的孤儿节点

上面的那种被链表记录的孤儿节点很好处理, 我还在现场中遇到一个复杂的. 表现出的问题是
`fsck.ext4` 的自动模式无法修复, 导致 systemd-fsck 失败, 从而导致 systemd 进入紧急
模式, 不挂载磁盘, 无法正常开机.

虽然 fsck 失败, 但是里面的文件数据还是好的, 能够正常挂载, 挂载之后能看到里面的数据还是
好的.

下面是 fsck.ext4 的修复记录:

```console
$ cp bad.ext4 1.ext4
$ fsck.ext4 -p 1.ext4
1.ext4 contains a file system with errors, check forced.
1.ext4: Inodes that were part of a corrupted orphan linked list found.

1.ext4: UNEXPECTED INCONSISTENCY; RUN fsck MANUALLY.
        (i.e., without -a or -p options)
$ echo $?
4
```

`man fsck.ext4` 标记了几个返回值的含义, 4 表示未能完全修复, 所以 systemd 未能正常
开机.

```
EXIT CODE
       The exit code returned by e2fsck is the sum of the following conditions:
            0    - No errors
            1    - File system errors corrected
            2    - File system errors corrected, system should
                   be rebooted
            4    - File system errors left uncorrected
            8    - Operational error
            16   - Usage or syntax error
            32   - E2fsck canceled by user request
            128  - Shared library error
```

查看 super block 中的 orphan 链表, 可以看到链表是空的.

```
$ imgeditor 1.ext4 -- sblock | grep orphan
last_orphan                       : 0
```

编写程序对所有 inode 中的 `dtime` 进行扫描, 能找到一些 `dtime` 异常的节点:

```console
$ imgeditor 1.ext4 -- orphan
inode #40961  in orphan list
inode #40962  in orphan list
inode #40963  in orphan list
inode #40964  in orphan list
inode #40965  in orphan list
inode #40966  in orphan list
inode #40967  in orphan list
inode #40968  in orphan list
inode #40969  in orphan list
inode #40970  in orphan list
inode #40971  in orphan list
inode #40972  in orphan list
inode #40973  in orphan list
inode #40974  in orphan list
inode #40975  in orphan list
inode #40976  in orphan list
inode #40977  in orphan list
```

随表挑一个查看, 能够看到 `dtime` 是一个 inode, 表示这个 40961 这个 inode 曾经是
孤儿链表中的一环, 但是不清楚是什么异常原因导致他没能被正确回收.

```
$ imgeditor 1.ext4 -- inode 40961
inode #40961 location on blk #164099 + 0x0000
mode                          : 0x41ed (Directory 755)
uid                           : 0
size                          : 0
atime                         : 1970-01-02T00:13:27 (58407)
ctime                         : 1970-01-02T00:13:28 (58408)
mtime                         : 1970-01-02T00:13:28 (58408)
dtime                         : 1970-01-02T00:13:28 (58408)
gid                           : 0
nlinks                        : 0
blockcnt                      : 0
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00000008
eh_magic                      : 0xf30a
eh_entries                    : 0
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
version                       : 0x4485dfe4
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

`fsck` 的自动模式不能修复这种问题, 但是 `-y` 模式能够修复:

```console
fsck.ext4 -y 1.ext4
e2fsck 1.45.5 (07-Jan-2020)
1.ext4: recovering journal
1.ext4 contains a file system with errors, check forced.
Pass 1: Checking inodes, blocks, and sizes
Inodes that were part of a corrupted orphan linked list found.  Fix? yes

Inode 40961 was part of the orphaned inode list.  FIXED.
Inode 40962 was part of the orphaned inode list.  FIXED.
Inode 40963 was part of the orphaned inode list.  FIXED.
Inode 40964 was part of the orphaned inode list.  FIXED.
Inode 40965 was part of the orphaned inode list.  FIXED.
Inode 40966 was part of the orphaned inode list.  FIXED.
Inode 40967 was part of the orphaned inode list.  FIXED.
Inode 40968 was part of the orphaned inode list.  FIXED.
Inode 40969 was part of the orphaned inode list.  FIXED.
Inode 40970 was part of the orphaned inode list.  FIXED.
Inode 40971 was part of the orphaned inode list.  FIXED.
Inode 40972 was part of the orphaned inode list.  FIXED.
Inode 40973 was part of the orphaned inode list.  FIXED.
Inode 40974 was part of the orphaned inode list.  FIXED.
Inode 40975 was part of the orphaned inode list.  FIXED.
Inode 40976 was part of the orphaned inode list.  FIXED.
Inode 40977 was part of the orphaned inode list.  FIXED.
Pass 2: Checking directory structure
Pass 3: Checking directory connectivity
Pass 4: Checking reference counts
Pass 5: Checking group summary information
Padding at end of inode bitmap is not set. Fix? yes


1.ext4: ***** FILE SYSTEM WAS MODIFIED *****
1.ext4: 9053/262144 files (0.4% non-contiguous), 383357/1048576 blocks
```

修复之后的 inode 内容如下:

```console
$ imgeditor 1.ext4 -- inode 40961
inode #40961 location on blk #164099 + 0x0000
mode                          : 0x41ed (Directory 755)
uid                           : 0
size                          : 0
atime                         : 1970-01-02T00:13:27 (58407)
ctime                         : 1970-01-02T00:13:28 (58408)
mtime                         : 1970-01-02T00:13:28 (58408)
dtime                         : 2024-10-25T10:23:58 (1729823038)
gid                           : 0
nlinks                        : 0
blockcnt                      : 0
flags                         : 0x00080080 ( EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00000008
eh_magic                      : 0xf30a
eh_entries                    : 0
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
version                       : 0x4485dfe4
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

另外经过我的测试, mount 并不会对这种孤儿节点进行修复.
