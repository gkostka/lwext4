# SPDX-License-Identifier: BSD-3-Clause
# base: base
# 16-bit MSP430: TI's msp430-elf GCC (Mitto Systems build) with its device
# support files and msp430-elf-run, the GDB MSP430 simulator that runs the
# test firmware. TI only publishes x86_64 Linux binaries, hence the
# "# platform: linux/amd64" header of the jobs using this environment.
ARG BASE
FROM ${BASE}

ARG TI_URL=https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-LlCjWuAbzH/9.3.1.2
ARG GCC_TAR=msp430-gcc-9.3.1.11_linux64.tar.bz2
ARG GCC_SHA256=b60851b61565e3ddd2193298a3fac5120c2402597174106cc5f8482b9e9986e6
ARG SUPPORT_ZIP=msp430-gcc-support-files-1.212.zip
ARG SUPPORT_SHA256=3b1a39f10a344dfefb767e60ac35becef4c065013be86993195b138a5fb0b8d6

RUN apt-get update && apt-get install -y --no-install-recommends \
	bzip2 \
	curl \
	unzip \
	&& rm -rf /var/lib/apt/lists/*

RUN set -eu; cd /tmp; \
	curl -fsSLo "$GCC_TAR" "$TI_URL/$GCC_TAR"; \
	curl -fsSLo "$SUPPORT_ZIP" "$TI_URL/$SUPPORT_ZIP"; \
	echo "$GCC_SHA256  $GCC_TAR" | sha256sum -c -; \
	echo "$SUPPORT_SHA256  $SUPPORT_ZIP" | sha256sum -c -; \
	mkdir -p /opt/msp430; \
	tar -xjf "$GCC_TAR" -C /opt/msp430 --strip-components=1; \
	unzip -q "$SUPPORT_ZIP" -d /tmp/support; \
	mv /tmp/support/msp430-gcc-support-files/include /opt/msp430/support; \
	rm -rf /tmp/*

ENV PATH=/opt/msp430/bin:$PATH
# Device headers and linker scripts, used by toolchain/common/msp430-gcc.cmake
ENV MSP430_SUPPORT=/opt/msp430/support
