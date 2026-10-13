#!/usr/bin/env python3
"""Bounded DAP/C++ parser and transport DATA; real TinyGo stops use the CLI runner."""
from pathlib import Path
import importlib.util,io,json,os,subprocess,unittest
SPEC=importlib.util.spec_from_file_location('go_dap',Path(__file__).resolve().parents[2]/'tools/debug/dap_adapter.py')
dap=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(dap)
SELECTORS=['len(s)','cap(s)','len(((box.Numbers)))','cap((*p).Numbers)','len(p->Text)','len(arr[0].Text)','len(s[0])','length','capability','ns::len','len(ns::s)','len(s'+'.x'*31+')','len ','cap ','len .x','cap .x']
SCALARS=['len(s)+cap(s)','len(s)-1','(cap(s))-1','len(s)*2','2*cap(s)','len(s)>>1','sizeof(len(s))','int(cap(s))','len(s)?cap(s):0','len(s) as i32','-len(s)','len(s) + (cap(s) - 1)']
BAD=['len()','len(s,t)','len(s+1)','len(call())','cap(len(s))','len(cap(s))','len(s=1)','len(s)[0]','(len(s)).x','len(s)->x','*len(s)','*(len(s))','len(s);quit','len(s)\nquit','len(s'+'.x'*32+')','len('+'('*33+'s'+')'*33+')','len(s)++','len(s)--','len(s) value','len(s) + call()']
class Broker:
    def __init__(self,retire=0):self.commands=[];self.statuses=0;self.retire=retire
    def request(self,text):
        self.commands.append(text)
        if text=='status':
            self.statuses+=1;sid=42 if self.retire and self.statuses>=self.retire else 41
            return f'stopped: selected participant step\nstop-id {sid}\nthread 7 module=3 function=1 byte-offset=4 generation=8\n'
        if text.startswith('print-frame 7 41 0 '):return 'source-stop 41\ni32=3\n'
        raise AssertionError(text)
def response(output):
    data=io.BytesIO(output.getvalue());line=data.readline();assert data.readline()==b'\r\n'
    return json.loads(data.read(int(line[16:-2])))
def setup(retire=0):
    out=io.BytesIO();adapter=dap.Adapter(out);broker=Broker();adapter.broker=broker
    adapter.observe(broker.request('status'),notify=False);broker.commands.clear();broker.statuses=0;broker.retire=retire
    adapter.source_frame_ordinals[99]=(adapter.stop_key,dict(current=True,thread=7,stop_id=41,ordinal=0))
    return adapter,broker,out
class Tests(unittest.TestCase):
    def test_production_cpp_parser_matches_bounded_adapter(self):
        binary=os.environ.get('UWVM_GO_DAP_BRIDGE');self.assertTrue(binary,'guarded compiled parser bridge required')
        rows=SELECTORS+SCALARS+BAD
        parsed=subprocess.run([binary,*rows],stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True,timeout=30)
        lines=parsed.stdout.decode().splitlines();self.assertEqual(len(lines),len(rows))
        for i,(text,line) in enumerate(zip(rows,lines)):
            with self.subTest(expression=text):
                index,scalar,selector=map(int,line.split('\t'));self.assertEqual(index,i)
                if text in SELECTORS:
                    self.assertEqual((scalar,selector),(0,0));self.assertEqual(dap.validate_source_expression(text),text.replace(' ',''));dap.validate_source_evaluation_expression(text)
                elif text in SCALARS:
                    self.assertEqual(scalar,0);self.assertNotEqual(selector,0);dap.validate_source_evaluation_expression(text)
                    with self.assertRaises(ValueError):dap.validate_source_expression(text)
                else:
                    self.assertNotEqual(scalar,0);self.assertNotEqual(selector,0)
                    with self.assertRaises(ValueError):dap.validate_source_evaluation_expression(text)
    def test_watch_and_hover_route_only_coherent_source_queries(self):
        for context in ['watch','hover','variables']:
            for text in ['len(box.Numbers)','len(box.Numbers)-1']:
                adapter,broker,out=setup();adapter.handle(dict(type='request',seq=1,command='evaluate',arguments=dict(context=context,frameId=99,expression=text)))
                self.assertTrue(response(out)['success']);self.assertEqual(broker.commands,['status',f'print-frame 7 41 0 {text}','status'])
    def test_invalid_syntax_reaches_no_broker(self):
        for text in BAD:
            adapter,broker,out=setup();adapter.handle(dict(type='request',seq=1,command='evaluate',arguments=dict(context='watch',frameId=99,expression=text)))
            self.assertFalse(response(out)['success']);self.assertEqual(broker.commands,[])
    def test_retired_frames_before_and_after_copy_publish_no_value(self):
        for retire,commands in [(1,['status']),(2,['status','print-frame 7 41 0 len(s)','status'])]:
            adapter,broker,out=setup(retire);adapter.handle(dict(type='request',seq=1,command='evaluate',arguments=dict(context='hover',frameId=99,expression='len(s)')))
            self.assertFalse(response(out)['success']);self.assertEqual(broker.commands,commands);self.assertEqual(adapter.source_frame_ordinals,{})
if __name__=='__main__':unittest.main()
