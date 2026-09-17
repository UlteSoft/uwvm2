"""Execute every documented constrained comparison predicate on MIPS R6.

Checks exact 0/-1 masks and FE_INVALID separately: quiet comparisons signal
only sNaNs, signaling comparisons signal any NaN. The C oracle never evaluates
a floating expression, so host/compiler NaN conventions cannot bless bad code.
Includes discarded-result comparisons to test preservation of FP exceptions.
"""
import argparse
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--llvm-tools', type=Path, required=True,
                    help='directory containing the exact llc to validate; never searches PATH')
parser.add_argument('--profile', type=Path, required=True,
                    help='JSON object with triple, cpu, cxx, flags, run and optional llc_flags/features')
parser.add_argument('--out', type=Path, required=True)
args = parser.parse_args()
tool = args.llvm_tools.resolve()
out = args.out.resolve()
profile = json.loads(args.profile.read_text())
arch = profile['triple'].split('-')[0]
assert arch in ('mips', 'mipsel', 'mips64', 'mips64el')
assert profile['cpu'] in ('mips32r6', 'mips64r6'), 'This oracle assumes R6 NaN2008'
out.mkdir(parents=True, exist_ok=False)
little_endian = arch.endswith('el')
target_flags = ['-mtriple='+profile['triple'], '-mcpu='+profile['cpu']]
target_flags += profile.get('llc_flags', [])
if profile.get('features'):
    target_flags.append('-mattr='+profile['features'])
predicates = ['oeq', 'ogt', 'oge', 'olt', 'ole', 'one', 'ord', 'ueq', 'ugt', 'uge', 'ult', 'ule', 'une', 'uno']
ir, declarations, functions, descriptors = [], [], [], []
for width, ty in [(32, 'float'), (64, 'double')]:
    for signaling in (0, 1):
        intrinsic = 'llvm.experimental.constrained.fcmp'+('s' if signaling else '')+'.f'+str(width)
        ir.append(f'declare i1 @{intrinsic}({ty}, {ty}, metadata, metadata)')
        for discard in (0, 1):
            for index, predicate in enumerate(predicates):
                name = f'probe_{width}_{signaling}_{discard}_{predicate}'
                ir.append(f'''define void @{name}(ptr %out, ptr %in) #0 {{
  %a = load {ty}, ptr %in, align 1
  %rp = getelementptr i8, ptr %in, i32 {width//8}
  %b = load {ty}, ptr %rp, align 1
  %cmp = call i1 @{intrinsic}({ty} %a, {ty} %b, metadata !"{predicate}", metadata !"fpexcept.strict")
  %mask = sext i1 %cmp to i32
  store i32 {'0' if discard else '%mask'}, ptr %out, align 1
  ret void
}}''')
                declarations.append(f'extern void {name}(unsigned char*, const unsigned char*);')
                functions.append(name)
                descriptors.append(f'{{{width},{signaling},{discard},{index}}}')
ir.append('attributes #0 = { noinline nounwind strictfp }')
(out/'predicates.ll').write_text('\n'.join(ir)+'\n')
constants32 = [0, 0x80000000, 0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 1, 0x80000001,
               0x7f7fffff, 0xff7fffff, 0x7fc00000, 0xffc00000, 0x7fc00123, 0xffc00123, 0x7f800123, 0xff800123]
constants64 = [0, 0x8000000000000000, 0x3ff0000000000000, 0xbff0000000000000, 0x7ff0000000000000, 0xfff0000000000000,
               1, 0x8000000000000001, 0x7fefffffffffffff, 0xffefffffffffffff, 0x7ff8000000000000, 0xfff8000000000000,
               0x7ff8000000000123, 0xfff8000000000123, 0x7ff0000000000123, 0xfff0000000000123]
