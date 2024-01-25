一个文件夹下能容纳多少文件夹?
==========================

创建测试用的文件系统镜像, 并挂载.

```console
$ mkdir ext4 mount
$ make_ext4fs -l 16M ext4.img ext4
$ sudo mount ext4.img mount
$ sudo chown qianfan:qianfan mount
```

创建一个 test1 文件夹, 并查看其 inode 的信息:

```console
$ imgeditor ext4.img
ext2: ext filesystem image
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  dir1
$ imgeditor ext4.img -- inode 12
mode                          : 0x41ed (Directory 755)
uid                           : 1000
size                          : 4096
atime                         : 2023-04-20T14:54:06
ctime                         : 2023-04-20T14:54:06
mtime                         : 2023-04-20T14:54:06
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 2
```

可以看到, 刚创建的文件夹的 `nlinks` 属性为 2, 被文件夹下的 "." 和 ".." 引用.
当在 dir1 下面再次新建一个文件夹时, 可以看到 `nlinks` 的个数会响应的增加.

```console
$ mkdir dir1/dir1_1; sync
$ imgeditor ext4.img -- inode 12
mode                          : 0x41ed (Directory 755)
uid                           : 1000
size                          : 4096
atime                         : 2023-04-20T14:54:06
ctime                         : 2023-04-20T15:01:16
mtime                         : 2023-04-20T15:01:16
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 3
```

在文件夹下面创建文件时, `nlinks` 的个数并不会增加.

```console
$ touch dir1/file1_1; sync
$ imgeditor ext4.img -- inode 12
mode                          : 0x41ed (Directory 755)
uid                           : 1000
size                          : 4096
atime                         : 2023-04-20T14:54:06
ctime                         : 2023-04-20T15:02:32
mtime                         : 2023-04-20T15:02:32
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 3
```

当我在一个 inode 充足的磁盘分区上测试无限制的创建文件夹时, 发现 `nlinks` 的数值并不会
无限制的增加, 看下面的测试脚本:

```consosle
$ n=0; while mkdir testdir_$n; do let n++; done
mkdir: cannot create directory 'testdir_64998': Too many links
```

linux 内核中对一个 inode 的最大 `nlinks` 数量做了限制:

```c
/*
 * Maximal count of links to a file
 */
#define EXT2_LINK_MAX		32000
#define EXT4_LINK_MAX		65000
```
