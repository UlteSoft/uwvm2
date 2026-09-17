// Load/relocate a foreign object without executing foreign instructions in the
// compiler process. The guest runner later maps exactly these bytes/addresses
// and registers exactly the returned EH-frame range with its native unwinder.
// This tests the LLVM loader/code/CFI boundary, not foreign ROS/LLVM bootstrap,
// signal handlers, instruction fallback or signed-cache validation.
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/RuntimeDyld.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

struct manager final : llvm::RTDyldMemoryManager
{
    struct section
    {
        std::unique_ptr<std::uint8_t[]> storage;
        std::uint8_t* local;
        std::uint64_t address;
        std::size_t size;
        bool code;
        std::string name;
    };
    struct frame { std::uint64_t address; std::size_t size; };
    std::vector<section> sections;
    std::vector<frame> frames;
    std::uint64_t capture;
    std::uint64_t state;
    manager(std::uint64_t address, std::uint64_t state_address) : capture(address), state(state_address) {}
    std::uint8_t* allocate(std::uintptr_t size, unsigned alignment, bool code, llvm::StringRef name)
    {
        // Fresh 64 KiB-separated sections avoid target-page sharing and make
        // stub/EH-frame address ranges explicit. Reject overlarge fixtures.
        if(size > 32768 || alignment > 65536 || sections.size() >= 32) { std::abort(); }
        auto const align{std::max(alignment, 1u)};
        auto storage{std::make_unique<std::uint8_t[]>(size + align + 16)};
        auto value{reinterpret_cast<std::uintptr_t>(storage.get())};
        auto local{reinterpret_cast<std::uint8_t*>((value + align - 1) & ~(std::uintptr_t(align) - 1))};
        auto address{std::uint64_t{0x20000000} + sections.size() * 65536};
        sections.push_back({std::move(storage), local, address, size, code, name.str()});
        return local;
    }
    std::uint8_t* allocateCodeSection(std::uintptr_t size, unsigned alignment, unsigned, llvm::StringRef name) override
    { return allocate(size, alignment, true, name); }
    std::uint8_t* allocateDataSection(std::uintptr_t size, unsigned alignment, unsigned, llvm::StringRef name, bool) override
    { return allocate(size, alignment, false, name); }
    bool finalizeMemory(std::string*) override { return false; }
    llvm::JITSymbol findSymbol(std::string const& name) override
    {
        if(name == "uwvm_host_capture") { return {capture, llvm::JITSymbolFlags::Exported}; }
        if(name == "uwvm_remote_state") { return {state, llvm::JITSymbolFlags::Exported}; }
        llvm::errs() << "Unexpected external symbol: " << name << '\n';
        std::abort();
    }
    void registerEHFrames(std::uint8_t*, std::uint64_t address, std::size_t size) override
    { frames.push_back({address, size}); }
    void deregisterEHFrames() override {}
};

int main(int argc, char** argv)
{
    if(argc != 5) { return 2; }
    auto buffer{llvm::MemoryBuffer::getFile(argv[1])};
    if(!buffer) { return 3; }
    auto object{llvm::object::ObjectFile::createObjectFile((*buffer)->getMemBufferRef())};
    if(!object) { llvm::logAllUnhandledErrors(object.takeError(), llvm::errs()); return 4; }
    manager memory{std::strtoull(argv[2], nullptr, 0), std::strtoull(argv[3], nullptr, 0)};
    llvm::RuntimeDyld loader{memory, memory};
    auto loaded{loader.loadObject(**object)};
    if(!loaded || loader.hasError()) { llvm::errs() << loader.getErrorString(); return 5; }
    for(auto const& section: memory.sections) { loader.mapSectionAddress(section.local, section.address); }
    loader.resolveRelocations();
    if(loader.hasError()) { llvm::errs() << loader.getErrorString(); return 6; }
    loader.registerEHFrames();
    if(memory.frames.empty()) { llvm::errs() << "No dynamic EH frames\n"; return 7; }
    auto entry{loader.getSymbol("uwvm_remote_entry")};
    if(!entry) { return 8; }
    std::ofstream manifest{std::string{argv[4]} + "/manifest.txt"};
    manifest << "E " << std::hex << entry.getAddress() << '\n';
    for(std::size_t index{}; index != memory.sections.size(); ++index)
    {
        auto const& section{memory.sections[index]};
        std::string filename{"section-" + std::to_string(index) + ".bin"};
        std::ofstream file{std::string{argv[4]} + '/' + filename, std::ios::binary};
        file.write(reinterpret_cast<char const*>(section.local), section.size);
        if(!file) { return 9; }
        manifest << "S " << std::hex << section.address << ' ' << section.size << ' '
                 << section.code << ' ' << filename << '\n';
    }
    for(auto const& frame: memory.frames)
    { manifest << "F " << std::hex << frame.address << ' ' << frame.size << '\n'; }
    return manifest ? 0 : 10;
}
