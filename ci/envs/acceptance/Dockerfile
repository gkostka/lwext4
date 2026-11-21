# SPDX-License-Identifier: BSD-3-Clause
# base: native
# The native build environment plus the few tools the README acceptance
# tests (tests/acceptance) need on top of it.
ARG BASE
FROM ${BASE}

RUN apt-get update && apt-get install -y --no-install-recommends \
	binutils \
	fdisk \
	psmisc \
	&& rm -rf /var/lib/apt/lists/*
