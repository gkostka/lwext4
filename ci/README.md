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

GitHub Actions workflows call `ci/run.sh <job>` and nothing else, so CI and
local runs are identical.