c = '#include <stdint.h>\n#include <stdio.h>\n#include <fenv.h>\n'+ '\n'.join(declarations)+'\n'
c += 'static void (*const functions[])(unsigned char*, const unsigned char*) = {'+','.join(functions)+'};\n'
c += 'static const unsigned descriptors[][4] = {'+','.join(descriptors)+'};\n'
c += 'static const uint64_t values[2][16] = {'+','.join('{'+','.join(hex(x)+'ULL' for x in xs)+'}' for xs in [constants32, constants64])+'};\n'
c += '#define ORACLE_LITTLE_ENDIAN '+str(int(little_endian))+'\n'
c += r'''
int main(void) {
  unsigned checks = 0;
  for(unsigned f=0; f<sizeof(functions)/sizeof(functions[0]); ++f) {
    unsigned width=descriptors[f][0], signaling=descriptors[f][1], discard=descriptors[f][2], p=descriptors[f][3];
    unsigned bytes=width/8;
    uint64_t sign=1ULL<<(width-1), exponent=width==32 ? 0x7f800000ULL : 0x7ff0000000000000ULL;
    uint64_t quiet=width==32 ? 0x400000ULL : 0x8000000000000ULL;
    for(unsigned a=0; a<16; ++a) for(unsigned b=0; b<16; ++b) {
      uint64_t x=values[width==64][a], y=values[width==64][b], ax=x&~sign, ay=y&~sign;
      int nx=ax>exponent, ny=ay>exponent, nan=nx||ny;
      int snan=(nx&&!(x&quiet))||(ny&&!(y&quiet));
      int eq=x==y || (!ax&&!ay), lt=!eq && (((x^y)&sign) ? !!(x&sign) : ((x&sign) ? x>y : x<y));
      int predicates[]={!nan&&eq,!nan&&!eq&&!lt,!nan&&(!lt||eq),!nan&&lt,!nan&&(lt||eq),!nan&&!eq,!nan,
                         nan||eq,nan||(!eq&&!lt),nan||!lt,nan||lt,nan||lt||eq,nan||!eq,nan};
      uint32_t expected=discard ? 0 : -(uint32_t)predicates[p], got=0;
      unsigned char in[16]={0}, out[4]={0};
      for(unsigned j=0;j<bytes;++j){in[j]=(unsigned char)(x>>(8*(ORACLE_LITTLE_ENDIAN?j:bytes-1-j)));in[bytes+j]=(unsigned char)(y>>(8*(ORACLE_LITTLE_ENDIAN?j:bytes-1-j)));}
      if(feclearexcept(FE_ALL_EXCEPT)) return 3;
      functions[f](out,in);
      int invalid=!!fetestexcept(FE_INVALID);
      for(unsigned j=0;j<4;++j) got|=(uint32_t)out[j]<<(8*(ORACLE_LITTLE_ENDIAN?j:3-j));
      if(got!=expected || invalid!=(signaling?nan:snan)) {
        printf("FAIL f=%u width=%u signaling=%u discard=%u pred=%u a=%u b=%u got=%08x expected=%08x invalid=%d expected_invalid=%d\n",
               f,width,signaling,discard,p,a,b,got,expected,invalid,signaling?nan:snan);
        return 1;
      }
      ++checks;
    }
  }
  printf("PASS %u strict predicate/value/exception checks\n",checks);
  return 0;
}
'''
(out/'oracle.c').write_text(c)
commands = [('object', [str(tool/'llc'), '-O3', '-verify-machineinstrs', *target_flags, '-relocation-model=pic', '-filetype=obj', str(out/'predicates.ll'), '-o', str(out/'predicates.o')]),
            ('assembly', [str(tool/'llc'), '-O3', '-verify-machineinstrs', *target_flags, str(out/'predicates.ll'), '-o', str(out/'predicates.s')]),
            ('link', [*profile['cxx'], *profile['flags'], '-O2', '-x', 'c', str(out/'oracle.c'), '-x', 'none', str(out/'predicates.o'), '-lm', '-o', str(out/'test')]),
            ('run', [*profile['run'], str(out/'test')])]
rows=[]
for name, cmd in commands:
    with (out/(name+'.log')).open('wb') as log:
        status=subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT).returncode
    rows.append(dict(name=name, command=cmd, status=status))
    (out/'results.json').write_text(json.dumps(rows, indent=2)+'\n')
    print(name, status, flush=True)
    if status: raise SystemExit(status)
