# Common ground for every lwext4 environment: a C build system and the
# e2fsprogs tools used as an independent oracle (mke2fs builds test images,
# e2fsck/debugfs check what lwext4 wrote). Toolchain- or emulator-specific
# environments build on this one with "# base: base".
FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
	bash \
	ca-certificates \
	cmake \
	e2fsprogs \
	git \
	make \
	python3 \
	xz-utils \
	&& rm -rf /var/lib/apt/lists/*
