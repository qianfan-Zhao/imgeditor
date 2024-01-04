sys partition
=============

使用解包工具拆解全志的刷机包, 可以从里面找到 sys_partition.fex 这个文件. 他是用来描述
存储设备分区信息的.

```ini
[mbr]
size = 16384

[partition_start]

[partition]
        name         = boot-resource
        size         = 14336
        downloadfile = "boot-resource.fex"
        user_type    = 0x8000

[partition]
        name         = env
        size         = 8192
        downloadfile = "env.fex"
        user_type    = 0x8000
```

其中 size 的单位是 block, 每个 block 为 512 字节. 所以上面的 env 分区实际上的大小是
8192 * 512 = 4096KB = 4MiB.

# sunxi mbr

全志没有使用 MBR, 或者 GPT 这种标准的分区格式, 而是自己创造了一个叫 sunxi_mbr 的分区
格式, sunxi_mbr.fex 是在打包阶段由 sys_partition.fex 文件转换生成的, 并烧录到存储
器的 0 地址处, 供 linux 启动之后进行解析使用.

可以使用 imgeditor 查看 sunxi_mbr 的内容:

```console
$ imgeditor sunxi_mbr.fex
sunxi_mbr: allwinner sunxi_mbr image editor
        offset     size name             mmcblk
[01]:    16MiB     7MiB boot-resource    mmcblk0p2
[02]:    23MiB     4MiB env              mmcblk0p5
[03]:    27MiB    32MiB boot             mmcblk0p6
...
[16]:  7339MiB     0B   UDISK            mmcblk0p1
```

全志内部定义了一种规则, sunxi_mbr 中最后一个分区映射成 mmcblk0p1, 第一个分区映射成
mmcblk0p2, 从第二个分区开始从 mmcblk0p5 开始递加. 这个分区映射格式可以在 imgeditor
的输出中找到.

# dlinfo

sunxi_mbr.fex 是直接烧录到存储芯片中的, 供 linux 启动之后使用. 并且其中没有指定烧写
哪个文件到哪个分区, 对下载软件 (PhoenixSuit) 而言是无法直接使用的. 所以全志的打包
脚本在通过 sys_partition.fex 生成 sunxi_mbr.fex 的同时, 会自动生成一个名为
dlinfo.fex 的文件, 这个文件是共下载软件 (PhoenixSuit) 使用的, 里面包含分区的地址,
下载文件, 是否需要校验等信息.

dlinfo 的校验类型与 sunxi_mbr 相同. imgeditor 无法自动区分这两个文件. 因此 imgeditor
不会自动检测文件为 dlinfo, 需要传递参数指定:

```console
$ imgeditor --type sunxi_dlinfo dlinfo.fex
sunxi_dlinfo: allwinner dlinfo.fex image editor
version                 : 512
magic                   : softw411
download_counts         : 14
Downloads:
    addr                : 0x00008000(   16MiB)
    len                 : 0x00003800(    7MiB)
    download_filename   : BOOT-RESOURCE_FE
    verify_filename     : VBOOT-RESOURCE_F
    encrypt             : 0
    verify              : 1

    addr                : 0x0000b800(   23MiB)
    len                 : 0x00002000(    4MiB)
    download_filename   : ENV_FEX000000000
    verify_filename     : VENV_FEX00000000
    encrypt             : 0
    verify              : 1
...
```

估计是因为历史兼容的原因, dlinfo 中的 download_filename 字段只保留了 16 个字符.
全志设计了一个转换规则, 用于将原始的文件名为转换成 download_filename 中的名字.
长名字超出的部分被截断.

# 使用 imgeditor 自动生成 sunxi_mbr 和 dlinfo

下面的代码会将 sys_partition.fex 转换成 new_sunxi_mbr.fex 和
new_sunxi_mbr.fex.dlinfo. 可以看到转换之后的文件与全志打包工具生成的文件完全一致.

```console
$ imgeditor --type sunxi_mbr --pack sys_partition.fex new_sunxi_mbr.fex
$
$ sha1sum new_sunxi_mbr.fex
395f5f965c4a31ebc593facb0cece59301652ea4  new_sunxi_mbr.fex
$ sha1sum new_sunxi_mbr.fex.dlinfo
dd8aafea1fdb1dc5afe373f9adcf0f3338a43384  new_sunxi_mbr.fex.dlinfo

$ sha1sum sunxi_mbr.fex
395f5f965c4a31ebc593facb0cece59301652ea4  sunxi_mbr.fex
$ sha1sum dlinfo.fex
dd8aafea1fdb1dc5afe373f9adcf0f3338a43384  dlinfo.fex
```
