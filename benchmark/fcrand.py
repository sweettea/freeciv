"""Freeciv's RNG seeding, ported from utility/rand.c, for writing a
seed-derived `[random]` section into a resumed start save.

A savegame carries the generator's full state, so one start save
replays identically under every benchmark seed. fc_srand(seed) fills
the 56-entry lagged-Fibonacci table from the seed and draws 10000
values to heat it; this reproduces that state bit for bit (checked
against fc_srand() in libfreeciv.so, see europe-ally in STATUS) so a
resumed game under seed N is the save's position played with the RNG
a fresh `set gameseed N` game would have had.
"""

MASK = 0xFFFFFFFF


def srand_state(seed):
    """(v[56], j, k, x) after fc_srand(seed), warm-up included."""
    v = [0] * 56
    v[0] = seed & MASK
    for i in range(1, 56):
        v[i] = (3 * v[i - 1] + 257) & MASK
    j, k, x = 0, 55 - 24, 55
    for _ in range(10000):
        while True:                 # fc_rand(MAX_UINT32): divisor 1, max MASK-1
            new = (v[j] + v[k]) & MASK
            x = (x + 1) % 56
            j = (j + 1) % 56
            k = (k + 1) % 56
            v[x] = new
            if new <= MASK - 1:
                break
    return v, j, k, x


def random_section(seed):
    """The savegame `[random]` section text for this seed."""
    v, j, k, x = srand_state(seed)
    lines = ["[random]", "saved=TRUE", "index_J=%d" % j, "index_K=%d" % k,
             "index_X=%d" % x]
    for i in range(8):
        lines.append('table%d="%s"' % (i, " ".join("%8x" % n for n in v[7 * i:7 * i + 7])))
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    import sys
    print(random_section(int(sys.argv[1])), end="")
