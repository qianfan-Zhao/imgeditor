#!/bin/bash
# android dtimg test script

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

generate_dtbo() {
    local outdir=$1

    mkdir -p $outdir
    for dts in board1v1.dts board1v1_1.dts board2v1.dts ; do
        dtc -O dtb --symbols -qq -a 4 -o $outdir/${dts}.dtb $dts || exit $?
    done
}

generate_dtbo ${TEST_TMPDIR}/1 || exit $?
cp mkdtimg.cfg ${TEST_TMPDIR}/1/dtboimg.cfg || exit $?
assert_imgeditor_successful --type adtimg --pack ${TEST_TMPDIR}/1 ${TEST_TMPDIR}/1.img || exit $?

# make sure imgeditor can detect and search it
assert_imgeditor_successful -s ${TEST_TMPDIR}/1.img || exit $?
assert_imgeditor_successful ${TEST_TMPDIR}/1.img || exit $?

# the last line is the size of dtb, it may not a const number.
# let's remove it.
#
# $ ./imgeditor tests/android/dtimg/1.img
# adtimg: android mkdtimg editor
# idx   id         rev        custom                                    size
# [00]: 0x00010000 0x00010000 0x00000abc 0x00000000 00000000 0x00000000 380
# [01]: 0xaabbccdd 0x00010001 0x00000abc 0x00000000 00000000 0x00000000 412
# [02]: 0x00020000 0x00000201 0x00000abc 0x00000000 00000000 0x00000000 380
# [03]: 0x00010000 0x00010000 0x00000def 0x00000000 00000000 0x00000000 380
# [04]: 0x00010000 0x00010000 0x00000dee 0x00000000 00000000 0x00000000 380
awk -i inplace '{$NF=""; print $0}' ${TEST_TMPDIR}/imgeditor-stdio.txt
assert_sha1sum \
        ${TEST_TMPDIR}/imgeditor-stdio.txt \
        1c9ecbe6d83915543207cd23281d0d73510eb297 || exit $?

# test unpack
assert_imgeditor_successful --unpack ${TEST_TMPDIR}/1.img || exit $?
assert_imgeditor_successful --pack ${TEST_TMPDIR}/1.img.dump ${TEST_TMPDIR}/2.img || exit $?
assert_fileeq ${TEST_TMPDIR}/1.img ${TEST_TMPDIR}/2.img || exit $?
