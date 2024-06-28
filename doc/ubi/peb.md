UBI PEB/LEB
===========

UBI 文件系统中按照块对数据进行管理, 块是底层 flash 的最小擦除单位, 这个数值在文件系统
制作的时候指定, 这个最小擦除单位被称为 PEB.

根据 flash 的特性, 编程时是按照页进行编程的, 典型的页大小是 2K. UBI 在设计时, 每一个
PEB 上面都需要存储两组独立的数据, 一组叫 ec_hdr, 另一组叫 vid_hdr. 这两组数据分别
占用一页. 将 PEB 去掉这两个页之后的数据成为 LEB.

一片 128MiB, 块大小 128KiB, 页大小是 2KiB 的 nand flahs, 共有 1024 个 PEB, 也有
1024 个 LEB, 只不过 LEB 的大小是 124KiB. LEB 不是按照顺序排列的, 它跟 PEB 存在一个
无序的映射关系.

下面的讨论中使用自动测试生成的镜像 `tests/fs/ubi/simple_abc.ubi` 进行讨论.
ubi 镜像制作的过程如下:

```shell
#!/bin/sh
# total size: 16MiB
# pagesize: 2K
# peb size: 128K(0x20000)
# leb size: 128K - 2K * 2 = 124K(0x1f000)
# peb count: 16MiB / 128KiB = 128

cat << EOF > ubinize.cfg
[ubifs]
mode=ubi
vol_id=0
vol_type=dynamic
vol_name=rootfs
vol_alignment=1
vol_size=16MiB
image=rootfs.ubifs
EOF

cat << EOF > fakeroot.sh
#!/bin/sh
mkfs.ubifs -d ./rootfs -e 0x1f000 -c 100 -m 2048 -x lzo -F -o rootfs.ubifs
ubinize -o rootfs.ubi -m 2048 -p 0x20000 -s 2048  ./ubinize.cfg
EOF

chmod a+x ./fakeroot.sh
fakeroot -- ./fakeroot.sh
retval=$?

# cleanup
rm ubinize.cfg fakeroot.sh
exit $retval
```

ubi 会记录上面提供的信息到 super block:

```console
$ ./imgeditor tests/fs/ubi/simple_abc.ubi -v -- node sblock
Node offset                   : 0x00000000
magic                         : 0x06101831(UBI NODE)
crc                           : 0x93c7e973
sqnum                         : 19
len                           : 4096
node_type                     : 0x06 (superblock node)
group_type                    : 0x00 (this node is not part of a group)
    key_hash                  : 0
    key_fmt                   : 0
    flags                     : 0x00000004 ( SPACE_FIXUP )
    min_io_size               : 2048
    leb_size                  : 126976
    leb_cnt                   : 13
    max_leb_cnt               : 100
    max_bud_bytes             : 1396736
    log_lebs                  : 4
    lpt_lebs                  : 2
    orph_lebs                 : 1
    jhead_cnt                 : 1
    fanout                    : 8
    lsave_cnt                 : 256
    fmt_version               : 4
    default_compr             : 1
    rp_uid                    : 0
    rp_gid                    : 0
    rp_size                   : 0
    time_gran                 : 1000000000
    uuid                      : b6 6b 77 47 91 07 48 b7 85 df 17 60 1d 9f ef b7
    ro_compat_version         : 0
    hmac                      : 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    hmac_wkm                  : 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    hash_algo                 : 0
    hash_mst                  : 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
```

PEB 和 LEB 的编号都是从 0 开始计数的.
UBI 保留了 PEB0 和 PEB1 用于保存 vtbl(a record in the volume table), 这两个 PEB 中
的内容相同, 互为备份, 并且不参与与 LEB 的映射.

```console
$ ./imgeditor tests/fs/ubi/simple_abc.ubi -- vtbl
volume table record #0:
    reserved_pebs             : 133
    alignment                 : 0x00000001
    data_pad                  : 0x00000000
    vol_type                  : 0x01
    upd_marker                : 0x00
    name_len                  : 10
    name                      : simple_abc
    flags                     : 0
    crc                       : 0xac3b4463
```

# 映射

在上面的介绍中, 说明了每一个 PEB 都有两个页的记录, 保存 ec_hdr 和 vid_hdr. 其中
vid_hdr 信息中的 `lnum` 字段表明了当前 PEB 对应的 LEB 的编号.

例如, 从下面的例子中可以看出, PEB7 对应了 LEB5.

```console
$ ./imgeditor tests/fs/ubi/simple_abc.ubi -- leb 5
EC_HDR of PEB#7:
    magic                     : 0x55424923(EC)
    version                   : 0x01
    ec                        : 0x0000000000000000
    vid_hdr_offset            : 0x00000800
    data_offset               : 0x00001000
    image_seq                 : 0x5548f4c8
    hdr_crc                   : 0xd499e43f
VID_HDR of PEB#7:
    magic                     : 0x55424921(VID)
    version                   : 0x01
    vol_type                  : 0x01
    copy_flag                 : 0x00
    compat                    : 0x00
    vol_id                    : 0x00000000
    lnum                      : 5
    data_size                 : 0
    used_ebs                  : 0
    data_pad                  : 0
    data_crc                  : 0x00000000
    sqnum                     : 0
    hdr_crc                   : 0x33ec28e8
00000000 ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff |................|
*
0001f000
```
