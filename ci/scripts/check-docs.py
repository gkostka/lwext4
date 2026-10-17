#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Keep the documentation from rotting.

Usage: check-docs.py <cc> <build dir> <markdown files...>

For each markdown file:

- every ```c code block is compiled on its own (cc -c -Wall -Wextra
  -Werror) against the lwext4 headers of <build dir>, so the snippets must
  be complete translation units that use the current API;
- there must be at least one C snippet in total;
- every relative link ([text](path) or [text](path#anchor)) must point
  to an existing file or directory.

The compiler errors point back at the markdown file and line (#line).
Before that, a snippet with a deliberate mistake is compiled to prove
that a broken snippet does fail the check.
"""

import os
import re
import subprocess
import sys

FENCE = re.compile(r"^(\s*)```(\w*)\s*$")
LINK = re.compile(r"\]\(([^)\s]+)\)")


def snippets(path):
    """Yield (first line number, language, code) of the fenced blocks."""
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    i = 0
    while i < len(lines):
        m = FENCE.match(lines[i])
        if not m:
            i += 1
            continue
        lang = m.group(2)
        start = i + 1
        i += 1
        while i < len(lines) and not lines[i].strip().startswith("```"):
            i += 1
        yield start + 1, lang, "\n".join(lines[start:i]) + "\n"
        i += 1


def compile_snippet(cc, includes, workdir, name, source, line, code):
    c_file = os.path.join(workdir, name + ".c")
    with open(c_file, "w", encoding="utf-8") as f:
        f.write('#line %d "%s"\n' % (line, source))
        f.write(code)
    cmd = [cc, "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-c",
           "-o", os.path.join(workdir, name + ".o"), c_file]
    for inc in includes:
        cmd += ["-I", inc]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       universal_newlines=True)
    return p.returncode == 0, p.stdout


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    cc, build, docs = sys.argv[1], sys.argv[2], sys.argv[3:]
    includes = ["include", os.path.join(build, "include"),
                os.path.join(build, "examples", "include")]
    workdir = os.path.join(build, "docs-check")
    os.makedirs(workdir, exist_ok=True)
    failed = 0
    total = 0

    # Negative control: a snippet calling a function lwext4 does not have.
    ok, _ = compile_snippet(cc, includes, workdir, "negative-control",
                            "negative-control", 1,
                            "#include <ext4.h>\n"
                            "int f(void) { return ext4_no_such_call(\"/\"); }\n")
    if ok:
        print("FAIL: a snippet calling a missing function compiles; "
              "the snippet check would not catch anything")
        return 1

    for doc in docs:
        count = 0
        for line, lang, code in snippets(doc):
            if lang != "c":
                continue
            count += 1
            name = "%s-%d" % (doc.replace("/", "_"), line)
            ok, out = compile_snippet(cc, includes, workdir, name, doc,
                                      line, code)
            if ok:
                print("ok: %s:%d: C snippet compiles" % (doc, line))
            else:
                print("FAIL: %s:%d: C snippet does not compile:" % (doc, line))
                print(out)
                failed += 1
        print("%s: %d C snippet(s)" % (doc, count))
        total += count

        base = os.path.dirname(doc)
        with open(doc, encoding="utf-8") as f:
            text = f.read()
        for target in LINK.findall(text):
            if re.match(r"^[a-z]+:", target) or target.startswith("#"):
                continue
            path = os.path.normpath(os.path.join(base, target.split("#")[0]))
            if not os.path.exists(path):
                print("FAIL: %s: broken link to %s" % (doc, target))
                failed += 1

    if total == 0:
        print("FAIL: no C snippets found, nothing was checked")
        failed += 1

    if failed:
        print("check-docs: %d problem(s)" % failed)
        return 1
    print("check-docs: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
