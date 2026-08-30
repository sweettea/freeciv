"""What --dry-run promises a campaign will cost (W25).

Pure arithmetic: schedule() launches nothing and reads nothing, so
these cases are the specification of the game counts a campaign is
budgeted against.

    python3 -m pytest benchmark/tests
    python3 -m unittest discover -s benchmark/tests
"""

import argparse
import math
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import tune                                             # noqa: E402

ALLP = [{"name": n, "default": 0.0, "lo": 0.0, "hi": 1.0,
         "is_int": False} for n in ("a", "b", "c", "d")]
BLOCKS = {"E": ["a", "b"], "M": ["c", "d"]}


def make_args(**kw):
    a = argparse.Namespace(configs=["one.conf"], mixed=False, pop=6,
                           gens=4, rounds=1, race=None, race_z=None,
                           reeval=None, reeval_seeds=None,
                           reeval_pool=[], sigma_seed=None)
    for k, v in kw.items():
        setattr(a, k, v)
    return a


SEEDS = [1, 2, 3, 4, 5, 6, 7, 8]


def sched(**kw):
    args = make_args(**kw)
    return tune.schedule(args, SEEDS, ALLP, BLOCKS, ["E"])


class TestGameCounts(unittest.TestCase):
    def test_flat_schedule(self):
        s = sched()
        self.assertEqual(s["per_gen"], 6 * 8)
        self.assertEqual(s["per_block"], 4 * 6 * 8)
        self.assertEqual(s["baseline"], 8)
        self.assertEqual(s["total"], 8 + 4 * 6 * 8)

    def test_blended_configs_multiply_every_game(self):
        s = sched(configs=["a.conf", "b.conf"])
        self.assertEqual(s["per_gen"], 2 * 6 * 8)
        self.assertEqual(s["baseline"], 16)

    def test_mixed_doubles_the_arms_and_drops_the_baseline(self):
        s = sched(mixed=True)
        self.assertEqual(s["per_gen"], 2 * 6 * 8)
        self.assertEqual(s["baseline"], 0)

    def test_racing_is_cheaper_than_the_flat_schedule(self):
        s = sched(race=3, race_z=1.0)
        # 6 candidates x 3 seeds, then 3 survivors x 5 seeds
        self.assertEqual(s["per_gen"], 6 * 3 + 3 * 5)
        self.assertEqual(s["flat_per_gen"], 6 * 8)
        self.assertLess(s["per_gen"], s["flat_per_gen"])

    def test_reeval_costs_the_top_k_plus_one_incumbent_per_block(self):
        s = sched(reeval=2, reeval_seeds=4, reeval_pool=[9, 10, 11, 12])
        # 4 generations x (48 population + 2 candidates x 4 fresh),
        # plus the incumbent's 4 fresh games, paid once per block.
        self.assertEqual(s["per_block"], 4 * (48 + 8) + 4)
        # The fresh seeds need a baseline arm of their own.
        self.assertEqual(s["baseline"], 12)

    def test_rounds_and_blocks_are_separate_passes(self):
        args = make_args(rounds=2)
        s = tune.schedule(args, SEEDS, ALLP, BLOCKS, ["E", "M"])
        self.assertEqual(len(s["rows"]), 4)
        self.assertEqual(s["total"],
                         s["baseline"] + 4 * s["per_block"])

    def test_rows_carry_the_block_width(self):
        args = make_args()
        s = tune.schedule(args, SEEDS, ALLP, BLOCKS, ["E", "M"])
        self.assertEqual([r["params"] for r in s["rows"]], [2, 2])
        self.assertEqual([r["candidates"] for r in s["rows"]], [24, 24])


class TestExpectedSE(unittest.TestCase):
    def test_sigma_over_root_n(self):
        self.assertAlmostEqual(tune.expected_se(115.0, 1, 8),
                               115.0 / math.sqrt(8))

    def test_configs_count_as_seeds_for_the_blend(self):
        self.assertAlmostEqual(tune.expected_se(115.0, 2, 8),
                               115.0 / math.sqrt(16))

    def test_no_sigma_means_no_prediction(self):
        # Rule 7: the tool does not invent the number it was not given.
        self.assertIsNone(tune.expected_se(None, 1, 8))

    def test_the_campaign_that_failed_reads_as_it_did(self):
        # 8 training seeds, per-seed sigma 115: SE ~41, against
        # headline "gains" of +104 and +62.
        self.assertAlmostEqual(tune.expected_se(115.0, 1, 8), 40.7,
                               places=1)

    def test_a_race_rung_is_wider_than_the_full_objective(self):
        s = sched(race=3, race_z=1.0, sigma_seed=115.0)
        self.assertGreater(s["se_rung"], s["se"])


if __name__ == "__main__":
    unittest.main()
