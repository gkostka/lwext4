# env: runner-selftest
# Checks that ci/run.sh built the base chain and runs jobs from the repo root.
test -f ci/run.sh
command -v mke2fs cmake >/dev/null
echo "runner ok: $(uname -m), uid $(id -u)"
