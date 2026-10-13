"""Independent live-GC quota fixtures; no debugger-derived expected values."""


def quota_examples():
    node = '(rec (type $node (struct (field (mut (ref null $node))) (field (mut i32)))))'
    seed = '(global $prefix (mut i64) (i64.const -991))'
    def module(types, body, local_type, extra=''):
        return f'''(module {types} {seed} {extra}
          (func (export "_start") (local $root {local_type}) (local $i i32)
            {body} global.get $prefix nop i32.const 777 local.set $i nop drop))'''
    cases = []
    array = '''i32.const 0 i32.const 513 array.new $array local.set $root
      loop $fill local.get $root local.get $i local.get $i i32.const 1000 i32.add array.set $array
        local.get $i i32.const 1 i32.add local.tee $i i32.const 513 i32.lt_u br_if $fill end'''
    cases.append(dict(name='array-513', kind='array', size=513,
        wat=module('(type $array (array (mut i32)))', array, '(ref null $array)')))
    for count in (128, 129):
        body = f'''loop $build local.get $root local.get $i i32.const 1000 i32.add struct.new $node local.set $root
          local.get $i i32.const 1 i32.add local.tee $i i32.const {count} i32.lt_u br_if $build end'''
        cases.append(dict(name=f'chain-{count}', kind='chain', size=count,
            wat=module(node, body, '(ref null $node)')))
    cycle = '''ref.null $node i32.const 31 struct.new $node local.set $root
      local.get $root local.get $root struct.set $node 0'''
    cases.append(dict(name='cycle-path-quotas', kind='cycle', size=1,
        wat=module(node, cycle, '(ref null $node)', '(global $sink (mut (ref null $node)) (ref.null $node))')))
    # A typed table fills the graph in a short loop. Its longer element labels
    # reach the text cap without a large unrolled JIT body or thousands of locals.
    declarations = '(type (func)) '*1000 + '(rec (type $node (struct ' + '(field (mut (ref null $node))) '*4 + ')))'
    body = '''loop $build ref.null $node ref.null $node ref.null $node ref.null $node struct.new $node local.set $child
      local.get $child local.get $child local.get $child local.get $child struct.new $node local.set $root
      local.get $child local.get $root struct.set $node 0
      local.get $child local.get $child struct.set $node 1
      local.get $child local.get $child struct.set $node 2
      local.get $child local.get $child struct.set $node 3
      local.get $i local.get $root table.set $roots
      local.get $i i32.const 1 i32.add local.tee $i i32.const 64 i32.lt_u br_if $build end'''
    cases.append(dict(name='reply-32k', kind='reply', size=128, marker=2, initial_marker=64,
        wat=f'''(module {declarations} {seed} (table $roots 64 (ref null $node))
          (func (export "_start") (local $root (ref null $node)) (local $child (ref null $node)) (local $i i32)
            {body} global.get $prefix nop i32.const 777 local.set $i nop drop))'''))
    return cases
