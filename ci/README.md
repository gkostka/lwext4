# CI environments

Every CI job runs inside a container defined in this directory, so the whole
test chain can be reproduced on any machine with only docker installed —
nothing else needs to be set up on the host.

```sh
ci/run.sh --list          # jobs and the environment each one uses
ci/run.sh <job>           # build the image if needed and run the job
ci/run.sh --shell <env>   # interactive shell in an environment
```

- `envs/<env>/Dockerfile` — one image per toolchain/emulator environment.
  Images are tagged with a hash of their directory and rebuilt when it
  changes. Environments should build natively on both x86_64 and arm64.
- `envs/<env>/README.md` — what the environment contains and why. The
  environments double as documentation of how to build lwext4 for each
  target, so keep each one small, readable and pinned to exact versions.

## Principles

- **One environment per scenario, no super-container.** An environment
  holds exactly one toolchain/emulator stack (for example `arm-none-eabi`,
  `esp-idf`, `renode`), so reading its Dockerfile shows everything that
  scenario needs.
- **Reuse through layering, not copying.** Common tooling lives in a base
  environment; others start with `# base: <env>` + `ARG BASE` +
  `FROM ${BASE}`, and `ci/run.sh` builds the chain in order. `ci/run.sh
  --list` shows each job's chain.
- **Jobs are plain scripts.** Anything a job does can be read in
  `jobs/<job>.sh` and repeated by hand inside `ci/run.sh --shell <env>`.
- `jobs/<job>.sh` — the job itself, run from the repository root inside the
  container. The `# env:` header selects the image; an optional
  `# platform: linux/amd64` header is only for tools without an arm64 build.

GitHub Actions workflows call `ci/run.sh <job>` and nothing else (the one
exception is macOS, see below), so CI and
local runs are identical.

## Running on a shared machine

Outside CI (`CI` unset), `ci/run.sh` starts containers with the lowest CPU
weight (`--cpu-shares 2`, cgroup `cpu.weight` 1) and I/O weight, and runs the
job under `nice -n 19` inside the container. Several jobs can run at once
and still only use capacity other work leaves idle. Running `nice` on the
host has no effect on containers, whose processes are started by the
docker daemon. Set `CI_PRIORITY=normal` to run at normal priority, and
`CI_JOBS=<n>` to cap build parallelism.

## The one exception: macOS

macOS cannot run in a container, so the macOS job
(`.github/workflows/macos.yml`) runs `host/macos.sh` directly on the GitHub
macOS runner (arm64, Apple clang and ld). It is the only job that needs
anything on the host: the Xcode command line tools, cmake, and e2fsprogs
from Homebrew (keg-only, so its `sbin` has to be put on `PATH`). To run it
on a Mac:

```sh
brew install e2fsprogs
PATH="$(brew --prefix e2fsprogs)/sbin:$PATH" ci/host/macos.sh [clang|asan-ubsan]
```

It uses the same `scripts/` as the container jobs, which is why those stay
portable (POSIX sh, BSD userland: no GNU-only tools such as `nproc`).
Everything else stays in `ci/run.sh`.
