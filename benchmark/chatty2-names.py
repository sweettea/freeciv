#!/usr/bin/env python3
"""Read a freeciv savegame and print the names the inbound-request smoke
needs to be able to type.

The smoke has to say `defend <city>` and `road to <city>` with names
that actually exist at the turn the save was taken, and `attack <city>`
with the name of an enemy city the deep player has ALREADY SEEN --
because deep refuses a target it cannot ground, which is the honesty
property, so a name picked at random would only ever prove the refusal.

Three tables in the save carry all of it:
  player<N>.c=      that player's own cities (id, name)
  player<N>.dc=     the cities that player REMEMBERS, i.e. its own
                    knowledge of the map (owner, id, name) -- this is
                    the struct player_tile / vision_site store, which
                    is exactly the "has deep seen it" question
  players.nplayers  how many player sections there are

Output is shell-sourceable KEY=value lines.
"""
import csv, io, shlex, subprocess, sys


def load(path):
    if path.endswith(".zst"):
        return subprocess.run(["zstd", "-dc", path], check=True,
                              capture_output=True).stdout.decode("utf-8")
    if path.endswith(".gz"):
        return subprocess.run(["gzip", "-dc", path], check=True,
                              capture_output=True).stdout.decode("utf-8")
    return open(path, encoding="utf-8").read()


def split_row(line):
    return next(csv.reader(io.StringIO(line)))


def table(lines, start):
    """The rows under a `name={header}` line: everything until the next
    section, table or blank."""
    out, j = [], start
    while (j < len(lines) and lines[j] and not lines[j].startswith("[")
           and not lines[j].startswith("}") and "={" not in lines[j]):
        out.append(split_row(lines[j]))
        j += 1
    return out


def main(path, me):
    lines = load(path).split("\n")
    sec = None
    names = {}                  # player section -> display name
    own = {}                    # player section -> [(id, name)]
    known = []                  # (owner index, id, name) as `me` sees it

    for i, line in enumerate(lines):
        if line.startswith("["):
            sec = line.strip("[]")
        elif sec is None or not sec.startswith("player"):
            continue
        elif line.startswith("name="):
            names[sec] = line[5:].strip('"')
        elif line.startswith("c={"):
            hdr = split_row(line[3:])
            k_id, k_nm = hdr.index("id"), hdr.index("name")
            own.setdefault(sec, []).extend(
                [(r[k_id], r[k_nm]) for r in table(lines, i + 1)])
        elif line.startswith("dc={") and names.get(sec) == me:
            hdr = split_row(line[4:])
            k_ow, k_id = hdr.index("owner"), hdr.index("id")
            k_nm = hdr.index("name")
            known = [(int(r[k_ow]), r[k_id], r[k_nm])
                     for r in table(lines, i + 1)]

    mysec = [s for s, n in names.items() if n == me]
    if not mysec:
        sys.exit("no player named %s in %s" % (me, path))
    mysec = mysec[0]
    myidx = int(mysec[len("player"):])

    # shlex.quote, not a bare single-quoted string: freeciv city names
    # routinely contain an apostrophe (Nit'ahende, Ch'laande), which
    # closes a hand-quoted string and makes the shell try to run the
    # rest of the line as a command. It did.
    mine = own.get(mysec, [])
    print("OWN_CITIES=%s" % shlex.quote(";".join("%s,%s" % c for c in mine)))

    # An ally city: any player on our team. The save does not spell the
    # team out in a form worth parsing here, so the smoke passes the
    # ally's player name in and we look that up.
    for sec, plist in own.items():
        if sec != mysec:
            print("CITIES_%s=%s"
                  % (names[sec],
                     shlex.quote(";".join("%s,%s" % c for c in plist))))

    # Enemy cities we have SEEN: in our own dc table, owned by somebody
    # who is neither us nor an ally the caller named.
    print("SEEN=%s" % shlex.quote(";".join(
        "%d,%s,%s" % k for k in known if k[0] != myidx)))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: chatty2-names.py <savegame> <deep player name>")
    main(sys.argv[1], sys.argv[2])
