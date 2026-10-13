#!/usr/bin/env python3
"""The exnref publication gate cannot pass an uncollected or partial run."""

import unittest

from generate_exnref_reclamation import CASES
from release_check_exnref_gc import gate_reasons
from release_check_gc import PRODUCT_ENGINES


class ExnrefReleaseScope(unittest.TestCase):
    def test_flat_rss_without_actual_collections_still_fails(self):
        reasons = gate_reasons(CASES, PRODUCT_ENGINES, {}, {})
        self.assertTrue(any("no measured positive collection count" in reason
                            for reason in reasons))

    def test_one_case_or_one_mode_does_not_qualify(self):
        counts = {case: {engine: 1 for engine in PRODUCT_ENGINES} for case in CASES}
        reasons = gate_reasons(CASES[:1], PRODUCT_ENGINES[:1], {}, counts)
        self.assertTrue(any("exnref workloads not tested" in reason for reason in reasons))
        self.assertTrue(any("product modes not tested" in reason for reason in reasons))

    def test_each_case_and_mode_requires_plateau_and_collection(self):
        counts = {case: {engine: 1 for engine in PRODUCT_ENGINES} for case in CASES}
        self.assertEqual(gate_reasons(CASES, PRODUCT_ENGINES, {}, counts), [])
        counts[CASES[1]][PRODUCT_ENGINES[-1]] = 0
        self.assertTrue(gate_reasons(CASES, PRODUCT_ENGINES, {}, counts))
        counts[CASES[1]][PRODUCT_ENGINES[-1]] = 1
        self.assertTrue(gate_reasons(CASES, PRODUCT_ENGINES,
                                     {CASES[0]: {PRODUCT_ENGINES[0]}}, counts))


if __name__ == "__main__":
    unittest.main()
