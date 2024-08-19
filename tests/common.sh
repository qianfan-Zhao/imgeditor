#!/usr/bin/env bash

# gen_random_num(min, [max])
function gen_random_num() {
    local random_num=$1 min=$1 max=$2

    if [ $# -eq 2 ] ; then
        random_num=$((($RANDOM % ($max - $min + 1)) + $min))
    fi

    echo "${random_num}"
}

# gen_random_file(path_to_file, min, [max])
function gen_random_file() {
    local size

    size=$(gen_random_num $2 $3)
    dd if=/dev/urandom of=$1 bs=${size} count=1
}

# gen_random_file_silence(path_to_file, min, [max])
function gen_random_file_silence() {
    local size

    size=$(gen_random_num $2 $3)
    dd if=/dev/urandom of=$1 bs=${size} count=1 status=none
}

# black="\e[0;30m"
# red="\e[0;31m"
# green="\e[0;32m"
# yellow="\e[0;33m"
# blue="\e[0;34m"
# purple="\e[0;35m"
# cyan="\e[0;36m"
# white="\e[0;37m"
# orange="\e[0;91m"

# Methods and functions to be used in all other scripts.
 : ${LOGGING_SHOW_COLORS:="false"}
 : ${LOGGING_INFO_PREFEX_COLOR:="\e[0;32m"}
 : ${LOGGING_INFO_PREFEX:=":INFO"}
 : ${LOGGING_ERROR_PREFEX_COLOR:="\e[0;31m"}
 : ${LOGGING_ERROR_PREFEX:=":ERROR"}
 : ${LOGGING_WARNING_PREFEX_COLOR:="\e[0;33m"}
 : ${LOGGING_WARNING_PREFEX:=":WARNING"}
 : ${LOGGING_ARCHIVE_PREFEX_COLOR:="\e[0;34m"}
 : ${LOGGING_ARCHIVE_PREFEX:=":INFO"}
 : ${LOGGING_SCRIPT_PREFEX:=":INFO"}
 : ${LOGGING_SCRIPT_PREFEX_COLOR:="\e[0;36m"}
 : ${LOGGING_SCRIPT_TEXT_COLOR:="\e[0;35m"}

# TODO: Apply common format for stackdriver.
logging_core_print() {
  local log_type="$1"
  local reset_color="\e[0m"
  # remove the first argument.
  shift

  local log_type_prefex_env_name="LOGGING_${log_type}_PREFEX"
  log_type_prefex="${!log_type_prefex_env_name}"

  if [ "$LOGGING_SHOW_COLORS" != "true" ]; then
    echo "$LOGGING_PREFIX$log_type_prefex " "$@"
  else
    local log_prefex_color_env_name="LOGGING_${log_type}_PREFEX_COLOR"
    local log_text_color_env_name="LOGGING_${log_type}_TEXT_COLOR"
    log_prefex_color="${!log_prefex_color_env_name}"
    log_text_color="${!log_text_color_env_name}"

    if [ -z "$log_text_color" ]; then log_text_color="$reset_color"; fi

    echo -e "$log_prefex_color$LOGGING_PREFIX$log_type_prefex$log_text_color" "$@" "$reset_color"
  fi
}

# log a line with the logging prefex.
function log:info() {
  logging_core_print "INFO" "$@"
}

function log:error() {
  logging_core_print "ERROR" "$@"
}

function log:warning() {
  logging_core_print "WARNING" "$@"
}

function log:script() {
  logging_core_print "SCRIPT" "$@"
}

function log:archive() {
  logging_core_print "ARCHIVE" "$@"
}

# log:error_default(msg, default)
function log:error_default() {
    [[ -n "$1" ]] && log:error "$1" || log:error "$2"
}

function print_bash_error_stack(){
  for ((i=1;i<${#FUNCNAME[@]}-1;i++)); do
      local fpath="$(realpath ${BASH_SOURCE[$i+1]})"
      log:error "$i: $fpath:${BASH_LINENO[$i]} @ ${FUNCNAME[$i]}"
  done
}

# the source file is generated with `xxd -p -g 1 -c 32`
# $1: the source file's path
# $2: the dest path
function xxd_reverse() {
    local srcpath=$1 dstpath=$2
    local srcname

    srcname=$(basename "${srcpath}" | awk -F'.' '{print $1}')
    if ! xxd -r -p "${srcpath}" "${dstpath}/${srcname}.bin" ; then
        log:error "xxd reverse ${srcpath} failed"
        return 1
    fi
}

# assert_env(name)
function assert_env() {
    local var_name="$1"
    if [ -z "${!var_name}" ]; then
        log:error "Environment variable '$var_name' is not set."
        return 1
    fi
}

# assert_success(fail_msg)
function assert_success() {
    [[ $? -eq 0 ]] && return 0
    [[ ! -z "$1" ]] && log:error "$1"
    print_bash_error_stack
    return 1
}

# assert_pipe_success(fail_msg)
#
# Example about PIPESTATUS in bash:
# $ false | true | true
# $ echo ${PIPESTATUS[0]} ${PIPESTATUS[1]} ${PIPESTATUS[2]}
# $ 1 0 0
function assert_pipe_success() {
    [[ ${PIPESTATUS[0]} -eq 0 ]] && return 0
    [[ ! -z "$1" ]] && log:error "$1"
    print_bash_error_stack
    return 1
}

# assert_sha1sum(path_to_file, sha1, fail_msg)
function assert_sha1sum() {
    local SHA1

    SHA1=$(sha1sum "$1" | awk '{print $1};')
    if [ X"${SHA1}" != X"$2" ] ; then

        log:error_default "$3" "check sha1sum of $(basename $1) failed"
        print_bash_error_stack
        return 1
    fi

    return 0
}

# assert_fileeq(file1, file2, [fail_msg])
function assert_fileeq() {
    if diff -q "$1" "$2" ; then
        return 0
    fi

    log:error_default "$3" "file $1 and $2 are differ"
    print_bash_error_stack
    return 1
}

# assert_imgeditor_successful(args...)
function assert_imgeditor_successful() {
    log:info "imgeditor $@"

    ${CMAKE_CURRENT_BINARY_DIR}/imgeditor --disable-plugin "$@" \
            | tee ${TEST_TMPDIR}/imgeditor-stdio.txt
    assert_pipe_success "Running imgeditor failed"
}

# those variable are exported from CMakeLists.txt
assert_env CMAKE_SOURCE_DIR
assert_env CMAKE_CURRENT_BINARY_DIR
assert_env TEST_TMPDIR

if [ -z "${KEEP_TEST_TMPDIR}" ] ; then
    # cleanup the TMPDIR
    rm -rf ${TEST_TMPDIR}
    mkdir -p "${TEST_TMPDIR}"
fi
