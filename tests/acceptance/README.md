# README acceptance tests

Everything README.md says lwext4 can do, or tells the reader to run, is
tested here mechanically. The oracle is e2fsprogs: `e2fsck -fn` must find
nothing to fix after lwext4 wrote to an image, and `debugfs` must read back
what lwext4 wrote (and lwext4 must read back what e2fsprogs wrote).

The tests run in the containers of [ci/](../../ci), locally exactly as in
GitHub Actions ([readme-acceptance.yml](../../.github/workflows/readme-acceptance.yml)):

```sh
ci/run.sh readme-docs          # README.md vs. repository and this inventory
ci/run.sh readme-debian        # README's commands with only README's Debian packages
ci/run.sh readme-native        # README's commands in README order, checked by e2fsprogs
ci/run.sh readme-api           # feature claims through the API, journal, power loss
ci/run.sh readme-tools         # every tool option, library build claims
ci/run.sh readme-cross-linux   # README's big endian example, then the API tests on s390x
ci/run.sh readme-arm-none-eabi # Cortex-M builds, memory footprint, baremetal on QEMU
ci/run.sh readme-avr           # AVR build
```

## How README.md's commands are run

[readme-blocks.sh](readme-blocks.sh) extracts the fenced code blocks from
README.md and runs them unmodified with `bash -ex`, one shell per block. A
block's key is the slug of its heading plus its number under that heading.
The only rewrite: CI is unprivileged, so `sudo` is replaced by a `DESTDIR`
that points that one `make install` at a staging directory whose `bin` comes
first in `PATH` (the later blocks run the installed tools). `readme-blocks.sh check`
(job readme-docs) fails when a README.md block is missing from the table
below or the table names a block that no longer exists.

## Scripts

| Script | What it does |
|---|---|
| [lwext4_acceptance.c](lwext4_acceptance.c) | `lwext4-acceptance`: runs a script of lwext4 API calls, with power loss injection and block I/O counters |
| [lib.sh](lib.sh) | helpers: e2fsck/debugfs oracles, test data pattern |
| [readme-blocks.sh](readme-blocks.sh) | extracts and runs README.md code blocks |
| [test-docs.sh](test-docs.sh) | README.md vs. repository (blocks, Debian packages, project tree, make targets) |
| [test-readme-native.sh](test-readme-native.sh) | README.md commands in order, results checked by e2fsprogs |
| [test-features.sh](test-features.sh) | feature claims, both directions of e2fsprogs interoperability |
| [test-journal.sh](test-journal.sh) | journal replay, power loss at every block write, cache modes |
| [test-tools.sh](test-tools.sh) | every option of the five fs_test tools |
| [test-build.sh](test-build.sh) | lib_only, shared library, C library only dependency, licensing |
| [test-cortex-m.sh](test-cortex-m.sh) | Cortex-M libraries, memory footprint, tests/baremetal |

## Inventory: README.md claim → test

Status: **pass** = passes as the README states; **fixed** = was broken,
fixed in this series (see Findings); **doc** = README.md was wrong and has
been corrected; **n/a** = cannot be verified in CI, reason given.

### Commands (README.md code blocks)

