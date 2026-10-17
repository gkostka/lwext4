# SPDX-License-Identifier: BSD-3-Clause
# A plain Debian with nothing but the packages README.md lists, to prove the
# documented dependencies are enough to build and test lwext4. Deliberately
# not based on "base", which already contains cmake/make/e2fsprogs.
#
# The two install commands are README.md's, word for word, plus "-y" (there
# is no terminal to answer apt-get's question). The readme-docs job checks
# that they still match README.md.
FROM debian:trixie-slim

RUN apt-get update
# README.md, "Compile" / "Dependencies" / "Linux"
RUN apt-get install -y make gcc cmake
# README.md, "Run regression tests": the tests need mke2fs (e2fsprogs)
RUN apt-get install -y e2fsprogs
