
About lwext4
============

The main goal of the lwext4 project is to provide ext2/3/4 filesystem
library for microcontrolers.

kostka.grzegorz@gmail.com

Credits lwext4
==============

A lot of the implementation of lwext4 was taken from HelenOS:
 - http://www.helenos.org/
 
Some of ideas and features are based on FreeBSD and Linux implementations.

lwext4 supported/unsupported ext2/3/4 fs features
=================================================
FEATURE_INCOMPAT (unable to mount with unsupported feature):
 - COMPRESSION: no
 - FILETYPE: 	yes
 - RECOVER: 	no
 - JOURNAL_DEV:	no
 - META_BG:		no
 - EXTENTS: 	yes
 - 64BIT:		yes
 - MMP:			no
 - FLEX_BG 		no
 - EA_INODE:	no
 - DIRDATA:		no
 
FEATURE_INCOMPAT (able to mount with unsupported feature):
 - DIR_PREALLOC:   no
 - IMAGIC_INODES:  no
 - HAS_JOURNAL:    no
 - EXT_ATTR: 	   no
 - RESIZE_INODE:   no
 - DIR_INDEX:      yes

FEATURE_RO (able to mount in read only with unsupported feature):
 - SPARSE_SUPER:  yes
 - LARGE_FILE:    yes
 - BTREE_DIR:     no
 - HUGE_FILE:     yes
 - GDT_CSUM:      yes
 - DIR_NLINK:     yes
 - EXTRA_ISIZE:   yes

Supported filetypes:
 - FIFO: 	  no
 - CHARDEV:	  no
 - DIRECTORY: yes
 - BLOCKDEV:  no
 - FILE:      yes
 - SOFTLINK:  no
 - SOCKET:    no

Other:
 - block_size: 1KB, 2KB, 4KB ... 64KB
 - little/big endian architecture support


lwext4 project tree
===================

+blockdev      - block devices set, supported blockdevs
 - filedev     - file based block device
 - io_raw      - wiodows IO block device

+demos         - demo directory sources
 - generic     - generic demo app, used for development and and debbuging purpose
 
+lwext4        - internals of the lwext4 library

+toolchain     - specific toolchain cmake files

+ext4.h	       - lwext4 client library header
CMakeLists.txt - CMake config file
ext_images.7z  - ext2/3/4 100MB images
Makefile       - helper makefile to call cmake
readme.txt     - yes, you are here ;)
  
lwext4 compile Windows
======================

Tools needed:
 - CMake:  http://www.cmake.org/cmake/resources/software.html
 - MinGw:  http://www.mingw.org/
 - GnuWin: http://gnuwin32.sourceforge.net/ 

Build:
>>make

Clean:
>>clean

Successful build generates out of source build directory:
+build_generic

lwext4 compile Linux
====================
Tools needed:
 - CMake:  http://www.cmake.org/cmake/resources/software.html
 
Build:
>>make

Clean:
>>clean

Successful build generates out of source build directory:
+build_generic

lwext4 generic demo app
=======================

Features:
 - load ext2/3/4 images
 - load linux block device with ext2/3/4 part
 - load windows volume with ext2/3/4 filesystem 
 - directory speed test
 - file write/read speed test

How to use:
Windows/Linux fileimages:
> cd build_generic
> fileimage_demo --in ext2 

Windows volumes:
> cd build_generic
> fileimage_demo --in I: --wpart

Linux block devices:
> cd build_generic
> fileimage_demo --in /dev/your_block_device

Usage:                                                          
    --i   - input file              (default = ext2)            
    --rws - single R/W size         (default = 1024 * 1024)     
    --rwc - R/W count               (default = 10)                     
    --cache  - 0 static, 1 dynamic  (default = 1)               
    --dirs   - directory test count (default = 0)               
    --clean  - clean up after test                              
    --bstat  - block device stats                               
    --sbstat - superblock stats                                 
    --wpart  - windows partition mode                           


lwext4 compile Cross
====================

Toolchain for ARM Cortex-m3/4: https://launchpad.net/gcc-arm-embedded
Toolchain for Blackfin: http://blackfin.uclinux.org/doku.php

Build bf158 library:
> make bf518

Build cortex-m3 library:
> make cortex-m3

Build cortex-m4 library:
> make cortex-m4

lwext4 ports
============

Blackfin BF518 EZKIT SD Card Demo: TBD
ETM32F4-Dis SD Card Demo: TBD 


lwext4 footprint
================                              
