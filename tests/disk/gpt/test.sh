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

function gpt_partitions_test() {
    local show_sha1sum=$1 list_sha1sum=$2 out=${TEST_TMPDIR}/$3

    shift 3

    # generate gpt partition
    assert_imgeditor_successful --type gpt -- partitions "${out}" "$@" || return $?

    # make sure imgeditor can detect it
    assert_imgeditor_successful "${out}" || return $?

    # show the partition's detail
    assert_imgeditor_successful "${out}" -- show || return $?
    assert_sha1sum ${TEST_TMPDIR}/imgeditor-stdio.txt ${show_sha1sum} || return $?

    # list the partition's info
    assert_imgeditor_successful "${out}" -- partitions --reverse || return $?
    assert_sha1sum ${TEST_TMPDIR}/imgeditor-stdio.txt ${list_sha1sum} || return $?
}

gpt_partitions_test \
        22eba655f1b0a08bbd7a72cd568ee2df59cd1f99 \
        d400dbc2dc9ab15473086c9dbe26387b92de44b7 \
        mmcblk0-case1.bin \
        --last-lba 15286238 \
        --alt-lba 15286271 \
        --disk-type-uid ebd0a0a2-b9e5-4433-87c0-68b6b72699c7 \
        "partitions=uuid_disk=0f0ebbbb-4767-d74f-b6ae-d4cb1b6e529d;name=loader,start=2M,size=1M,uuid=9cce0066-2ee7-c44f-8616-380932bdf276;name=logo,size=1M,uuid=43a3305d-150f-4cc9-bd3b-38fca8693846;name=misc,size=512K,uuid=db72e11c-7434-6d45-ad2c-81bd0bcd9499;name=boot,size=32M,bootable,uuid=9cdefc87-9920-2a49-b62b-1f2cca7e48e6;name=recovery,size=32M,uuid=997762b5-fc85-d14e-8223-45df189d9e1d;name=data,size=3000M,uuid=98134204-ff8d-fa44-87c1-78df4306402e;name=container,size=500M,uuid=972c2895-b0ca-554e-9197-01fde0cc9933;name=app,size=1000M,uuid=63c1c681-aa29-3948-bf1f-dea10f131826;name=rootfs,size=500M,uuid=ddb8c3f6-d94d-4394-b633-3134139cc2e0;name=update,size=500M,uuid=521c8609-2b87-9945-ad64-afbe384a7717;name=backup,size=1000M,uuid=a34cfc92-fd5d-a74a-ad00-85ec455a80de;name=log,size=500M,uuid=ba3f50af-8ffa-5549-994a-59f49c874a7c;" \
        || exit $?