| README block | Claim | Test | Status |
|---|---|---|---|
| `readme:dependencies#1` | MSYS2: `pacman -S make gcc cmake` | not run: needs Windows. The MinGW build of the same sources is covered by `ci.yml` (build / mingw) | n/a |
| `readme:dependencies#2` | Debian: `apt-get install make gcc cmake` is enough | the acceptance-debian image installs exactly this (checked by test-docs.sh), readme-debian builds, installs and tests in it | doc (p7zip removed: the 7z image archive is gone) |
| `readme:compile-install-tools#1` | `make generic; cd build_generic; make; sudo make install` | readme-debian, readme-native: run as is (see sudo above); the five installed tools must run | pass |
| `readme:lwext4-generic-demo-application#1` | `lwext4-generic -i ext_images/ext2..4` | readme-native, after `make test` created the images; e2fsck clean, debugfs reads back /test1 and /hello.txt | doc (images come from `make test`/`images_small`, not from the removed ext_images.7z) |
| `readme:lwext4-generic-demo-application#2` | `lwext4-generic --help` shows the full option set | readme-native, readme-debian; test-tools.sh checks every option is documented | fixed (exited 1 as unknown option; `-w` listed twice, `-s`, `-v`, `-x` missing) |
| `readme:run-automatic-tests#1` | `make test` runs the suite on the ext2/3/4 images | readme-native: t0..t20 on 128 MiB images, then e2fsck clean and nothing left behind | fixed, fix/fs-test-mk-rootless (needed `sudo mkfs`; left the server running on failure) |
| `readme:run-automatic-tests#2` | `make test_all`: 1 GiB images + fsck | readme-native: t0..t26 including 512 MiB file, 50000 files/dirs, then e2fsck | fixed, fix/fs-test-mk-rootless (as above; `sudo fsck` replaced by `e2fsck -fn`) |
| `readme:run-regression-tests#1` | `make generic; cd build_generic; make; ctest --output-on-failure` | readme-native, readme-debian | pass |
| `readme:run-regression-tests#2` | s390x cross build, ctest and fs-roundtrip.sh under qemu-user | readme-cross-linux, verbatim; then test-features.sh and test-journal.sh with the s390x build | fixed (fix/ondisk-byte-order) |
| `readme:run-regression-tests#3` | `ci/run.sh --list`, `ci/run.sh qemu-user s390x`, `ci/run.sh avr` | needs docker, so not run inside a container; test-docs.sh checks every job README.md shows exists in ci/jobs and is a job (with that matrix value) of ci.yml, which runs exactly these commands | pass |
| `readme:using-lwext4-mkfs-tool#1` | `dd ... count=1024` makes a 1 GiB image | readme-native, readme-debian | pass |
| `readme:using-lwext4-mkfs-tool#2` | `lwext4-mkfs -i ext_image -e 2` creates ext2 | readme-native: e2fsck clean, no journal, no extents; lwext4-generic then works on it | fixed (created a journal, i.e. ext3) |
| `readme:using-lwext4-mkfs-tool#3` | `-e 3` creates ext3 | readme-native: journal, no extents | pass |
| `readme:using-lwext4-mkfs-tool#4` | `-e 4` creates ext4 | readme-native, readme-debian: journal and extents, e2fsck clean | pass |
| `readme:using-lwext4-mkfs-tool#5` | `lwext4-mkfs --help` shows the full option set | readme-native, test-tools.sh | fixed (exited 1) |
| `readme:build-avrxmega7-library#1` | `make avrxmega7; cd build_avrxmega7; make lwext4` | readme-avr: AVR liblwext4.a | pass |
| `readme:build-cortex-m0-library#1` | `make cortex-m0 ...` | readme-arm-none-eabi: ARM liblwext4.a | pass |
| `readme:build-cortex-m3-library#1` | `make cortex-m3 ...` | readme-arm-none-eabi | pass |
| `readme:build-cortex-m4-library#1` | `make cortex-m4 ...` | readme-arm-none-eabi | pass |

### Statements

