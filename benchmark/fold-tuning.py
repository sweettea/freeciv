#!/usr/bin/env python3
"""Fold a tuned [tuning] file back into deeptuning.c as new defaults.

    fold-tuning.py TUNINGFILE [--source ai/deep/deeptuning.c]

For every key in the tuning file that matches a field in the defaults
initializer, rewrite the value in place and change its comment from
PLACEHOLDER to 'tuned <date> (<basename>)'. Prints a diff-style
summary; refuses keys that have no matching field. This is the
post-campaign step: best-tuning.ini in, calibrated defaults out,
one reviewable commit.
"""

import argparse
import datetime
import re
import sys
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tuningfile")
    ap.add_argument("--source", default="ai/deep/deeptuning.c")
    args = ap.parse_args()

    vals = {}
    for line in open(args.tuningfile):
        line = line.strip()
        m = re.match(r"^(\w+)\s*=\s*([-\d.]+)$", line)
        if m:
            vals[m.group(1)] = m.group(2)
    if not vals:
        sys.exit(f"no key=value pairs found in {args.tuningfile}")

    src = Path(args.source).read_text()
    tag = (f"tuned {datetime.date.today().isoformat()} "
           f"({Path(args.tuningfile).name})")
    changed = []
    for name, val in vals.items():
        rx = re.compile(r"(\.%s\s*=\s*)([-\d.]+)(,\s*/\* )([^*]*)(\*/)"
                        % re.escape(name))
        m = rx.search(src)
        if not m:
            sys.exit(f"no field .{name} in {args.source}")
        old = m.group(2)
        # Preserve int-ness of int fields.
        newval = val
        if "." not in old:
            newval = str(int(round(float(val))))
        if old != newval:
            changed.append((name, old, newval))
        src = rx.sub(lambda mm: f"{mm.group(1)}{newval},"
                                f"{' ' * max(1, 38 - len(name) - len(newval) - 6)}"
                                f"/* {tag} */", src, count=1)

    Path(args.source).write_text(src)
    if changed:
        for name, old, new in changed:
            print(f"{name}: {old} -> {new}")
    else:
        print("no value changes (comments retagged only)")
    print(f"{len(vals)} fields folded into {args.source}")


if __name__ == "__main__":
    main()
