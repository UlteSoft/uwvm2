// HOST registry/configuration DATA only; no metadata graph or memory read is fabricated.
#include <uwvm2/uwvm/debugger/source_pretty_registry.h>
#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/console_aliases.h>
#include <uwvm2/uwvm/debugger/console_completion.h>
#include <uwvm2/uwvm/debugger/console_help.h>
#include <fast_io.h>
#include <initializer_list>
#include <string>
#include <string_view>
namespace reg = ::uwvm2::uwvm::debugger::source_pretty_registry;
namespace dbg = ::uwvm2::uwvm::debugger;
static void require(bool okay, ::fast_io::string_view why)
{ if(!okay) { ::fast_io::io::perrln("source pretty registry: ", why); ::fast_io::fast_terminate(); } }
static reg::request parse(::std::string_view line)
{ auto value{reg::parse_host_command(line)}; require(value.operation != reg::action::invalid,"test command is admitted"); return value; }
int main()
{
    auto sequence{parse("pretty-printer sequence buffer \"my::Buffer<*, int>\" storage.data count capacity")};
    require(sequence.value.type_pattern == "my::Buffer<*, int>" && sequence.value.data_path == "storage.data" &&
        sequence.value.kind == reg::profile_kind::sequence, "quoted type pattern and dotted paths remain owned data");
    require(reg::matches("my::Buffer<*, int>","my::Buffer<long, int>") && reg::matches("a?c","abc") &&
        reg::matches("a*c","abbbc") && !reg::matches("a?c","abbc") && !reg::matches("Buffer<*>","outer::Buffer<int>"),
        "globs match full bounded type spellings rather than regexes or suffixes");
    require(reg::matches("*a","*ba") && reg::matches("a*b","a*xb") && reg::matches("std::*<int*>","std::Buffer<int*>"),
        "wildcards remain wildcards when the emitted type itself contains pointer asterisks");
    require(reg::matches("a[0]","a[0]") && !reg::matches("a[0]","a0") && !reg::matches(::std::string(513u,'*'),"A") &&
        !reg::matches("*",::std::string(4097u,'A')), "pattern/type limits and literal regex punctuation");
    dbg::console_print_preferences prefs{};
    require(prefs.configure(sequence) == reg::error::none && prefs.source_printers && reg::valid(*prefs.source_printers), "publish first immutable snapshot");
    auto old{prefs.source_printers};
    auto command{dbg::parse_console_command("print buffer")}; prefs.apply(command);
    require(command.source_printers == old, "source value retains its complete registry snapshot");
    require(prefs.configure(parse("pretty-printer disable buffer")) == reg::error::none && prefs.source_printers != old &&
        !prefs.source_printers->profiles[0u].enabled && old->profiles[0u].enabled, "policy changes do not mutate prior query snapshots");
    auto locals{dbg::parse_console_command("locals source 1")}; locals.source_printers = old; prefs.apply(locals);
    require(!locals.source_printers, "only source value queries receive a printer registry");
    require(prefs.configure(parse("pretty-printer enable buffer")) == reg::error::none, "re-enable named profile");
    require(prefs.configure(parse("pretty-printer disable builtin")) == reg::error::none && !prefs.source_printers->builtins_enabled &&
        prefs.source_printers->profiles[0u].enabled, "builtins and custom profiles have independent policy");
    require(prefs.configure(parse("pretty-printer delete builtin")) == reg::error::reserved_name && !prefs.source_printers->builtins_enabled,
        "reserved builtin group cannot be removed");
    auto retained{prefs.source_printers};
    require(prefs.configure(parse("pretty-printer delete missing")) == reg::error::not_found && prefs.source_printers == retained,
        "failed mutation retains the old snapshot");
    require(prefs.configure(parse("pretty-printer list")) == reg::error::none && prefs.source_printers == retained, "listing does not publish a mutation");
    require(prefs.configure(parse("pretty-printer sequence any \"*\" p n")) == reg::error::none &&
        prefs.configure(parse("pretty-printer pointer exact \"my::Buffer<long, int>\" owner.ptr")) == reg::error::none, "register overlapping profiles");
    auto selected{reg::find(*prefs.source_printers,"my::Buffer<long, int>")};
    require(selected != nullptr && selected->name == "exact", "exact type profile wins before earlier globs");
    require(prefs.configure(parse("pretty-printer disable exact")) == reg::error::none, "disable exact match");
    selected = reg::find(*prefs.source_printers,"my::Buffer<long, int>");
    require(selected != nullptr && selected->name == "buffer", "glob priority follows stable registration order");
    require(prefs.configure(parse("pretty-printer string buffer \"my::Text\" data length")) == reg::error::none &&
        prefs.source_printers->profiles.size() == 3u && prefs.source_printers->profiles[0u].kind == reg::profile_kind::string,
        "same name replaces in place without consuming another profile slot");
    require(prefs.configure(parse("pretty-printer optional maybe 'Optional With Spaces' engaged payload.value")) == reg::error::none,
        "single quoted type and optional field profile");
    auto listing{reg::describe(prefs.source_printers.get())};
    require(listing.find("builtin disabled") != listing.npos && listing.find("Optional With Spaces") != listing.npos &&
        listing.find("engaged=engaged value=payload.value") != listing.npos, "listing identifies policy and exact profile fields");
    for(auto text : ::std::initializer_list<::std::string_view>{"pretty-printer sequence x Type data len extra extra",
        "pretty-printer pointer builtin Type ptr", "pretty-printer optional x Type ready value()", "pretty-printer pointer x Type *ptr",
        "pretty-printer sequence x Type data[0] len", "pretty-printer sequence x Type data .len", "pretty-printer pointer x Type ptr..next",
        "pretty-printer pointer x Type ptr.", "pretty-printer pointer x \"unterminated ptr", "pretty-printer list extra",
        "pretty-printer enable", "pretty-printer pointer 'bad name' Type ptr", "pretty-printer pointer x Type ptr;continue",
        "pretty-printer pointer x \"bad\\q\" ptr", "pretty-printer pointer x Type \x1b"})
    { require(reg::parse_host_command(text).operation == reg::action::invalid, "invalid grammar cannot become a script or field selector"); }
    auto large{::std::string{"pretty-printer sequence large \""}+::std::string(512u,'T')+"\" "+::std::string(128u,'d')+" "+::std::string(128u,'n')};
    command = dbg::parse_console_command(::fast_io::string_view{large.data(),large.size()});
    require(command.kind == dbg::console_command_kind::source_printer_config && command.payload_size == 0u &&
        command.source_printer_request.value.type_pattern.size() == 512u, "bounded configuration has its own long-line budget and no runtime payload");
    large.insert(large.find("\" "),1u,'T');
    require(dbg::parse_console_command(::fast_io::string_view{large.data(),large.size()}).kind == dbg::console_command_kind::invalid, "pattern overflow refuses complete command");
    reg::registry full{};
    for(::std::size_t i{};i != reg::maximum_profiles;++i)
    { auto value{sequence.value}; value.name = "profile_"+::std::to_string(i); full.profiles.push_back(::std::move(value)); }
    ::std::shared_ptr<reg::registry const> result{::std::make_shared<reg::registry>(full)}; old = result;
    require(reg::apply_request(result.get(),parse("pretty-printer pointer extra Type ptr"),result) == reg::error::capacity && result == old,
        "65th named profile is rejected without publishing a prefix");
    require(reg::apply_request(result.get(),parse("pretty-printer pointer profile_0 Type ptr"),result) == reg::error::none && result->profiles.size() == 64u,
        "replacement remains possible at capacity");
    auto corrupt{*result}; corrupt.profiles[1u].name = corrupt.profiles[0u].name;
    require(!reg::valid(corrupt) && reg::find(corrupt,"Type") == nullptr, "duplicate invalid registry has no matching profile");
    command = dbg::console_aliases::parse("pretty-p list",0u,0u).command;
    require(command.kind == dbg::console_command_kind::source_printer_config && !dbg::console_aliases::repeatable(command),
        "host registry root supports unique prefixes without implicit repeat");
    ::fast_io::string_view input{"pretty-printer disable buf"}; auto completion{dbg::console_completion::context(input,input.size())};
    require(completion.type == dbg::console_completion::kind::printer && completion.root == "disable" && completion.prefix == "buf",
        "registry-name completion requests only copied host configuration");
    require(dbg::console_help::menu_request("pretty-printer") != nullptr &&
        dbg::console_help::find(dbg::console_help::canonical("pretty-printer optional")) != nullptr, "registry commands have menu/help routes");
    reg::registry long_profiles{};
    for(::std::size_t i{};i != reg::maximum_profiles;++i)
    {
        auto value{sequence.value};value.name = "profile_"+::std::to_string(i)+::std::string(110u,'n');
        value.type_pattern.assign(512u,'"');value.data_path.assign(128u,'d');value.size_path.assign(128u,'s');value.capacity_path.assign(128u,'c');
        long_profiles.profiles.push_back(::std::move(value));
    }
    listing = reg::describe(&long_profiles);
    require(listing.size() <= reg::maximum_description_bytes && listing.find("total=64 truncated=true\n") != listing.npos &&
        listing.find("pretty-printers shown=0 ") == listing.npos, "64 long quoted profiles produce bounded complete rows with explicit shown/total");
    require(listing.find("capacity="+::std::string(128u,'c')+"\npretty-printers shown=") != listing.npos,
        "truncation never splits a field path, quoted pattern or profile row");
    auto acknowledgement{reg::acknowledge(sequence)};
    require(acknowledgement == "Pretty-printer buffer registered.\n" && acknowledgement.size() < 256u,
        "successful mutation acknowledgement is independent from registry listing size");
    require(reg::describe(nullptr).find("shown=0 total=0 truncated=false") != ::std::string::npos,
        "empty registry also describes its complete finite result");
    ::fast_io::io::println("source pretty registry checks passed");
}
