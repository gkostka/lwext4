[![Join the chat at https://gitter.im/gkostka/lwext4](https://badges.gitter.im/gkostka/lwext4.svg)](https://gitter.im/gkostka/lwext4?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![License (GPL v2.0)](https://img.shields.io/badge/license-GPL%20(v2.0)-blue.svg?style=flat-square)](http://opensource.org/licenses/GPL-2.0)
[![Build Status](https://travis-ci.org/gkostka/lwext4.svg)](https://travis-ci.org/gkostka/lwext4)
[![](http://img.shields.io/gratipay/user/gkostka.svg)](https://gratipay.com/gkostka/)

![lwext4](https://cloud.githubusercontent.com/assets/8606098/11697327/68306d88-9eb9-11e5-8807-81a2887f077e.png)

About
=====


The main goal of the lwext4 project is to provide ext2/3/4 filesystem for microcontrollers. It may be an interesting alternative for traditional MCU filesystem libraries (mostly based on FAT32). Library has some cool and unique features in microcontrollers world:
 - directory indexing - fast file find and list operations
 - extents - fast big file truncate
 - journaling transactions & recovery - power loss resistance

Lwext4 is an excellent choice for SD/MMC card, USB flash drive or any other wear
leveled memory types. However it is not good for raw flash devices.

Feel free to contact me:
kostka.grzegorz@gmail.com

Credits
=====

The most of the source code of lwext4 was taken from HelenOS:
* http://helenos.org/

Some features are based on FreeBSD and Linux implementations.

KaHo Ng (https://github.com/ngkaho1234):
* advanced extents implementation
* xattr support
* metadata checksum support
* journal recovery & transactions
* many bugfixes & improvements

Lwext4 could be used also as fuse internals. Here is a nice project which uses lwext4 as a filesystem base:
* https://github.com/ngkaho1234/fuse-lwext4

Some of the source files are licensed under GPLv2. It makes whole
lwext4 GPLv2 licensed. To use library as a BSD3, GPLv2 licensed source
files must be removed first. At this point there are two files
licensed under GPLv2:
* ext4_xattr.c
* ext4_extents.c

All other modules and headers are BSD-3-Clause licensed code.


Features
=====

* filetypes: regular, directories, softlinks
* support for hardlinks
* multiple blocksize supported: 1KB, 2KB, 4KB ... 64KB
* little/big endian architectures supported
* multiple configurations (ext2/ext3/ext4)
* only C standard library dependency
* various CPU architectures supported (x86/64, cortex-mX, msp430 ...)
* small memory footprint
* flexible configurations

Memory footprint
------------

Advanced ext4 filesystem features, like extents or journaling require some memory. 
However most of the memory expensive features could be disabled at compile time.
Here is a brief summary for cortex-m4 processor:

* .text:  20KB - only ext2 fs support , 50KB - full ext4 fs feature set
* .data:  8KB - minimum 8 x 1KB  block cache, 12KB - when journaling and extents are enabled
* .stack: 2KB - is enough (not measured precisely)

Blocks are allocated dynamically. Previous versions of library could work without
malloc but from 1.0.0 dynamic memory allocation is required. However, block cache
should not allocate more than CONFIG_BLOCK_DEV CACHE_SIZE.

Supported ext2/3/4 features
=====
incompatible:
------------
*  filetype, recover, meta_bg, extents, 64bit, flex_bg: **yes**
*  compression, journal_dev, mmp, ea_inode, dirdata, bg_meta_csum, largedir, inline_data: **no**

compatible:
------------
*  has_journal, ext_attr, dir_index: **yes**
*  dir_prealloc, imagic_inodes, resize_inode: **no**

read-only:
------------
*  sparse_super, large_file, huge_file, gdt_csum, dir_nlink, extra_isize, metadata_csum: **yes**
*  quota, bigalloc, btree_dir: **no**

Project tree
=====
*  blockdev         - block devices set, supported blockdev
*  fs_test          - test suite, mkfs and demo application
*  src              - source files
*  include          - header files
*  toolchain        - cmake toolchain files
*  CMakeLists.txt   - CMake config file
*  ext_images.7z    - compressed ext2/3/4 100MB images
*  fs_test.mk       - automatic tests definitions
*  Makefile         - helper makefile to generate cmake and run test suite
*  README.md       - readme file
  
Compile
=====
Dependencies
------------
* Windows 

Download MSYS-2:  https://sourceforge.net/projects/msys2/

Install required packages is MSYS2 Shell package manager:
```bash
 pacman -S make gcc cmake p7zip
  ```
  
* Linux 

Package installation (Debian):
```bash
 apt-get install make gcc cmake p7zip
  ```
 
Compile & install tools
------------
```bash
 make generic
 cd build_generic
 make
 sudo make install
 ```

lwext4-generic demo application
=====
Simple lwext4 library test application:
* load ext2/3/4 images
* load linux block device with ext2/3/4 part
* load windows volume with ext2/3/4 filesystem 
* directory speed test
* file write/read speed test

How to use for images/blockdevices:
```bash
 lwext4-generic -i ext_images/ext2 
 lwext4-generic -i ext_images/ext3 
 lwext4-generic -i ext_images/ext4 
 ```
 
Show full option set:
```bash
 lwext4-generic --help
   ```

Run automatic tests
=====

Execute tests for 100MB unpacked images:
```bash
 make test
   ```
Execute tests for autogenerated 1GB images (only on Linux targets) + fsck:
```bash
 make test_all
   ```
Using lwext4-mkfs tool
=====
It is possible to create ext2/3/4 partition by internal library tool.

Generate empty file (1GB):
```bash
 dd if=/dev/zero of=ext_image bs=1M count=1024
   ```
Create ext2 partition:
```bash
 lwext4-mkfs -i ext_image -e 2
   ```
Create ext3 partition:
```bash
 lwext4-mkfs -i ext_image -e 3
   ```
Create ext4 partition:
```bash
 lwext4-mkfs -i ext_image -e 4
   ```
Show full option set:
```bash
 lwext4-mkfs --help
   ```

Cross compile standalone library
=====
Toolchains needed:
------------

Lwext4 could be compiled for many targets. Here are an examples for 8/16/32/64 bit architectures.
* generic for x86 or amd64
* arm-none-eabi-gcc for ARM cortex-m0/m3/m4 microcontrollers
* avr-gcc for AVR xmega microcontrollers
* bfin-elf-gcc for blackfin processors
* msp430-gcc for msp430 microcontrollers

Library has been tested only for generic (amd64) & ARM Cortex M architectures.
For other targets compilation passes (with warnings somewhere) but tests are
not done yet. Lwext4 code is written with endianes respect. Big endian
behavior also hasn't been tested yet.

Build avrxmega7 library:
------------
```bash
 make avrxmega7
 cd build_avrxmega7
 make lwext4
 ```

Build cortex-m0 library:
------------
```bash
 make cortex-m0
 cd build_cortex-m0
 make lwext4
 ```

Build cortex-m3 library:
------------
```bash
 make cortex-m3
 cd build_cortex-m3
 make lwext4
 ```

Build cortex-m4 library:
------------
```bash
 make cortex-m4
 cd build_cortex-m4
 make lwext4
```


