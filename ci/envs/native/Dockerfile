# SPDX-License-Identifier: BSD-3-Clause
# base: base
# Host compilers for the native jobs: gcc and clang (with the sanitizer
# runtimes) building lwext4 for the machine the container runs on.
ARG BASE
FROM ${BASE}

RUN apt-get update && apt-get install -y --no-install-recommends \
	clang \
	file \
	gcc \
	libc6-dev \
	libclang-rt-dev \
	&& rm -rf /var/lib/apt/lists/*
