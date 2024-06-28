#!/bin/bash
# Test unit for u-boot mkimage tools
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

FIT_IMAGE_GEN=${TEST_TMPDIR}/fit_image_gen
mkdir -p ${FIT_IMAGE_GEN}

# setup materials
cp -v image.its ${FIT_IMAGE_GEN}
gen_random_file ${FIT_IMAGE_GEN}/kernel.bin     10000000 20000000
gen_random_file ${FIT_IMAGE_GEN}/ramdisk.bin    10000000 20000000

# generate
(
    cd ${FIT_IMAGE_GEN}
    mkimage -E -p 0x1000 -B 0x200 -f image.its ${TEST_TMPDIR}/1.img || exit $?
) || exit $?

# make sure imgeditor can detect it
assert_imgeditor_successful --type fit ${TEST_TMPDIR}/1.img || exit $?

# and then unpack it.
assert_imgeditor_successful \
    --type fit --unpack ${TEST_TMPDIR}/1.img \
    || exit $?

# compare materials
FIT_IMAGE_GEN_DUMP=${TEST_TMPDIR}/1.img.dump
for file in image.its kernel.bin ramdisk.bin ; do
    assert_fileeq ${FIT_IMAGE_GEN}/${file} \
                  ${FIT_IMAGE_GEN_DUMP}/${file} \
                  "${file} is differ" \
                  || exit $?
done
