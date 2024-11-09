#!/bin/bash
# Test unit for ext4 filesystem.
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

function squashfs_no_compression() {
    if [ X"${MKSQUASHFS_NO_COMPRESSION}" = X1 ] ; then
        return 0
    fi

    return 1
}

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

function gen_squashfs() {
    local dir=$1

    shift

    mksquashfs ${dir} ${dir}.squashfs ${MKSQUASHFS_COMMON_ARGS} "$@"
    assert_success "Create ${dir}.squashfs failed"
}

# $1: unit name
# $2: image size
function imgeditor_unpack_squashfs_test() {
    local unit=$1
    local dir=${TEST_TMPDIR}/${unit}

    # create the basic directory and generate ext image
    mkdir -p ${dir}
    ${unit} ${dir}

    gen_squashfs ${dir}

    # check the search feature
    assert_imgeditor_successful -s ${dir}.squashfs || exit $?

    # make sure imgeditor can read the filesystem meta data
    if squashfs_no_compression ; then
        for cmd in sblock inodes dirs fragments ; do
            assert_imgeditor_successful ${dir}.squashfs -- $cmd || exit $?
        done
    fi

    # unpack and compare
    assert_imgeditor_successful --unpack ${dir}.squashfs || exit $?
    assert_direq ${dir}.squashfs.dump ${dir} || exit $?
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

imgeditor_unpack_squashfs_test simple_abc || exit $?
imgeditor_unpack_squashfs_test file_unaligned || exit $?
imgeditor_unpack_squashfs_test simple_link || exit $?
imgeditor_unpack_squashfs_test many_files || exit $?
imgeditor_unpack_squashfs_test many_longname_files || exit $?
imgeditor_unpack_squashfs_test long_link_target_name || exit $?
