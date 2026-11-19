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

Getting started
=====

lwext4 is a library: your program registers a *block device* (a struct
with read/write callbacks for your storage), formats or mounts it, and then
uses a file API much like stdio. Everything below runs on a PC first.

1. Build the library, the tools and the examples (see [Compile](#compile)
   for the dependencies):
   ```bash
    make generic
    cd build_generic
    make
    ```
2. Run the basic example and check its result with e2fsprogs:
   ```bash
    ./examples/lwext4-example-basic disk.img
    e2fsck -fn disk.img
    debugfs -R 'ls -l /docs' disk.img
    ```
   [examples/basic/main.c](examples/basic/main.c) walks through the whole
   life cycle step by step: block device, `ext4_mkfs`, mount, journal,
   write-back cache, directories and files, unmount.
3. Port it to your hardware by writing a block device:
   [examples/blockdev-template](examples/blockdev-template/my_blockdev.c)
   is an annotated skeleton that CI runs on a RAM disk.
4. Pick the features and buffer sizes you need with the `CONFIG_*`
   options of [include/ext4_config.h](include/ext4_config.h) and build for
   your target with a toolchain file from [toolchain/](toolchain), e.g.
   `make cortex-m4`.

The core of a program, as a function (CI compiles this snippet, like every
C snippet in this file):
```c
#include <ext4.h>
#include <ext4_mkfs.h>
#include <blockdev/file_dev.h>

/* Format an image file as ext4, then write "hello" to /mp/hello.txt. */
int format_and_write(const char *image)
{
	static struct ext4_fs fs; /* used by ext4_mkfs only; large */
	struct ext4_mkfs_info info = {.block_size = 1024, .journal = true};
	struct ext4_blockdev *bd;
	ext4_file f;
	size_t written;
	int r;

	file_dev_name_set(image);  /* the block device: a file */
	bd = file_dev_get();

	r = ext4_mkfs(&fs, bd, &info, F_SET_EXT4);
	if (r != EOK)
		return r;

	r = ext4_device_register(bd, "disk");
	if (r != EOK)
		return r;
	r = ext4_mount("disk", "/mp/", false);
	if (r == EOK) {
		ext4_recover("/mp/");       /* replay the journal if needed */
		ext4_journal_start("/mp/");

		r = ext4_fopen(&f, "/mp/hello.txt", "wb");
		if (r == EOK) {
			r = ext4_fwrite(&f, "hello\n", 6, &written);
			ext4_fclose(&f);
		}

		ext4_journal_stop("/mp/");
		ext4_umount("/mp/");
	}
	ext4_device_unregister("disk");
	return r;
}
```

More examples:
* [examples/](examples/README.md) - the host examples above and how to
  check what they write.
* ESP32, ESP32-C3 and ESP32-S3 (ESP-IDF): an lwext4 component with block
  devices for SPI flash partitions and SD cards, and an example firmware
  that CI runs in Espressif's QEMU:
  [examples/esp-idf in antimatter-studios/lwext4](https://github.com/antimatter-studios/lwext4/tree/integration/examples/esp-idf).
* [fs_test/](fs_test) - the `lwext4-generic`, `lwext4-mkfs` and
  `lwext4-mbr` tools (see below) are complete programs too.

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
* partition tables: MBR including logical partitions, and GPT
  (`ext4_partition_scan` in `ext4_partition.h`)

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
*  examples         - example programs (see [Getting started](#getting-started))
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

Installation goes to /usr/local by default. To install somewhere else, pick
the prefix at install time or when configuring:
```bash
 cmake --install build_generic --prefix $HOME/.local
 cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local build_generic
 ```

Using the installed library
------------
`make install` puts the library, the file block device library and their
headers below the prefix, together with a pkg-config file and a CMake
package:
```
 include/lwext4/                 ext4.h, ext4_mkfs.h, ... generated/ext4_config.h
 include/lwext4/blockdev/        blockdev.h, file_dev.h (file_windows.h on Windows)
 lib/liblwext4.a                 the library (liblwext4.so with -DLWEXT4_BUILD_SHARED_LIB=ON)
 lib/libblockdev.a               file_dev_get(): an image file or device as ext4_blockdev
 lib/pkgconfig/lwext4.pc
 lib/cmake/lwext4/               lwext4Config.cmake, lwext4ConfigVersion.cmake, targets
 ```
Both package files find the prefix relative to their own location, so an
install tree can be moved or unpacked elsewhere. A program includes
`<ext4.h>` (and `<blockdev/file_dev.h>` for the file block device) and is
built with any of:
```bash
 cc app.c $(pkg-config --cflags --libs lwext4)
 cc app.c -I$PREFIX/include/lwext4 -L$PREFIX/lib -lblockdev -llwext4
 ```
```cmake
 find_package(lwext4 1.0 CONFIG REQUIRED)
 target_link_libraries(app lwext4::blockdev lwext4::lwext4)
 ```
(set `PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig` or
`CMAKE_PREFIX_PATH=$PREFIX` for a prefix outside the default search path).
The `install_package` CTest test builds and runs such a program in all
three ways; `tests/package/` is a complete example.

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

Run regression tests
=====

Small self-contained tests live in `tests/` and run through CTest. They need
`mke2fs` (e2fsprogs), `sfdisk` (util-linux) and `python3` on the host to
build their images; no root access is required:
```bash
 make generic
 cd build_generic
 make
 ctest --output-on-failure
   ```
To run them with AddressSanitizer/UBSan, configure with
`-DLWEXT4_SANITIZE=address,undefined`.

Other Linux architectures can be tested under qemu-user with the
`toolchain/linux-cross.cmake` toolchain (`gcc-<triple>` and `qemu-user`
packages). The byte order is taken from the compiler:
```bash
 cmake -S . -B build_s390x -DCMAKE_TOOLCHAIN_FILE=toolchain/linux-cross.cmake \
       -DCROSS_TRIPLE=s390x-linux-gnu -DCROSS_EMULATOR=qemu-s390x
 cmake --build build_s390x
 ctest --test-dir build_s390x --output-on-failure
 ci/scripts/fs-roundtrip.sh build_s390x qemu-s390x -L /usr/s390x-linux-gnu
   ```
`fs-roundtrip.sh` formats images with mke2fs and lwext4-mkfs, exercises them
with lwext4-generic and checks the result with e2fsck.

The bare-metal and simulator toolchains build `tests/baremetal`, a test
firmware (unit tests, read-only mounts of mke2fs images, ext4_mkfs and
read/write tests on a RAM disk where there is RAM for one) that `ctest`
runs on an emulator: `cortex-m*` on QEMU MPS2 boards, `arm-sim` on
qemu-arm, `atmega1284` on simavr and `msp430-sim` on msp430-elf-run. The
MinGW build runs its tests under Wine.

Every CI job runs in a container defined in `ci/`, so the whole chain runs
locally with only docker installed, exactly as in GitHub Actions
(`.github/workflows/ci.yml`):
```bash
 ci/run.sh --list
 ci/run.sh qemu-user s390x
 ci/run.sh avr
   ```
See `ci/README.md`.

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
not done yet. Lwext4 code is written with endianes respect. The byte order
is taken from the compiler (`__BYTE_ORDER__`, or the usual per architecture
macros such as `__ARMEB__`/`_MIPSEB`). `CONFIG_BIG_ENDIAN` (1 or 0) overrides
it for compilers that define none of them; a value that contradicts the
compiler is a build error.

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


