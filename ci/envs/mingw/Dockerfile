# SPDX-License-Identifier: BSD-3-Clause
# base: base
# Windows (x86_64, MinGW-w64): the mingw-w64 cross GCC builds lwext4, the
# fs_test tools and the regression tests as Windows executables, and Wine
# runs them. Wine can only run x86_64 Windows programs on an x86_64 host,
# hence the "# platform: linux/amd64" header of the job.
ARG BASE
FROM ${BASE}

RUN apt-get update && apt-get install -y --no-install-recommends \
	file \
	gcc-mingw-w64-x86-64 \
	wine \
	wine64 \
	&& rm -rf /var/lib/apt/lists/*

# Quiet Wine: no debug channels, no Gecko/Mono installer prompts.
ENV WINEDEBUG=-all \
	WINEDLLOVERRIDES="mscoree,mshtml="
