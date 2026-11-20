# coverage

Builds on [native](../native). Adds:

| Package | Why |
|---|---|
| gcovr | turns the gcc `--coverage` counters into per-file line and branch reports (text, HTML, JSON) |

Used by `ci/jobs/coverage.sh`: build the generic target with
`gcc --coverage -O0`, run the whole CTest suite and the e2fsprogs round trip
(`ci/scripts/fs-roundtrip.sh`), then report the line and branch coverage of
the library (`src/`). The job fails if the totals drop below the floor in
`ci/coverage-floor`, so coverage can only go up: when tests raise it, raise
the floor in the same change.

```sh
ci/run.sh coverage
# text summary in the log, details in:
#   build-ci/coverage/report/index.html   per-file, annotated source
#   build-ci/coverage/summary.md          the table from the log
#   build-ci/coverage/coverage.json       gcovr JSON summary
```

What it teaches: how to measure what the tests actually exercise, and
where the untested code is.
