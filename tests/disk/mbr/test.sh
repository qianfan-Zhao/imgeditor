#!/bin/bash
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

fdisk_mbr_test() {
    local name=$1 mbr_sha1sum=$2

    xxd_reverse ./${name}.txt ${TEST_TMPDIR} || return $?

    # make sure imgeditor can detect it
    assert_imgeditor_successful ${TEST_TMPDIR}/${name}.bin || return $?
    cp ${TEST_TMPDIR}/imgeditor-stdio.txt ${TEST_TMPDIR}/${name}.fdisk
    assert_sha1sum ${TEST_TMPDIR}/${name}.fdisk ${mbr_sha1sum} || return $?
}

fdisk_mbr_test 1 2d93c3abb967f85b67e4ec638b384a9f88df9c2f || exit $?
