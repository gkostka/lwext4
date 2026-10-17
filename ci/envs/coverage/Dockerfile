# SPDX-License-Identifier: BSD-3-Clause
# base: native
# gcovr on top of the native compilers: line and branch coverage of the
# CTest suite (ci/jobs/coverage.sh).
ARG BASE
FROM ${BASE}

RUN apt-get update && apt-get install -y --no-install-recommends \
	gcovr \
	&& rm -rf /var/lib/apt/lists/*
