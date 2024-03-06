cmake_minimum_required(VERSION 3.6)
project(imgeditor C)

include_directories(.)
include_directories(libs/cJSON)
include_directories(libs/GmSSL/include)
include_directories(libs/minilzo)

set(src
        main.c
        dd.c
        structure.c
        json_helper.c
        string_helper.c
        rc4.c
        crc32.c
        xstring.c
        hexdump_printf.c
        virtual_file.c
	libs/cJSON/cJSON.c
        libs/GmSSL/src/md5.c
        libs/GmSSL/src/sha1.c
        libs/GmSSL/src/sha256.c
        libs/GmSSL/src/sha512.c
        libs/minilzo/minilzo.c
)

set(tests)

add_compile_options(-Wall)
add_compile_options(-fstack-protector)
add_compile_options(-Werror)
add_compile_options(-Wfatal-errors)
add_compile_options(-Wno-unused-result)
add_compile_options(-Wsign-compare)

# for lseek64
add_compile_options(-D_LARGEFILE64_SOURCE)

option(DEBUG "Compile in debug mode" OFF)
if (DEBUG)
        add_compile_options(-O0)
        add_compile_options(-g)
else()
        add_compile_options(-O2)
endif()

option(ENABLE_ALLWINNER         "Enable Allwinner images"               ON)
option(ENABLE_ROCKCHIP          "Enable Rockchip images"                ON)
option(ENABLE_ANDROID           "Enable Android images"                 ON)
option(ENABLE_DISK              "Enable disk paritition images"         ON)
option(ENABLE_FDT               "Enable flatted device tree(dtb)"       ON)
option(ENABLE_FS                "Enable filesystem images"              ON)
option(ENABLE_TIMEZONE          "Enable timezone tzfile"                ON)
option(ENABLE_UBOOT             "Enable U-Boot images"                  ON)
option(ENABLE_PCAP              "Enable tcpdump/wireshark pcap file"    ON)

if (ENABLE_ALLWINNER)
        list(APPEND src
                allwinner/eGON.c
                allwinner/sunxi_mbr.c
                allwinner/sunxi_package.c
                allwinner/sysconfig.c
                allwinner/carboot.c
        )

        list(APPEND tests
                allwinner/sysconfig/test.sh
                allwinner/sunxi_package/test.sh
        )
endif()

if (ENABLE_ROCKCHIP)
        list(APPEND src
                rockchip/rockchip_crc32.c
                rockchip/rockchip_krnl.c
                rockchip/rockchip_resource.c
                rockchip/rockchip_rkaw.c
                rockchip/rockchip_rkfw.c
                rockchip/rockchip_rkns.c
                rockchip/rockchip.c
        )
        include_directories(rockchip)
endif()

if (ENABLE_ANDROID)
        list(APPEND src android/android_bootimg.c android/android_sparse.c)

        list(APPEND tests
                android/bootimg/test.sh
        )
endif()

if (ENABLE_DISK)
        list(APPEND src disk/gpt.c disk/mbr.c)

        list(APPEND tests
                disk/gpt/test.sh
        )
endif()

if (ENABLE_FDT)
        list(APPEND src fdt/fdt.c)
endif()

if (ENABLE_FS)
        list(APPEND src fs/ext2.c fs/ubi.c)

        list(APPEND tests
                fs/ext4/test.sh
                fs/ext2/test.sh
                fs/ubi/test.sh
                fs/ubi_none/test.sh
        )
endif()

if (ENABLE_TIMEZONE)
        list(APPEND src timezone/tzfile.c)
endif()

if (ENABLE_UBOOT)
        list(APPEND src u-boot/uenv.c)

        list(APPEND tests
                uboot/env/test.sh
        )
endif()

if (ENABLE_PCAP)
        list(APPEND src
                pcap/pcap_linktype.c
                pcap/pcapng.c)
        include_directories(pcap)

        list(APPEND tests pcap/test.sh)
endif()

add_executable(imgeditor ${src})
install(TARGETS imgeditor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

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

foreach(name ${tests})
        add_imgeditor_shell_test(${name})
endforeach()

