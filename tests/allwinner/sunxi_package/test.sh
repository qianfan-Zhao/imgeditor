#!/bin/bash
# Test unit for allwinner sunxi-package image.
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

SUNXI_PACKAGE_GEN=${TEST_TMPDIR}/sunxi_package_gen
mkdir -p ${SUNXI_PACKAGE_GEN}

# setup materials
cp -v sunxi_package.json ${SUNXI_PACKAGE_GEN}
gen_random_file ${SUNXI_PACKAGE_GEN}/u-boot     800000 900000
gen_random_file ${SUNXI_PACKAGE_GEN}/scp        100000 200000
gen_random_file ${SUNXI_PACKAGE_GEN}/optee      200000 300000
gen_random_file ${SUNXI_PACKAGE_GEN}/soc-cfg     60000  80000
gen_random_file ${SUNXI_PACKAGE_GEN}/dtb        131072
gen_random_file ${SUNXI_PACKAGE_GEN}/logo       100000 200000

# generate
assert_imgeditor_successful \
    --type sunxi_package --pack ${SUNXI_PACKAGE_GEN} \
    ${TEST_TMPDIR}/sunxi_package.fex \
    || exit $?

# make sure imgeditor can detect it
assert_imgeditor_successful ${TEST_TMPDIR}/sunxi_package.fex || exit $?

# and then unpack it.
assert_imgeditor_successful \
    --unpack ${TEST_TMPDIR}/sunxi_package.fex \
    || exit $?

# compare materials
SUNXI_PACKAGE_FEX_DUMP=${TEST_TMPDIR}/sunxi_package.fex.dump
for file in sunxi_package.json u-boot scp optee soc-cfg dtb logo ; do
    assert_fileeq ${SUNXI_PACKAGE_GEN}/${file} \
                  ${SUNXI_PACKAGE_FEX_DUMP}/${file} \
                  "${file} is differ" \
                  || exit $?
done

# pack the dump again
assert_imgeditor_successful \
    --pack ${SUNXI_PACKAGE_FEX_DUMP} \
    ${TEST_TMPDIR}/1.bin || exit $?

assert_fileeq ${TEST_TMPDIR}/sunxi_package.fex ${TEST_TMPDIR}/1.bin || exit $?

# Appending sunxi_package.fex to a random file
# The offset of sunxi_package should be 100000(0x186a0)
gen_random_file ${SUNXI_PACKAGE_GEN}/offset 100000
cat ${TEST_TMPDIR}/sunxi_package.fex >> ${SUNXI_PACKAGE_GEN}/offset

assert_imgeditor_successful -s ${SUNXI_PACKAGE_GEN}/offset
assert_sha1sum ${TEST_TMPDIR}/imgeditor-stdio.txt \
        7b30b5990900cce6ee358a436d1af4a8c69f7934 \
        || exit $?

# unpack from the offset and check again
assert_imgeditor_successful --offset 100000 \
         --unpack ${SUNXI_PACKAGE_GEN}/offset \
         || exit $?

for file in sunxi_package.json u-boot scp optee soc-cfg dtb logo ; do
    assert_fileeq ${SUNXI_PACKAGE_GEN}/offset.dump/${file} \
                  ${SUNXI_PACKAGE_GEN}/${file} \
                  "${file} is differ" \
                  || exit $?
done

# this test script failed sometimes and here is the file length when failed.
gen_random_file ${SUNXI_PACKAGE_GEN}/u-boot     823287
gen_random_file ${SUNXI_PACKAGE_GEN}/scp        121790
gen_random_file ${SUNXI_PACKAGE_GEN}/optee      226454
gen_random_file ${SUNXI_PACKAGE_GEN}/soc-cfg     63918
gen_random_file ${SUNXI_PACKAGE_GEN}/dtb        131072
gen_random_file ${SUNXI_PACKAGE_GEN}/logo       119683

# generate and detect
assert_imgeditor_successful \
    --type sunxi_package --pack ${SUNXI_PACKAGE_GEN} \
    ${TEST_TMPDIR}/sunxi_package.fex \
    || exit $?

assert_imgeditor_successful ${TEST_TMPDIR}/sunxi_package.fex || exit $?
