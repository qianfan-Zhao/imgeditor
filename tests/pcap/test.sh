#!/bin/sh
# Test unit for wireshark pcapng file.
# qianfan Zhao <qianfanguijin@163.com>

source "${CMAKE_SOURCE_DIR}/tests/common.sh"

# $1: the input file
# $2: the output file name
# $3: the linktype number for text2pcap
# $*: the other options for text2pcap
function assert_text2pcap_successful() {
    local in=$1 out=$2 linktype=$3

    shift 3

    text2pcap -t "%Y-%m-%d %H:%M:%S." -n -l "${linktype}" \
            "$@" "${in}" "${out}"
    assert_success "text2pcap ${in} failed"
}

# imgeditor doesn't check the option sections by default.
# appending "-vv" to enable it.
assert_text2pcap_successful \
        ppp.txt ${TEST_TMPDIR}/ppp.pcapng 9 "-D" || exit $?
assert_imgeditor_successful ${TEST_TMPDIR}/ppp.pcapng || exit $?
assert_sha1sum \
        "${TEST_TMPDIR}/imgeditor-stdio.txt" \
        "6708c6643027e29bc6eb8097f17b81f8e95e18ba" || exit $?
assert_imgeditor_successful ${TEST_TMPDIR}/ppp.pcapng "-vv" || exit $?
