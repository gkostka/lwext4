[![Build Status](https://travis-ci.org/gkostka/lwext4.svg)](https://travis-ci.org/gkostka/lwext4)

About
=====

The main goal of the lwext4 project is to provide ext2/3/4 filesystem for microcontrollers. It may be an interesting alternative for traditional MCU filesystem libraries (mostly based on FAT32).

Lwext4 may be used with SD/MMC card, USB flash drive or other block based memory device. However it is not good for flash memory–based storage devices.

Code is also available on github:
https://github.com/gkostka/lwext4

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
* metadata checksum suport
* many bugfixes & improvements

fuse-lwext4 project:
* https://github.com/ngkaho1234/fuse-lwext4

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


Memory footprint (for cortex-m4)
------------
* .text:  20KB - 40KB 
* .data:  8KB         (minimum 8 x 1KB  block cache)
* .stack: 2KB

Supported ext2/3/4 features
=====
Features incompatible:
------------
*  compression: no
*  filetype: yes
*  recover: no
*  journal_dev: no
*  meta_bg: yes
*  extents: yes
*  64bit: yes
*  mmp: no
*  flex_bg: yes
*  ea_inode: no
*  dirdata: no
*  bg_meta_csum: no
*  largedir: no
*  inline_data: no

Features compatible:
------------
*  dir_prealloc: no
*  imagic_inodes: no
*  has_journal: no
*  ext_attr: yes
*  resize_inode: no
*  dir_index: yes

Features read-only:
------------
*  sparse_super: yes
*  large_file: yes
*  btree_dir: yes
*  huge_file: yes
*  gdt_csum: yes
*  dir_nlink: yes
*  extra_isize: yes
*  quota: no
*  bigalloc: no
*  metadata_csum: yes


Project tree
=====
*  blockdev         - block devices set, supported blockdev
*  demos            - demo directory sources
*  fs_test          - test suite
*  lwext4           - internals of the lwext4 library
*  toolchain        - specific toolchain cmake files
*  CMakeLists.txt   - CMake config file
*  ext_images.7z    - compressed ext2/3/4 100MB images
*  fs_test.mk       - automatic tests definitions
*  Makefile         - helper makefile to generate cmake and run test suite
*  readme.mediawiki - readme file
  
Compile
=====
Windows
------------
* CMake:  http://www.cmake.org/cmake/resources/software.html
* MinGw:  http://www.mingw.org/
* GnuWin: http://gnuwin32.sourceforge.net/ 

Linux
------------
* CMake, make, gcc

Generate makefiles
------------
```bash
 make
 ```

Compile
------------
```bash
 cd build_generic
 make
 ```


Generic demo application
=====
Simple lwext4 library presentation:
* load ext2/3/4 images
* load linux block device with ext2/3/4 part
* load windows volume with ext2/3/4 filesystem 
* directory speed test
* file write/read speed test

How to use for images/blockdevices:
```bash
 cd build_generic
 generic -i ext_images/ext2 
 generic -i ext_images/ext3 
 generic -i ext_images/ext4 
 ```


Build and run automatic tests
=====
Build automatic test tools:
```bash
 make
 cd build_generic
 make
  ```
Uncompress ext/2/3/4 images:
```bash
 make unpack_images
   ```
Run server for one of the image file:
```bash
 make server_ext2
 make server_ext3
 make server_ext4
  ```
Execute tests:
```bash
 make test
   ```

Cross compile standalone library
=====
Toolchains needed:
------------
* arm-none-eabi-gcc for cortex-mX
* avr-gcc for avr
* bfin-elf-gcc for bfin
* msp430-gcc for msp430


Build bf518 library:
------------
```bash
 make bf518
 cd build_bf518
 make lwext4
 ```

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


