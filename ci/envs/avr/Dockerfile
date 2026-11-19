# SPDX-License-Identifier: BSD-3-Clause
# base: base
# 8-bit AVR: avr-gcc with avr-libc and the simavr instruction set
# simulator, which runs the test firmware on a simulated ATmega1284.
ARG BASE
FROM ${BASE}

RUN apt-get update && apt-get install -y --no-install-recommends \
	avr-libc \
	binutils-avr \
	gcc-avr \
	simavr \
	&& rm -rf /var/lib/apt/lists/*
