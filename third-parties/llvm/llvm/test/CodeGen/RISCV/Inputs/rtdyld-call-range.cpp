// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Standalone RuntimeDyld regression; compile against the repaired LLVM libraries
// and pass RV64 and RV32 objects assembled from ../rtdyld-call-range.s.
#include "llvm/ExecutionEngine/RuntimeDyld.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MathExtras.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

using namespace llvm;
static unsigned Checks;
static void check(bool Value) {
  ++Checks;
  if (!Value) {
    std::fprintf(stderr, "FAIL RuntimeDyld RISC-V call check %u\n", Checks);
    std::abort();
  }
}

class Memory final : public SectionMemoryManager {
public:
  bool Stubs;
  std::map<std::string, uint8_t *> Code;
  explicit Memory(bool Stubs) : Stubs(Stubs) {}
  bool allowStubAllocation() const override { return Stubs; }
  uint8_t *allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned ID,
                               StringRef Name) override {
    auto *Address = SectionMemoryManager::allocateCodeSection(Size, Alignment, ID,
                                                             Name);
    Code[Name.str()] = Address;
    return Address;
  }
};

class Resolver final : public LegacyJITSymbolResolver {
public:
  uint64_t Target;
  explicit Resolver(uint64_t Target) : Target(Target) {}
  JITSymbol findSymbolInLogicalDylib(const std::string &) override { return nullptr; }
  JITSymbol findSymbol(const std::string &Name) override {
    check(Name == "external_target");
    return JITSymbol(Target, JITSymbolFlags::Exported);
  }
};

static uint64_t verifyCall(RuntimeDyld &Dyld, Memory &Mem, char const *Name,
                           uint64_t Target, uint64_t Base, bool RV32,
                           bool Tail = false) {
  auto *Code = static_cast<uint8_t *>(Dyld.getSymbolLocalAddress(Name));
  check(Code != nullptr);
  uint64_t PC = Dyld.getSymbol(Name).getAddress();
  uint32_t Hi = support::endian::read32le(Code);
  uint32_t Lo = support::endian::read32le(Code + 4);
  // Immediate patching must retain the original AUIPC/JALR register fields,
  // including rd=x0 for a tail call and its original t1 source register.
  check((Hi & 0xfff) == (Tail ? 0x317u : 0x97u));
  check((Lo & 0xfffff) == (Tail ? 0x30067u : 0x80e7u));
  uint64_t Destination = PC + static_cast<int32_t>(Hi & 0xfffff000u) +
                         SignExtend32<12>(Lo >> 20);
  if (RV32) {
    check(static_cast<uint32_t>(Destination) == static_cast<uint32_t>(Target));
    return static_cast<uint32_t>(Destination);
  }
  uint64_t Delta = Target - PC;
  if (isInt<32>(static_cast<int64_t>(Delta + 0x800))) {
    check(Destination == Target); // A reachable call must bypass its stub.
  } else {
    check(Destination != Target);
    check((Destination & 7) == 0);
    check(Destination >= Base && Destination - Base < 4096);
    // The loader's tiny section allocation includes these reserved stub bytes.
    auto *Stub = Mem.Code.at(".text") + (Destination - Base);
    check(support::endian::read32le(Stub) == 0x00000e17);
    check(support::endian::read32le(Stub + 4) == 0x010e3e03);
    check(support::endian::read32le(Stub + 8) == 0x000e0067);
    check(support::endian::read32le(Stub + 12) == 0x00000013);
    check(support::endian::read64le(Stub + 16) == Target);
  }
  return Destination;
}

static void run(char const *Object, uint64_t Base, uint64_t Target,
                bool Stubs, bool RV32, bool FarCross) {
  auto Binary = cantFail(object::ObjectFile::createObjectFile(Object));
  Memory Mem(Stubs);
  Resolver Symbols(Target);
  RuntimeDyld Dyld(Mem, Symbols);
  auto Loaded = Dyld.loadObject(*Binary.getBinary());
  check(Loaded && !Dyld.hasError());
  uint64_t Cross = FarCross ? Base + (1ull << 40) : Base + 0x10000;
  if (RV32)
    Cross = static_cast<uint32_t>(Cross);
  Dyld.mapSectionAddress(Mem.Code.at(".text"), Base);
  Dyld.mapSectionAddress(Mem.Code.at(".text.other"), Cross);
  Dyld.resolveRelocations();
  check(!Dyld.hasError());
  auto Call = verifyCall(Dyld, Mem, "call_target", Target, Base, RV32);
  auto Tail = verifyCall(Dyld, Mem, "tail_target", Target, Base, RV32, true);
  verifyCall(Dyld, Mem, "call_addend", Target + 16, Base, RV32);
  verifyCall(Dyld, Mem, "call_local", Dyld.getSymbol("local_target").getAddress(),
             Base, RV32);
  verifyCall(Dyld, Mem, "call_cross", Cross, Base, RV32);
  if (!RV32 && Call != Target && Tail != Target)
    check(Call == Tail); // The same target/addend reuses one local stub.
}

int main(int Argc, char **Argv) {
  if (Argc == 3 && std::string(Argv[2]) == "--no-stubs-far") {
    run(Argv[1], 0x4000000000, 0x6000000000, false, false, false);
    std::fputs("FAIL expected a relocation range error\n", stderr);
    return 1;
  }
  check(Argc == 3);
  for (uint64_t Base : {0x4000000000ull, 0x7000000000ull}) {
    for (int64_t Delta : {-0x10000000000ll, -0x80000802ll, -0x80000800ll,
                           -0x1000ll, 0x1000ll, 0x7ffff7fell, 0x7ffff800ll,
                           0x10000000000ll})
      run(Argv[1], Base, Base + Delta, true, false, true);
    run(Argv[1], Base, Base + 0x1000, false, false, false);
  }
  // AUIPC/JALR on RV32 wraps at XLEN: even a displacement whose unwrapped
  // value is outside the RV64 rounded range must keep its original semantics.
  for (uint64_t Target : {0x1000ull, 0x7ffffffeull, 0x80000000ull, 0xfffffff0ull})
    run(Argv[2], 0x1000, Target, false, true, false);
  std::printf("PASS RuntimeDyld RISC-V calls: %u checks\n", Checks);
}
