#!/usr/bin/env python3
"""Ordinary Wasm locals pages; protocol DATA, not native capture authority."""
import unittest
import test_dap_frame_scope_lifetime as life


class Broker(life.Broker):
    def __init__(self):
        super().__init__()
        self.total = 160
        self.pages = 0
        self.changed_total = False
        self.short_page = False
        self.after_page = None
        self.probe_epoch = 7
        self.probe_module = 0
        self.empty_probe = False

    def request(self, command):
        if command == "locals 1":
            self.commands.append(command)
            return f"local 0 i32=0\nlocal 1 i32=1\nlocals truncated: showing 2 of {self.total}\n"
        if command.startswith("locals wasm 1 0 "):
            self.commands.append(command)
            self.pages += 1
            words = command.split(); first, count = map(int, words[-2:])
            total = self.total + int(self.changed_total and self.pages > 1)
            end = min(first + count, total)
            if self.short_page and count > 1: end -= 1
            rows = ''.join(f"local {i} i32 = {i}\n" for i in range(first, end))
            if self.empty_probe and self.pages == 1: rows = ""; end = first
            result = f"wasm-stop {self.stop}\nWasm state thread=1 module={self.probe_module} epoch={self.probe_epoch} first={first} total={total}\n" + rows
            if end < total: result += "Wasm state truncated: rows=" + str(end - first) + " objects=0\n"
            if self.after_page is not None: self.after_page()
            return result
        return super().request(command)


class Pages(unittest.TestCase):
    send = life.Lifetime.send
    handles = life.Lifetime.handles

    def setUp(self):
        life.Lifetime.setUp(self)
        self.broker = Broker(); self.adapter.broker = self.broker

    def check_page(self, start, count, expected, **options):
        _, scope, _ = self.handles()
        result = self.send("variables", variablesReference=scope, start=start, count=count, **options)
        self.assertTrue(result["success"], result)
        rows = result["body"]["variables"]
        self.assertEqual([v["name"] for v in rows], [f"local {i}" for i in expected])
        self.assertEqual([v["value"] for v in rows], [f"i32={i}" for i in expected])
        self.assertTrue(all(v["variablesReference"] == 0 and "memoryReference" not in v for v in rows))

    def test_window_across_old_capture_limit_and_end(self):
        for start, count, expected in ((0, 1, range(1)), (63, 80, range(63, 143)),
                                       (129, 8, range(129, 137)), (159, 64, [159]), (160, 4, []), ((1 << 64) - 1, 1, [])):
            with self.subTest(start=start, count=count):
                self.setUp(); self.check_page(start, count, expected, filter="indexed")

    def test_count_zero_and_omission_return_remaining_with_bounded_copies(self):
        self.check_page(60, 0, range(60, 160))
        self.setUp(); _, scope, _ = self.handles()
        result = self.send("variables", variablesReference=scope)
        self.assertTrue(result["success"], result)
        self.assertEqual(len(result["body"]["variables"]), 160)
        self.assertTrue(all(int(command.split()[-1]) <= 64 for command in self.broker.commands if command.startswith("locals wasm ")))

    def test_named_filter_has_no_indexed_locals_or_value_borrow(self):
        _, scope, _ = self.handles(); begin = len(self.broker.commands)
        result = self.send("variables", variablesReference=scope, filter="named", start=10, count=2)
        self.assertEqual(result["body"]["variables"], [])
        self.assertFalse(any(command.startswith("locals ") for command in self.broker.commands[begin:]))

    def test_invalid_pages_fail_before_broker_and_retire_scope(self):
        invalid = [{"start": x} for x in (-1, True, 1.5, "1", 1 << 64)]
        invalid += [{"count": x} for x in (-1, True, 1.5, "1", 1025)]
        invalid += [{"filter": x} for x in ("all", "", True, [], {})]
        for args in invalid:
            with self.subTest(args=args):
                self.setUp(); _, scope, _ = self.handles(); before = list(self.broker.commands)
                self.assertFalse(self.send("variables", variablesReference=scope, **args)["success"])
                self.assertEqual(self.broker.commands, before)
                self.assertFalse(self.adapter.scopes)

    def test_changed_or_short_page_never_publishes_a_partial_prefix(self):
        for kind in ("total", "short", "stop"):
            with self.subTest(kind=kind):
                self.setUp(); _, scope, _ = self.handles()
                if kind == "total": self.broker.changed_total = True
                elif kind == "short": self.broker.short_page = True
                else: self.broker.after_page = lambda: setattr(self.broker, "stop", self.broker.stop + 1)
                result = self.send("variables", variablesReference=scope, start=63, count=80)
                self.assertFalse(result["success"], result); self.assertNotIn("body", result)
                self.assertFalse(self.adapter.scopes)

    def test_unbounded_remainder_requires_a_finite_request(self):
        self.broker.total = 2000; _, scope, _ = self.handles()
        result = self.send("variables", variablesReference=scope)
        self.assertFalse(result["success"], result)
        self.assertNotIn("body", result)

    def test_probe_requires_current_module_epoch_and_its_first_local(self):
        for attribute, value in (("probe_epoch", 8), ("probe_module", 9), ("empty_probe", True)):
            with self.subTest(attribute=attribute):
                self.setUp(); _, scope, _ = self.handles(); setattr(self.broker, attribute, value)
                result = self.send("variables", variablesReference=scope, start=160, count=1)
                self.assertFalse(result["success"], result); self.assertNotIn("body", result)


if __name__ == "__main__":
    unittest.main()
