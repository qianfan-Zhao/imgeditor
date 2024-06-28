#!/bin/bash
# Test ubi filesystem without compression

export MKFS_UBIFS_EXT_ARGS="-x none"
source ../test_ubi.sh
