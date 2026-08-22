#!/usr/bin/env python3
"""Generate block-family ablation arms between compiled defaults and a
tuned tuning file.

The question this exists for: a whole tuned package (pv3) loses to the
compiled defaults on some venue, and nobody knows WHICH of its ~200
overrides is responsible.  The unit of blame is the block, as defined
by benchmark/tuning-blocks.ini, so an arm is "compiled defaults, plus
one block-family's tuned values".

Why the arms carry ONLY the family's keys and never a dumped default
for the rest: ai/deep's loader (deeptuning.c, deep_tuning_load_file)
overlays a file's [tuning] keys onto the compiled defaults and leaves
absent keys "at their compiled defaults EXACTLY" -- the existence
guard on TUNE_DOUBLE is there precisely so a zero-key file is
indistinguishable from tuning=none.  Omission is therefore an exact
default, while a dumped-and-reprinted default is a value that has been
through a float round-trip.  Omission is the safer of the two, so this
script never writes a value it did not copy verbatim from the base
file.

Every emitted line is copied byte-for-byte from the base ini, and the
emitted files are checked for duplicate keys before they are written:
secfile_load() refuses a whole file over one duplicate key and the
server then plays compiled defaults while claiming to be tuned, which
is how the 2026-08-14 "lean" evidence line had to be voided.

Usage:
    ablate-blocks.py --out DIR [--base FILE] [--blocks FILE]
                     [--arm NAME=SEL[,SEL...]]...

With no --arm, the default round-1 plan is emitted: `defaults` (zero
keys), `full` (every base key), and one arm per block of the S/X/G/M
partition plus `REST` for base keys that partition does not reach.

A SEL is a block name from tuning-blocks.ini, the literal REST, or a
bare knob name.
"""
import argparse
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
PARTITION = ("S", "X", "G", "M")


def read_ini(path):
    """[(key, verbatim_line)] in file order, plus the header comment."""
    entries, header, seen_section = [], [], False
    for line in path.read_text().splitlines():
        stripped = line.split(";", 1)[0].strip()
        if not stripped or stripped.startswith("#"):
            if not seen_section:
                header.append(line)
            continue
        if stripped.startswith("["):
            seen_section = True
            continue
        key = stripped.split("=", 1)[0].strip()
        entries.append((key, line.rstrip()))
    dupes = sorted({k for k, _ in entries
                    if [x for x, _ in entries].count(k) > 1})
    if dupes:
        sys.exit(f"{path}: duplicate keys {dupes} -- secfile would "
                 f"refuse the whole file")
    return entries, header


def read_blocks(path):
    blocks = {}
    for line in path.read_text().splitlines():
        line = line.split(";", 1)[0].strip()
        m = re.match(r"([A-Z0-9]+)\s*=\s*(.+)$", line)
        if m:
            blocks[m.group(1)] = [x.strip() for x in m.group(2).split(",")]
    return blocks


def resolve(sel, blocks, base_keys, partition_keys):
    if sel == "REST":
        return [k for k in base_keys if k not in partition_keys]
    if sel in blocks:
        return [k for k in base_keys if k in set(blocks[sel])]
    if sel in base_keys:
        return [sel]
    sys.exit(f"unknown selector {sel!r}: not a block, not REST, not a "
             f"key of the base file")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default=str(HERE / "tuning/pv3.ini"))
    ap.add_argument("--blocks", default=str(HERE / "tuning-blocks.ini"))
    ap.add_argument("--out", required=True)
    ap.add_argument("--arm", action="append", default=[],
                    metavar="NAME=SEL[,SEL...]")
    args = ap.parse_args()

    base = pathlib.Path(args.base)
    entries, _ = read_ini(base)
    base_keys = [k for k, _ in entries]
    line_of = dict(entries)
    blocks = read_blocks(pathlib.Path(args.blocks))
    missing = [b for b in PARTITION if b not in blocks]
    if missing:
        sys.exit(f"blocks file lacks {missing}")
    partition_keys = set()
    for b in PARTITION:
        partition_keys |= set(blocks[b])

    plan = []
    if args.arm:
        for spec in args.arm:
            name, _, sels = spec.partition("=")
            if not sels:
                sys.exit(f"bad --arm {spec!r}: want NAME=SEL[,SEL...]")
            plan.append((name, sels.split(",")))
    else:
        plan = [("defaults", []), ("full", list(PARTITION) + ["REST"])]
        plan += [(b, [b]) for b in PARTITION] + [("rest", ["REST"])]

    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for name, sels in plan:
        keys, order = set(), []
        for sel in sels:
            for k in resolve(sel, blocks, base_keys, partition_keys):
                if k not in keys:
                    keys.add(k)
                    order.append(k)
        body = [line_of[k] for k in base_keys if k in keys]
        text = [f"; ABLATION ARM {name}: compiled defaults, overridden by",
                f"; the {','.join(sels) or 'nothing'} keys of "
                f"{base.name}, copied verbatim.",
                "; Generated by benchmark/ablate-blocks.py -- do not "
                "hand-edit.",
                "; Keys absent here keep their compiled defaults exactly",
                "; (ai/deep/deeptuning.c deep_tuning_load_file).",
                "[tuning]"] + body
        path = out / f"{name}.ini"
        path.write_text("\n".join(text) + "\n")
        # Re-read what we wrote: same duplicate check the loader's own
        # validate step applies, and proof the values survived verbatim.
        written, _ = read_ini(path)
        for k, line in written:
            if line != line_of[k]:
                sys.exit(f"{path}: line for {k} is not verbatim")
        print(f"{path}: {len(written)} keys")


if __name__ == "__main__":
    main()
