#!/usr/bin/env python3
"""Real CLI co-signing, protected distinct-key quorums and actual VM gates.

Run only through run_bounded_v2.py in the authorized SSH Linux cgroup. All PKI
is disposable. Existing fixture keys, policies and protected state are preserved.
"""
import argparse
import datetime as dt
import hashlib
import http.server
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import ssl
import threading
import time


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ['fixtures', 'go-cli', 'vm', 'runner', 'output']:
        ap.add_argument('--' + name, type=Path, required=True)
    ap.add_argument('--go-prefix-json', default='[]')
    ap.add_argument('--go-subcommands-json', default='["wasm","secure"]')
    ap.add_argument('--exec-prefix-json', required=True)
    ap.add_argument('--matrix-driver', type=Path, default=Path(__file__).with_name('algorithm_matrix.py'))
    args = ap.parse_args()
    if os.uname().sysname != 'Linux':
        raise RuntimeError('authorized SSH Linux only')
    spec = importlib.util.spec_from_file_location('multisig_matrix', args.matrix_driver)
    matrix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(matrix)
    before = matrix.cgroup_snapshot(True)
    os.umask(0o077)
    out = args.output.resolve()
    out.mkdir(mode=0o700, parents=True, exist_ok=False)
    fixtures = json.loads((args.fixtures / 'fixtures.json').read_text())
    witness = fixtures['guest_algorithms']['ES256']
    core, unused, unused2, unused3 = matrix.embedded_parts(Path(witness['module']).read_bytes())
    marker = witness['guest_marker']
    base = witness['pki']
    go = [*json.loads(args.go_prefix_json), str(args.go_cli.resolve()), *json.loads(args.go_subcommands_json)]
    prefix = json.loads(args.exec_prefix_json)
    if not prefix:
        raise RuntimeError('an explicit actual QEMU/loader prefix is required')
    vm = [*prefix, str(args.vm.resolve())]
    runner = [*prefix, str(args.runner.resolve())]
    report = {'status': 'IN_PROGRESS', 'scope': __doc__, 'records': [],
              'source_sha256': matrix.sha(__file__), 'fixtures_sha256': matrix.sha(args.fixtures / 'fixtures.json'),
              'go_binary_sha256': matrix.sha(args.go_cli), 'vm_binary_sha256': matrix.sha(args.vm),
              'runner_sha256': matrix.sha(args.runner), 'go_exec_prefix': json.loads(args.go_prefix_json),
              'exec_prefix': prefix, 'cgroup_before': before}
    producers = matrix.Fixtures(out / 'producers', openssl='/usr/bin/openssl')
    now = int(time.time())
    stamp = lambda n: dt.datetime.fromtimestamp(n, dt.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')

    def check(name, passed, **details):
        report['records'].append(dict(name=name, passed=bool(passed), **details))
        if len(report['records']) % 25 == 0:
            temporary = out / 'report.json.tmp'
            temporary.write_text(json.dumps(report, indent=2) + '\n')
            temporary.replace(out / 'report.json')
            print(json.dumps({'completed_assertions': len(report['records']), 'last': name}), flush=True)
        if not passed:
            raise RuntimeError('failed assertion: ' + name)

    def command(name, argv, *, exits=(0,), codes=(), contains=(), guest=False, env=None):
        argv = list(map(str, argv))
        # Record the exact public module/policy bytes at each invocation, even
        # when the scenario deliberately changes an output later. Never hash or
        # archive private signing keys as test evidence.
        inputs = {value: matrix.sha(value) for value in argv if
                  (value.endswith('.wasm') or Path(value).name == 'policy') and Path(value).is_file()}
        start = time.perf_counter_ns()
        result = subprocess.run(argv, capture_output=True, text=True, timeout=90, env=env)
        text = result.stdout + result.stderr
        passed = result.returncode in exits and all(s in text for s in contains)
        if codes:
            passed = passed and any(s in text for s in codes)
        if guest:
            passed = passed and (marker in result.stdout if 0 in exits else matrix.GUEST_PREFIX not in text)
        check(name, passed, argv=argv, expected_exit_codes=list(exits), returncode=result.returncode,
              stdout=result.stdout, stderr=result.stderr, input_sha256=inputs, elapsed_ns=time.perf_counter_ns()-start)
        return result

    def policy(directory, signers, algorithms, threshold, *, cli=False):
        directory.mkdir(mode=0o700)
        roots = directory / 'roots.pem'
        roots.write_bytes(b''.join(dict.fromkeys(Path(s['root']).read_bytes() for s in signers)))
        path, state = directory / 'policy', directory / 'state'
        if cli:
            argv = [*go, 'policy-init', '--output', path, '--trust-root', roots, '--state-dir', state,
                    '--certificate', signers[0]['certificate'], '--module-id', 'matrix.app',
                    '--algorithms', ','.join(algorithms), '--signature-threshold', str(threshold), '--capabilities', '3']
            for signer in signers[1:]:
                argv += ['--additional-certificate', signer['certificate']]
            command('administrator CLI provisions distinct signer set ' + directory.name, argv)
        else:
            state.mkdir(mode=0o700)
            (state / 'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
            text = ('UWVM-WSEA-POLICY-1\npolicy_version=1\ntrust_root=' + str(roots) + '\nstate_dir=' + str(state) +
                    '\nmax_crl_age_seconds=86400\nmax_module_bytes=104857600\nmodule=matrix.app|uwvm2|' +
                    signers[0]['pin'] + '|1|0|3|' + ','.join(algorithms) + '\n')
            text += ''.join('module_signer=matrix.app|' + s['pin'] + '\n' for s in signers[1:])
            text += 'module_threshold=matrix.app|' + str(threshold) + '\n'
            path.write_text(text)
        return path

    def sign(name, image, path, ca, algorithm, *, append=False, partial=False, version=1, exits=(0,), codes=()):
        argv = [*go, 'sign', image, '--policy', path, '--key', ca['key'], '--certificate', ca['certificate'],
                '--crl', ca['crl'], '--algorithm', algorithm, '--module-id', 'matrix.app',
                '--module-version', str(version), '--security-epoch', '0', '--capabilities', '3',
                '--not-before', stamp(now-60), '--not-after', stamp(now+7200)]
        if append:
            argv.append('--append-signature')
        if partial:
            argv.append('--allow-partial')
        return command(name, argv, exits=exits, codes=codes)

    def both(name, image, path, *, allow=True, codes=()):
        command(name + ' Go', [*go, 'verify', image, '--policy', path], exits=(0,) if allow else (1,),
                codes=codes, contains=('VERIFIED',) if allow else ())
        command(name + ' production verifier', [*runner, path, image, 'matrix.app'], exits=(0,) if allow else (2,), codes=codes)
        command(name + ' actual guest gate', [*vm, '--secure', 'module', image, path, '--run', image],
                exits=(0,) if allow else (126, 127), codes=codes, guest=True)

    # This small test-only reader mutates already-produced envelopes. Product
    # verification is always performed by Go/OpenSSL and the actual VM above.
    def unpack(image):
        data = Path(image).read_bytes()
        assert data.startswith(core) and data[len(core)] == 0
        def number(p):
            value = shift = 0
            while True:
                byte = data[p]; p += 1; value |= (byte & 127) << shift
                if not byte & 128:
                    return value, p
                shift += 7
        size, pos = number(len(core)+1)
        end = pos + size
        size, pos = number(pos)
        assert data[pos:pos+size] == matrix.SECTION_NAME and end == len(data)
        pos += size
        def item():
            nonlocal pos
            first = data[pos]; pos += 1
            major, value = first >> 5, first & 31
            if value >= 24:
                width = {24: 1, 25: 2, 26: 4, 27: 8}[value]
                value = int.from_bytes(data[pos:pos+width], 'big'); pos += width
            if major == 0:
                return value
            if major == 2:
                result = data[pos:pos+value]; pos += value; return result
            if major == 4:
                return [item() for unused in range(value)]
            raise ValueError('unexpected producer envelope')
        envelope = item()
        assert pos == end
        return [envelope[1:]] if envelope[0] == 1 else envelope[1]

    def pack(path, entries, *, raw=None):
        payload = raw if raw is not None else matrix.cbor([1, *entries[0]]) if len(entries) == 1 else matrix.cbor([2, entries])
        section = matrix.uleb(len(matrix.SECTION_NAME)) + matrix.SECTION_NAME + payload
        path.write_bytes(core + b'\0' + matrix.uleb(len(section)) + section)
        return path

    try:
        auxiliaries = [producers.pki('aux-' + str(i)) for i in range(3)]
        # Every protected algorithm appears as a genuine contribution alongside
        # another distinct signer, including both signatures sharing one curve.
        saved = None
        scenarios = [(name, name, auxiliaries[0] if name in ('ES256', 'ESP256') else fixtures['guest_algorithms'][name]['pki'])
                     for name in matrix.ALGORITHMS]
        boundaries = [case for case in fixtures['cases'] if case['expected'] == 'ALLOW' and 'genuine ' in case['name']]
        if len(boundaries) != 4:
            raise RuntimeError('four genuine RSA4096/8192 signing boundaries required')
        scenarios += [('RSA' + str(case['pki']['bits']) + '-' + case['algorithm'], case['algorithm'], case['pki']) for case in boundaries]
        for label, algorithm, ca in scenarios:
            algorithms = list(dict.fromkeys(['ES256', algorithm]))
            path = policy(out / ('pair-' + label), [base, ca], algorithms, 2, cli=True)
            image = path.parent / 'signed.wasm'; image.write_bytes(core)
            result = sign('first authenticated partial ' + label, image, path, base, 'ES256', partial=True)
            check('partial status never says VERIFIED ' + label, json.loads(result.stdout)['status'] == 'SIGNED_PARTIAL')
            partial = path.parent / 'partial.wasm'; partial.write_bytes(image.read_bytes())
            both('incomplete quorum ' + label, partial, path, allow=False, codes=('POL_002',))
            sign('append distinct publisher ' + label, image, path, ca, algorithm, append=True)
            both('complete mixed signatures ' + label, image, path)
            entries = unpack(image)
            reversed_image = pack(path.parent / 'reversed.wasm', list(reversed(entries)))
            both('reordered authenticated signatures ' + label, reversed_image, path)
            for index in range(2):
                bad = [list(entry) for entry in entries]
                bad[index][0] = bad[index][0][:-1] + bytes([bad[index][0][-1] ^ 1])
                forged = pack(path.parent / ('forged-' + str(index) + '.wasm'), bad)
                both('valid plus forged signature ' + label + '/' + str(index), forged, path, allow=False, codes=('SIG_003',))
            snapshot = image.read_bytes()
            sign('different manifest cannot be appended ' + label, image, path, ca, algorithm,
                 append=True, version=2, exits=(1,), codes=('SIG_002',))
            check('refused append preserves exact original ' + label, image.read_bytes() == snapshot)
            if label == 'Ed25519':
                saved = path, image, partial, entries
        path, image, partial, entries = saved
        unlisted = policy(out / 'unlisted-extra', [base, fixtures['guest_algorithms']['Ed25519']['pki']], ['ES256', 'Ed25519'], 1)
        line = 'module_signer=matrix.app|' + fixtures['guest_algorithms']['Ed25519']['pki']['pin'] + '\n'
        unlisted.write_text(unlisted.read_text().replace(line, ''))
        both('a trusted CA does not authorize an unlisted extra publisher', image, unlisted, allow=False, codes=('CERT_006',))
        one_of_two = policy(out / 'one-of-two', [base, fixtures['guest_algorithms']['Ed25519']['pki']], ['ES256', 'Ed25519'], 1)
        both('explicit one-of-two policy allows a single authorized contribution', partial, one_of_two)
        for name, raw in [('duplicate', matrix.cbor([2, [entries[0], entries[0]]])),
                          ('zero', matrix.cbor([2, []])), ('one-in-multi', matrix.cbor([2, [entries[0]]])),
                          ('nine', matrix.cbor([2, entries*4 + entries[:1]])),
                          ('unknown-version', matrix.cbor([3, entries])),
                          ('trailing', matrix.cbor([2, entries]) + b'\0')]:
            invalid = pack(path.parent / (name + '.wasm'), [], raw=raw)
            both('malformed multi envelope ' + name, invalid, path, allow=False, codes=('SIG_002',))
        # Same SPKI with another allowed algorithm is valid multi-signing but
        # cannot substitute for an independently authorized second public key.
        same = policy(out / 'same-key', [base, auxiliaries[0]], ['ES256', 'ESP256'], 2)
        same_image = same.parent / 'same.wasm'; same_image.write_bytes(core)
        sign('same-key first partial', same_image, same, base, 'ES256', partial=True)
        sign('same-key second algorithm remains partial', same_image, same, base, 'ESP256', append=True, partial=True)
        both('one public key is never two votes', same_image, same, allow=False, codes=('POL_002',))
        # Exercise all eight distinct public keys and stripping any one vote.
        keys = [fixtures['guest_algorithms'][name]['pki'] for name in ['ES256', 'ES384', 'ES512', 'Ed25519', 'PS256']] + auxiliaries
        check('eight-vote fixture has eight distinct SPKIs', len({key['pin'] for key in keys}) == 8)
        names = ['ES256', 'ES384', 'ES512', 'Ed25519', 'PS256', 'ES256', 'ES256', 'ES256']
        maximum = policy(out / 'eight-of-eight', keys, list(dict.fromkeys(names)), 8, cli=True)
        maximum_image = maximum.parent / 'eight.wasm'; maximum_image.write_bytes(core)
        for index, (ca, algorithm) in enumerate(zip(keys, names)):
            sign('build eight-key quorum contribution ' + str(index), maximum_image, maximum, ca, algorithm,
                 append=index != 0, partial=index != 7)
        both('eight distinct authorized keys', maximum_image, maximum)
        maximum_entries = unpack(maximum_image)
        for missing in range(8):
            stripped = pack(maximum.parent / ('missing-' + str(missing) + '.wasm'), maximum_entries[:missing] + maximum_entries[missing+1:])
            both('stripped required signer ' + str(missing), stripped, maximum, allow=False, codes=('POL_002',))
        # Authenticated revocation of ANY present contribution fails the whole
        # bundle, including a policy that needs only one key. The actual VM
        # learns the permanent fact before the standalone verifier is invoked.
        retired = producers.pki('retired-extra')
        rev_policy = policy(out / 'revocation', [base, retired], ['ES256'], 2)
        rev_image = rev_policy.parent / 'good.wasm'; rev_image.write_bytes(core)
        sign('revocation fixture first partial', rev_image, rev_policy, base, 'ES256', partial=True)
        sign('revocation fixture complete', rev_image, rev_policy, retired, 'ES256', append=True)
        both('revocation fixture initially authorized', rev_image, rev_policy)
        rev_entries = unpack(rev_image)
        config = Path(retired['directory']) / 'ca.cnf'
        revoked_crl = rev_policy.parent / 'revoked.crl.pem'
        producers.openssl('ca', '-batch', '-config', config, '-revoke', retired['certificate'])
        producers.openssl('ca', '-batch', '-config', config, '-gencrl', '-out', revoked_crl)
        revoked_entries = [list(entry) for entry in rev_entries]
        revoked_entries[1][2] = revoked_crl.read_bytes()
        revoked_image = pack(rev_policy.parent / 'revoked.wasm', revoked_entries)
        command('Go rejects genuine revoked additional publisher', [*go, 'verify', revoked_image, '--policy', rev_policy],
                exits=(1,), codes=('CERT_004',))
        command('actual VM authenticates and remembers additional publisher revocation',
                [*vm, '--secure', 'module', revoked_image, rev_policy, '--run', revoked_image],
                exits=(126, 127), codes=('CERT_004',), guest=True)
        tombstone = rev_policy.parent / 'state' / ('revoked-' + retired['pin'])
        check('actual VM retained additional publisher tombstone', tombstone.is_file())
        tombstone_bytes = tombstone.read_bytes()
        both('old fresh CRL cannot undo a multi-signer tombstone', rev_image, rev_policy, allow=False, codes=('CERT_004',))
        state = rev_policy.parent / 'state/state'
        old_state = state.read_bytes()
        command('administrator explicitly changes quorum without resetting state',
                [*go, 'policy-update', '--policy', rev_policy, '--policy-version', '2', '--module-id', 'matrix.app', '--signature-threshold', '1'])
        check('administrator update preserved protected state', state.read_bytes() == old_state)
        both('a valid signature cannot mask another revoked signature', revoked_image, rev_policy, allow=False, codes=('CERT_004',))
        live = rev_policy.parent / 'live.wasm'; live.write_bytes(core)
        sign('one explicitly authorized live publisher', live, rev_policy, base, 'ES256')
        both('unused retired additional key does not poison live signature', live, rev_policy)
        command('administrator swaps primary and additional pins explicitly',
                [*go, 'policy-update', '--policy', rev_policy, '--policy-version', '3', '--module-id', 'matrix.app',
                 '--certificate', retired['certificate'], '--additional-certificate', base['certificate'], '--signature-threshold', '1'])
        both('live additional key survives unused revoked primary pin', live, rev_policy)
        check('quorum and pin updates never erase tombstones', tombstone.read_bytes() == tombstone_bytes)
        # Download a genuine two-signature module over privately authenticated
        # HTTPS. Deployment refusal must publish no artifact; the VM repeats the
        # check even when an attacker bypasses the upper gate or changes output.
        tls = out / 'tls'; tls.mkdir(mode=0o700)
        for name in ['ca', 'server']:
            producers.openssl('genpkey', '-algorithm', 'EC', '-pkeyopt', 'ec_paramgen_curve:prime256v1', '-out', tls / (name + '.key'))
        producers.openssl('req', '-new', '-x509', '-sha256', '-key', tls / 'ca.key', '-out', tls / 'ca.pem',
                          '-days', '1', '-subj', '/CN=WSEA multi-signature transport CA',
                          '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=critical,keyCertSign')
        producers.openssl('req', '-new', '-key', tls / 'server.key', '-out', tls / 'server.csr', '-subj', '/CN=localhost')
        (tls / 'server.ext').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\nsubjectAltName=IP:127.0.0.1,DNS:localhost\n')
        producers.openssl('x509', '-req', '-in', tls / 'server.csr', '-CA', tls / 'ca.pem', '-CAkey', tls / 'ca.key',
                          '-CAcreateserial', '-sha256', '-days', '1', '-extfile', tls / 'server.ext', '-out', tls / 'server.pem')
        empty = tls / 'empty'; empty.mkdir()
        env = dict(os.environ, SSL_CERT_FILE=str(tls / 'ca.pem'), SSL_CERT_DIR=str(empty), NO_PROXY='127.0.0.1,localhost')
        bad = [list(entry) for entry in entries]
        bad[1][0] = bad[1][0][:-1] + bytes([bad[1][0][-1] ^ 1])
        forged = pack(path.parent / 'https-forged.wasm', bad)
        objects = {'/good': image.read_bytes(), '/forged': forged.read_bytes(), '/partial': partial.read_bytes(),
                   '/duplicate': (path.parent / 'duplicate.wasm').read_bytes()}
        report['requests'] = []
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                report['requests'].append(self.path)
                if self.path not in objects:
                    self.send_response(404); self.end_headers(); return
                body = objects[self.path]
                self.send_response(200); self.send_header('Content-Length', str(len(body))); self.end_headers()
                try:
                    self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError):
                    pass
            def log_message(self, *args):
                pass
        server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(tls / 'server.pem', tls / 'server.key')
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
        url = 'https://127.0.0.1:' + str(server.server_port)
        try:
            accepted = out / 'downloaded.wasm'
            command('HTTPS deployment gate verifies two genuine signatures',
                    [*go, 'fetch', accepted, '--policy', path, '--url', url + '/good'], contains=('VERIFIED',), env=env)
            check('downloaded artifact is the exact verified module', accepted.read_bytes() == image.read_bytes())
            command('actual VM independently verifies downloaded multi-signature module',
                    [*vm, '--secure', 'module', accepted, path, '--run', accepted], guest=True)
            for name, code in [('forged', 'SIG_003'), ('partial', 'POL_002'), ('duplicate', 'SIG_002')]:
                destination = out / ('refused-' + name + '.wasm')
                command('HTTPS refuses multi-signature ' + name,
                        [*go, 'fetch', destination, '--policy', path, '--url', url + '/' + name], exits=(1,), codes=(code,), env=env)
                check('refused download publishes no artifact ' + name, not destination.exists())
            unlisted_output = out / 'refused-unlisted.wasm'
            command('HTTPS refuses an unlisted extra publisher despite its trusted CA',
                    [*go, 'fetch', unlisted_output, '--policy', unlisted, '--url', url + '/good'],
                    exits=(1,), codes=('CERT_006',), env=env)
            check('unlisted publisher download publishes no artifact', not unlisted_output.exists())
            accepted.write_bytes(forged.read_bytes())
            command('VM second gate rejects mutation after successful deployment',
                    [*vm, '--secure', 'module', accepted, path, '--run', accepted], exits=(126, 127), codes=('SIG_003',), guest=True)
        finally:
            server.shutdown(); server.server_close(); thread.join(timeout=3)
            check('private HTTPS server thread closed', not thread.is_alive())
        report['algorithms'] = list(matrix.ALGORITHMS)
        report['genuine_signing_scenarios'] = [row[0] for row in scenarios]
        report['maximum_distinct_signers'] = 8
        report['expected_checks'] = 461
        if len(report['records']) != report['expected_checks']:
            raise RuntimeError('incomplete multi-signature assertion count: ' + str(len(report['records'])))
        report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'; report['error'] = repr(error)
        raise
    finally:
        report['producer_commands'] = producers.commands
        report['cgroup_after'] = matrix.cgroup_snapshot(True)
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'checks': len(report['records'])}))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
