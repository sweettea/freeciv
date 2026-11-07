#!/usr/bin/env python3
"""Hand a human player's seat in a savegame to an AI module.

    handover.py IN.sav[.zst] OUT.sav[.zst] PLAYER_NAME [ai_type]

Rewrites the named player's block: adds the "ai" flag, clears the
username (Unassigned), sets ai_type (default deep). Everything else in
the save is untouched, so the game resumes from the same position with
the AI playing that nation. Used for the Europe play games.
"""
import re
import subprocess
import sys


def main():
    src, dst, name = sys.argv[1:4]
    ai = sys.argv[4] if len(sys.argv) > 4 else "deep"
    if src.endswith(".zst"):
        text = subprocess.run(["zstdcat", src], check=True, capture_output=True).stdout.decode("utf-8", "surrogateescape")
    else:
        text = open(src, encoding="utf-8", errors="surrogateescape").read()
    m = None
    for pm in re.finditer(r"^\[player(\d+)\]\n(.*?)(?=^\[)", text, re.M | re.S):
        if re.search(r'^name="%s"$' % re.escape(name), pm.group(2), re.M):
            m = pm
            break
    if m is None:
        sys.exit("no player named %s" % name)
    blk = "\n" + m.group(2)
    fm = re.search(r'\nflags="([^"]*)"\n', blk)
    flags = [f.strip('"') for f in fm.group(1).split('","')] if fm and fm.group(1) else []
    if "ai" not in flags:
        flags.insert(0, "ai")
    blk = re.sub(r'\nflags="[^"]*"\n', '\nflags=%s\n' % ",".join('"%s"' % f for f in flags), blk, count=1)
    blk = re.sub(r'\nai_type="[^"]*"\n', '\nai_type="%s"\n' % ai, blk, count=1)
    blk = re.sub(r'\nusername="[^"]*"\n', '\nusername="Unassigned"\n', blk, count=1)
    blk = re.sub(r'\nunassigned_user=FALSE\n', '\nunassigned_user=TRUE\n', blk, count=1)
    blk = re.sub(r'\nranked_username="[^"]*"\n', '\nranked_username="Unassigned"\n', blk, count=1)
    blk = re.sub(r'\nunassigned_ranked=FALSE\n', '\nunassigned_ranked=TRUE\n', blk, count=1)
    text = text[:m.start(2)] + blk[1:] + text[m.end(2):]
    out = dst[:-4] if dst.endswith(".zst") else dst
    open(out, "w", encoding="utf-8", errors="surrogateescape").write(text)
    if dst.endswith(".zst"):
        subprocess.run(["zstd", "-q", "-f", "--rm", out], check=True)
    print("%s -> %s: %s now %s (flags %s)" % (src, dst, name, ai, ",".join(flags)))


if __name__ == "__main__":
    main()
