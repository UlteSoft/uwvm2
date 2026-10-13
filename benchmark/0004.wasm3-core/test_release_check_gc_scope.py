#!/usr/bin/env python3
"""Offline regression for the GC release gate's staged-scope safeguards."""

import unittest

from release_check_gc import PRODUCT_ENGINES, release_gate_reasons


class ReleaseScopeTest(unittest.TestCase):
    def test_staged_jit_subset_cannot_pass(self):
        selected = {"ordinary-jit", "ros-jit"}
        counts = {engine: 1 for engine in selected}
        reasons = release_gate_reasons(selected, set(), counts)
        self.assertTrue(any("ordinary-int" in reason and "ros-int" in reason
                            for reason in reasons))

    def test_flat_rss_without_collections_cannot_pass(self):
        reasons = release_gate_reasons(set(PRODUCT_ENGINES), set(), {})
        self.assertTrue(any("no measured positive collection count" in reason
                            for reason in reasons))

    def test_rss_growth_cannot_pass_even_with_collections(self):
        counts = {engine: 1 for engine in PRODUCT_ENGINES}
        reasons = release_gate_reasons(set(PRODUCT_ENGINES), {"ros-int"}, counts)
        self.assertTrue(any("bounded-RSS check failed: ros-int" in reason
                            for reason in reasons))

    def test_full_scope_with_rss_and_collections_is_eligible(self):
        counts = {engine: 1 for engine in PRODUCT_ENGINES}
        self.assertEqual(release_gate_reasons(set(PRODUCT_ENGINES), set(), counts), [])


if __name__ == "__main__":
    unittest.main()
