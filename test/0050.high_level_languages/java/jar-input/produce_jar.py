#!/usr/bin/env python3
"""Existing JARs -> TeaVM 0.13.1 linear WASI; future Linux execution only.

Never runs the application on a JVM. Maven runs the real compiler, not javac
over reconstructed application sources. Trusted build input is required.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import signal
import stat
import types
import sys
import xml.etree.ElementTree as ET
import zipfile

VERSION = '0.13.1'
CGROUP = '/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
CORE = Path('org/teavm/teavm-core/0.13.1')
NAME = 'teavm-core-0.13.1.jar'
REQUIRED_PATCHES = {'teavm-0.13.1-wasm-dwarf.patch', 'teavm-0.13.1-ordered-names.patch',
                    'teavm-0.13.1-typed-locals.patch', 'teavm-0.13.1-wasi-environ.patch'}


class InputContractError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise InputContractError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_json(path):
    def pairs(items):
        result={}
        for key,value in items:
            require(key not in result,'duplicate JSON key')
            result[key]=value
        return result
    return json.loads(Path(path).read_text(),object_pairs_hook=pairs)


def record_file(path):
    path = Path(path).resolve(strict=True)
    require(path.is_file(), 'regular input required: ' + str(path))
    return {'path': str(path), 'sha256': digest(path), 'bytes': path.stat().st_size}


def safe_name(name):
    p = PurePosixPath(name)
    require(name and not p.is_absolute() and '..' not in p.parts and '\\' not in name
            and '\0' not in name, 'unsafe archive/source name')
    return p


def manifest_main(raw):
    require(len(raw) <= 65536, 'manifest exceeds 64KiB')
    text = raw.decode('utf-8', errors='strict').replace('\r\n', '\n')
    require('\r' not in text, 'invalid bare CR in manifest')
    logical = []
    for line in text.split('\n'):
        if not line:
            break  # Main-Class is only an attribute of the main section.
        if line.startswith(' '):
            require(bool(logical), 'orphan manifest continuation')
            logical[-1] += line[1:]
        else:
            logical.append(line)
    attrs = {}
    for line in logical:
        require(': ' in line, 'malformed manifest attribute')
        key, value = line.split(': ', 1)
        key = key.lower()
        require(key not in attrs, 'duplicate main manifest attribute')
        attrs[key] = value
    require(not attrs.get('class-path'), 'Manifest Class-Path is not resolved; pass explicit --dependency-jar inputs')
    require(attrs.get('multi-release', 'false').lower() != 'true', 'Multi-Release JAR selection is not implemented')
    return attrs.get('main-class')


class ClassReader:
    def __init__(self, data):
        self.data, self.at = data, 0

    def take(self, n):
        require(0 <= n <= len(self.data) - self.at, 'truncated class file')
        b = self.data[self.at:self.at+n]
        self.at += n
        return b

    def u(self, n):
        return int.from_bytes(self.take(n), 'big')


def class_debug(data):
    """Static class attributes only. Do not load or execute the class."""
    r = ClassReader(data)
    require(r.take(4) == b'\xca\xfe\xba\xbe', 'bad class magic')
    minor, major, count = r.u(2), r.u(2), r.u(2)
    cp = {}; i = 1
    while i < count:
        tag = r.u(1)
        if tag == 1:
            # Attribute names are ASCII. Arbitrary strings use modified UTF-8;
            # retain bytes rather than pretending standard UTF-8 decoded them.
            cp[i] = r.take(r.u(2))
        elif tag in (3, 4): r.take(4)
        elif tag in (5, 6): r.take(8); i += 1
        elif tag in (7, 8, 16, 19, 20): r.take(2)
        elif tag in (9, 10, 11, 12, 17, 18): r.take(4)
        elif tag == 15: r.take(3)
        else: raise InputContractError('unknown class constant-pool tag')
        i += 1
    def attributes(reader):
        out = []
        for _ in range(reader.u(2)):
            name = cp.get(reader.u(2), b'')
            out.append((name, reader.take(reader.u(4))))
        return out
    r.take(6)  # access_flags, this_class, super_class
    r.take(2 * r.u(2))
    for _ in range(r.u(2)):
        r.take(6); attributes(r)
    methods = []; has_lines = has_locals = False
    for _ in range(r.u(2)):
        access, name, descriptor = r.u(2), r.u(2), r.u(2)
        attrs = attributes(r); line = local = False
        for attr, payload in attrs:
            if attr != b'Code': continue
            c = ClassReader(payload); c.take(4); c.take(c.u(4)); c.take(8*c.u(2))
            nested = attributes(c)
            require(c.at == len(payload), 'Code trailing bytes')
            line = any(k == b'LineNumberTable' for k, _ in nested)
            local = any(k == b'LocalVariableTable' for k, _ in nested)
        has_lines |= line; has_locals |= local
        methods.append({'name': cp.get(name, b'').decode('ascii', errors='backslashreplace'),
                        'descriptor': cp.get(descriptor, b'').decode('ascii', errors='backslashreplace'),
                        'native': bool(access & 0x100), 'line_table': line, 'local_variable_table': local})
    attrs = attributes(r)
    require(r.at == len(data), 'class trailing bytes')
    return {'major': major, 'minor': minor, 'line_tables_present': has_lines,
            'local_variable_tables_present': has_locals,
            'source_file_attribute': any(k == b'SourceFile' for k, _ in attrs), 'methods': methods}


def jar_inventory(path, seen_classes):
    require(path.stat().st_size <= 256 << 20, 'input JAR exceeds 256MiB')
    result = {'file': record_file(path), 'classes': {}, 'manifest_main': None}
    found_manifest = False
    with zipfile.ZipFile(path) as jar:
        entries = jar.infolist()
        require(len(entries) <= 100000, 'too many JAR entries')
        names = set(); total = 0
        for item in entries:
            safe_name(item.filename)
            require(item.filename not in names, 'duplicate JAR entry')
            names.add(item.filename)
            require(not item.flag_bits & 1, 'encrypted JAR not supported')
            require((item.external_attr >> 16) & 0o170000 != 0o120000, 'JAR symlink not supported')
            total += item.file_size
            require(total <= 512 << 20, 'JAR expanded size exceeds 512MiB')
            require(not item.filename.startswith('META-INF/versions/'), 'versioned classes not supported')
            if item.filename.upper() == 'META-INF/MANIFEST.MF':
                require(not found_manifest and item.filename == 'META-INF/MANIFEST.MF', 'ambiguous/noncanonical manifest name')
                found_manifest = True
                require(item.file_size <= 65536, 'manifest exceeds 64KiB')
                result['manifest_main'] = manifest_main(jar.read(item))
            if item.filename.endswith('.class'):
                require(item.file_size <= 8 << 20, 'single class exceeds 8MiB')
                require(item.filename not in seen_classes, 'duplicate class across explicit inputs: ' + item.filename)
                seen_classes.add(item.filename)
                raw = jar.read(item)
                info = class_debug(raw)
                info['sha256'] = hashlib.sha256(raw).hexdigest()
                result['classes'][item.filename] = info
    result['expanded_bytes'] = total
    return result


def tree_inventory(root):
    result = {}
    for p in sorted(Path(root).rglob('*')):
        require(not p.is_symlink(), 'tool repository/source symlink unsupported: ' + str(p))
        if p.is_file():
            require(len(result) < 50000, 'tool repository file cap')
            result[str(p.relative_to(root))] = {'sha256': digest(p), 'bytes': p.stat().st_size}
    return result


def pom(project, jar_paths, main, optimization, max_heap):
    ns = 'http://maven.apache.org/POM/4.0.0'
    ET.register_namespace('', ns)
    root = ET.Element('{'+ns+'}project')
    def add(parent, name, text=None):
        item = ET.SubElement(parent, '{'+ns+'}'+name)
        if text is not None: item.text = str(text)
        return item
    for k, v in [('modelVersion','4.0.0'),('groupId','uwvm.jar-input'),('artifactId','translation'),('version','1')]: add(root,k,v)
    deps = add(root,'dependencies')
    for i, p in enumerate(jar_paths):
        dep = add(deps,'dependency')
        for k,v in [('groupId','uwvm.explicit-input'),('artifactId','input-'+str(i)),('version','1'),('scope','system'),('systemPath',p)]: add(dep,k,v)
    for artifact, scope in [('teavm-classlib','compile'),('teavm-core','provided'),('teavm-interop','provided')]:
        dep=add(deps,'dependency')
        for k,v in [('groupId','org.teavm'),('artifactId',artifact),('version',VERSION),('scope',scope)]: add(dep,k,v)
    build=add(root,'build'); plugins=add(build,'plugins'); plugin=add(plugins,'plugin')
    for k,v in [('groupId','org.teavm'),('artifactId','teavm-maven-plugin'),('version',VERSION)]: add(plugin,k,v)
    config=add(plugin,'configuration')
    for k,v in [('mainClass',main),('targetType','WEBASSEMBLY_WASI'),('targetDirectory',project/'target/wasi'),
                ('targetFileName','application.wasm'),('optimizationLevel',optimization),('minifying','false'),
                ('strict','true'),('stopOnErrors','true'),('outOfProcess','false'),('incremental','false'),
                ('debugInformationGenerated','true'),('wasmDebugInfoLocation','EMBEDDED'),
                ('sourceMapsGenerated','true'),('sourceFilesCopied','true'),('minHeapSize',1),('maxHeapSize',max_heap)]: add(config,k,v)
    return ET.tostring(root,encoding='utf-8',xml_declaration=True)+b'\n'


def client_abi_contract(document, backend):
    """Closed optional import contract, never a capability or native oracle."""
    require(isinstance(document,dict) and document.get('kind')=='optional-service-client-v1'
            and document.get('schema_version')==1 and document.get('extension_version')==1
            and document.get('original_business_modules_unchanged') is True,
            'unsupported optional service-client ABI catalog')
    require(document.get('output_contract')=={'features_bytes':8,'availability_bytes':1,
            'nonzero_error_leaves_output_untouched':True}
            and document.get('maximum_wait_ns')==60000000000,
            'optional service-client output contract mismatch')
    expected=[{'name':'abi_version','params':[],'results':['i32']},
              {'name':'features','params':['i64','i64','i32','i64'],'results':['i32']},
              {'name':'service_available','params':['i64','i64','i64','i32','i64'],'results':['i32']}]
    modules=document.get('modules')
    require(isinstance(modules,dict) and set(modules)=={'uwvm_ros1_client_v1','uwvm_ros2_client_v1'},
            'optional service-client module closure mismatch')
    for number in ['1','2']:
        row=modules['uwvm_ros'+number+'_client_v1']
        require(isinstance(row,dict) and row.get('backend')==int(number) and row.get('function_count')==3
                and row.get('functions')==expected,'optional service-client signatures mismatch')
    return {'module':'uwvm_ros'+backend+'_client_v1','functions':expected}


def wasm_imports(path, client_contract=None):
    data=Path(path).read_bytes();require(data[:8]==b'\0asm\1\0\0\0','bad produced Core Wasm header')
    r=ClassReader(data);r.at=8
    def leb(x):
        value=shift=0
        for _ in range(5):
            b=x.u(1);value|=(b&127)<<shift
            if not b&128:
                require(value < 1<<32,'u32 overflow');return value
            shift+=7
        raise InputContractError('overlong u32')
    def string(x):return x.take(leb(x)).decode('utf-8',errors='strict')
    imports=[];custom=[];types=[];type_seen=False
    value_types={0x7f:'i32',0x7e:'i64',0x7d:'f32',0x7c:'f64',0x7b:'v128',0x70:'funcref',0x6f:'externref'}
    def values(x):
        result=[]
        for _ in range(leb(x)):
            value=x.u(1)
            require(value in value_types,'optional client inventory requires simple Core function value types')
            result.append(value_types[value])
        return result
    while r.at<len(data):
        kind=r.u(1);sub=ClassReader(r.take(leb(r)))
        if kind==0:custom.append(string(sub))
        elif kind==1 and client_contract is not None:
            require(not type_seen,'duplicate Core type section');type_seen=True
            for _ in range(leb(sub)):
                require(sub.u(1)==0x60,'optional client inventory requires a linear Core function type section')
                types.append({'params':values(sub),'results':values(sub)})
            require(sub.at==len(sub.data),'type section trailing bytes')
        elif kind==2:
            for _ in range(leb(sub)):
                module,name,external=string(sub),string(sub),sub.u(1)
                require(external==0,'this linear-WASI JAR route admits function imports only')
                imports.append({'module':module,'name':name,'type_index':leb(sub)})
            require(sub.at==len(sub.data),'import trailing bytes')
    result={'imports':imports,'custom_sections':custom}
    if client_contract is not None:
        expected={x['name']:{'params':x['params'],'results':x['results']} for x in client_contract['functions']}
        selected=[];seen=set()
        for item in imports:
            if item['module']!=client_contract['module']:continue
            require(item['name'] in expected,'unknown optional service-client function')
            require(item['name'] not in seen,'duplicate optional service-client import')
            seen.add(item['name'])
            require(item['type_index']<len(types),'optional service-client type index out of bounds')
            signature=types[item['type_index']]
            require(signature==expected[item['name']],'optional service-client import signature mismatch')
            selected.append({**item,**signature})
        result['optional_service_client_imports']=selected
        result['full_wasm_validation_performed']=False
    return result



def captured_owned_source(path, expected, maximum=2*1024*1024):
    """New optional path only: exact bytes, never local import/pyc lookup.
    Entry/interpreter/stdlib and original producer ownership remain keeper trust.
    """
    require(re.fullmatch('[0-9a-f]{64}',expected) is not None,'explicit lowercase Source SHA required')
    require(path.is_absolute() and path.resolve(strict=True)==path,'resolved absolute Source path required')
    fd=os.open(path,os.O_RDONLY|os.O_CLOEXEC|os.O_NONBLOCK|os.O_NOFOLLOW)
    try:
        first=os.fstat(fd);require(stat.S_ISREG(first.st_mode) and 0<=first.st_size<=maximum,'bounded ordinary Source required')
        chunks=[];count=0
        while True:
            part=os.read(fd,min(1024*1024,maximum-count+1))
            if not part:break
            count+=len(part);require(count<=maximum,'captured Source grew');chunks.append(part)
        raw=b''.join(chunks);last=os.fstat(fd);named=os.lstat(path)
        fields=('st_dev','st_ino','st_mode','st_size','st_mtime_ns','st_ctime_ns','st_nlink')
        require(count==first.st_size and all(getattr(first,k)==getattr(last,k)==getattr(named,k)for k in fields) and hashlib.sha256(raw).hexdigest()==expected,'captured exact Source changed/differs')
        return raw
    finally:os.close(fd)


def owned_binary_check(root,wasm,policy,policy_sha,catalog,catalog_sha,inspector_sha,checker_sha,checker_raw,client_contract):
    name='_uwvm_java_owned_checker_'+os.urandom(16).hex();require(name not in sys.modules,'fresh private checker module')
    module=types.ModuleType(name);path=root/'sdk/java/owned-service/check_imports.py';module.__file__=str(path);module.__package__='';module.__loader__=module.__spec__=module.__cached__=None;sys.modules[name]=module
    try:
        require(hashlib.sha256(checker_raw).hexdigest()==checker_sha,'captured checker pairing changed')
        exec(compile(checker_raw,str(path),'exec',dont_inherit=True),module.__dict__)
        require(sys.modules.get(name)is module,'checker module owner changed')
        result=module.check(root,wasm,policy,policy_sha,catalog,catalog_sha,inspector_sha,client_contract)
        require(captured_owned_source(path,checker_sha)==checker_raw,'checker source changed before receipt')
        return result
    finally:
        if sys.modules.get(name)is module:del sys.modules[name]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--jar',type=Path,required=True)
    p.add_argument('--dependency-jar',type=Path,action='append',default=[])
    p.add_argument('--main-class')
    p.add_argument('--source-root',type=Path,action='append',default=[])
    p.add_argument('--source-map',type=Path,help='JSON: repository-relative .java path -> actual source file')
    p.add_argument('--m2',type=Path,required=True)
    p.add_argument('--toolchain-receipt',type=Path,required=True)
    p.add_argument('--toolchain-receipt-sha256',required=True)
    p.add_argument('--patch-root',type=Path,required=True)
    p.add_argument('--maven',type=Path,required=True)
    p.add_argument('--java-home',type=Path,required=True)
    p.add_argument('--helpers',type=Path,required=True,help='suite root, containing python/linux_control.py and owned_process.py')
    p.add_argument('--cpu',type=int,default=8)
    p.add_argument('--optimization',choices=['SIMPLE','FULL'],default='SIMPLE')
    p.add_argument('--max-heap-mib',type=int,default=128)
    p.add_argument('--ros-backend',choices=['1','2'],help='import inventory ceiling only, never a Host capability grant')
    p.add_argument('--ros-client-abi',type=Path,help='explicit optional service-client ABI JSON; requires --ros-backend and its SHA256')
    p.add_argument('--ros-client-abi-sha256',help='SHA256 of the exact optional ABI JSON, separately pinned from the business ABI')
    p.add_argument('--ros-rmw-service-enabled',choices=('0','1'),default='0',help='explicit defaultoff owned12; backend2 only, no grants')
    p.add_argument('--ros-rmw-source-root',type=Path)
    p.add_argument('--ros-rmw-service-abi',type=Path)
    p.add_argument('--ros-rmw-service-abi-sha256')
    p.add_argument('--ros-rmw-imports',type=Path,help='reviewed complete actual module/name/kind/params/results policy, including WASI')
    p.add_argument('--ros-rmw-imports-sha256')
    p.add_argument('--ros-rmw-inspector-sha256')
    p.add_argument('--ros-rmw-checker-sha256')
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    require(sys.platform=='linux','future execution only on authorized SSH Linux')
    require(Path('/proc/self/cgroup').read_text().strip()=='0::'+CGROUP,'wrong existing cgroup')
    require(os.sched_getaffinity(0)=={a.cpu},'launch under taskset on the explicitly authorized CPU')
    require(1<=a.max_heap_mib<=1024,'bounded Guest max heap required')
    a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=False)
    record={'passed':False,'scope':'JAR bytecode translation only, not Guest or JVM application execution',
            'threaded_profile_claim':False,'host_ros_authority_granted':False,'inputs':{},'commands':[],
            'builder':record_file(__file__),'error':None}
    def save():
        target=a.out/'receipt.json';temp=a.out/'receipt.tmp'
        with temp.open('w') as f:json.dump(record,f,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
        os.replace(temp,target)
    def bind(path):
        row=record_file(path);record['inputs'][row['path']]=row;return Path(row['path'])
    def ignore_signals():
        for sig in (signal.SIGINT,signal.SIGTERM,signal.SIGHUP):signal.signal(sig,signal.SIG_IGN)
    def stop(sig,frame):
        ignore_signals();raise InterruptedError('producer interrupted: '+str(sig))
    for sig in (signal.SIGINT,signal.SIGTERM,signal.SIGHUP):signal.signal(sig,stop)
    save()
    try:
        require((a.ros_client_abi is None)==(a.ros_client_abi_sha256 is None),
                'optional service-client ABI path and SHA256 must be supplied together')
        client_contract=None
        if a.ros_client_abi is not None:
            require(a.ros_backend is not None,'optional service-client ABI requires --ros-backend')
            require(re.fullmatch(r'[0-9a-f]{64}',a.ros_client_abi_sha256) is not None,
                    'optional service-client ABI requires lowercase SHA256')
            client_path=bind(a.ros_client_abi)
            require(digest(client_path)==a.ros_client_abi_sha256,'optional service-client ABI pin mismatch')
            client_contract=client_abi_contract(read_json(client_path),a.ros_backend)
            record['optional_service_client']={'catalog':record_file(client_path),'contract':client_contract,
                'host_authority_granted':False,'native_delivery_qualified':False}
        rmw=None
        rmw_values=[a.ros_rmw_source_root,a.ros_rmw_service_abi,a.ros_rmw_service_abi_sha256,a.ros_rmw_imports,a.ros_rmw_imports_sha256,a.ros_rmw_inspector_sha256,a.ros_rmw_checker_sha256]
        if a.ros_rmw_service_enabled=='1':
            require(a.ros_backend=='2' and all(v is not None for v in rmw_values),'owned12 requires explicit backend2 and complete pinned inputs')
            root=a.ros_rmw_source_root.resolve(strict=True);catalog=bind(a.ros_rmw_service_abi);policy=bind(a.ros_rmw_imports)
            checker=root/'sdk/java/owned-service/check_imports.py';inspector=root/'sdk/tools/check_wasm_imports.py'
            checker_raw=captured_owned_source(bind(checker),a.ros_rmw_checker_sha256)
            captured_owned_source(bind(inspector),a.ros_rmw_inspector_sha256)
            captured_owned_source(catalog,a.ros_rmw_service_abi_sha256);captured_owned_source(policy,a.ros_rmw_imports_sha256)
            rmw=(root,catalog,policy,checker_raw)
        else:require(all(v is None for v in rmw_values),'defaultoff does not accept optional owned12 inputs')
        helpers=a.helpers.resolve(strict=True)
        for helper in [helpers/'python/linux_control.py',helpers/'owned_process.py']:bind(helper)
        sys.path.insert(0,str(helpers/'python'))
        from linux_control import guard,invoke
        record['admission_before']=guard(a.out)
        require(record['admission_before']['disk_free']>=4<<30,'4GiB free required before private repository copy')
        require(record['admission_before']['anon']<50<<30,'shared anonymous budget unavailable')
        tc=bind(a.toolchain_receipt);require(digest(tc)==a.toolchain_receipt_sha256,'toolchain receipt pin mismatch')
        tool=read_json(tc);require(tool.get('passed') and tool.get('wasi_environ_mapping') and tool.get('ordered_name_maps') and tool.get('typed_local_values'),'require actual isolated DWARF/ordered/typed/environ toolchain receipt')
        require(set(tool['patch_sha256'])==REQUIRED_PATCHES,'unexpected patch combination')
        for name in sorted(REQUIRED_PATCHES):require(digest(bind(a.patch_root/name))==tool['patch_sha256'][name],'patch identity mismatch')
        original=a.m2.resolve(strict=True)
        require(not a.out.is_relative_to(original), 'output must be outside the source tool repository')
        core=bind(original/CORE/NAME)
        require(digest(core)==tool['patched_jar_sha256'],'private patched core differs from receipt')
        sources=bind(original/CORE/'teavm-core-0.13.1-sources.jar')
        with zipfile.ZipFile(core) as z:
            for name,sha in tool['replaced_classes'].items():require(hashlib.sha256(z.read(name)).hexdigest()==sha,'patched class mismatch')
        with zipfile.ZipFile(sources) as z:
            for name,sha in tool['source_after'].items():require(hashlib.sha256(z.read(name)).hexdigest()==sha,'patched source mismatch')
        require(tool['official_jar']['sha256']=='b9fe1b008512c8f11ff3efbd9cf2f0565b4bfa76943a9f23910befc0812f7ebf',
                'receipt upstream core is not the pinned TeaVM0.13.1 base')
        record['toolchain']=tool;record['original_m2']=str(original);record['m2_before']=tree_inventory(original)
        m2_bytes=sum(x['bytes'] for x in record['m2_before'].values())
        require(m2_bytes<=1<<30,'private tool repository exceeds 1GiB copy cap')
        seen=set();jar_rows=[];jar_paths=[]
        require(len(a.dependency_jar)<=64,'dependency JAR count cap')
        for source in [a.jar,*a.dependency_jar]:
            source=bind(source);jar_rows.append(jar_inventory(source,seen));jar_paths.append(source)
        require(sum(x['file']['bytes'] for x in jar_rows)<=512<<20,'combined JAR compressed byte cap')
        require(sum(x['expanded_bytes'] for x in jar_rows)<=1<<30 and len(seen)<=100000,
                'combined JAR expanded byte/class count cap')
        require(guard(a.out)['disk_free'] >= (4<<30)+m2_bytes+sum(x['file']['bytes'] for x in jar_rows)+(16<<20),
                'copy headroom must preserve 4GiB floor before compiler launch')
        main=a.main_class or jar_rows[0]['manifest_main']
        require(main is not None,'no Main-Class: pass --main-class')
        require(re.fullmatch(r'[A-Za-z_$][A-Za-z0-9_$]*(?:\.[A-Za-z_$][A-Za-z0-9_$]*)*',main),'unsupported main class spelling')
        require(main.replace('.','/')+'.class' in seen,'main class absent from explicit classpath')
        record['main_class']=main;record['main_selection']='explicit' if a.main_class else 'manifest'
        record['application_jars']=jar_rows
        project=a.out/'project';(project/'target/classes').mkdir(parents=True);copied=[]
        for i,source in enumerate(jar_paths):
            dest=project/'inputs'/f'{i:03d}.jar';dest.parent.mkdir(exist_ok=True);shutil.copy2(source,dest);require(digest(dest)==digest(source),'JAR copy mismatch');copied.append(dest)
        source_map={}
        for root in a.source_root:
            root=root.resolve(strict=True)
            for source in root.rglob('*'):
                require(not source.is_symlink(),'source-root symlink unsupported')
                if source.is_file() and source.suffix=='.java':
                    key=str(source.relative_to(root))
                    require(key not in source_map,'duplicate source-root path')
                    source_map[key]=source
        if a.source_map:
            m=read_json(bind(a.source_map));require(isinstance(m,dict),'source map must be object')
            for key,value in m.items():require(key not in source_map,'duplicate mapped source');source_map[key]=Path(value)
        require(len(source_map)<=10000,'source-map file cap');total=0
        for key,source in source_map.items():
            rel=safe_name(key);require(rel.suffix=='.java','only .java source-map entries')
            source=bind(source);total+=source.stat().st_size;require(total<=16<<20,'source map exceeds 16MiB')
            dest=project/'src/main/java'/str(rel);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
        record['source_map']={k:str(v) for k,v in source_map.items()}
        (project/'pom.xml').write_bytes(pom(project,copied,main,a.optimization,a.max_heap_mib))
        private=a.out/'m2';shutil.copytree(original,private)
        require(tree_inventory(private)==record['m2_before'],'private m2 snapshot mismatch')
        java_home=a.java_home.resolve(strict=True);maven=bind(a.maven)
        # The compile goal reads existing bytecode; no javac or jar subprocess
        # is invoked. Modular JDK installations can expose these tools only
        # through `java -m`, so their unused launcher files are not prerequisites.
        java_launcher = java_home/'bin/java'
        require(java_launcher.is_file() and os.access(java_launcher, os.X_OK),
                'Java runtime launcher must be an executable regular file')
        bind(java_launcher)
        bind(java_home/'lib/modules')
        bind(java_home/'lib/server/libjvm.so')
        for relative in ['lib/libjava.so', 'lib/libjimage.so', 'lib/jvm.cfg',
                         'lib/server/classes.jsa']:
            runtime_input = java_home/relative
            if runtime_input.is_file():bind(runtime_input)
        env={'JAVA_HOME':str(java_home),'PATH':str(java_home/'bin')+':/usr/bin:/bin','LC_ALL':'C.UTF-8',
             'MAVEN_OPTS':'-Xmx1536m -XX:ActiveProcessorCount=1 -Dfile.encoding=UTF-8'}
        def run(argv,name):
            folder=a.out/'commands'/name
            pending={'argv':list(map(str,argv)),'folder':str(folder),'status':'started'}
            record['commands'].append(pending);save()
            try:
                row=invoke(argv,folder,cwd=project,env=env,timeout=900)
                require(row.get('retired') and row.get('ownership',{}).get('closed') and
                        all(not member['pidfd_retained'] for member in row['ownership']['members']),
                        'owned compiler process retirement is incomplete')
                pending.update(status='completed',receipt=record_file(folder/'receipt.json'))
                return row
            finally:
                if (folder/'receipt.json').is_file():
                    pending['receipt']=record_file(folder/'receipt.json')
                    pending['actual']=json.loads((folder/'receipt.json').read_text())
                save()
        run([java_home/'bin/java','-version'],'java-version')
        run([maven,'--version'],'maven-version')
        # Exact official plugin goal. No project lifecycle javac, jar -xf,
        # -jar app execution, target JVM/JNI oracle or guessed TeaVM CLI flag.
        run([maven,'-o','--batch-mode','--no-transfer-progress','-f',project/'pom.xml',
             '-Dmaven.repo.local='+str(private),'org.teavm:teavm-maven-plugin:0.13.1:compile'],'teavm')
        wasm=project/'target/wasi/application.wasm';record['artifact']=record_file(wasm)
        module=wasm_imports(wasm,client_contract);record['module']=module
        permitted={'wasi_snapshot_preview1'}
        if a.ros_backend:permitted.add('uwvm_ros'+a.ros_backend+'_v1')
        if client_contract is not None:permitted.add(client_contract['module'])
        if rmw is not None:
            root,catalog,policy,checker_raw=rmw
            record['optional_owned_service']=owned_binary_check(root,wasm,policy,a.ros_rmw_imports_sha256,catalog,a.ros_rmw_service_abi_sha256,a.ros_rmw_inspector_sha256,a.ros_rmw_checker_sha256,checker_raw,client_contract)
            permitted.add('uwvm_ros2_rmw_srv_v1')
        require(all(x['module'] in permitted for x in module['imports']),'unapproved host import namespace; no stub is generated')
        require('.debug_info' in module['custom_sections'] and '.debug_line' in module['custom_sections'],'requested producer DWARF missing')
        record['import_namespace_ceiling']=sorted(permitted)
        record['debug_scope']={'bytecode_attributes_only':True,'source_files_supplied':len(source_map),
                               'source_line_breakpoints_or_typed_variables_qualified':False,
                               'limitation':'No source/LNT/LVT is reconstructed; optimization and original bytecode attributes limit DWARF. Run actual DBG qualification separately.'}
        record['producer_outputs']=tree_inventory(project/'target/wasi')
        record['all_recorded_commands_owned_retired']=all(x['actual'].get('retired') and
            x['actual'].get('ownership',{}).get('closed') and
            all(not m['pidfd_retained'] for m in x['actual']['ownership']['members']) for x in record['commands'])
        require(record['all_recorded_commands_owned_retired'],'compiler ownership not closed')
        record['passed']=True
    except BaseException as error:
        record['passed']=False;record['error']={'type':type(error).__name__,'message':str(error)}
    finally:
        ignore_signals()
        try:
            record['inputs_after']={k:record_file(k) for k in record['inputs']}
            require(record['inputs_after']==record['inputs'],'original input changed')
            if 'm2_before' in record:require(tree_inventory(original)==record['m2_before'],'original repository changed')
            if 'admission_before' in record:
                record['admission_after']=guard(a.out)
                before,after=record['admission_before'],record['admission_after']
                require(before['boot']==after['boot'] and before['cgroup']==after['cgroup'] and before['memory_events']==after['memory_events'],'admission drift')
        except BaseException as error:
            record['passed']=False;record['finalization_error']=repr(error)
        save()
    print(json.dumps({'passed':record['passed'],'error':record['error'],'receipt':str(a.out/'receipt.json')}),flush=True)
    return 0 if record['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())
