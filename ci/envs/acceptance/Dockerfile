# SPDX-License-Identifier: BSD-3-Clause
# base: base
# Parse the workflow YAML when checking README.md's CI references.
ARG BASE
FROM ${BASE}

RUN apt-get update && apt-get install -y --no-install-recommends \
	python3-yaml \
	&& rm -rf /var/lib/apt/lists/*
