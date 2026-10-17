# base

Shared starting point for the other environments. It only contains what
every lwext4 test scenario needs regardless of the target:

| Package | Why |
|---|---|
| cmake, make | lwext4's build system |
| e2fsprogs | `mke2fs` creates test images, `e2fsck`/`debugfs` independently verify what lwext4 wrote |
| python3 | small helper scripts (image patching, output checks) |
| git, xz-utils, ca-certificates | fetching and unpacking pinned toolchains in derived environments |

No compiler is installed here on purpose: each environment adds exactly the
toolchain it teaches.
