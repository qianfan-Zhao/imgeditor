#!/bin/bash

export MKSQUASHFS_NO_COMPRESSION=1
export MKSQUASHFS_COMMON_ARGS="-noI -noD -noF -noX"

source ../test_squashfs.sh
