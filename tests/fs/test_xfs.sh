#!/bin/bash
#
# Test unit for xfs filesystem.
# I can not found a tools like sload.f2fs which can put some files to image.
# So we need root permission to create a xfs image and this script must run
# as root.
#
# Usage:
# $ sudo ctest --rerun-failed --output-on-failure -R xfs
#
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

if [ "$EUID" -ne 0 ]; then
    echo "This test must be run as root"
    exit 0
fi

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

function gen_xfs() {
    local dir=$1 img_size=$2

    shift 2

    dd if=/dev/zero of=${dir}.xfs bs=${img_size} count=1 status=none
    mkfs.xfs ${MKFS_XFS_OPTIONS} ${dir}.xfs > /dev/null
    assert_success "format ${dir}.xfs failed" || return $?

    mkdir ${dir}.mount
    mount -o loop ${dir}.xfs ${dir}.mount || return $?
    cp ${dir}/* ${dir}.mount
    umount ${dir}.mount
}

# $1: unit name
# $2: image size
function imgeditor_unpack_test() {
    local unit=$1 img_size=$2
    local dir=${TEST_TMPDIR}/${unit}

    if [ -z "$img_size" ] ; then
        img_size=16M
    fi

    # create the basic directory and generate ext image
    mkdir -p ${dir}
    ${unit} ${dir}

    gen_xfs ${dir} ${img_size} || exit $?
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

        for name in 1 12 123 1234 123456 1234567 12345678 123456789 looooooooooooooooooooooooooooooooooooooog oo ; do
            echo "file name $name in $name" > $name
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

imgeditor_unpack_test simple_abc || exit $?
imgeditor_unpack_test longname || exit $?
imgeditor_unpack_test file_unaligned || exit $?
imgeditor_unpack_test many_files || exit $?
imgeditor_unpack_test many_longname_files || exit $?
imgeditor_unpack_test simple_link || exit $?
imgeditor_unpack_test long_link_target_name || exit $?
