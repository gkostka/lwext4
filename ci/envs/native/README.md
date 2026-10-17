# native

Builds on [base](../base). Adds the host compilers:

| Package | Why |
|---|---|
| gcc, libc6-dev | default compiler for the generic (Linux file image) target |
| clang, libclang-rt-dev | second compiler with different warnings and code generation; sanitizer runtimes |
| file | shows what was built |

Used by `ci/jobs/native.sh <gcc|clang|asan-ubsan>`: build the generic target
for the machine the container runs on (x86_64 in GitHub Actions, arm64 on a
Raspberry Pi), run the CTest suite in `tests/` and the e2fsprogs round trip
(`ci/scripts/fs-roundtrip.sh`: images made by `mke2fs` are modified by
lwext4 and checked by `e2fsck`, images made by `lwext4-mkfs` are checked by
`e2fsck`). `asan-ubsan` builds everything with
`-DLWEXT4_SANITIZE=address,undefined`, so memory errors, leaks
(`ci/lsan.supp` lists the known ones) and undefined behaviour fail the job.

```sh
ci/run.sh native asan-ubsan
ci/run.sh --shell native      # then e.g. cmake -S . -B build ...
```

What it teaches: the baseline every other environment is compared with,
and how sanitizers turn silent memory corruption into test failures.
