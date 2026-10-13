"""Reply DATA regression checks; these do not qualify a Wasm feature or an OS."""
import unittest
from wasm_query_evidence import query_evidence


class QueryEvidenceTests(unittest.TestCase):
    def query(self, reply, command="operands 1"):
        return query_evidence(command, reply, participant=1, stop=41)

    def test_empty_current_state_is_observed(self):
        r=self.query(b"wasm-stop 41\nWasm state thread=1 module=0 epoch=7 first=0 total=0\n")
        self.assertTrue(r["available"]);self.assertEqual(r["rows"],0)

    def test_claimed_nonempty_state_requires_actual_contiguous_rows(self):
        header=b"wasm-stop 41\nWasm state thread=1 module=0 epoch=7 first=0 total=2\n"
        self.assertFalse(self.query(header)["available"])
        rows=b"operand 0 i32 = 7\noperand 1 i64 = 9\n"
        result=self.query(header+rows)
        self.assertTrue(result["available"]);self.assertEqual(result["rows"],2)
        self.assertFalse(self.query(header+rows.replace(b"operand 1",b"operand 0"))["available"])

    def test_recognized_command_error_is_not_support(self):
        self.assertFalse(self.query(b"error: unsupported command\n")["available"])

    def test_unavailable_state_is_not_support(self):
        self.assertFalse(self.query(b"wasm-stop 41\nWasm state unavailable: snapshot unavailable\n")["available"])

    def test_stale_identity_missing_header_and_zero_epoch_are_rejected(self):
        valid=b"wasm-stop 41\nWasm state thread=1 module=0 epoch=7 first=0 total=2\n"
        for invalid in [valid.replace(b"41",b"40"),valid.replace(b"thread=1",b"thread=2"),valid.replace(b"module=0",b"module=1"),valid.replace(b"epoch=7",b"epoch=0"),valid+valid,b"wasm-stop 41\n"]:
            with self.subTest(invalid=invalid):self.assertFalse(self.query(invalid)["available"])

    def test_physical_wasm_backtrace_required(self):
        reply=b"stopped: breakpoint\nstop-id 41\nthread 1 module=0 function=1 byte-offset=0 generation=1\n  #0 module=0 function=1 [fixture] run\n  inline metadata unavailable: no embedded metadata\n"
        self.assertTrue(self.query(reply,"bt 1")["available"])
        self.assertFalse(self.query(reply.replace(b"  #0 module=0 function=1 [fixture] run",b"  backtrace unavailable for this stop"),"bt 1")["available"])


if __name__=="__main__":unittest.main()
