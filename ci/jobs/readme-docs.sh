# env: acceptance
# SPDX-License-Identifier: BSD-3-Clause
# README.md vs. repository: every code block is covered by the acceptance
# inventory, the documented Debian packages are the ones acceptance-debian
# installs, the project tree and Makefile targets it names exist.
sh tests/acceptance/test-docs.sh
