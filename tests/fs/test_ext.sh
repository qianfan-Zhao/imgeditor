#!/bin/bash
# Test unit for ext4 filesystem.
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

# the default FSTYPE is ext4
 : ${FSTYPE:="ext4"}

# generate system image by `mkfs.ext4 -t ext2` to test the ext2 compatible.
function gen_ext2fs() {
    local dir=$1 img_size=$2

    shift 2

    dd if=/dev/zero of=${dir}.ext2 bs=${img_size} count=1 status=none
    mkfs.ext2 -d ${dir} ${dir}.ext2 "$@"
    assert_success "Create ${dir}.ext2 failed"
}

function gen_ext4fs() {
    local dir=$1 img_size=$2

    shift 2

    dd if=/dev/zero of=${dir}.ext4 bs=${img_size} count=1 status=none
    mkfs.ext4 -d ${dir} ${dir}.ext4 "$@"
    assert_success "Create ${dir}.ext4 failed"
}

# $1: unit name
# $2: image size
function imgeditor_unpack_ext4_test() {
    local unit=$1 img_size=$2
    local dir=${TEST_TMPDIR}/${unit}

    # create the basic directory and generate ext image
    mkdir -p ${dir}
    ${unit} ${dir}

    case ${FSTYPE} in
        ext2)
            gen_ext2fs ${dir} ${img_size} || exit $?
            ;;
        ext4)
            gen_ext4fs ${dir} ${img_size} || exit $?
            ;;
    esac

    # make sure we can read it
    assert_imgeditor_successful ${dir}.${FSTYPE} || exit $?

    # unpack and compare
    assert_imgeditor_successful --unpack ${dir}.${FSTYPE} || exit $?
    assert_direq ${dir}.${FSTYPE}.dump ${dir} || exit $?

    # unpack at offset
    dd if=/dev/zero of=${dir}_1M.${FSTYPE} bs=1M cont=1
    dd if=${dir}.${FSTYPE} of=${dir}_1M.${FSTYPE} bs=1M seek=1
    assert_imgeditor_successful --offset 1048576 --unpack ${dir}_1M.${FSTYPE} || exit $?
    assert_direq ${dir}_1M.${FSTYPE}.dump ${dir} || exit $?
}

function simple_abc() {
    (
        cd $1
        mkdir -p a/b
        mkdir -p a/c
        echo "filepath: a/b, filename: ab" > a/b/ab
    )
}

function file_unaligned() {
    (
        cd $1

        dd if=/dev/urandom of=a bs=34 count=1
        dd if=/dev/urandom of=b bs=4097 count=1
        dd if=/dev/urandom of=c bs=8193 count=1
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

function symlink_60() {
    local len59=12345678901234567890123456789012345678901234567890123456789
    local len60=${len59}0
    local len61=${len60}a

    (
        cd $1

        touch ${len59} ${len60} ${len61}

        ln -s ${len59} a59
        ln -s ${len60} a60
        ln -s ${len61} a61
    )
}

function large_file() {
    (
        cd $1

        dd if=/dev/urandom of=1.bin bs=1M count=40
    )
}

imgeditor_unpack_ext4_test simple_abc 16MiB || exit $?
imgeditor_unpack_ext4_test file_unaligned 16MiB || exit $?
imgeditor_unpack_ext4_test many_files 16MiB || exit $?
imgeditor_unpack_ext4_test many_longname_files 64MiB || exit $?
imgeditor_unpack_ext4_test simple_link 16MiB || exit $?
imgeditor_unpack_ext4_test symlink_60 16MiB || exit $?
imgeditor_unpack_ext4_test long_link_target_name 16MiB || exit $?
imgeditor_unpack_ext4_test large_file 64MiB || exit $?
