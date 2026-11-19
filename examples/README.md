# lwext4 examples

Small, complete programs that show how lwext4 is used. They are built with
the generic (PC) target and run by CI, which checks the filesystems they
write with `e2fsck -fn` and `debugfs` (`ci/run.sh examples`).

| Example | What it shows |
|---|---|
| [basic](basic/main.c) | The whole life cycle on a PC: register a file-backed block device, `ext4_mkfs`, mount, journal recovery and start, write-back cache, create directories, write, read, list, rename and remove files, unmount. Commented step by step. |
| [blockdev-template](blockdev-template/my_blockdev.c) | An annotated skeleton of a custom `struct ext4_blockdev`: the part you write to port lwext4 to new storage hardware. Runs on a RAM disk ([ram_storage.c](blockdev-template/ram_storage.c)) so it is tested on a PC; replace the `storage_*()` functions of [storage.h](blockdev-template/storage.h) with your driver. |

## Build and run

```bash
 make generic
 cd build_generic
 make lwext4-example-basic lwext4-example-blockdev-template
 ./examples/lwext4-example-basic disk.img
 ./examples/lwext4-example-blockdev-template ram.img
 ```

Both leave an ordinary ext4 image behind. With e2fsprogs installed:

```bash
 e2fsck -fn disk.img                      # consistency check, changes nothing
 debugfs -R 'ls -l /docs' disk.img        # list a directory
 debugfs -R 'cat /docs/hello.txt' disk.img
 ```

On Linux the image can also be mounted: `sudo mount -o loop disk.img /mnt`.

## From here

- The API is declared, with comments, in [include/ext4.h](../include/ext4.h);
  formatting in [include/ext4_mkfs.h](../include/ext4_mkfs.h), partition
  tables in [include/ext4_mbr.h](../include/ext4_mbr.h).
- Features and buffer sizes are chosen at compile time with `CONFIG_*`
  options: see [include/ext4_config.h](../include/ext4_config.h) for the
  list and defaults, and [CMakeLists.txt](../CMakeLists.txt) for the values
  each target uses.
- For microcontrollers, build the library with a toolchain file from
  [toolchain/](../toolchain) (e.g. `make cortex-m4`) and link it with your
  block device.
