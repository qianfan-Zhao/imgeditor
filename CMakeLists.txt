cmake_minimum_required(VERSION 3.6)
project(imgeditor C)

set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install path prefix")

# for md5, sha1, sha256, sha512
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

include_directories(.)
include_directories(libs/cJSON)
include_directories(libs/GmSSL/include)
include_directories(libs/minilzo)
include_directories(libs/libxopt)

set(src main.c)

set(src_api_test
        tests/api_test/xopt.c
        tests/api_test/hash_compatible.c
        tests/api_test/main.c
)

set(tests)

add_compile_options(-Wall)
add_compile_options(-fstack-protector)
add_compile_options(-Werror)
add_compile_options(-Wfatal-errors)
add_compile_options(-Wno-unused-result)
add_compile_options(-Wsign-compare)

if (NOT DEFINED VERSION)
        set(SCMVERSION "")

        execute_process(
                COMMAND cat .scmversion
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                ERROR_QUIET
                OUTPUT_VARIABLE SCMVERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process(
                COMMAND git describe --tags
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                RESULT_VARIABLE GIT_VERSION_FOUND
                ERROR_QUIET
                OUTPUT_VARIABLE GIT_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if (SCMVERSION)
                set(VERSION "${SCMVERSION}")
                message(STATUS "scmversion: ${VERSION}")
        else()
                set(VERSION "${GIT_VERSION}")
                message(STATUS "Auto generated version: ${VERSION}")
        endif()
endif()

add_compile_options(-DBUILD_IMGEDITOR_CORE=1)
add_compile_options(-DIMGEDITOR_VERSION="${VERSION}")

# for lseek64
add_compile_options(-D_LARGEFILE64_SOURCE)

option(DEBUG "Compile in debug mode" OFF)
if (DEBUG)
        add_compile_options(-O0)
        add_compile_options(-g)
else()
        add_compile_options(-O2)
endif()

option(ENABLE_ALL               "Enable all features"                   OFF)
option(ENABLE_ANDROID           "Enable Android images"                 ON)
option(ENABLE_DISK              "Enable disk paritition images"         ON)
option(ENABLE_FDT               "Enable flatted device tree(dtb)"       ON)
option(ENABLE_FS                "Enable filesystem images"              ON)
option(ENABLE_UBOOT             "Enable U-Boot images"                  ON)

macro(require_program execname)
    find_program(${execname}_binary ${execname})
    if(NOT ${execname}_binary)
        message(FATAL_ERROR "${execname} not found")
    endif()
endmacro()

if (ENABLE_ANDROID OR ENABLE_ALL)
        list(APPEND src
                android/android_bootimg.c
                android/android_sparse.c
        )

        list(APPEND tests
                android/bootimg/test.sh
                android/sparse/test.sh
        )

        if (ENABLE_FDT OR ENABLE_ALL)
                # dtimg need fdt
                list(APPEND src android/dtimg.c)
                list(APPEND tests android/dtimg/test.sh)

                require_program(dtc)
        endif()

        require_program(img2simg)
endif()

if (ENABLE_DISK OR ENABLE_ALL)
        list(APPEND src disk/gpt.c disk/mbr.c)

        list(APPEND tests
                disk/gpt/test.sh
        )

        list(APPEND tests disk/mbr/test.sh)
endif()

if (ENABLE_FDT OR ENABLE_ALL)
        list(APPEND src fdt/fdt.c)

        list(APPEND tests fdt/fit/test.sh)
        require_program(mkimage)
endif()

if (ENABLE_FS OR ENABLE_ALL)
        list(APPEND src fs/ext2.c fs/ubi.c)

        list(APPEND tests
                fs/ext4/test.sh
                fs/ext2/test.sh
                fs/ubi/test.sh
                fs/ubi_none/test.sh
        )

        require_program(mkfs.ext2)
        require_program(mkfs.ext4)
        require_program(mkfs.ubifs)
        require_program(ubinize)
endif()

if (ENABLE_UBOOT OR ENABLE_ALL)
        list(APPEND src u-boot/uenv.c)

        list(APPEND tests
                uboot/env/test.sh
        )
endif()

set(libimgeditor_src
        dd.c
        structure.c
        json_helper.c
        string_helper.c
        rc4.c
        crc32.c
        xstring.c
        keymap.c
        hexdump_printf.c
        hash_compatible.c
        virtual_file.c
        disk_partition.c
        exini.c
        gd.c
        misc.c
        libs/cJSON/cJSON.c
        libs/minilzo/minilzo.c
        libs/libxopt/libxopt.c
)

# generate libimgeditor.a and libimgeditor.so
add_library(imgeditor_so SHARED ${libimgeditor_src})
add_library(imgeditor_static STATIC ${libimgeditor_src})
set_target_properties(imgeditor_so PROPERTIES OUTPUT_NAME imgeditor)
set_target_properties(imgeditor_static PROPERTIES OUTPUT_NAME imgeditor)

add_executable(imgeditor_elf ${src})
target_link_libraries(imgeditor_elf imgeditor_static)
target_link_libraries(imgeditor_elf -ldl)
target_link_libraries(imgeditor_elf -lrt) # for shm
target_link_libraries(imgeditor_elf OpenSSL::Crypto OpenSSL::SSL)
set_target_properties(imgeditor_elf PROPERTIES OUTPUT_NAME imgeditor)

set(IMGEDITOR_HEADERS
        "imgeditor.h"
        "hash_compatible.h"
        "json_helper.h"
        "string_helper.h"
        "structure.h"
        "list_head.h"
        "exini.h"
        "libs/cJSON/cJSON.h"
        "libs/libxopt/libxopt.h"
)

add_executable(imgeditor_api_test ${src_api_test})
target_link_libraries(imgeditor_api_test OpenSSL::Crypto OpenSSL::SSL)
target_link_libraries(imgeditor_api_test imgeditor_static)
target_link_libraries(imgeditor_api_test -lrt)

# TEST_NAME: sometings like allwinner/sysconfig/test.sh
function(add_imgeditor_shell_test TEST_NAME)
        find_program(BASH_PROGRAM bash)
        if(BASH_PROGRAM)
                get_filename_component(TEST_DIR ${TEST_NAME} DIRECTORY)

                # Set up the test directory
                set(TEST_TMPDIR "${CMAKE_CURRENT_BINARY_DIR}/tests/${TEST_DIR}")

                add_test(
                        NAME ${TEST_NAME}
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST_DIR}
                        COMMAND ${BASH_PROGRAM} "${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST_NAME}"
                )

                set_property(
                        TEST "${TEST_NAME}"
                        PROPERTY ENVIRONMENT
                        "CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
                        "CMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                        "TEST_TMPDIR=${TEST_TMPDIR}"
                )
        endif()
endfunction()

# Add test functions
enable_testing()

add_test(NAME api_test
        COMMAND imgeditor_api_test
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)

foreach(name ${tests})
        add_imgeditor_shell_test(${name})
endforeach()

# install targets
install(TARGETS imgeditor_elf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${IMGEDITOR_HEADERS} DESTINATION "include/imgeditor")

install(TARGETS imgeditor_so
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(TARGETS imgeditor_static
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)