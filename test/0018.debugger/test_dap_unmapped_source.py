#!/usr/bin/env python3
"""Protocol routing DATA for source scopes at guest instructions without lines.

Only genuine Linux cgroup sessions establish runtime/guest-read qualification.
"""
import unittest
import test_dap_source_frames as source


class UnmappedBroker(source.Broker):
    def status(self):
        result = super().status()
        return result.replace("  source src/scope.cpp:11:1\n", "")

    def request(self, command):
        if command == f"frames wasm 1 {self.stop} 0 128":
            self.commands.append(command)
            return (f"wasm-frames stop={self.stop} thread=1 selected=0 count=1 total=1 first=0 physical=0\n"
                    f"  frame 0 kind=physical module=0 function=1 generation=1 runtime-epoch={self.epoch} "
                    "scope-unit=0 scope-offset=0 variables=current name=Wasm activation\nwasm-frames end\n")
        return super().request(command)


class UnmappedSourceTests(unittest.TestCase):
    send = source.SourceFrameTests.send
    frames = source.SourceFrameTests.frames
    source_scope = source.SourceFrameTests.source_scope
    assert_retired = source.SourceFrameTests.assert_retired

    def setUp(self):
        source.SourceFrameTests.setUp(self)
        self.broker = UnmappedBroker()
        self.adapter.broker = self.broker

    def test_authenticated_scopes_without_invented_lines_or_source_paths(self):
        rows = self.frames()
        self.assertEqual(self.broker.commands, ["status", "bt 1", "frames 1 41", "status"])
        self.assertEqual([row["name"] for row in rows[:3]], ["same", "same", "physical"])
        for row in rows:
            self.assertEqual((row["line"], row["column"]), (0, 0))
            self.assertNotIn("source", row)
        self.assertEqual(rows[2]["instructionPointerReference"], "wasm:0:1:4")
        self.assertEqual(len(self.adapter.source_frame_ordinals), 4)
        for ordinal in (0, 1, 2):
            scope = self.source_scope(rows[ordinal])
            result = self.send("variables", variablesReference=scope)
            self.assertTrue(result["success"], result)
            self.assertEqual(result["body"]["variables"][0]["value"], str(10 + ordinal))

    def test_watch_hover_and_variables_bind_exact_unmapped_frame(self):
        rows = self.frames()
        for ordinal, context in enumerate(("watch", "hover", "variables")):
            before = len(self.broker.commands)
            reply = self.send("evaluate", frameId=rows[ordinal]["id"], context=context, expression="value")
            self.assertTrue(reply["success"], reply)
            self.assertEqual(reply["body"]["result"], str(10 + ordinal))
            self.assertEqual(self.broker.commands[before:], ["status", f"print-frame 1 41 {ordinal} value", "status"])

    def test_line_mapping_appearing_does_not_change_activation_authority(self):
        old = self.frames()[2]
        scope = self.source_scope(old)
        self.broker.status = lambda: source.Broker.status(self.broker)
        reply = self.send("variables", variablesReference=scope)
        self.assertTrue(reply["success"], reply)
        self.assertEqual(reply["body"]["variables"][0]["value"], "12")

    def test_actual_step_retires_unmapped_frame_scope_and_watch(self):
        row = self.frames()[2]
        scope = self.source_scope(row)
        self.broker.stop += 1
        for command, arguments in (("variables", {"variablesReference": scope}),
                                   ("scopes", {"frameId": row["id"]}),
                                   ("evaluate", {"frameId": row["id"], "context": "watch", "expression": "value"})):
            result = self.send(command, **arguments)
            self.assertFalse(result["success"], result)
        self.assert_retired()

    def test_stop_or_epoch_change_during_page_cannot_grant_read(self):
        for name, value in (("stop", 42), ("epoch", 8), ("state", "running"), ("native", True)):
            with self.subTest(name=name):
                self.setUp()
                self.broker.after_frames = lambda: setattr(self.broker, name, value)
                result = self.send("stackTrace", threadId=1)
                self.assertFalse(result["success"], result)
                self.assert_retired()

    def test_wrong_current_activation_in_authenticated_page_rejected(self):
        self.broker.frame_packet = self.broker.frames().replace("kind=physical module=0 function=1", "kind=physical module=0 function=9")
        result = self.send("stackTrace", threadId=1)
        self.assertFalse(result["success"], result)
        self.assert_retired()

    def test_complete_source_page_required_without_line(self):
        self.broker.frame_packet = self.broker.frames().replace("source-frames end\n", "")
        result = self.send("stackTrace", threadId=1)
        self.assertFalse(result["success"], result)
        self.assert_retired()

    def test_metadata_failure_fallback_labels_grant_no_source_scope(self):
        self.broker.frame_error = True
        rows = self.frames()
        self.assertFalse(self.adapter.source_frame_ordinals)
        self.assertFalse(self.adapter.source_frames)
        for row in rows:
            scopes = self.send("scopes", frameId=row["id"])
            self.assertTrue(scopes["success"], scopes)
            self.assertFalse(any(s["name"] == "Source variables" for s in scopes["body"]["scopes"]))
        result = self.send("evaluate", frameId=rows[-1]["id"], context="watch", expression="value")
        self.assertFalse(result["success"], result)
        self.assert_retired()

    def test_native_stop_never_queries_language_page(self):
        self.broker.native = True
        rows = self.frames()
        self.assertEqual(len(rows), 1)
        self.assertFalse(any(c.startswith("frames ") for c in self.broker.commands))
        self.assertFalse(self.adapter.source_frame_ordinals)
        self.assertTrue(self.adapter.native_frames)

    def test_wasm_level_keeps_identity_only_activation_scopes(self):
        self.adapter.step_level = "wasm"
        rows = self.frames()
        self.assertEqual(len(rows), 1)
        self.assertIn("frames wasm 1 41 0 128", self.broker.commands)
        self.assertFalse(self.adapter.source_frame_ordinals)
        self.assertFalse(self.adapter.source_frames)
        scopes = self.send("scopes", frameId=rows[0]["id"])
        self.assertTrue(scopes["success"], scopes)
        self.assertFalse(any(s["name"] == "Source variables" for s in scopes["body"]["scopes"]))


if __name__ == "__main__":
    unittest.main()