| Claim (README.md section) | Test | Status |
|---|---|---|
| directory indexing - fast directory find (About) | test-features.sh: lwext4 builds htrees (1 and 2 levels, `stat` flag 0x1000, e2fsck validates); lookups in a 12000 entry directory need ≤20 block reads and ≥10x fewer than without dir_index | fixed (2 level htrees with metadata_csum were corrupt / crashed) |
| extents - fast big file truncate (About) | test-features.sh: truncating 64 MiB needs ≥10x fewer block I/Os with extents than with block maps | pass |
| journaling transactions & recovery - power loss resistance (About) | test-journal.sh: power loss after every single block write of a workload (mkdir, write, rename, link, symlink, xattr, remove, truncate, mknod, chmod, dir_mv, dir_rm, 17 MB file), ext3/ext4, 1k/4k, write-through and write-back; after ext4_recover and after e2fsck's journal replay e2fsck -fn is clean | fixed (dir_rm/fremove were split over several transactions; new directories had a bad leaf checksum) |
| journal recovery of journals written by others | test-journal.sh: debugfs writes transactions (with and without journal checksums, with a revoke record), ext4_recover replays them | pass |
| filetypes: regular, directories, softlinks; hardlinks (Features) | test-features.sh: created by lwext4 and verified by debugfs (fast and slow symlinks, link counts, same inode), created by e2fsprogs and read by lwext4 | fixed (slow symlink blocks were not zero padded, e2fsck cleared them) |
| multiple blocksize supported: 1KB ... 64KB (Features) | test-features.sh: ext4 with 1, 2, 4, 8, 16, 32, 64 KiB blocks | pass |
| little/big endian architectures supported (Features) | readme-cross-linux: test-features.sh and test-journal.sh on s390x under qemu-user | fixed (fix/ondisk-byte-order: hardlinks/renames, htree hash seed and index nodes, xattr value offsets, extent splits, uid/gid) |
| multiple configurations (ext2/ext3/ext4) (Features) | test-features.sh on ext2, ext3, ext4 images; test-cortex-m.sh builds an ext2 only configuration | fixed (CONFIG_XATTR_ENABLE=0 did not link) |
| only C standard library dependency (Features) | test-build.sh: undefined symbols of liblwext4.a (lib_only) are C library functions only | pass |
| various CPU architectures (Features) | readme-cross-linux, readme-arm-none-eabi, readme-avr; ci.yml covers more | pass |
| Memory footprint: cortex-m4 .text, RAM, stack | test-cortex-m.sh measures .text of ext2 only and full builds, and peak heap + static data + stack of a workload on QEMU mps2-an386 (footprint.c); README.md's numbers must be within 25% | doc (README said 20/50 KB .text and 8/12 KB .data; measured 48/65 KiB .text, 13/19 KiB RAM, <1 KiB stack) |
| block cache should not allocate more than CONFIG_BLOCK_DEV_CACHE_SIZE | test-features.sh (`cache_check` after every workload), test-tools.sh (`--bstat`) | pass |
| Supported features, incompatible **yes** (filetype, recover, meta_bg, extents, 64bit, flex_bg) | test-features.sh: images with each feature (and without), written by lwext4, e2fsck clean, debugfs reads back; recover: test-journal.sh | fixed (see above) |
| Supported features, compatible **yes** (has_journal, ext_attr, dir_index) | test-features.sh (xattrs set/get/list/remove both ways) | fixed (removing an in-inode xattr corrupted memory) |
| Supported features, read-only **yes** (sparse_super, large_file, huge_file, gdt_csum, dir_nlink, extra_isize, metadata_csum) | test-features.sh: 5 GiB sparse files read and appended beyond 4 GiB; uninit_bg; 65010 subdirectories (link count 1); 128 and 256 byte inodes | pass |
| unsupported incompatible features | test-features.sh: inline_data, large_dir, ea_inode, journal_dev refused with ENOTSUP, image untouched; mmp is ignored and e2fsck stays clean | doc (mmp behaviour documented) |
| unsupported read-only features | test-features.sh: quota, bigalloc mounted read-only, writes refused, image untouched (bigalloc: reading fails with an error, never wrong data) | doc |
| e2fsprogs ≥ 1.47 defaults | test-features.sh: metadata_csum_seed refused, images made with the documented options work | doc (new note) |
| GPLv2 files: ext4_xattr.c, ext4_extent.c; everything else BSD-3-Clause | test-build.sh compares the list with the license headers | doc (README said ext4_extents.c) |
| "To use library as a BSD3, GPLv2 licensed source files must be removed first" | test-build.sh: library builds and links without the two files (extents and xattr disabled) | fixed, fix/xattr-disabled-build (CONFIG_XATTR_ENABLE=0 left undefined references) |
| Project tree | test-docs.sh: every entry exists | doc (ext_images.7z removed) |
| lwext4-generic: load images, directory and file speed tests | test-tools.sh: every option (-i -s -c -d -l -b -t -w -v -x -h, long forms), results checked by debugfs | fixed (--verbose had no effect, debug output was always on) |
| lwext4-generic: load linux block device / windows volume | n/a: needs root (loop device) or Windows; the block device path uses the same stdio code as image files | n/a |
| lwext4-mkfs: ext2/3/4, block sizes (--help) | test-tools.sh: -e 2/3/4 × -b 1024/2048/4096 on an image with a partial last group, e2fsck clean, then used by lwext4-generic; rejects -b 512/8192, -e 5 | fixed (free block count of partial groups, fix/issue-72) |
| lwext4-mbr, lwext4-server/-client (tools the README's project tree and `make install` ship) | test-tools.sh: partitions written by sfdisk reported exactly; server/client on custom ports, write back cache mode, failures reported through the exit status | pass |
| Makefile targets lib_only and the LWEXT4_BUILD_SHARED_LIB option | test-build.sh | fixed, fix/shared-lib-size (shared library build failed on the size step) |
| include/ext4.h: mount, recover, journal_start/stop, umount usage | test-features.sh, test-journal.sh follow exactly this sequence; ext4_journal_start/stop on ext2 (no journal) succeed | pass |
| include/ext4.h: write-through by default, write back "data is NOT flushed", nested enable/disable, ext4_cache_flush | test-journal.sh: block write counters and power loss | doc (file data written by ext4_fwrite is never cached; the comment now says so) |
| The Cortex-M toolchains also build tests/baremetal, ctest runs it on QEMU (Run regression tests) | test-cortex-m.sh | pass |

## Findings

Code bugs, each fixed on its own branch based on `tests/harness` with a
red/green regression test (`ci/run.sh red-green`), and included here:

| Branch | Bug |
|---|---|
| fix/xattr-remove-ibody | removing an in-inode xattr used an uninitialised search context (memory corruption) |
| fix/symlink-slow-zero-pad | slow symlink blocks not zero padded, e2fsck clears the symlink |
| fix/dir-mv-dotdot-csum | moving a linear directory leaves a bad block checksum |
| fix/dx-init-leaf-csum (existing) | new htree leaf checksummed before it is filled |
| fix/htree-two-levels | two level htrees broken with metadata_csum; double release of the root block |
| fix/unlink-single-transaction | unlink/rmdir split over several journal transactions: not power loss safe |
| fix/ondisk-byte-order | big endian: several on-disk fields unconverted; uid/gid high 16 bits lost |
| fix/fs-test-tools-options | --help failed, lwext4-mkfs -e 2 created a journal, lwext4-generic always verbose |
| fix/issue-72-mkfs-free-blocks, fix/issue-77-bmap-find-clr (existing) | lwext4-mkfs free block counts; allocation failures on big endian |
| fix/xattr-disabled-build | with `CONFIG_XATTR_ENABLE=0` and ext4_xattr.c removed (the BSD-3-Clause only build) nothing linked |
| fix/shared-lib-size | `LWEXT4_BUILD_SHARED_LIB=ON` builds failed in the `lib_size` step |
| fix/fs-test-mk-rootless | `make test`/`make test_all` needed sudo, made images lwext4 cannot mount with e2fsprogs 1.47 and left the server running on failure |

The acceptance scripts check the same behaviour end to end (test-build.sh,
readme-native).
