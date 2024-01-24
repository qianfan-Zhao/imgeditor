#!/bin/bash
# Test unit for gpt partition
# qianfan Zhao <qianfanguijin@163.com>
source "${CMAKE_SOURCE_DIR}/tests/common.sh"

# pack to binary
assert_imgeditor_successful \
        --type gpt --pack gpt.json ${TEST_TMPDIR}/gpt.bin \
        || exit $?

assert_sha1sum ${TEST_TMPDIR}/gpt.bin \
        524236099857f22faf5604672b63b2afe8f4f9ea || exit $?

# unpack it
assert_imgeditor_successful \
        --unpack ${TEST_TMPDIR}/gpt.bin \
        ${TEST_TMPDIR}/gpt.bin.json \
        || exit $?

assert_fileeq ${TEST_TMPDIR}/gpt.bin.json gpt.json || exit $?

# test the search function
# the gpt offset should be 512.
assert_imgeditor_successful -s ${TEST_TMPDIR}/gpt.bin || exit $?
assert_sha1sum ${TEST_TMPDIR}/imgeditor-stdio.txt \
        08bcf1387a0e85ac59b01d91374c2f2a6b22e978 \
        || exit $?

# Move this gpt file to another location.
gen_random_file ${TEST_TMPDIR}/offset 100000
cat ${TEST_TMPDIR}/gpt.bin >> ${TEST_TMPDIR}/offset

# the gpt's location should be 100000 + 512
assert_imgeditor_successful -s ${TEST_TMPDIR}/offset
assert_sha1sum ${TEST_TMPDIR}/imgeditor-stdio.txt \
        24ba3e559c46bf90b15feaa3cdcd5b4f6e597c37 \
        || exit $?
