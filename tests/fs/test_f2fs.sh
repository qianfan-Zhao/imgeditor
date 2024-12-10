#!/bin/bash
# Test unit for f2fs filesystem.
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

# assert_direq(dir1, dir2, [fail_msg])
function assert_direq() {
    if diff -r "$1" "$2" \
        --exclude "lost+found" \
        --exclude ".imgeditor"; then
        return 0
    fi

    log:error_default "$3" "directory $1 and $2 are differ"
    print_bash_error_stack
    return 1
}

function gen_f2fs() {
    local dir=$1 img_size=$2

    shift 2

    dd if=/dev/zero of=${dir}.f2fs bs=${img_size} count=1 status=none
    mkfs.f2fs ${MKFS_F2FS_OPTIONS} ${dir}.f2fs > /dev/null
    assert_success "format ${dir}.f2fs failed" || return $?
    sload.f2fs -f ${dir} ${dir}.f2fs "$@" > /dev/null
    assert_success "Create ${dir}.f2fs failed"
}

# $1: unit name
# $2: image size
function imgeditor_unpack_test() {
    local unit=$1 img_size=$2
    local dir=${TEST_TMPDIR}/${unit}

    if [ -z "$img_size" ] ; then
        img_size=64M
    fi

    # create the basic directory and generate ext image
    mkdir -p ${dir}
    ${unit} ${dir}

    gen_f2fs ${dir} ${img_size} || exit $?

    # check list function
    assert_imgeditor_successful ${dir}.f2fs || exit $?

    # unpack and compare
    assert_imgeditor_successful --unpack ${dir}.f2fs || exit $?
    assert_direq ${dir}.f2fs.dump ${dir} || exit $?
}

function simple_abc() {
    (
        cd $1

        for dir in a b c ; do
            filename="$dir$dir"

            mkdir -p $dir
            echo "file name $filename in $dir" > $dir/$filename
        done
    )
}

function longname() {
    (
        cd $1

        # f2fs_dentry_blocks 中的 filename_slot 长度是 8 字节.
        # 当文件名长度超过 8 字节之后, 会占用多个槽位.
        for name in 1 12 123 1234 123456 1234567 12345678 123456789 looooooooooooooooooooooooooooooooooooooog oo ; do
            echo "file name $name in $name" > $name
        done
    )
}

function inline_data() {
    (
        cd $1

        # 当数据比较短时, 会使用 inline 的方式直接存储到 inode 中.
        for size in $(seq 1 4096) ; do
            dd if=/dev/urandom of=$size.bin bs=$size count=1 &> /dev/null
        done
    )
}

function file_unaligned() {
    (
        cd $1

        dd if=/dev/urandom of=a bs=34 count=1
        dd if=/dev/urandom of=b bs=4097 count=1
        dd if=/dev/urandom of=c bs=8193 count=1
        dd if=/dev/urandom of=c bs=16385 count=1
    )
}

function extension() {
    (
        cd $1

        for e in jpg gif png avi divx mp4 mp3 3gp wmv wma mpeg mkv mov asx asf wmx svi wvx wm mpg mpe rm ogg jpeg video apk so db ; do
            gen_random_file_silence 1.$e 1024 16384
        done
    )
}

function many_files() {
    (
        cd $1

        mkdir -p a b c
        for i in $(seq 1 1000) ; do
            gen_random_file_silence a/$i.bin 1024
        done
    )
}

function many_longname_files() {
    (
        cd $1

        mkdir -p a b c
        for i in $(seq 1 1000) ; do
            gen_random_file_silence \
                a/very_lonoooooooooooooooooooooooooooooooooooooooooog_name_$i.bin \
                1024
        done
    )
}

function simple_link() {
    (
        cd $1

        echo "this is file a" > a
        ln -s a symlink_of_a
        ln a hardlink_of_a
    )
}

function long_link_target_name() {
    local long_name=very_loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong_file
    (
        cd $1

        echo "abc" > ${long_name}
        ln -s ${long_name} symlink_of_long_file
    )
}

function large_file() {
    (
        cd $1

        # 小于 3.6M 的文件可以不用 direct block 存储. 超过 3.6M 的文件会使用
        # direct/indirect/double_indirect 存储.
        #
        # direct 使用 i_nid 来索引 block 编号. 一个 block 中最大能存储 455 个
        # f2fs_nat_entry 结构, 在前面创建大量的空文件占用 nat 中的结构, 造成
        # large.bin 中引用的 i_nid 跨页测试 @f2fs_get_nat_entry.
        for i in $(seq 1 500) ; do
            touch $i
        done

        dd if=/dev/urandom of=large.bin bs=1K count=102400
    )
}

imgeditor_unpack_test simple_abc || exit $?
imgeditor_unpack_test longname || exit $?
imgeditor_unpack_test inline_data 256M || exit $?
imgeditor_unpack_test file_unaligned || exit $?
imgeditor_unpack_test large_file 256M || exit $?
imgeditor_unpack_test extension || exit $?
imgeditor_unpack_test many_files || exit $?
imgeditor_unpack_test many_longname_files || exit $?
imgeditor_unpack_test simple_link || exit $?
imgeditor_unpack_test long_link_target_name || exit $?
