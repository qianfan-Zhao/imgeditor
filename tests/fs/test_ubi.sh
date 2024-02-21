#!/bin/bash
# Test unit for ubi filesystem.
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

# assert_direq(dir1, dir2, [fail_msg])
function assert_direq() {
    if diff -r "$1" "$2" \
        --exclude "lost+found" \
        --exclude ".imgeditor"; then
        return 0
    fi

    log:error_default "$3" "directory $1 and $2 are differ"
    print_bash_error_stack
    return 1
}

 : ${UBI_MIN_IO_SIZE:="2048"} # minimum I/O unit size
 : ${UBI_MAX_LEB_CNT:="100"}  # maximum logical erase block count
 : ${UBI_LEB_SIZE:="124KiB"}  # logical erase block size
 : ${UBI_PEB_SIZE:="128KiB"}  # physical erase block size

# $1:    test unit name
function gen_ubifs() {
    local unit=$1 UBI_VOL_SIZE=$2
    local UBI_MAX_LEB_CNT

    case ${UBI_VOL_SIZE} in
        16MiB)
            UBI_MAX_LEB_CNT=100
            ;;
        32MiB)
            UBI_MAX_LEB_CNT=200
            ;;
        *)
            UBI_MAX_LEB_CNT=400
            ;;
    esac

cat << EOF > ${TEST_TMPDIR}/ubinize.cfg
[ubifs]
mode=ubi
vol_id=0
vol_type=dynamic
vol_name=${unit}
vol_alignment=1
vol_size=${UBI_VOL_SIZE}
image=${TEST_TMPDIR}/${unit}.ubifs
EOF

cat << EOF > ${TEST_TMPDIR}/fakeroot.sh
#!/bin/sh
mkfs.ubifs -d ${TEST_TMPDIR}/${unit} \
           -e ${UBI_LEB_SIZE} \
           -c ${UBI_MAX_LEB_CNT} \
           -m ${UBI_MIN_IO_SIZE} \
           ${MKFS_UBIFS_EXT_ARGS} \
           -F \
           -o ${TEST_TMPDIR}/${unit}.ubifs

ubinize -o ${TEST_TMPDIR}/${unit}.ubi \
        -m ${UBI_MIN_IO_SIZE} \
        -p ${UBI_PEB_SIZE} \
        -s ${UBI_MIN_IO_SIZE} \
        ${TEST_TMPDIR}/ubinize.cfg
EOF

    chmod a+x ${TEST_TMPDIR}/fakeroot.sh
    fakeroot -- ${TEST_TMPDIR}/fakeroot.sh
    assert_success "Create ${dir}.ubi failed"
}

# $1: unit name
# $2: image size
function imgeditor_unpack_ubi_test() {
    local unit=$1 img_size=$2
    local dir=${TEST_TMPDIR}/${unit}

    # create the basic directory and generate ext image
    mkdir -p ${dir}
    ${unit} ${dir}

    gen_ubifs ${unit} ${img_size} || exit $?

    # make sure we can read it
    assert_imgeditor_successful ${dir}.ubi || exit $?

    # unpack and compare
    assert_imgeditor_successful --unpack ${dir}.ubi || exit $?
    assert_direq ${dir}.ubi.dump ${dir} || exit $?
}

function single_file() {
    (
        cd $1

        echo a > a
    )
}

function simple_abc() {
    (
        cd $1
        mkdir -p a/b
        mkdir -p a/c
        echo "filepath: a/b, filename: ab" > a/b/ab
    )
}

function many_files() {
    (
        cd $1

        mkdir -p a b c
        for i in $(seq 1 1000) ; do
            gen_random_file_silence a/$i.bin 1024
        done
    )
}

function many_longname_files() {
    (
        cd $1

        mkdir -p a b c
        for i in $(seq 1 1000) ; do
            gen_random_file_silence \
                a/very_lonoooooooooooooooooooooooooooooooooooooooooog_name_$i.bin \
                1024
        done
    )
}

function simple_link() {
    (
        cd $1

        echo "this is file a" > a
        ln -s a symlink_of_a
        ln a hardlink_of_a
    )
}

function long_link_target_name() {
    local long_name=very_loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong_file
    (
        cd $1

        echo "abc" > ${long_name}
        ln -s ${long_name} symlink_of_long_file
    )
}

function symlink_60() {
    local len59=12345678901234567890123456789012345678901234567890123456789
    local len60=${len59}0
    local len61=${len60}a

    (
        cd $1

        touch ${len59} ${len60} ${len61}

        ln -s ${len59} a59
        ln -s ${len60} a60
        ln -s ${len61} a61
    )
}

function large_file() {
    (
        cd $1

        dd if=/dev/urandom of=1.bin bs=1M count=40
    )
}

imgeditor_unpack_ubi_test single_file 16MiB || exit $?
imgeditor_unpack_ubi_test simple_abc 16MiB || exit $?
imgeditor_unpack_ubi_test many_files 16MiB || exit $?
imgeditor_unpack_ubi_test many_longname_files 64MiB || exit $?
imgeditor_unpack_ubi_test simple_link 16MiB || exit $?
imgeditor_unpack_ubi_test symlink_60 16MiB || exit $?
imgeditor_unpack_ubi_test long_link_target_name 16MiB || exit $?
imgeditor_unpack_ubi_test large_file 64MiB || exit $?
