# SPDX-License-Identifier: BSD-3-Clause
# base: base
# Host compilers for the native jobs: gcc and clang (with the sanitizer
# runtimes and llvm-symbolizer for their reports) building lwext4 for the
# machine the container runs on.
ARG BASE
FROM ${BASE}

RUN apt-get update && apt-get install -y --no-install-recommends \
	clang \
	llvm \
	file \
	gcc \
	libc6-dev \
	libclang-rt-dev \
	pkgconf \
	&& rm -rf /var/lib/apt/lists/*
