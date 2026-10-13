#!/usr/bin/env python3
"""Detached source-symbol routing DATA; real guest proof uses the Linux driver."""
import unittest
import test_dap_source_objects as objects
import test_dap_source_frames as frames

dap=frames.dap
UNAVAILABLE='unavailable (unsupported or unknown scalar type)'
SOFT='error: source value unavailable: current captured location, layout or single unshared local-memory read is unavailable\n'


class ScopeBroker(frames.Broker):
    def __init__(self):
        super().__init__();self.names=['box'];self.root_value='guest:0x20';self.root_type='pointer'
        self.root_error=None;self.target_error=None;self.target_body=objects.BODY
        self.after_root=None;self.after_target=None;self.root_header=None
    def request(self, command):
        if command.startswith('locals source '):
            self.commands.append(command)
            return f'source-stop {self.stop}\n'+''.join(f'source local {name} type= = {UNAVAILABLE}\n' for name in self.names)+'source local value type=int = i32=11\n'
        if command.startswith('print-frame '):
            self.commands.append(command);words=command.split(maxsplit=4)
            if words[:4]!=['print-frame','1',str(self.stop),'1']:
                return 'error: unexpected frame identity\n'
            name=words[4];target=name.startswith('*')
            error=self.target_error if target else self.root_error
            result=error if error is not None else (f'source-value stop={self.stop} name={self.root_header or name}\n'+
                (self.target_body if target else f'object: type={self.root_type}, offset=0, bytes=4, value={self.root_value}')+'\nsource-value end\n')
            callback=self.after_target if target else self.after_root
            if callback:callback()
            return result
        return super().request(command)


