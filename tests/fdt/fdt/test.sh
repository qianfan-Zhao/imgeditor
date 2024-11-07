#!/bin/bash
#
# DTS is copied from:
# https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/arch/arm/boot/dts/sun4i-a10.dtsi?h=v3.18

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

function compile_dts() {
    local dts_path=$1 out_dir=$2 out_name=$3
    local dts_file=$(basename ${dts_path})
    local dts_name=${dts_file%%.*}

    mkdir -p $out_dir

    if [ -z "${out_name}" ] ; then
        out_name=${dts_name}
    fi

    gcc -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
            -I "$(dirname ${dts_path})" \
            -o ${out_dir}/${out_name}.tmp.dts ${dts_path} \
            || return $?

    dtc -O dtb --symbols -qq -a 4 -i "$(dirname ${dts_path})" \
            -o ${out_dir}/${out_name}.dtb ${out_dir}/${out_name}.tmp.dts \
            || return $?
}

function imgeditor_convert_dtb() {
    local dtb=$1 dts=$2

    ${CMAKE_CURRENT_BINARY_DIR}/imgeditor --disable-plugin $dtb > $dts
    assert_success "imgeditor convert $dtb failed"
}

#                                                           |------------compare----------|
#                                                           |                             |
#                    (dtc)                     (imgeditor)  |    (dtc)       (imgeditor)  |
# sun8i-a10-a1000.dts ---> sun8i-a10-a1000.dtb ----------> 1.dts ----> 1.dtb ---------> 2.dts
#                                     |                                  |
#                                     |---------------compare------------|
#
function dtb_unpack_repack_test() {
    local unit=$1 dts=$2
    local dts_name=${dts%%.*}
    local dtb="${dts_name}.dtb"
    local tmpdir=${TEST_TMPDIR}/${unit}/${dts_name}

    log:info "DTB: ${unit} ${dts}"

    compile_dts $(pwd)/${unit}/${dts} ${tmpdir} || return $?

    # make sure imgeditor can search it
    assert_imgeditor_successful -s ${tmpdir}/${dtb} || return $?

    # convert to dts and recompile again
    imgeditor_convert_dtb ${tmpdir}/${dtb} ${tmpdir}/1.dts || return $?
    compile_dts ${tmpdir}/1.dts ${tmpdir} || return $?

    if assert_fileeq ${tmpdir}/1.dtb ${tmpdir}/${dtb} ; then
        log:info "The binary dtb is same"
        return 0
    fi

    # maybe the phandle doesn't same, let's compare the dts again
    imgeditor_convert_dtb ${tmpdir}/1.dtb ${tmpdir}/2.dts || return $?
    assert_fileeq ${tmpdir}/1.dts ${tmpdir}/2.dts
}

dtb_unpack_repack_test sun4i-a10-v3.18 sun4i-a10-a1000.dts || exit $?
dtb_unpack_repack_test sun4i-a10-v3.18 sun4i-a10-inet97fv2.dts || exit $?

dtb_unpack_repack_test sun8i-r40-v6.4 sun8i-r40-bananapi-m2-ultra.dts || exit $?
