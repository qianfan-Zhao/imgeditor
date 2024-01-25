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

# $1:    directory
# $2:    image size
# $3...: other params passed to `make_ext4fs`
function gen_ext4fs() {
    local dir=$1 img_size=$2

    shift 2

    make_ext4fs "$@" -l ${img_size} ${dir}.ext4 ${dir}
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
    gen_ext4fs ${dir} ${img_size} || exit $?

    # make sure we can read it
    assert_imgeditor_successful ${dir}.ext4 || exit $?

    # unpack and compare
    assert_imgeditor_successful --unpack ${dir}.ext4 || exit $?
    assert_direq ${dir}.ext4.dump ${dir} || exit $?
}

function simple_abc() {
    (
        cd $1
        mkdir -p a/b
        mkdir -p a/c
        echo "filepath: a/b, filename: ab" > a/b/ab
    )
}

function many_files() {
    (
        cd $1

        mkdir -p a b c
        for i in $(seq 1 1000) ; do
            gen_random_file_silence $i.bin 1024
        done
    )
}

function many_longname_files() {
    (
        cd $1

        mkdir -p a b c
        for i in $(seq 1 500) ; do
            gen_random_file_silence \
                very_lonoooooooooooooooooooooooooooooooooooooooooog_name_$i.bin \
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
    local len60=123456789012345678901234567890123456789012345678901234567890
    local len61=${len60}a

    (
        cd $1

        touch ${len60} ${len61}

        ln -s ${len60} a60
        ln -s ${len61} a61
    )
}

imgeditor_unpack_ext4_test simple_abc 16MiB || exit $?
imgeditor_unpack_ext4_test many_files 16MiB || exit $?
imgeditor_unpack_ext4_test many_longname_files 16MiB || exit $?
imgeditor_unpack_ext4_test simple_link 16MiB || exit $?
imgeditor_unpack_ext4_test symlink_60 16MiB || exit $?
imgeditor_unpack_ext4_test long_link_target_name 16MiB || exit $?
