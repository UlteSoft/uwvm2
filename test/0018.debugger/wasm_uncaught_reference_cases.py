"""Cross-module reference payload oracles, independent of debugger output."""


def reference_uncaught_examples():
    payload = '(ref any) (ref eq) (ref i31) (ref null extern) (ref null extern) (ref func)'
    types = '''(rec (type $s (struct (field (mut i32)) (field (mut (ref null $a)))))
                    (type $a (array (mut i16))))
      (type $void (func))'''
    tag_type = f'(type $tag (func (param {payload})))'
    tag = f'(module {tag_type} (tag $e (export "e") (type $tag)))'
    alias_tag = f'(module {tag_type} (import "P" "e" (tag $e (type $tag))) (export "e" (tag $e)))'
    seed = '(global $p (mut i64) (i64.const -992)) (global $z (mut f32) (f32.const -0))'
    prefix = 'global.get $p global.get $z '
    create = '''i32.const 0 i32.const -1 i32.const 4660 array.new_fixed $a 3 local.set $a
      i32.const 31 local.get $a struct.new $s local.set $s'''
    arguments = '''local.get $s ref.as_non_null local.get $a ref.as_non_null i32.const -123 ref.i31 ref.null extern
      local.get $s extern.convert_any ref.func $target'''
    local_body = f'(local $s (ref null $s)) (local $a (ref null $a)) {create} {prefix} {arguments} nop throw $e'
    provider = f'''(module {types} {tag_type} (tag $e (export "e") (type $tag))
      (global $p (mut i64) (i64.const -991)) (global $z (mut f32) (f32.const -0))
      (func $f (export "f") (type $void) {local_body})
      (func $target (type $void)) (elem declare func $target))'''
    alias_call = f'''(module (type $void (func)) {tag_type}
      (import "P" "e" (tag $e (type $tag))) (import "P" "f" (func $f (type $void)))
      (export "e" (tag $e)) (export "f" (func $f)))'''

    def ref(heap, kind=None, symbol=None, role=None, index=None, nullable=False, function=None):
        return dict(heap=heap, kind=kind, symbol=symbol, role=role, index=index,
                    nullable=nullable, function=function)
    def s(role, nullable=True, symbol='S'):
        return ref('type', 'struct', symbol, role, 0, nullable)
    def a(role, nullable=True):
        return ref('type', 'array', 'A', role, 1, nullable)
    def fn(role, abstract=False):
        return ref('func' if abstract else 'type', 'function', role=role,
                   index=None if abstract else 2, function=1)
    null = ref('extern', 'null', nullable=True)
    i31 = ref('i31', 'i31')
    wrapper = ref('extern', 'extern-wrapper', 'X', nullable=True)

    def gc_objects(role):
        return dict(S=dict(kind='struct', role=role, type=0, values=['i32 = 31 mutable', dict(**a(role), suffix=' mutable')]),
                    A=dict(kind='array', role=role, type=1, values=[f'i32 = packed-u16={n} mutable' for n in (0, 65535, 4660)]),
                    X=dict(kind='extern-wrapper', values=[dict(**ref('any', 'struct', 'S', nullable=True), suffix=' immutable')]))
    def gc_pages(selection, struct_root, array_root, wrapper_root=None, prefix_path=()):
        pages = [dict(selection=selection, root=struct_root, path=list(prefix_path), symbol='S'),
                 dict(selection=selection, root=struct_root, path=[*prefix_path, 1], symbol='A'),
                 dict(selection=selection, root=array_root, path=list(prefix_path), symbol='A')]
        if wrapper_root is not None:
            pages += [dict(selection=selection, root=wrapper_root, path=list(prefix_path), symbol='X'),
                      dict(selection=selection, root=wrapper_root, path=[*prefix_path, 0], symbol='S'),
                      dict(selection=selection, root=wrapper_root, path=[*prefix_path, 0, 1], symbol='A')]
        return pages
    def leaf(role, function=0):
        return dict(role=role, function=function,
                    values=[f'i64 = {-991 if role == "P" else -992}', 'f32 = bits=0x80000000',
                            s(role, False), a(role, False), i31, null, wrapper, fn(role)],
                    locals=[s(role), a(role)], objects=gc_objects(role),
                    members=gc_pages('operands', 2, 3, 6) + gc_pages('locals', 0, 1))
    def caller(function):
        cs = s('C', symbol='CS')
        return dict(role='C', function=function,
                    values=['i64 = -992', 'f32 = bits=0x80000000', cs], locals=[cs],
                    objects=dict(CS=dict(kind='struct', role='C', type=0,
                        values=['i32 = -93 mutable', dict(**ref('type', 'null', role='C', index=1, nullable=True), suffix=' mutable')])),
                    members=[dict(selection=selection, root=root, path=[], symbol='CS')
                             for selection, root in (('operands', 2), ('locals', 0))])
    cases = []
    def add(name, providers, consumer, frame, handled=False, caller_frame=None):
        cases.append(dict(name=name, providers=providers, consumer=consumer,
            leaf_role=frame['role'], leaf_function=frame['function'], handled=handled,
            frames=[frame] + ([] if caller_frame is None else [caller_frame]), tag_owner='P'))

    imports = f'(import "P" "e" (tag $e (type $tag)))'
    add('refs-import-tag-unhandled', [('P', tag)],
        f'(module {types} {tag_type} {imports} {seed} (func (export "_start") (type $void) {local_body}) (func $target (type $void)) (elem declare func $target))', leaf('C'))
    mismatch_body = f'''(local $s (ref null $s)) (local $a (ref null $a)) {create} {prefix}
      block $caught (result {payload} (ref exn))
        try_table (catch_ref $other $caught) {arguments} nop throw $e end unreachable
      end {'drop ' * 9}'''
    add('refs-alias-distinct-tag-unhandled', [('P', tag), ('Q', tag), ('A', alias_tag)],
        f'''(module {types} {tag_type} (import "A" "e" (tag $e (type $tag)))
          (import "Q" "e" (tag $other (type $tag))) {seed}
          (func (export "_start") (type $void) {mismatch_body})
          (func $target (type $void)) (elem declare func $target))''', leaf('C'))

    def consumer(source, handled=False, tail=False):
        declarations = f'(import "{source}" "e" (tag $e (type $tag))) (import "{source}" "f" (func $f (type $void)))'
        middle = '(func $middle (type $void) return_call $f)' if tail else ''
        body = f'(local $s (ref null $s)) i32.const -93 ref.null $a struct.new $s local.set $s {prefix} local.get $s '
        invocation = 'call $middle' if tail else 'call $f'
        if handled:
            body += f'block $caught (result {payload} (ref exn)) try_table (catch_ref $e $caught) {invocation} end unreachable end ' + 'drop ' * 7
        else:
            body += invocation
        body += ' drop drop drop'
        return f'(module {types} {tag_type} {declarations} {seed} (func $target (type $void)) {middle} (func (export "_start") (type $void) {body}))'
    add('refs-cross-call-unhandled', [('P', provider)], consumer('P'), leaf('P'), caller_frame=caller(2))
    add('refs-cross-tail-alias-unhandled', [('P', provider), ('A', alias_call)], consumer('A', tail=True), leaf('P'), caller_frame=caller(3))
    add('refs-handled-alias-call-tag', [('P', provider), ('A', alias_call)], consumer('A', handled=True), leaf('P'), True, caller(2))

    for handled in (False, True):
        objects = gc_objects('P')
        objects.update(caller(2)['objects'])
        objects['E'] = dict(kind='exception', role='P', tag=0,
            values=[dict(**ref('any', 'struct', 'S'), suffix=' immutable'),
                    dict(**ref('eq', 'array', 'A'), suffix=' immutable'),
                    dict(**i31, suffix=' immutable'), dict(**null, suffix=' immutable'),
                    dict(**wrapper, suffix=' immutable'), dict(**fn('P', True), suffix=' immutable')])
        exn = ref('exn', 'exception', 'E')
        frame = dict(role='C', function=2, values=caller(2)['values'] + [exn],
                     locals=[],
                     objects=objects, members=caller(2)['members'])
        # The local declaration is nullable even though the stack's catch result is non-null.
        frame['locals'] = caller(2)['locals'] + [ref('exn', 'exception', 'E', nullable=True)]
        for selection, root in (('operands', 3), ('locals', 1)):
            frame['members'] += [dict(selection=selection, root=root, path=[], symbol='E'),
                                 dict(selection=selection, root=root, path=[0], symbol='S'),
                                 dict(selection=selection, root=root, path=[0, 1], symbol='A'),
                                 dict(selection=selection, root=root, path=[1], symbol='A'),
                                 dict(selection=selection, root=root, path=[4], symbol='X'),
                                 dict(selection=selection, root=root, path=[4, 0], symbol='S')]
        throw = '''block $caught (result (ref exn))
          try_table (catch_all_ref $caught) call $f end unreachable
          end local.tee $exn ref.as_non_null nop throw_ref'''
        if handled:
            throw = f'block $done (result {payload} (ref exn)) try_table (catch_ref $e $done) {throw} end unreachable end ' + 'drop ' * 7
        body = f'''(local $s (ref null $s)) (local $exn (ref null exn))
          i32.const -93 ref.null $a struct.new $s local.set $s {prefix} local.get $s {throw} drop drop drop'''
        add('refs-' + ('handled-cross-rethrow' if handled else 'cross-rethrow-unhandled'),
            [('P', provider), ('A', alias_call)],
            f'''(module {types} {tag_type} (import "A" "e" (tag $e (type $tag)))
              (import "P" "f" (func $f (type $void))) {seed}
              (func $target (type $void)) (func (export "_start") (type $void) {body}))''', frame, handled)

    matched_body = mismatch_body.replace('catch_ref $other', 'catch_ref $same')
    add('refs-handled-import-alias', [('P', tag), ('A', alias_tag)],
        f'''(module {types} {tag_type} (import "A" "e" (tag $e (type $tag)))
          (import "P" "e" (tag $same (type $tag))) {seed}
          (func (export "_start") (type $void) {matched_body})
          (func $target (type $void)) (elem declare func $target))''', leaf('C'), True)
    return cases
