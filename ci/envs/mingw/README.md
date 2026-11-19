# mingw

Builds on [base](../base). Adds the Windows cross toolchain and Wine:

| Package | Why |
|---|---|
| gcc-mingw-w64-x86-64 | builds Windows x86_64 executables (LLP64: 32-bit `long`) |
| wine, wine64 | runs them, including the tests, on Linux |
| file | shows what was built |

Wine runs x86_64 Windows programs only on an x86_64 host, so the job has a
`# platform: linux/amd64` header (on arm64 it needs Docker's amd64
emulation).

Used by `ci/jobs/mingw.sh`: builds lwext4, the `fs_test` tools and the
regression tests with `toolchain/mingw.cmake`, then runs the CTest suite
and the e2fsprogs round trip with the `.exe` files under Wine (Wine is the
`CMAKE_CROSSCOMPILING_EMULATOR`). The tools use the portable stdio block
device (`blockdev/linux/file_dev.c`) on image files; `blockdev/windows`
(raw `\\.\PhysicalDrive` access) needs a real Windows disk and is only
compiled.

```sh
ci/run.sh mingw
ci/run.sh --shell mingw
```

What it teaches: testing the Windows build of portable C code on Linux.
