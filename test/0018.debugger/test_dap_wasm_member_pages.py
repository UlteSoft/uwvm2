#!/usr/bin/env python3
"""Standard GC member pages; detached DATA, not capture or address authority."""
import unittest
import test_dap_wasm_state as state


class Broker(state.Broker):
    def __init__(self):
        super().__init__(); self.total = 160; self.pages = 0
        self.change = None; self.short = False; self.after_page = None
        self.probe_total = None; self.probe_label = 'array module=0 type=3'
        self.selection = 'globals'; self.probe_module = 0

    def request(self, command):
        prefix = 'local' if self.selection == 'locals' else 'global'
        root_command = 'locals wasm 1 0 0 64' if self.selection == 'locals' else 'globals 1 0 0 64'
        if command == root_command:
            self.commands.append(command)
            return (f'wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first=0 total=1\n'
                    f'{prefix} 0 (ref null type-index=3 module=0) = array #1\n'
                    f'object #1 array module=0 type=3 members={self.total}\n  members truncated\n')
        if command.startswith(f'members {self.selection} 1 0 0 0 0 '):
            self.commands.append(command); self.pages += 1
            words = command.split(); first, count = map(int, words[7:9])
            total = self.total if self.pages > 1 or self.probe_total is None else self.probe_total
            epoch = 7
            if self.pages > 1 and self.change == 'total': total += 1
            if self.pages > 1 and self.change == 'epoch': epoch += 1
            end = min(first + count, total)
            if self.short and count > 1: end -= 1
            label = self.probe_label if self.pages == 1 else 'array module=0 type=3'
            text = (f'wasm-stop {self.stop}\nWasm state thread=1 module={self.probe_module} epoch={epoch} first=0 total=1\n'
                    f'Wasm members object=1 first={first} count={count} path=-\n'
                    f'{prefix} 0 (ref null type-index=3 module=0) = array #1\n'
                    f'object #1 {label} members={total} first={first} next={end} more={"yes" if end < total else "no"}\n')
            text += ''.join(f'  {i} i32 = {1000+i} mutable\n' for i in range(first, end))
            if end < total: text += '  member page; continue with original root/path and next\n'
            elif first: text += '  final member page; earlier members are outside this window\n'
            if self.after_page is not None: self.after_page()
            return text
        return super().request(command)


