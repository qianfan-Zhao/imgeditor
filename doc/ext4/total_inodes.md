# total_inodes

创建一个 16M 大小的 ext4 文件镜像, 并在主机上挂载. 为了方便操作, 将挂载之后的文件夹
的所属者改为当前使用的账户.

```console
$ make_ext4fs -l 16MiB total_inodes.ext4 total_inodes
$ sudo mount -o loop total_inodes.ext4 mnt
$ sudo chown qianfan:qianfan mnt
```

chown 之后可以看到 ext4 root inode(2) 中的 uid 和 gid 已经变成了当前使用的账户:

```console
imgeditor total_inodes.ext4 -- inode 2
ext2: ext2 image editor
mode                          : 0x41ed (Directory 755)
uid                           : 1000
size                          : 16384
atime                         : 1970-01-01T08:00:00
ctime                         : 2022-11-17T08:48:07
mtime                         : 2022-11-17T08:48:07
dtime                         : 1970-01-01T08:00:00
gid                           : 1000
nlinks                        : 6
blockcnt                      : 32
```

通过查看 super block 的信息, 发现 total_inodes 是 1024.

```console
$ imgeditor total_inodes.ext4 -- sblock
ext2: ext2 image editor
total_inodes                      : 1024
total_blocks                      : 4096
reserved_blocks                   : 0
free_blocks                       : 2994
free_inodes                       : 1013
first_data_block                  : 0
log2_block_size                   : 2
log2_fragment_size                : 2
blocks_per_group                  : 32768
fragments_per_group               : 32768
inodes_per_group                  : 1024
```

挂载之后, 在文件系统中创建三个文件夹:

```console
$ mkdir a b c
```

在 a 文件夹下创建大量文件:

```console
$ cd a
$ for i in $(seq 1 4096) ; do if ! dd if=/dev/urandom of=$i.bin bs=1K count=1 ; then break; fi; done
...
1024 bytes (1.0 kB, 1.0 KiB) copied, 3.8e-05 s, 26.9 MB/s
1+0 records in
1+0 records out
1024 bytes (1.0 kB, 1.0 KiB) copied, 3.65e-05 s, 28.1 MB/s
dd: failed to open '1011.bin': No space left on device
$ sync
```

在文件系统中, 文件是按照 blocksize 进行对齐的, 每个文件实际占用的空间是 4K,
当写入到 1011 个文件的时候, 提示 No space left, 按照计算, 这时候写入的数据量
应该是 4M 左右, 使用 df 查看与计算结果一致.

```console
$ df -h
/dev/loop0       12M  4.1M  7.4M  36% /home/.../mnt
```

查看整个文件系统的 inode 信息:

```console
$ imgeditor total_inodes.ext4
ext2: ext2 image editor
|-- #00000011: Directory  lost+found
|-- #00000012: Directory  a
|   |-- #00000015: File       1.bin 1024 bytes
|   |-- #00000016: File       2.bin 1024 bytes
|   |-- #00000017: File       3.bin 1024 bytes
|   |-- ...
|   |-- #00001023: File       1009.bin 1024 bytes
|   |-- #00001024: File       1010.bin 1024 bytes
|-- #00000013: Directory  b
|-- #00000014: Directory  c
```

可以看到整个文件系统中最大的 inode 是 1024. 虽然磁盘还有大量的空间, 但是我们无法再
继续创建任何文件, 因为 inode 的数量已经达到最大值:

```console
$ echo abc > mnt/abc.bin
zsh: no space left on device: mnt/abc.bin
```