class SourceScopeObjectTests(unittest.TestCase):
    send=objects.SourceObjectTests.send
    children=objects.SourceObjectTests.children
    def setUp(self):
        objects.SourceObjectTests.setUp(self);self.broker=ScopeBroker();self.adapter.broker=self.broker
        self.adapter.supports_variable_type=True
    def source(self):
        frame=self.send('stackTrace',threadId=1)['body']['stackFrames'][1]
        scopes=self.send('scopes',frameId=frame['id'])['body']['scopes']
        return frame,next(s['variablesReference'] for s in scopes if s['name']=='Source variables')
    def roots(self,ref,**kwargs):
        return self.children(ref,**kwargs)
    def prints(self):
        return [c for c in self.broker.commands if c.startswith('print-frame ')]
    def test_scope_pointer_is_reselected_and_only_expanded_on_request(self):
        _,scope=self.source();rows=self.roots(scope);box=rows[0]
        self.assertEqual((box['name'],box['type'],box['value']),('box','pointer','guest:0x20'))
        self.assertGreater(box['variablesReference'],0);self.assertEqual(self.prints(),['print-frame 1 41 1 box'])
        for key in ('memoryReference','evaluateName','namedVariables','indexedVariables'):self.assertNotIn(key,box)
        members=self.children(box['variablesReference'])
        self.assertEqual([m['name'] for m in members],['count','group'])
        self.assertEqual(self.prints(),['print-frame 1 41 1 box','print-frame 1 41 1 *box'])
        nested=self.children(members[1]['variablesReference'])
        self.assertEqual(nested[0]['value'],'guest:0x20');self.assertEqual(nested[0]['variablesReference'],0)
        self.children(box['variablesReference'],start=1,count=1)
        self.assertEqual(len(self.prints()),2) # Snapshot pagination cannot chase labels.
    def test_null_named_pointer_and_client_type_negotiation(self):
        for value,typed in (('guest:0x0',True),('guest:0x20',False)):
            self.setUp();self.adapter.supports_variable_type=typed
            self.broker.root_type='NamedPointer';self.broker.root_value=value
            _,scope=self.source();box=self.roots(scope)[0]
            self.assertEqual(box['value'],value);self.assertEqual('type' in box,typed)
            self.assertEqual(box['variablesReference']==0,value=='guest:0x0')
            self.assertEqual(len(self.prints()),1)
    def test_off_page_indexed_and_escaped_names_do_not_issue_root_queries(self):
        self.broker.names=['x;continue','box\\x1b','a.b','true','len','_ok']
        _,scope=self.source()
        self.assertEqual(self.roots(scope,filter='indexed'),[]);self.assertFalse(self.prints())
        self.roots(scope,start=0,count=5);self.assertFalse(self.prints())
        rows=self.roots(scope,start=5,count=1);self.assertGreater(rows[0]['variablesReference'],0)
        self.assertEqual(self.prints(),['print-frame 1 41 1 _ok'])
    def test_duplicate_names_outside_page_are_not_reselected(self):
        self.broker.names=['box','other','box'];_,scope=self.source()
        self.assertEqual(self.roots(scope,start=0,count=1)[0]['value'],UNAVAILABLE)
        self.assertFalse(self.prints())
    def test_per_page_query_budget_preserves_unqueried_rows(self):
        self.broker.names=[f'p{i}' for i in range(64)]
        _,scope=self.source();rows=self.roots(scope)
        self.assertEqual(len(self.prints()),32)
        self.assertTrue(all(r['variablesReference']>0 for r in rows[:32]))
        self.assertTrue(all(r['value']==UNAVAILABLE for r in rows[32:64]))
        self.roots(scope,start=32,count=32);self.assertEqual(len(self.prints()),64)
    def test_root_unavailability_does_not_hide_supported_sibling(self):
        self.broker.root_error=SOFT;frame,scope=self.source();rows=self.roots(scope)
        self.assertEqual(rows[0]['value'],UNAVAILABLE);self.assertEqual(rows[1]['value'],'11')
        self.assertIn(frame['id'],self.adapter.source_frame_ordinals);self.assertIn(scope,self.adapter.source_scope_stops)
    def test_target_unavailability_is_copied_readonly_and_not_retried(self):
        self.broker.target_error=SOFT;frame,scope=self.source();pointer=self.roots(scope)[0]['variablesReference']
        rows=self.children(pointer)
        self.assertEqual(rows[0]['name'],'<unavailable>');self.assertEqual(rows[0]['variablesReference'],0)
        self.assertIn(frame['id'],self.adapter.source_frame_ordinals)
        self.children(pointer);self.assertEqual(len(self.prints()),2)
    def test_scalar_pointee_and_unsupported_root_packets(self):
        self.broker.target_body='object: type=int32, offset=0, bytes=4, value=3'
        _,scope=self.source();rows=self.children(self.roots(scope)[0]['variablesReference'])
        self.assertEqual((rows[0]['name'],rows[0]['value'],rows[0]['variablesReference']),('*','3',0))
        self.setUp();self.broker.root_value='true (ENUM)';_,scope=self.source()
        self.assertEqual(self.roots(scope)[0]['value'],UNAVAILABLE)
    def test_wrong_echo_and_incomplete_target_cannot_create_a_prefix(self):
        self.broker.root_header='another';_,scope=self.source()
        self.assertEqual(self.roots(scope)[0]['value'],UNAVAILABLE);self.assertFalse(self.adapter.source_pointer_views)
        self.setUp();_,scope=self.source();ref=self.roots(scope)[0]['variablesReference']
        self.broker.target_error='source-value stop=41 name=*box\n'+objects.BODY+'\n'
        reply=self.send('variables',variablesReference=ref)
        self.assertFalse(reply['success']);self.assertNotIn('body',reply);self.assertFalse(self.adapter.source_pointer_views)
    def test_new_stop_clears_lazy_views_and_frame_bindings(self):
        _,scope=self.source();ref=self.roots(scope)[0]['variablesReference']
        self.assertTrue(self.adapter.source_scope_frames);self.assertTrue(self.adapter.source_pointer_views)
        self.broker.stop+=1;self.adapter.observe(self.broker.status(),notify=False)
        self.assertFalse(self.adapter.source_scope_frames);self.assertFalse(self.adapter.source_pointer_views)
        before=len(self.prints());self.assertFalse(self.send('variables',variablesReference=ref)['success'])
        self.assertEqual(len(self.prints()),before);self.assertEqual(self.adapter.source_object_nodes,0)
    def test_both_reads_reject_post_query_retirement(self):
        for target in (False,True):
            self.setUp();_,scope=self.source()
            if target:
                ref=self.roots(scope)[0]['variablesReference'];self.broker.after_target=lambda:setattr(self.broker,'stop',42)
            else:
                ref=scope;self.broker.after_root=lambda:setattr(self.broker,'stop',42)
            reply=self.send('variables',variablesReference=ref)
            self.assertFalse(reply['success']);self.assertNotIn('body',reply)
            self.assertFalse(self.adapter.source_pointer_views);self.assertFalse(self.adapter.source_scope_frames)
    def test_other_errors_and_transport_failure_retire_all_references(self):
        for error in ('error: command or thread is not valid in the current execution state\n','error: bounded controller capacity exhausted\n'):
            self.setUp();self.broker.root_error=error;_,scope=self.source()
            self.assertFalse(self.send('variables',variablesReference=scope)['success'])
            self.assertFalse(self.adapter.source_scope_frames)
        self.setUp();_,scope=self.source();original=self.broker.request
        def request(command):
            if command.startswith('print-frame '):raise TimeoutError('no authenticated reply')
            return original(command)
        self.broker.request=request;reply=self.send('variables',variablesReference=scope)
        self.assertFalse(reply['success']);self.assertFalse(self.adapter.source_pointer_views)
    def test_native_transition_never_reads_a_source_pointer(self):
        _,scope=self.source();ref=self.roots(scope)[0]['variablesReference'];before=len(self.prints())
        self.broker.native=True
        self.assertFalse(self.send('variables',variablesReference=ref)['success'])
        self.assertEqual(before,len(self.prints()))
    def test_lazy_and_scalar_copy_budgets_are_checked_before_publication(self):
        for metric,limit in (('source_object_nodes',8192),('source_object_bytes',512*1024)):
            self.setUp();_,scope=self.source();setattr(self.adapter,metric,limit);before=self.adapter.next_reference
            reply=self.send('variables',variablesReference=scope)
            self.assertFalse(reply['success']);self.assertEqual(before,self.adapter.next_reference)
            self.assertFalse(self.adapter.source_pointer_views)
            self.setUp();_,scope=self.source();ref=self.roots(scope)[0]['variablesReference']
            self.broker.target_body='object: type=int32, offset=0, bytes=4, value=3';setattr(self.adapter,metric,limit)
            self.assertFalse(self.send('variables',variablesReference=ref)['success'])
            self.assertFalse(self.adapter.source_pointer_views)


if __name__=='__main__':unittest.main()
