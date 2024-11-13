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
    local dir=$1 img_size=64M

    shift 1

    dd if=/dev/zero of=${dir}.f2fs bs=${img_size} count=1 status=none
    mkfs.f2fs ${MKFS_F2FS_OPTIONS} ${dir}.f2fs
    assert_success "format ${dir}.f2fs failed" || return $?
    sload.f2fs -f ${dir} ${dir}.f2fs "$@"
    assert_success "Create ${dir}.f2fs failed"
}

# $1: unit name
# $2: image size
function imgeditor_unpack_test() {
    local unit=$1
    local dir=${TEST_TMPDIR}/${unit}

    # create the basic directory and generate ext image
    mkdir -p ${dir}
    ${unit} ${dir}

    gen_f2fs ${dir} ${img_size} || exit $?
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

imgeditor_unpack_test simple_abc || exit $?
imgeditor_unpack_test file_unaligned || exit $?
imgeditor_unpack_test many_files || exit $?
imgeditor_unpack_test many_longname_files || exit $?
imgeditor_unpack_test simple_link || exit $?
imgeditor_unpack_test long_link_target_name || exit $?
