#!/bin/bash
# Test unit for android boot.img
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

BOOTIMG_GEN=${TEST_TMPDIR}/bootimg_gen

# $1: the bootimg's json configuration file. v0 or v2.
function bootimg_pack_unpack_test() {
    local version=$1

    # setup materials
    rm -rf ${BOOTIMG_GEN}
    mkdir -p ${BOOTIMG_GEN}
    gen_random_file ${BOOTIMG_GEN}/kernel.bin     8000000 9000000
    gen_random_file ${BOOTIMG_GEN}/ramdisk.bin    1000000 2000000
    gen_random_file ${BOOTIMG_GEN}/dtb.bin         100000  200000

    cp -v bootimg_${version}.json ${BOOTIMG_GEN}/abootimg.json

    # pack it
    assert_imgeditor_successful \
            --type abootimg --pack ${BOOTIMG_GEN} \
            ${TEST_TMPDIR}/${version}.img || exit $?

    # show it
    assert_imgeditor_successful ${TEST_TMPDIR}/${version}.img || exit $?

    # peek it
    assert_imgeditor_successful --peek ${TEST_TMPDIR}/${version}.img ${TEST_TMPDIR}/1.img || exit $?

    # unpack it
    assert_imgeditor_successful \
            --unpack ${TEST_TMPDIR}/1.img || exit $?

    # compare
    for file in kernel.bin ramdisk.bin dtb.bin ; do
        if [ X"${version}" = X"v0" ] ; then
            case ${file} in
                dtb.bin)
                    continue
                    ;;
            esac
        fi

        assert_fileeq ${BOOTIMG_GEN}/${file} \
            ${TEST_TMPDIR}/1.img.dump/${file} \
            "${file} is differ" || exit $?
    done
}

bootimg_pack_unpack_test v0
bootimg_pack_unpack_test v2
