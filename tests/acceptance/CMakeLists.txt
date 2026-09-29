# SPDX-License-Identifier: BSD-3-Clause
# Helper for the README acceptance tests (see README.md in this directory).
# The tests themselves are shell scripts run by
# .github/workflows/readme-acceptance.yml, not CTest tests.
add_executable(lwext4-acceptance lwext4_acceptance.c)
target_link_libraries(lwext4-acceptance lwext4 blockdev)
