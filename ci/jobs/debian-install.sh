# env: acceptance-debian
# SPDX-License-Identifier: BSD-3-Clause
# Check the installed tools on Debian with only the documented packages.
. ci/scripts/common.sh
build=$(ci_build_dir debian-install)
export PATH="$PATH:/sbin:/usr/sbin"
for tool in mke2fs e2fsck sfdisk python3 pkg-config; do
	command -v "$tool"
done
cmake -S . -B "$build/b" -DCMAKE_TOOLCHAIN_FILE=toolchain/generic.cmake \
	-DCMAKE_BUILD_TYPE=Release -DLWEXT4_BUILD_TESTS=OFF
cmake --build "$build/b" -j"$(ci_jobs)"
cmake --install "$build/b" --prefix "$PWD/$build/prefix"
for tool in lwext4-generic lwext4-mkfs lwext4-mbr lwext4-server lwext4-client; do
	"$build/prefix/bin/$tool" --version
done
