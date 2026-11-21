# SPDX-License-Identifier: BSD-3-Clause
"""Check that the documented suites are wired into CI without running them."""

from pathlib import Path
import re
import shlex
import sys

import yaml


ROOT = Path(__file__).resolve().parents[2]
CTEST = 'ctest --test-dir "$build" --no-tests=error'
SUITES = {
    "native gcc": {
        "ci/jobs/native.sh": [
            'ctest --test-dir "$build/b" --no-tests=error',
            "ci/scripts/fs-roundtrip.sh",
            "sh tests/acceptance/test-legacy.sh",
        ],
        "tests/acceptance/test-legacy.sh": ["make -j1 test", "make -j1 test_all"],
    },
    "qemu-user s390x": {
        "ci/jobs/qemu-user.sh": [
            CTEST,
            "ci/scripts/fs-roundtrip.sh",
            "sh tests/acceptance/test-features.sh",
            "CRASH_STEP=3 sh tests/acceptance/test-journal.sh",
        ],
    },
    "cortex-m cortex-m3": {"ci/jobs/cortex-m.sh": [CTEST]},
    "avr": {"ci/jobs/avr.sh": [CTEST]},
}
SUPPORTING_JOBS = {
    "debian-install": {"ci/jobs/debian-install.sh": ["cmake --install"]},
    "examples": {
        "ci/jobs/examples.sh": ["ci/scripts/check-examples.sh", "python3 ci/scripts/check-docs.py"],
    },
}
RUNNER = "ci/run.sh"
MATRIX_REF = re.compile(r"\$\{\{\s*matrix\.(\w+)\s*\}\}")


def invocations(job):
    """Resolve the direct and single-axis matrix run steps used by CI."""
    if "if" in job:
        return set()

    matrix = job.get("strategy", {}).get("matrix", {})
    commands = set()
    for step in job.get("steps", []):
        run = step.get("run", "").strip()
        if "if" in step or not run.startswith(RUNNER + " "):
            continue
        tail = run.removeprefix(RUNNER).strip()
        refs = MATRIX_REF.findall(tail)
        if not refs:
            commands.add(tail)
            continue

        # Match only matrix variants that can actually run this step.
        if len(refs) != 1 or set(matrix) - {refs[0], "exclude"}:
            continue
        axis = refs[0]
        excluded = {entry.get(axis) for entry in matrix.get("exclude", [])}
        for value in matrix.get(axis, []):
            if value not in excluded:
                commands.add(MATRIX_REF.sub(value, tail))
    return commands


def check_commands(path, expected):
    lines = (ROOT / path).read_text().splitlines()
    for command in expected:
        if not any(
            line.strip() == command or line.lstrip().startswith(command + " ")
            for line in lines
        ):
            raise SystemExit(f"FAIL: {path} does not invoke {command}")


def main():
    documented = set(Path(sys.argv[1]).read_text().splitlines())
    missing = SUITES.keys() - documented
    if missing:
        raise SystemExit(f"FAIL: README is missing CI commands: {', '.join(sorted(missing))}")

    workflow = yaml.load(
        (ROOT / ".github/workflows/ci.yml").read_text(), Loader=yaml.BaseLoader
    )
    if not {"push", "pull_request"} <= set(workflow.get("on", {})):
        raise SystemExit("FAIL: CI must run on pushes and pull requests")

    jobs = workflow["jobs"]
    docs = jobs["readme-docs"]
    if "readme-docs" not in invocations(docs):
        raise SystemExit("FAIL: CI does not run readme-docs")

    for command in sorted(documented | SUPPORTING_JOBS.keys()):
        job_script = shlex.split(command)[0]
        if not (ROOT / f"ci/jobs/{job_script}.sh").is_file():
            raise SystemExit(f"FAIL: CI job script is missing for {command}")

        if not any(
            command in invocations(jobs[name]) for name in docs.get("needs", [])
        ):
            raise SystemExit(f"FAIL: readme-docs does not depend on a job running {command}")

        checks = SUITES.get(command, SUPPORTING_JOBS.get(command, {}))
        for path, suite_commands in checks.items():
            check_commands(path, suite_commands)
        print(f"ok: readme-docs depends on CI running {command}")


if __name__ == "__main__":
    main()
