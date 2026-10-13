"""Protocol DATA only. Real code qualification remains in the backend."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("dap_public_rows", Path(__file__).resolve().parents[2] / "tools/debug/dap_adapter.py")
dap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dap)
location = dict(stop_id=17, id=3, module=0, function=2, generation=8, native_pc="0x1100")
plain = "native-disassembly stop=17 thread=3 module=0 function=2 function-generation=2 runtime-epoch=8"
page = ("native-disassembly-range stop=17 thread=3 module=0 function=2 function-generation=2 runtime-epoch=8 "
        "{origin}reference-pc=0x1100 owner-begin=0x1000 owner-end=0x1600 byte-offset=0 instruction-offset=0 resolve-symbols=0\n")
end = "native-disassembly-end\n"


class Tests(unittest.TestCase):
    def test_current_production_origins_and_legacy_packets(self):
        for origin in ("", "origin=native-instruction-stop ", "origin=safepoint-code-view "):
            with self.subTest(origin=origin):
                identity, rows = dap.parse_native_disassembly_range(page.format(origin=origin) + "  instruction 0 unavailable\n" + end,
                                                                    location, 1, 0, 0, False)
                self.assertEqual(rows, [dict(address="-1", instruction="<unavailable instruction>", presentationHint="invalid")])
                self.assertEqual(identity[0], 17)
                header = plain + (" " + origin.rstrip() if origin else "") + "\n"
                rows = dap.parse_native_disassembly(header + "  instruction 0 pc=0x1100 unavailable\n" + end, location, 1)
                self.assertEqual(rows, [dict(address="-1", instruction="<unavailable instruction>", presentationHint="invalid")])

    def test_current_hidden_row_cannot_resume_at_invented_boundary(self):
        text = page.format(origin="origin=native-instruction-stop ") + "  instruction 0 unavailable\n  instruction 1 pc=0x1100 bytes=90  nop\n" + end
        with self.assertRaises(ValueError):
            dap.parse_native_disassembly_range(text, location, 2, 0, 0, False)

    def test_unknown_origin_and_wrong_visible_current_pc_are_rejected(self):
        for text in (page.format(origin="origin=host-vm ") + "  instruction 0 unavailable\n" + end,
                     page.format(origin="") + "  instruction 0 pc=0x1101 bytes=90  nop\n" + end):
            with self.assertRaises(ValueError):
                dap.parse_native_disassembly_range(text, location, 1, 0, 0, False)


if __name__ == "__main__":
    unittest.main()
