# acceptance-debian

`debian:trixie-slim` plus exactly the packages README.md tells a Debian user
to install:

    apt-get install make gcc cmake                  # build dependencies
    apt-get install e2fsprogs fdisk python3 pkgconf  # regression test dependencies

The `debian-install` job builds and installs the tools in this image, starts
each installed binary and checks that the regression test tools are
available. The native CI job runs the regression suite. `readme-docs` checks
that README.md and this Dockerfile list the same packages.
