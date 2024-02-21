imgeditor
=========

A set of tools for editor binary images.

# Compile and run on host

```console
$ mkdir build
$ cd build
$ cmake ..
$ make
$ sudo make install
```

And we can do some builtin tests.

```console
$ make test
Running tests...
Test project /home/qianfan/debug/port/github-os/imgeditor/build
    Start 1: allwinner/sysconfig/test.sh
1/8 Test #1: allwinner/sysconfig/test.sh .......   Passed    0.01 sec
    Start 2: allwinner/sunxi_package/test.sh
2/8 Test #2: allwinner/sunxi_package/test.sh ...   Passed    0.07 sec
    Start 3: android/bootimg/test.sh
3/8 Test #3: android/bootimg/test.sh ...........   Passed    0.30 sec
    Start 4: disk/gpt/test.sh
4/8 Test #4: disk/gpt/test.sh ..................   Passed    0.25 sec
    Start 5: fs/ext4/test.sh
5/8 Test #5: fs/ext4/test.sh ...................   Passed    3.50 sec
    Start 6: fs/ext2/test.sh
6/8 Test #6: fs/ext2/test.sh ...................   Passed    3.14 sec
    Start 7: fs/ubi/test.sh
7/8 Test #7: fs/ubi/test.sh ....................   Passed    9.31 sec
    Start 8: uboot/env/test.sh
8/8 Test #8: uboot/env/test.sh .................   Passed    0.02 sec

100% tests passed, 0 tests failed out of 8

Total Test time (real) =  16.61 sec
```

# Cross-compile

```console
$ mkdir build-arm
$ cd build-arm
cmake -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc ..
-- The C compiler identification is GNU 7.5.0
-- Check for working C compiler: /opt/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
-- Check for working C compiler: /opt/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc -- works
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Detecting C compile features
-- Detecting C compile features - done
-- Configuring done
-- Generating done
-- Build files have been written to: /home/debug/port/github-os/imgeditor/build-arm
$ make
```
