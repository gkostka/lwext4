# SPDX-License-Identifier: BSD-3-Clause
# base: base
# Linux cross compilers for the qemu-user matrix and qemu-user itself.
#
# Every architecture is listed as <gnu triple>:<debian arch>. Debian ships
# the compiler for the build machine's own architecture as the native gcc
# (which also answers to <triple>-gcc), the others as gcc-<triple>, so the
# image builds natively on amd64 and arm64 alike.
ARG BASE
FROM ${BASE}

ARG TARGETS="\
	arm-linux-gnueabihf:armhf \
	aarch64-linux-gnu:arm64 \
	i686-linux-gnu:i386 \
	x86_64-linux-gnu:amd64 \
	riscv64-linux-gnu:riscv64 \
	powerpc64le-linux-gnu:ppc64el \
	s390x-linux-gnu:s390x \
	powerpc-linux-gnu:powerpc \
	mips-linux-gnu:mips"

RUN set -eu; \
	native=$(dpkg --print-architecture); \
	pkgs=""; \
	for t in $TARGETS; do \
		triple=${t%%:*}; arch=${t##*:}; \
		if [ "$arch" = "$native" ]; then \
			pkgs="$pkgs gcc libc6-dev"; \
		else \
			pkgs="$pkgs gcc-$(echo $triple | tr _ -) libc6-dev-$arch-cross"; \
		fi; \
	done; \
	apt-get update; \
	apt-get install -y --no-install-recommends file qemu-user $pkgs; \
	rm -rf /var/lib/apt/lists/*; \
	for t in $TARGETS; do \
		triple=${t%%:*}; \
		command -v $triple-gcc >/dev/null || \
			ln -s "$(command -v gcc)" /usr/local/bin/$triple-gcc; \
	done
