# acceptance-debian

`debian:trixie-slim` plus exactly the packages README.md tells a Debian user
to install:

    apt-get install make gcc cmake           # "Compile / Dependencies"
    apt-get install e2fsprogs                # "Run regression tests"

Nothing else (in particular not [base](../base)), so the `readme-debian` job
proves that these instructions are sufficient: the README's build, install
and regression test commands run in this image unmodified. The `readme-docs`
job fails if README.md and this Dockerfile disagree.
