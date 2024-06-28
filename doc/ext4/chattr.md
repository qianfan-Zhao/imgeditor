chattr的底层原理
===============

突然想起一件事情, `chattr +i` 可以让一个文件变得不可写. 不同于权限控制, chattr 之后
即使 root 账户也无法写文件. 看下 chattr 的解释:

> A  file  with  the 'i' attribute cannot be modified: it cannot be deleted
or renamed, no link can be created to this file, most of the file's metadata
can not be modified, and the file can not be opened in write mode.
Only the superuser or a process possessing the CAP_LINUX_IMMUTABLE capability
can set or clear this attribute.

使用之前创建的 ext4 文件系统镜像做一个测试:

```console
$ imgeditor leaf.ext4
ext2: ext filesystem image
|-- #00000011: File       abc 4 bytes

$ imgeditor leaf.ext4 -- inode 11
mode                          : 0x81a4 (File 644)
uid                           : 1000
size                          : 4
atime                         : 2022-11-25T18:05:53
ctime                         : 2022-11-25T18:05:53
mtime                         : 2022-11-25T18:05:53
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
ee_start_lo                   : 1101
version                       : 0xc8fea1e9
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

可以看到只有一个文件, 其 inode 编号是 11, 具有 EXT4_NOATIME_FL EXT4_EXTENTS_FL
两个标志.

当给文件 abc 添加 'i' 属性之后, 文件变得不可写:

```console
$ sudo chattr +i abc
$ echo x > abc
zsh: operation not permitted: abc
$ sudo sh -c 'echo x > abc'
sh: abc: Operation not permitted
$ sudo rm abc
rm: cannot remove 'abc': Operation not permitted
```

保存文件之后, 再次查看 inode 11 的信息:

```console
$ imgeditor leaf.ext4 -- inode 11
mode                          : 0x81a4 (File 644)
uid                           : 1000
size                          : 4
atime                         : 2022-11-25T18:05:53
ctime                         : 2022-11-25T18:07:58
mtime                         : 2022-11-25T18:05:53
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 1
blockcnt                      : 8
flags                         : 0x00080090 ( EXT4_IMMUTABLE_FL EXT4_NOATIME_FL EXT4_EXTENTS_FL )
osd1                          : 0x00000001
eh_magic                      : 0xf30a
eh_entries                    : 1
eh_max                        : 4
eh_depth                      : 0
eh_generation                 : 0
ee_block                      : 0
ee_len                        : 1
ee_start_hi                   : 0
ee_start_lo                   : 1101
version                       : 0xc8fea1e9
acl                           : 0
size_high                     : 0
fragment_addr                 : 0
osd2                          : 0x00000000 0x00000000 0x00000000
```

可以看到 `chattr +i` 之后, inode 中增加了 EXT4_IMMUTABLE_FL 标志用于指示文件是
不可改变的.

# 链接

在 [link](./link.md) 中解释了软链接和硬链接在文件系统中的存储方式. chattr manpage
中有这么一段话:

> no link can be created to this file

这个地方的 link 应该指的是硬链接, 而不是软链接. 因为软链接有自己的 inode, 硬链接
是将原文件 inode 中 nlinks 字段加一. 而 'chattr +i' 只是保护文件的 inode 及内容.
做个实验验证下:

```console
$ ln -s abc symlink_of_abc
$ ls -l
total 4
-rw-r--r-- 1 qianfan qianfan 4 Nov 25 18:05 abc
lrwxrwxrwx 1 qianfan qianfan 3 Nov 25 18:23 symlink_of_abc -> abc
$
$ ln abc hardlink_of_abc
ln: failed to create hard link 'hardlink_of_abc' => 'abc': Operation not permitted
$ sudo ln abc hardlink_of_abc
ln: failed to create hard link 'hardlink_of_abc' => 'abc': Operation not permitted
```
