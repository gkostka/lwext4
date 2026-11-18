#!/bin/sh
# Run a CI job inside its container. The only host requirement is docker.
#
#   ci/run.sh <job> [args...]   build the job's image if needed, run the job
#   ci/run.sh --list            list jobs and the environment each one uses
#   ci/run.sh --shell <env>     interactive shell in an environment
#
# Layout:
#   ci/envs/<env>/Dockerfile    one image per toolchain/emulator environment.
#                               It may build on another environment with
#                                 # base: <env>
#                                 ARG BASE
#                                 FROM ${BASE}
#                               ci/run.sh builds the base first and passes its
#                               tag as BASE, so layers are shared, not copied.
#   ci/envs/<env>/README.md     what the environment contains and why
#   ci/jobs/<job>.sh            job script, run inside the container from the
#                               repository root; its header selects the image:
#                                 # env: <env>
#                                 # platform: linux/amd64   (optional, only for
#                                 #   tools that have no arm64 build)
#
# GitHub Actions calls exactly the same entry point, so a job that passes
# in CI passes locally and vice versa.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)

die()
{
	echo "ci/run.sh: $*" >&2
	exit 1
}

header()
{
	sed -n "s/^# $1: *//p" "$2" | head -n 1
}

env_base()
{
	header base "$root/ci/envs/$1/Dockerfile"
}

image_tag()
{
	# Rebuild whenever anything in the environment directory (or in any
	# environment it builds on) changes
	base=$(env_base "$1")
	hash=$( (cd "$root/ci/envs/$1" && find . -type f | LC_ALL=C sort |
		xargs cat; [ -z "$base" ] || image_tag "$base") |
		sha256sum | cut -c1-16)
	echo "lwext4-ci-$1:$hash"
}

build_image()
{
	env=$1
	platform=$2
	[ -f "$root/ci/envs/$env/Dockerfile" ] || die "unknown environment '$env'"
	base=$(env_base "$env")
	base_tag=
	[ -z "$base" ] || base_tag=$(build_image "$base" "$platform")
	tag=$(image_tag "$env")
	if ! docker image inspect "$tag" >/dev/null 2>&1; then
		echo "ci/run.sh: building $tag" >&2
		docker build ${platform:+--platform "$platform"} \
			${base_tag:+--build-arg BASE="$base_tag"} -t "$tag" \
			"$root/ci/envs/$env" >&2
	fi
	echo "$tag"
}

run_in()
{
	tag=$1
	platform=$2
	shift 2
	tty=
	[ -t 0 ] && [ -t 1 ] && tty=-it
	# Outside CI the host is usually shared: give the container the lowest
	# CPU and I/O weight and run the job at the lowest nice level, so it
	# only uses capacity nobody else wants. Host-side nice does not reach
	# processes started by the docker daemon. CI_PRIORITY=normal opts out.
	prio=
	nice=
	if [ -z "${CI:-}" ] && [ "${CI_PRIORITY:-low}" = low ]; then
		prio="--cpu-shares 2 --blkio-weight 10"
		nice="nice -n 19"
	fi
	docker run --rm $tty ${platform:+--platform "$platform"} $prio \
		-v "$root":/src -w /src \
		-u "$(id -u):$(id -g)" -e HOME=/tmp \
		-e CI="${CI:-}" -e GITHUB_ACTIONS="${GITHUB_ACTIONS:-}" \
		-e CI_JOBS="${CI_JOBS:-}" \
		"$tag" $nice "$@"
}

command -v docker >/dev/null 2>&1 || die "docker is required"
[ $# -ge 1 ] || die "usage: ci/run.sh <job> [args] | --list | --shell <env>"

case "$1" in
--list)
	for f in "$root"/ci/jobs/*.sh; do
		env=$(header env "$f")
		chain=$env
		while base=$(env_base "$env") && [ -n "$base" ]; do
			chain="$chain <- $base"
			env=$base
		done
		printf '%-32s %s\n' "$(basename "$f" .sh)" "$chain"
	done
	;;
--shell)
	[ $# -eq 2 ] || die "usage: ci/run.sh --shell <env>"
	tag=$(build_image "$2" "")
	run_in "$tag" "" bash
	;;
*)
	job="$root/ci/jobs/$1.sh"
	[ -f "$job" ] || die "unknown job '$1' (see ci/run.sh --list)"
	shift
	env=$(header env "$job")
	platform=$(header platform "$job")
	[ -n "$env" ] || die "$job has no '# env:' header"
	tag=$(build_image "$env" "$platform")
	run_in "$tag" "$platform" bash -eu "ci/jobs/$(basename "$job")" "$@"
	;;
esac
