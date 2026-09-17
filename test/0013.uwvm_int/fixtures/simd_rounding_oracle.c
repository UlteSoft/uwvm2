/* Link with uwvm_int_simd_rounding.cc compiled with main renamed. Expected
   bytes come from the independent host-side Python arithmetic oracle. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DECL(name) extern void name(void*, void const*)
DECL(round_f32_ceil); DECL(round_f32_floor); DECL(round_f32_trunc); DECL(round_f32_nearest);
DECL(round_f64_ceil); DECL(round_f64_floor); DECL(round_f64_trunc); DECL(round_f64_nearest);
#undef DECL

static uint64_t little(unsigned char const* p, unsigned n)
{
    uint64_t value = 0;
    for(unsigned i = 0; i < n; ++i) { value |= (uint64_t)p[i] << (8 * i); }
    return value;
}

int main(int argc, char** argv)
{
    if(argc != 2) { return 2; }
    FILE* file = fopen(argv[1], "rb");
    if(!file) { return 2; }
    unsigned char header[12];
    if(fread(header, 1, 12, file) != 12 || memcmp(header, "SIMDRND1", 8)) { return 2; }
    uint64_t records = little(header + 8, 4);
    if(records == 0 || records > 100000) { return 2; }
    uint64_t checks = 0, failures = 0;
    for(uint64_t record = 0; record < records; ++record)
    {
        unsigned char width;
        _Alignas(16) unsigned char input[16], output[16];
        unsigned char expected[64];
        if(fread(&width, 1, 1, file) != 1 || (width != 4 && width != 8) ||
           fread(input, 1, 16, file) != 16 || fread(expected, 1, 64, file) != 64) { return 2; }
        void (*f32[])(void*, void const*) = {round_f32_ceil, round_f32_floor, round_f32_trunc, round_f32_nearest};
        void (*f64[])(void*, void const*) = {round_f64_ceil, round_f64_floor, round_f64_trunc, round_f64_nearest};
        uint64_t sign = UINT64_C(1) << (width * 8 - 1);
        uint64_t infinity = width == 4 ? UINT64_C(0x7f800000) : UINT64_C(0x7ff0000000000000);
        uint64_t quiet = UINT64_C(1) << (width == 4 ? 22 : 51);
        for(unsigned op = 0; op < 4; ++op)
        {
            (width == 4 ? f32[op] : f64[op])(output, input);
            for(unsigned lane = 0; lane < 16 / width; ++lane)
            {
                uint64_t raw = little(input + lane * width, width), magnitude = raw & ~sign;
                uint64_t actual = little(output + lane * width, width);
                uint64_t wanted = little(expected + op * 16 + lane * width, width);
                int pass = magnitude > infinity ?
                    ((actual & (infinity | quiet)) == (infinity | quiet) &&
                     (magnitude != (infinity | quiet) || (actual & ~sign) == (infinity | quiet))) :
                    actual == wanted;
                ++checks;
                if(!pass)
                {
                    ++failures;
                    if(failures <= 8) { printf("FAIL record=%llu width=%u op=%u input=%llx actual=%llx expected=%llx\n",
                        (unsigned long long)record, width * 8, op, (unsigned long long)raw,
                        (unsigned long long)actual, (unsigned long long)wanted); }
                }
            }
        }
    }
    if(fgetc(file) != EOF || ferror(file) || fclose(file) != 0) { return 2; }
    printf("SIMD independent rounding records=%llu checks=%llu failures=%llu\n",
           (unsigned long long)records, (unsigned long long)checks, (unsigned long long)failures);
    return failures ? 1 : 0;
}
