#!/bin/sh
# Test unit for allwinner sysconfig image.
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

# ini to fex
assert_imgeditor_successful \
        --type sysconfig --pack config.ini ${TEST_TMPDIR}/config.fex \
        || exit $?

assert_sha1sum ${TEST_TMPDIR}/config.fex \
        49c6bb15d9378e9c28b15e864f680a60a13ade22 || exit $?

# fex to ini
assert_imgeditor_successful \
        --unpack ${TEST_TMPDIR}/config.fex \
        ${TEST_TMPDIR}/1.ini \
        || exit $?

assert_sha1sum ${TEST_TMPDIR}/1.ini \
        01f1fe7f89e97dc2c8b5ca776f042b6da82d0b31 \
        || exit $?
