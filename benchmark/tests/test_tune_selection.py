"""How the tuner turns a generation into a block best (W25).

run_block() is exercised end to end with run_game mocked out: no
Freeciv server is started, no config is read, and the "score" of a
candidate is a pure function of its one parameter and the seed. That
is enough to pin the property the 2026-08-25 post-mortem says was
missing -- a population of pure noise must not produce a block best.

    python3 -m pytest benchmark/tests
    python3 -m unittest discover -s benchmark/tests
"""

import argparse
import hashlib
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import tune                                             # noqa: E402

BASELINE_SCORE = 1000.0
PARAM = {"name": "x", "default": 0.0, "lo": -10.0, "hi": 10.0,
         "is_int": False}


def noise(param, seed):
    """Deterministic pseudo-random per (candidate, seed), in [-1, 1).

    Two tunings play different games on the same seed, so their per-
    seed outcomes are independent draws -- which is exactly why the
    max over a generation is an order statistic of noise."""
    h = hashlib.sha256(f"{param:.6f}|{seed}".encode()).digest()
    return int.from_bytes(h[:4], "big") / 2 ** 31 - 1.0


def make_scorer(signal, sigma):
    """A fake game: score = baseline + signal * x + sigma * noise."""
    def score(param, seed):
        return BASELINE_SCORE + signal * param + sigma * noise(param, seed)
    return score


def install_fake_game(test, scorer, calls=None):
    real = tune.run_game

    def fake(config, args, arm, seed, outdir, port, tuning=None,
             mixed=False, arena=None):
        # Models the one behaviour of the real thing that matters
        # here: a directory with a DONE marker is not replayed. calls
        # therefore counts games actually PLAYED, not games read.
        outdir.mkdir(parents=True, exist_ok=True)
        param = None
        for line in open(tuning):
            key, _, val = line.partition("=")
            if key.strip() == PARAM["name"]:
                param = float(val)
        assert param is not None, f"no {PARAM['name']} in {tuning}"
        if not (outdir / "DONE").exists():
            if calls is not None:
                calls.append((arm, seed))
            (outdir / "DONE").write_text("")
        return scorer(param, seed)

    tune.run_game = fake
    test.addCleanup(lambda: setattr(tune, "run_game", real))


def make_args(**kw):
    a = argparse.Namespace(configs=["cfg0"], jobs=4, mixed=False,
                           server=None, portbase=7000, gens=3, pop=6,
                           accept="best", accept_z=None, reeval=None,
                           reeval_seeds=None, reeval_pool=[])
    for k, v in kw.items():
        setattr(a, k, v)
    return a


class BlockCase(unittest.TestCase):
    seeds = [1, 2, 3, 4, 5, 6]
    fresh = [7, 8, 9, 10]

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.out = pathlib.Path(self.tmp.name)
        self.baseline = {(0, s): BASELINE_SCORE
                         for s in self.seeds + self.fresh}

    def block(self, args, scorer, calls=None):
        install_fake_game(self, scorer, calls)
        import random
        return tune.run_block(args, self.seeds, [dict(PARAM)], [0],
                              [0.0], self.baseline, self.out,
                              random.Random(7), "t")

    def objective(self, scorer, param, seeds):
        vals = [scorer(param, s) - BASELINE_SCORE for s in seeds]
        return tune.blend_stats({(0, s): v
                                 for s, v in zip(seeds, vals)})[0]


class TestUnreplicatedMax(BlockCase):
    def test_pure_noise_still_produces_a_block_best(self):
        # The legacy behaviour, kept as the default and kept honest by
        # its label: with no signal at all, the max over 18 candidates
        # is comfortably positive.
        res = self.block(make_args(), make_scorer(0.0, 300.0))
        self.assertGreater(res["obj"], 0.0)
        self.assertIn("unreplicated max", res["label"])

    def test_a_real_signal_is_found(self):
        res = self.block(make_args(), make_scorer(40.0, 5.0))
        self.assertGreater(res["vals"][0], 3.0)


class TestPairedAcceptance(BlockCase):
    def test_noise_does_not_clear_the_gate(self):
        res = self.block(make_args(accept="paired", accept_z=3.0),
                         make_scorer(0.0, 300.0))
        self.assertEqual(res["vals"], [0.0])
        self.assertIn("incumbent held", res["label"])

    def test_a_real_signal_does_clear_the_gate(self):
        scorer = make_scorer(40.0, 5.0)
        res = self.block(make_args(accept="paired", accept_z=3.0), scorer)
        self.assertGreater(res["vals"][0], 3.0)
        self.assertIn("paired z=", res["label"])

    def test_the_gate_is_the_flag(self):
        # Same population, same games: an impossible threshold rejects
        # what a modest one accepts, so the flag is really the gate.
        scorer = make_scorer(40.0, 5.0)
        loose = self.block(make_args(accept="paired", accept_z=1.0), scorer)
        tight = self.block(make_args(accept="paired", accept_z=1e6), scorer)
        self.assertGreater(loose["vals"][0], 3.0)
        self.assertEqual(tight["vals"], [0.0])


class TestReeval(BlockCase):
    def test_block_best_is_the_fresh_seed_number(self):
        scorer = make_scorer(40.0, 5.0)
        args = make_args(reeval=2, reeval_seeds=len(self.fresh),
                         reeval_pool=list(self.fresh))
        res = self.block(args, scorer)
        self.assertIn(f"replicated on {len(self.fresh)} fresh seeds",
                      res["label"])
        # The reported objective is the champion's objective on the
        # fresh seeds, not on the seeds it was selected over.
        # (The tolerance is write_tuning's six decimal places, which
        # is the vector the game actually played.)
        self.assertAlmostEqual(
            res["obj"], self.objective(scorer, res["vals"][0], self.fresh),
            places=3)
        self.assertNotAlmostEqual(
            res["obj"], self.objective(scorer, res["vals"][0], self.seeds),
            places=3)

    def test_fresh_seeds_are_actually_played(self):
        calls = []
        args = make_args(reeval=2, reeval_seeds=len(self.fresh),
                         reeval_pool=list(self.fresh))
        self.block(args, make_scorer(40.0, 5.0), calls)
        self.assertTrue({s for _, s in calls} >= set(self.fresh))

    def test_noise_is_rejected_on_the_fresh_sample(self):
        # Top-2 of a noise generation are the two luckiest draws; on
        # seeds they were not chosen for they revert to the mean, and
        # the incumbent -- re-evaluated on the same fresh seeds -- is
        # the floor they have to clear.
        args = make_args(gens=1, reeval=2, reeval_seeds=len(self.fresh),
                         reeval_pool=list(self.fresh),
                         accept="paired", accept_z=3.0)
        res = self.block(args, make_scorer(0.0, 300.0))
        self.assertEqual(res["vals"], [0.0])


class TestIncumbentReuse(BlockCase):
    def test_incumbent_shares_generation_zero_candidate_zero(self):
        # The incumbent evaluation must not mint new result dirs: it
        # is gen0/cand0, which the population runs anyway.
        self.assertEqual(tune.incumbent_dir(self.out, "t"),
                         self.out / "t" / "gen0" / "cand0")
        calls = []
        self.block(make_args(gens=2, pop=3), make_scorer(10.0, 20.0),
                   calls)
        # 2 gens x 3 candidates x 6 seeds, and not one game more: the
        # incumbent's six are candidate 0's six.
        self.assertEqual(len(calls), 2 * 3 * len(self.seeds))


if __name__ == "__main__":
    unittest.main()