class MemberPages(unittest.TestCase):
    send = state.StateProtocol.send

    def setUp(self):
        state.StateProtocol.setUp(self); self.adapter.broker = self.broker = Broker()

    def root(self):
        reply = self.send('uwvm/wasmState', threadId=1, selection=self.broker.selection)
        self.assertTrue(reply['success'], reply)
        return reply['body']['variables'][0]['variablesReference']

    def test_member_count_hint_is_exact_or_omitted_outside_dap_int32(self):
        for total in (160, (1 << 31)-1, 1 << 31):
            with self.subTest(total=total):
                self.setUp(); self.broker.total = total
                reply = self.send('uwvm/wasmState',threadId=1,selection='globals')
                value = reply['body']['variables'][0]
                self.assertGreater(value['variablesReference'],0)
                self.assertNotIn('memoryReference',value)
                if total < 1 << 31:
                    self.assertEqual(value['indexedVariables'],total); self.assertEqual(value['namedVariables'],0)
                else:
                    self.assertNotIn('indexedVariables',value); self.assertNotIn('namedVariables',value)

    def test_finite_pages_cross_transport_boundary_and_end(self):
        for first, count in ((0, 1), (63, 80), (129, 8), (159, 64), (160, 4), ((1 << 64)-1, 1)):
            with self.subTest(first=first, count=count):
                self.setUp(); ref = self.root()
                reply = self.send('variables', variablesReference=ref, start=first, count=count, filter='indexed')
                self.assertTrue(reply['success'], reply); rows = reply['body']['variables']
                expected = range(first, min(first + count, self.broker.total))
                self.assertEqual([v['name'] for v in rows], [f'[{i}]' for i in expected])
                self.assertEqual([v['value'] for v in rows], [str(1000+i) for i in expected])
                self.assertTrue(all(v['variablesReference'] == 0 and 'memoryReference' not in v for v in rows))

    def test_zero_and_omission_copy_all_remaining_in_bounded_chunks(self):
        for args, expected in (({}, range(160)), ({'start':60, 'count':0}, range(60,160))):
            with self.subTest(args=args):
                self.setUp(); ref = self.root()
                result = self.send('variables', variablesReference=ref, **args)
                self.assertTrue(result['success'], result)
                self.assertEqual([v['name'] for v in result['body']['variables']], [f'[{i}]' for i in expected])
                self.assertTrue(all(int(c.split()[8]) <= 64 for c in self.broker.commands if c.startswith('members ')))

    def test_named_filter_is_empty_without_member_borrow(self):
        ref = self.root(); begin = len(self.broker.commands)
        result = self.send('variables', variablesReference=ref, filter='named')
        self.assertEqual(result['body']['variables'], [])
        self.assertFalse(any(c.startswith('members ') for c in self.broker.commands[begin:]))

    def test_invalid_pages_fail_before_broker_and_retire_references(self):
        invalid = [{'start':x} for x in (-1,True,1.5,'1',1<<64)]
        invalid += [{'count':x} for x in (-1,True,1.5,'1',1025)]
        invalid += [{'filter':x} for x in ('all','',True,[],{})]
        for args in invalid:
            with self.subTest(args=args):
                self.setUp(); ref = self.root(); before = list(self.broker.commands)
                result = self.send('variables', variablesReference=ref, **args)
                self.assertFalse(result['success'], result); self.assertNotIn('body', result)
                self.assertEqual(self.broker.commands, before); self.assertFalse(self.adapter.wasm_object_views)

    def test_probe_cardinality_and_type_match_original_display(self):
        for selection in ('globals','locals'):
            for attribute, value in (('probe_total',161), ('probe_label','array module=0 type=4'), ('probe_module',9)):
                with self.subTest(selection=selection,attribute=attribute):
                    self.setUp(); self.broker.selection = selection; ref = self.root(); setattr(self.broker,attribute,value)
                    result = self.send('variables',variablesReference=ref,start=160,count=1)
                    self.assertFalse(result['success'],result); self.assertNotIn('body',result)

    def test_changed_short_or_retired_chunk_never_returns_partial_success(self):
        for kind in ('total','epoch','short','stop'):
            with self.subTest(kind=kind):
                self.setUp(); ref = self.root()
                if kind in ('total','epoch'): self.broker.change = kind
                elif kind == 'short': self.broker.short = True
                else:
                    def retire():
                        if self.broker.pages == 3: self.broker.stop += 1
                    self.broker.after_page = retire
                result = self.send('variables',variablesReference=ref,start=63,count=80)
                self.assertFalse(result['success'],result); self.assertNotIn('body',result)
                self.assertFalse(self.adapter.wasm_object_views)

    def test_large_default_requires_bounded_page_but_deep_index_is_usable(self):
        self.broker.total = 2000; ref = self.root()
        reply = self.send('variables',variablesReference=ref)
        self.assertFalse(reply['success'],reply); self.assertNotIn('body',reply)
        ref = self.root(); reply = self.send('variables',variablesReference=ref,start=1900,count=80)
        self.assertTrue(reply['success'],reply)
        self.assertEqual(len(reply['body']['variables']),80)

    def test_native_or_external_stop_cannot_rebind_original_object(self):
        for native in (False,True):
            with self.subTest(native=native):
                self.setUp(); ref = self.root(); before = len(self.broker.commands)
                self.broker.stop += 1; self.broker.native = native
                result = self.send('variables',variablesReference=ref,start=129,count=8)
                self.assertFalse(result['success'],result)
                self.assertFalse(any(c.startswith('members ') for c in self.broker.commands[before:]))


if __name__ == '__main__': unittest.main()
