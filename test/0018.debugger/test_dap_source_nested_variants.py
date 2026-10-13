#!/usr/bin/env python3
"""Detached copied nested variant DATA; actual producer stops have a runner."""
import unittest
import test_dap_source_objects as objects
import test_dap_source_variants as variants

def branch(name, children):
    # All extents deliberately overlap in this formatting-only control.
    rows=[f'object: type=Outer, offset=0, bytes=128',
          '  <variant-part>: type=unavailable, offset=0, bytes=128, variant-part',
          f'    {name}: type=unavailable, offset=0, bytes=128, active-variant']
    return '\n'.join(rows + ['      '+r for r in children.splitlines()])

def inner(name, count, zeros=False):
    rows=[f'{name}: type=Inner, offset=0, bytes=128',
          '  <variant-part>: type=unavailable, offset=0, bytes=128, variant-part',
          '    In: type=unavailable, offset=0, bytes=128, active-variant']
    rows += [f'      f{i}: type=Unit, offset=0, bytes=0' if zeros else
             f'      f{i}: type=i32, offset=0, bytes=4, value=1' for i in range(count)]
    if zeros:rows += ['      value: type=i32, offset=0, bytes=4, value=7']
    return '\n'.join(rows)

class NestedVariantTests(unittest.TestCase):
    def control(self, body):
        c=objects.SourceObjectTests();c.setUp();_,r=c.evaluate(body)
        self.assertTrue(r['success'],r);self.assertGreater(r['body']['variablesReference'],0)
        self.assertEqual(sum(s.startswith('print-frame ') for s in c.broker.commands),1)
        self.assertNotIn('evaluateName',r['body']);self.assertNotIn('memoryReference',r['body'])
        return c,r

    def test_nested_cases_from_copied_markers_keep_every_original_node(self):
        body=branch('Wrapped',variants.BODY.replace('object:','data:',1))
        c,r=self.control(body)
        self.assertEqual(r['body']['result'],'Wrapped { data.seed: 5, data.payload: Negative { signed: -3 }, data.option: Some { __0: 13 } }')
        nodes=objects.dap.parse_source_object_display(objects.packet(body),41,1)
        self.assertEqual(len(nodes),len(body.splitlines()))
        payload=next(n for n in nodes if n['name']=='payload')
        self.assertEqual(payload['value'],'Negative { signed: -3 }')
        self.assertEqual(payload['type'],'Payload')
        self.assertFalse(any(s=='continue' or s.startswith('read ') for s in c.broker.commands))

    def test_unavailable_nested_selection_or_payload_never_publishes_a_partial_outer_payload(self):
        original=branch('Wrapped',variants.BODY.replace('object:','data:',1))
        for body in (original.replace('value=-3\n','unavailable=unsupported type\n'),
                     original.replace(', active-variant\n',', active-variant, unavailable=nested variant layout is unavailable\n',1),
                     original.replace('signed:','bad.name:')):
            with self.subTest(body=body):
                _,r=self.control(body)
                if ', active-variant, unavailable=' in body.splitlines()[2]:self.assertEqual(r['body']['result'],'Outer')
                else:self.assertEqual(r['body']['result'],'Wrapped')
        # Failure of the inner branch's copy keeps the known outer case only.
        body=original.replace('      Negative: type=unavailable, offset=4, bytes=8, active-variant',
                              '      Negative: type=unavailable, offset=4, bytes=8, active-variant, unavailable=nested variant layout is unavailable')
        _,r=self.control(body);self.assertEqual(r['body']['result'],'Wrapped')

    def test_array_path_is_display_text_and_keeps_indexed_expansion(self):
        body=branch('Packet','items: type=Array, offset=0, bytes=8\n'
                    '  [0]: type=i32, offset=0, bytes=4, value=17\n'
                    '  [1]: type=i32, offset=4, bytes=4, value=19')
        c,r=self.control(body);self.assertEqual(r['body']['result'],'Packet { items[0]: 17, items[1]: 19 }')
        case=c.children(c.children(r['body']['variablesReference'])[0]['variablesReference'])[0]
        array=c.children(case['variablesReference'])[0]
        self.assertEqual([v['value'] for v in c.children(array['variablesReference'],filter='indexed')],['17','19'])
        self.assertEqual(c.children(array['variablesReference'],filter='named'),[])
        self.assertEqual(sum(s.startswith('print-frame ') for s in c.broker.commands),1)

    def test_node_and_value_budgets_are_shared_across_nested_cases(self):
        for children in (inner('a',30,True)+'\n'+inner('b',30,True),
                         '\n'.join(inner(n,10) for n in ('a','b','c'))):
            body=branch('Out',children);_,r=self.control(body);self.assertEqual(r['body']['result'],'Out')
            nodes=objects.dap.parse_source_object_display(objects.packet(body),41,1)
            self.assertEqual(len(nodes),len(body.splitlines()))
            for n in nodes:
                if n['name'] in ('a','b','c'):self.assertIn('In {',n['value'])

    def test_depth_budget_and_unknown_nested_labels_keep_proven_case_only(self):
        nested=inner('payload',1)
        # Five ordinary containers followed by a nested case with four fields
        # exceed the shared path depth without exceeding the packet tree depth.
        nested=nested.replace('      f0:', '      w: type=Wrapper, offset=0, bytes=128\n'
                              '        x: type=Wrapper, offset=0, bytes=128\n'
                              '          y: type=Wrapper, offset=0, bytes=128\n'
                              '            f0:')
        for name in ('e','d','c','b','a'):
            nested=f'{name}: type=Wrapper, offset=0, bytes=128\n'+'\n'.join('  '+r for r in nested.splitlines())
        _,r=self.control(branch('Out',nested));self.assertEqual(r['body']['result'],'Out')
        _,r=self.control(branch('Out',inner('x;continue\\x1b',1)));self.assertEqual(r['body']['result'],'Out')

if __name__=='__main__':unittest.main()
