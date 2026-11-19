# SPDX-License-Identifier: BSD-3-Clause
# base: base
# Bare-metal ARM: the arm-none-eabi GCC with newlib, qemu-system-arm to boot
# the Cortex-M firmware on MPS2 boards and qemu-user's qemu-arm to run the
# arm-sim (ARM7TDMI, newlib rdimon semihosting) build as a plain program.
ARG BASE
FROM ${BASE}

RUN apt-get update && apt-get install -y --no-install-recommends \
	gcc-arm-none-eabi \
	libnewlib-arm-none-eabi \
	qemu-system-arm \
	qemu-user \
	&& rm -rf /var/lib/apt/lists/*
