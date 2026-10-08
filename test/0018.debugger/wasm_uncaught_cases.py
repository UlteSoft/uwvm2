"""Independent live inputs for actual unhandled throws and handled controls."""

def uncaught_examples():
    cases=[]
    seed='(global $p (mut i64) (i64.const -991)) (global $z (mut f32) (f32.const -0)) '
    prefix=['i64 = -991','f32 = bits=0x80000000']
    def add(name, wat, values, function=0, caller=None, total=1, handled=False, members=()):
        point=dict(function=function,ordinal=0,values=values,caller=caller,frame_total=total,
                   locals_page=None,tables=[],members=list(members))
        cases.append(dict(name=name,wat=wat,expected=[point],handled=handled))
    args='i32.const -17 i64.const -19 f32.const nan:0x412345 f64.const -0 v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 '
    numeric=['i32 = -17','i64 = -19','f32 = bits=0x7fc12345','f64 = bits=0x8000000000000000','v128 = bytes=000102030405060708090a0b0c0d0e0f']
    add('uncaught-numeric', '(module '+seed+'(tag $e (param i32 i64 f32 f64 v128)) (func (export "_start") global.get $p global.get $z '+args+'nop throw $e))',prefix+numeric)
    add('uncaught-gc-tuple','(module '+seed+'(type $s (struct (field (mut i32)))) (tag $e (param (ref $s))) (func (export "_start") global.get $p global.get $z i32.const 31 struct.new $s nop throw $e))',
        prefix+[r'\(ref type-index=0 module=0\) = struct #1'],members=[dict(selection='operands',root=2,table=0,path=[],kind='struct',type_index=0,values=['i32 = 31 mutable'])])
    add('uncaught-recursive-72','(module '+seed+'(tag $e (param i32)) (func $r (param i32) global.get $p local.get 0 i64.extend_i32_s i64.add global.get $z local.get 0 if local.get 0 i32.const 1 i32.sub call $r else i32.const -17 nop throw $e end drop drop) (func (export "_start") i32.const 70 call $r))',
        prefix+['i32 = -17'],caller=['i64 = -990','f32 = bits=0x80000000'],total=72)
    add('uncaught-retired-tail','(module '+seed+'(tag $e (param i32)) (func $leaf global.get $p global.get $z i32.const -17 nop throw $e) (func $middle return_call $leaf) (func (export "_start") global.get $p global.get $z call $middle drop drop))',
        prefix+['i32 = -17'],caller=prefix,total=2)
    add('uncaught-distinct-tag-caller','(module '+seed+'(tag $e (param i32)) (tag $other (param i32)) (func $leaf global.get $p global.get $z i32.const -17 nop throw $e) (func (export "_start") global.get $p global.get $z block $caught (result i32 (ref exn)) try_table (catch_ref $other $caught) call $leaf end i32.const 0 ref.null exn ref.as_non_null end drop drop drop drop))',
        prefix+['i32 = -17'],caller=prefix,total=2)
    add('uncaught-throw-ref','(module '+seed+'(tag $e (param i32)) (func $leaf i32.const -17 throw $e) (func (export "_start") global.get $p global.get $z block $caught (result (ref exn)) try_table (catch_all_ref $caught) call $leaf end unreachable end nop throw_ref))',
        prefix+[r'\(ref exn\) = exception #1'],function=1)
    for clause in ('catch_ref $e','catch_all_ref'):
        has_payload=clause.startswith('catch_ref')
        result='i32 (ref exn)' if has_payload else '(ref exn)'
        drops='drop drop' if has_payload else 'drop'
        add('handled-leaf-'+('tag' if has_payload else 'all-ref'), '(module '+seed+'(tag $e (param i32)) (func (export "_start") global.get $p global.get $z block $caught (result '+result+') try_table ('+clause+' $caught) i32.const -17 nop throw $e end unreachable end '+drops+' drop drop))',
            prefix+['i32 = -17'],handled=True)
        add('handled-caller-'+('tag' if has_payload else 'all-ref'), '(module '+seed+'(tag $e (param i32)) (func $leaf global.get $p global.get $z i32.const -17 nop throw $e) (func (export "_start") global.get $p global.get $z block $caught (result '+result+') try_table ('+clause+' $caught) call $leaf end unreachable end '+drops+' drop drop))',
            prefix+['i32 = -17'],caller=prefix,total=2,handled=True)
    return cases
