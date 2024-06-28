#!/bin/sh
# Test unit for U-Boot env file.
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

function imgeditor_uenv_pack_unpack_test() {
    local env_filename=$1 env_bin_hash=$2 move_offset=$3
    local bin_name=${env_filename} unpack_ext_args

    if [ -n "${move_offset}" ] ; then
        unpack_ext_args="--offset ${move_offset}"
        bin_name=${env_filename}-${move_offset}
    fi

    # txt to bin
    assert_imgeditor_successful \
            --type uenv --pack ${env_filename}.txt \
            ${TEST_TMPDIR}/${bin_name}.bin || exit $?
    assert_sha1sum ${TEST_TMPDIR}/${bin_name}.bin ${env_bin_hash} || exit $?

    # move the binary file to another location
    if [ -n "${move_offset}" ] ; then
        gen_random_file ${TEST_TMPDIR}/r1 ${move_offset}

        cat ${TEST_TMPDIR}/r1 \
            ${TEST_TMPDIR}/${bin_name}.bin \
            ${TEST_TMPDIR}/r1 > ${TEST_TMPDIR}/tmp
        mv ${TEST_TMPDIR}/tmp ${TEST_TMPDIR}/${bin_name}.bin
    fi

    # bin to txt
    # 'uenv' doesn't provide '--unpack' method, but it write unpack
    # result to stdout.
    assert_imgeditor_successful ${unpack_ext_args} \
            --type uenv ${TEST_TMPDIR}/${bin_name}.bin || exit $?
    assert_fileeq ${env_filename}.txt \
            ${TEST_TMPDIR}/imgeditor-stdio.txt || exit $?
}

imgeditor_uenv_pack_unpack_test \
    env afa01258d70e2ca4e1d5e1f4c12704e33f513ba9 || exit $?

imgeditor_uenv_pack_unpack_test \
    env afa01258d70e2ca4e1d5e1f4c12704e33f513ba9 9999 || exit $?

