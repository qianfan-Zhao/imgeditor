#!/bin/bash
# Test unit for android sparse images
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

function sparse_pack_api_test() {
    local filename=${TEST_TMPDIR}/$1
    local sha=$2

    assert_imgeditor_successful --type asparse -- gentest ${filename} || exit $?
    assert_imgeditor_successful ${filename} || exit $?
    assert_sha1sum ${filename} ${sha}
}

# generate system image by `mkfs.ext2 -t ext2` to test the ext2 compatible.
function gen_ext2fs() {
    local dir=$1 img_size=$2

    shift 2

    dd if=/dev/zero of=${dir}.ext2 bs=${img_size} count=1 status=none
    mkfs.ext2 -d ${dir} ${dir}.ext2 "$@"
    assert_success "Create ${dir}.ext2 failed"
}


# $1: unit name
# $2: image size
function imgeditor_unpack_ext4_simg_test() {
    local unit=$1 img_size=$2
    local dir=${TEST_TMPDIR}/${unit}

    # create the basic directory and generate ext image
    mkdir -p ${dir}
    ${unit} ${dir}
    gen_ext2fs ${dir} ${img_size} || exit $?

    img2simg ${dir}.ext2 ${dir}.simg || exit $?

    # make sure we can read it
    assert_imgeditor_successful ${dir}.simg || exit $?

    # unpack and compare
    assert_imgeditor_successful --unpack ${dir}.simg ${dir}.bin || exit $?
    assert_fileeq ${dir}.ext2 ${dir}.bin || exit $?
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
            gen_random_file_silence a/$i.bin 1024
        done
    )
}

sparse_pack_api_test 1.simg 426e9a5ce8adb8c1983ff189168fbbbfdae4b90d || exit $?

imgeditor_unpack_ext4_simg_test simple_abc 16MiB || exit $?
imgeditor_unpack_ext4_simg_test many_files 16MiB || exit $?
