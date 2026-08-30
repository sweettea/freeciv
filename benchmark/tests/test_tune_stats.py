"""The tuner's SE arithmetic (W25).

Pure functions only: nothing here runs a Freeciv game, and nothing
here needs a result directory. Run with

    python3 -m pytest benchmark/tests
    python3 -m unittest discover -s benchmark/tests
"""

import math
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import tune                                             # noqa: E402


class TestMeanSE(unittest.TestCase):
    def test_mean_and_se_of_a_sample(self):
        m, se = tune.mean_se([1.0, 2.0, 3.0])
        self.assertAlmostEqual(m, 2.0)
        # sample sd 1.0 over sqrt(3)
        self.assertAlmostEqual(se, 1.0 / math.sqrt(3))

    def test_single_point_has_no_se(self):
        m, se = tune.mean_se([5.0])
        self.assertEqual(m, 5.0)
        self.assertTrue(math.isnan(se))

    def test_empty_sample(self):
        m, se = tune.mean_se([])
        self.assertTrue(math.isnan(m))
        self.assertTrue(math.isnan(se))

    def test_identical_points_have_zero_se(self):
        m, se = tune.mean_se([7.0, 7.0, 7.0])
        self.assertEqual((m, se), (7.0, 0.0))


class TestBlendStats(unittest.TestCase):
    def test_one_config_is_sigma_over_root_n(self):
        diffs = {(0, s): v for s, v in enumerate([10.0, 20.0, 30.0, 40.0])}
        obj, se, n = tune.blend_stats(diffs)
        self.assertAlmostEqual(obj, 25.0)
        # sd = 12.909944..., n = 4
        self.assertAlmostEqual(se, math.sqrt(500.0 / 3.0) / 2.0)
        self.assertEqual(n, 4)

    def test_two_configs_average_the_means_and_add_variances(self):
        a = [10.0, 20.0, 30.0]
        b = [0.0, 0.0, 60.0]
        diffs = {}
        diffs.update({(0, s): v for s, v in enumerate(a)})
        diffs.update({(1, 100 + s): v for s, v in enumerate(b)})
        obj, se, n = tune.blend_stats(diffs)
        _, sea = tune.mean_se(a)
        _, seb = tune.mean_se(b)
        self.assertAlmostEqual(obj, (20.0 + 20.0) / 2.0)
        self.assertAlmostEqual(se, math.sqrt(sea ** 2 + seb ** 2) / 2.0)
        self.assertEqual(n, 6)

    def test_config_with_no_finished_game_is_skipped(self):
        diffs = {(0, 1): 4.0, (0, 2): 6.0}
        obj, _se, n = tune.blend_stats(diffs)
        self.assertAlmostEqual(obj, 5.0)
        self.assertEqual(n, 2)

    def test_candidate_with_no_games_ranks_last(self):
        obj, se, n = tune.blend_stats({})
        self.assertEqual(obj, -float("inf"))
        self.assertTrue(math.isnan(se))
        self.assertEqual(n, 0)

    def test_the_postfix_campaign_number_reads_as_noise(self):
        # 8 training seeds, per-seed sigma ~115: the SE the campaign
        # never printed (findings/tune-postfix-verdict-2026-08-25.md).
        vals = [-115.0, 115.0] * 4
        _obj, se, _n = tune.blend_stats(
            {(0, s): v for s, v in enumerate(vals)})
        self.assertGreater(se, 40.0)


class TestPairedStats(unittest.TestCase):
    def test_only_shared_seeds_count(self):
        cand = {(0, 1): 10.0, (0, 2): 20.0, (0, 3): 999.0}
        inc = {(0, 1): 4.0, (0, 2): 6.0}
        mean, _se, n = tune.paired_stats(cand, inc)
        self.assertAlmostEqual(mean, (6.0 + 14.0) / 2.0)
        self.assertEqual(n, 2)

    def test_pairing_cancels_the_common_component(self):
        # Wildly different seeds, identical challenger advantage: the
        # objective SE is huge and the paired SE is zero.
        inc = {(0, s): v for s, v in enumerate([-200.0, 0.0, 300.0])}
        cand = {k: v + 12.0 for k, v in inc.items()}
        _obj, obj_se, _ = tune.blend_stats(cand)
        mean, se, _ = tune.paired_stats(cand, inc)
        self.assertGreater(obj_se, 100.0)
        self.assertAlmostEqual(mean, 12.0)
        self.assertAlmostEqual(se, 0.0)

    def test_no_shared_seeds(self):
        mean, se, n = tune.paired_stats({(0, 1): 5.0}, {(0, 2): 5.0})
        self.assertEqual(n, 0)
        self.assertEqual(mean, -float("inf"))
        self.assertTrue(math.isnan(se))


class TestZScore(unittest.TestCase):
    def test_ordinary(self):
        self.assertAlmostEqual(tune.zscore(20.0, 10.0), 2.0)

    def test_unknown_se_is_nan(self):
        self.assertTrue(math.isnan(tune.zscore(20.0, float("nan"))))

    def test_unanimous_challenger_clears_any_gate(self):
        self.assertEqual(tune.zscore(12.0, 0.0), float("inf"))
        self.assertEqual(tune.zscore(-12.0, 0.0), -float("inf"))
        self.assertTrue(math.isnan(tune.zscore(0.0, 0.0)))


class TestFormatting(unittest.TestCase):
    def test_objective_always_carries_an_se(self):
        self.assertEqual(tune.pm(12.34, 5.6), "+12.3 +/- 5.6")
        self.assertEqual(tune.pm(-2.0, float("nan")), "-2.0 +/- n/a")


if __name__ == "__main__":
    unittest.main()
