#!/usr/bin/env python3
"""Detached formatter DATA controls; actual guest reads use the real Linux runner."""
import io, unittest
import test_dap_source_frames as frames

dap = frames.dap


def packet(body, stop=41):
    return f'source-value stop={stop} name=*box\n{body}\nsource-value end\n'


BODY = ('object: type=Holder, offset=0, bytes=16\n'
        '  count: type=int32, offset=0, bytes=4, value=3\n'
        '  group: type=Inner, offset=4, bytes=12\n'
        '    ptr: type=pointer, offset=4, bytes=4, value=guest:0x20\n'
        '    array: type=array, offset=8, bytes=8\n'
        '      [0]: type=int32, offset=8, bytes=4, value=-1\n'
        '      [1]: type=int32, offset=12, bytes=4, value=7')


class SourceObjectTests(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO(); self.adapter = dap.Adapter(self.output)
        self.broker = frames.Broker(); self.adapter.broker = self.broker
        self.seq = 0

    def send(self, command, **arguments):
        self.seq += 1;self.output.seek(0);self.output.truncate()
        self.adapter.handle(dict(seq=self.seq,type='request',command=command,arguments=arguments))
        replies = [r for r in frames.messages(self.output.getvalue()) if r['type']=='response']
        self.assertEqual(len(replies),1)
        return replies[0]

    def evaluate(self, body=BODY, typed=True):
        self.adapter.supports_variable_type = typed
        frame = self.send('stackTrace',threadId=1)['body']['stackFrames'][1]
        self.broker.print_packet = packet(body,self.broker.stop)
        result = self.send('evaluate',expression='*box',context='watch',frameId=frame['id'])
        return frame, result

    def children(self, ref, **kwargs):
        reply = self.send('variables',variablesReference=ref,**kwargs)
        self.assertTrue(reply['success'],reply)
        return reply['body']['variables']

    def test_nested_named_and_indexed_views_and_paging(self):
        _,reply = self.evaluate();self.assertTrue(reply['success'],reply)
        root=reply['body'];self.assertEqual(root['result'],'Holder')
        self.assertEqual((root['namedVariables'],root['indexedVariables']),(2,0))
        variables=self.children(root['variablesReference']);self.assertEqual([v['name'] for v in variables],['count','group'])
        self.assertEqual(self.children(root['variablesReference'],start=1,count=1),variables[1:])
        self.assertEqual(self.children(root['variablesReference'],filter='indexed'),[])
        inner=self.children(variables[1]['variablesReference'])
        array=inner[1];self.assertEqual((array['namedVariables'],array['indexedVariables']),(0,2))
        elements=self.children(array['variablesReference'],filter='indexed')
        self.assertEqual([v['value'] for v in elements],['-1','7'])
        self.assertEqual(self.children(array['variablesReference'],filter='named'),[])
        self.assertEqual(self.children(array['variablesReference'],start=1,count=0),elements[1:])
        self.assertEqual(self.children(array['variablesReference'],start=1024),[])
        for value in [root,*variables,*inner,*elements]:
            self.assertNotIn('memoryReference',value);self.assertNotIn('evaluateName',value)
            self.assertEqual(value['presentationHint']['attributes'],['readOnly'])
        # Expansion checks status twice and never turns display labels into
        # another print-frame, memory read or dereference request.
        self.assertEqual(sum(c.startswith('print-frame ') for c in self.broker.commands),1)

    def test_pointer_leaf_and_type_capability(self):
        _,reply=self.evaluate('object: type=pointer, offset=0, bytes=4, value=guest:0x20',typed=False)
        self.assertTrue(reply['success'],reply);self.assertEqual(reply['body']['result'],'guest:0x20')
        self.assertEqual(reply['body']['variablesReference'],0);self.assertNotIn('type',reply['body'])
        _,reply=self.evaluate(typed=False)
        for row in self.children(reply['body']['variablesReference']):self.assertNotIn('type',row)

    def test_labels_are_opaque_not_query_selectors(self):
        body=('object: type=Holder, offset=0, bytes=4\n'
              '  x;continue\\x1b: type=pointer, offset=0, bytes=4, value=guest:0x20')
        _,reply=self.evaluate(body);self.assertTrue(reply['success'],reply)
        self.assertEqual(self.children(reply['body']['variablesReference'])[0]['name'],'x;continue\\x1b')
        self.assertFalse(any(c=='continue' or c.startswith('read ') for c in self.broker.commands))

    def test_invalid_complete_tree_never_publishes_a_prefix(self):
        bad=('  '+BODY, BODY.replace('  count:', '    count:'), BODY+'\nobject: type=int, offset=0, bytes=4, value=1',
             BODY.replace('offset=12, bytes=4','offset=13, bytes=4'), BODY.replace('offset=4, bytes=12','offset=5, bytes=11'),
             BODY.replace('offset=0, bytes=16','offset=18446744073709551616, bytes=16'),
             BODY.replace('bytes=16','bytes=65537'), BODY.replace('  count:', '   count:'),
             BODY.replace('guest:0x20','guest:0x100000000'),
             BODY.replace('type=int32','type=int32, offset=0'), BODY.replace('count:', 'count: type=bad:'),
             BODY.replace('ptr:', 'ptr\\x1B:'))
        for body in bad:
            with self.subTest(body=body):
                # An escaped uppercase hex byte is rejected by the common
                # canonical metadata validator, not interpreted as a command.
                self.setUp();_,reply=self.evaluate(body)
                self.assertFalse(reply['success'],reply);self.assertNotIn('body',reply)
                self.assertFalse(self.adapter.source_object_views)

    def test_unsupported_or_truncated_packet_stays_complete_opaque(self):
        for body in (BODY+'\nsource-object output-truncated shown=7 total=8',
                     BODY.replace('value=3','value=true (COUNT)'), BODY.replace('bytes=16','bytes=16, omitted-elements=2'),
                     BODY.replace('bytes=12','bytes=12, variant-part')):
            with self.subTest(body=body):
                _,reply=self.evaluate(body);self.assertTrue(reply['success'],reply)
                self.assertEqual(reply['body']['variablesReference'],0)
                self.assertEqual(reply['body']['result'],packet(body).rstrip('\n'))
                self.assertNotIn('type',reply['body'])

    def test_unknown_scalar_encoding_does_not_reject_possible_float_display(self):
        for spelling in ('-0','4294967296','-2147483649','18446744073709551616','01'):
            # Encoding is absent in the existing text protocol. This is DATA
            # compatibility control, not a live float producer qualification.
            for body in (f'object: type=RealAlias, offset=0, bytes=4, value={spelling}',
                         BODY.replace('value=3','value='+spelling)):
                _,reply=self.evaluate(body)
                self.assertTrue(reply['success'],reply)
                self.assertEqual(reply['body']['result'],packet(body,self.broker.stop).rstrip('\n'))
                self.assertEqual(reply['body']['variablesReference'],0)
                self.assertFalse(self.adapter.source_object_views)

    def test_builtin_width_validation_is_not_bypassed(self):
        for body in ('$len: type=int, offset=0, bytes=4, value=-1',
                     '$expression: type=signed integer, offset=0, bytes=4, value=2147483648'):
            _,reply=self.evaluate(body);self.assertFalse(reply['success'],reply)

    def test_real_status_change_retires_copied_tree_before_expansion(self):
        _,reply=self.evaluate();ref=reply['body']['variablesReference'];self.broker.stop+=1
        result=self.send('variables',variablesReference=ref);self.assertFalse(result['success'])
        self.assertFalse(self.adapter.source_object_views)

    def test_direct_new_stopped_snapshot_retires_cache_and_budgets(self):
        _,reply=self.evaluate();old=reply['body']['variablesReference']
        self.assertGreater(self.adapter.source_object_nodes,0)
        self.assertGreater(self.adapter.source_object_bytes,0)
        self.broker.stop+=1
        self.adapter.observe(self.broker.status(),notify=False)
        self.assertFalse(self.adapter.source_object_views)
        self.assertEqual(self.adapter.source_object_nodes,0)
        self.assertEqual(self.adapter.source_object_bytes,0)
        _,reply=self.evaluate()
        self.assertGreater(reply['body']['variablesReference'],old)

    def test_post_status_change_prevents_publication(self):
        _,reply=self.evaluate();ref=reply['body']['variablesReference']
        original=self.broker.request;calls=0
        def request(command):
            nonlocal calls
            if command=='status':
                calls+=1
                if calls==2:self.broker.epoch+=1
            return original(command)
        self.broker.request=request
        result=self.send('variables',variablesReference=ref);self.assertFalse(result['success'])
        self.assertNotIn('body',result);self.assertFalse(self.adapter.source_object_views)

    def test_mutation_retirement_and_no_reference_reuse(self):
        _,reply=self.evaluate();old=reply['body']['variablesReference']
        # A real frame/stop is refreshed after adapter retirement; the fixture
        # Broker increments the same-PC stop for this synthetic replacement.
        self.send('evaluate',expression='replace 0 1 1 /tmp/payload',context='repl')
        self.assertFalse(self.send('variables',variablesReference=old)['success'])
        _,reply=self.evaluate();self.assertGreater(reply['body']['variablesReference'],old)

    def test_invalid_page_retires_without_querying_children(self):
        for args in ({'start':-1},{'start':True},{'start':1025},{'count':1025},{'count':True},{'filter':'all'}):
            _,reply=self.evaluate();before=len(self.broker.commands)
            response=self.send('variables',variablesReference=reply['body']['variablesReference'],**args)
            self.assertFalse(response['success']);self.assertEqual(before,len(self.broker.commands))
            self.assertFalse(self.adapter.source_object_views)

    def test_both_retained_view_budgets_enforced_before_reference_allocation(self):
        for metric,limit in (('source_object_nodes',8192),('source_object_bytes',512*1024)):
            self.setUp();frame,_=self.evaluate();self.adapter.source_object_views.clear()
            setattr(self.adapter,metric,limit);before=self.adapter.next_reference
            reply=self.send('evaluate',expression='*box',context='watch',frameId=frame['id'])
            self.assertFalse(reply['success']);self.assertEqual(self.adapter.next_reference,before)
            self.assertFalse(self.adapter.source_object_views)
            self.assertEqual(self.adapter.source_object_nodes,0);self.assertEqual(self.adapter.source_object_bytes,0)

    def test_depth_and_node_budgets(self):
        body='\n'.join('  '*d+f'level{d}: type=empty, offset=0, bytes=0' for d in range(33))
        self.assertEqual(len(dap.parse_source_object_display(packet(body),41,1)),33)
        with self.assertRaises(ValueError):dap.parse_source_object_display(packet(body+'\n'+'  '*33+'deep: type=empty, offset=0, bytes=0'),41,1)
        body='object: type=empty, offset=0, bytes=0\n'+'\n'.join(f'  a{i}: type=empty, offset=0, bytes=0' for i in range(1024))
        with self.assertRaises(ValueError):dap.parse_source_object_display(packet(body),41,1)


if __name__=='__main__':unittest.main()
