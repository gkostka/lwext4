# SPDX-License-Identifier: BSD-3-Clause
# Helpers sourced by ci/jobs/*.sh (which run with bash -eu from the
# repository root inside their container).

export CMAKE_POLICY_VERSION_MINIMUM=3.5 # CMakeLists.txt still says 3.4
export CTEST_OUTPUT_ON_FAILURE=1

# Fresh build directory for a job, below build-ci/ in the repository.
ci_build_dir()
{
	rm -rf "build-ci/$1"
	mkdir -p "build-ci/$1"
	echo "build-ci/$1"
}

# Parallel build jobs (CI_JOBS overrides, e.g. on a shared machine).
ci_jobs()
{
	echo "${CI_JOBS:-$(nproc)}"
}
