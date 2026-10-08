#include <cstddef>
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/console_line_editor.h>

// Genuine bounded HOST editor component. No console/VM/stop/register DTO,
// runtime permission, allocation, mock history, or synthetic native fixture.
namespace editing = ::uwvm2::uwvm::debugger::console_editing;
using editing::action;

static bool state(editing::editor const& value, ::fast_io::string_view text,
                  ::std::size_t cursor) noexcept
{
    return value.line().view() == text && value.cursor() == cursor &&
           cursor <= text.size() && text.size() <= editing::maximum_bytes;
}

static bool type(editing::editor& value, ::fast_io::string_view text) noexcept
{
    for(auto byte : text)
    {
        if(value.feed(static_cast<unsigned char>(byte)) != action::changed) { return false; }
    }
    return true;
}

static action meta(editing::editor& value, int byte) noexcept
{
    if(value.feed(27) != action::unchanged) { return action::invalid; }
    return value.feed(byte);
}

static bool undo_controls_and_owned_snapshots() noexcept
{
    editing::editor value{};
    value.begin();
    if(!type(value, "abc") || !state(value, "abc", 3u)) { return false; }
    // Three consecutive insertions are one incremental edit, not three bytes.
    if(value.feed(31) != action::changed || !state(value, "", 0u) ||
       value.feed(31) != action::unchanged || !state(value, "", 0u)) { return false; }
    if(!type(value, "abcd") || value.feed(1) != action::changed ||
       value.feed(6) != action::changed || value.feed(6) != action::changed ||
       !state(value, "abcd", 2u)) { return false; }
    if(value.feed(editing::key_delete) != action::changed || !state(value, "abd", 2u) ||
       value.feed(31) != action::changed || !state(value, "abcd", 2u)) { return false; }
    if(value.feed(8) != action::changed || !state(value, "acd", 1u) ||
       value.feed(24) != action::unchanged || !state(value, "acd", 1u) ||
       value.feed(21) != action::changed || !state(value, "abcd", 2u)) { return false; }
    // Undo restores a whole owned snapshot, including the pre-kill cursor.
    if(value.feed(11) != action::changed || !state(value, "ab", 2u) ||
       value.feed(31) != action::changed || !state(value, "abcd", 2u)) { return false; }
    // The kill-ring remains independently owned after undo.
    if(value.feed(5) != action::changed || value.feed(25) != action::changed ||
       !state(value, "abcdcd", 6u) || value.feed(31) != action::changed ||
       !state(value, "abcd", 4u)) { return false; }
    value.begin();
    return value.feed(31) == action::unchanged && state(value, "", 0u);
}

static bool undo_yank_pop_and_completion() noexcept
{
    editing::editor value{};
    value.begin();
    if(!type(value, "first") || value.feed(21) != action::changed) { return false; }
    value.begin();
    if(!type(value, "second") || value.feed(21) != action::changed) { return false; }
    value.begin();
    if(value.feed(25) != action::changed || !state(value, "second", 6u) ||
       meta(value, 'y') != action::changed || !state(value, "first", 5u) ||
       value.feed(31) != action::changed || !state(value, "second", 6u) ||
       value.feed(31) != action::changed || !state(value, "", 0u)) { return false; }
    value.begin();
    if(!type(value, "sta") || value.feed(9) != action::changed ||
       !state(value, "status", 6u) || value.feed(31) != action::changed ||
       !state(value, "sta", 3u) || value.feed(31) != action::changed ||
       !state(value, "", 0u)) { return false; }
    value.begin();
    if(!type(value, "break") || value.feed(9) != action::changed ||
       !state(value, "break ", 6u) || value.feed(31) != action::changed ||
       !state(value, "break", 5u)) { return false; }
    value.begin();
    if(!type(value, "info ") || value.feed(9) != action::candidates ||
       !state(value, "info ", 5u) || value.feed(31) != action::changed ||
       !state(value, "", 0u)) { return false; }
    value.begin();
    if(!type(value, "staX") || value.feed(1) != action::changed ||
       value.feed(9) != action::unchanged || !state(value, "staX", 0u)) { return false; }
    return value.feed(31) == action::changed && state(value, "", 0u);
}

static bool undo_fixed_capacity() noexcept
{
    editing::editor value{};
    value.begin();
    char expected[40]{};
    // Real cursor commands break insertion grouping; exactly forty groups.
    for(::std::size_t i{}; i != 40u; ++i)
    {
        expected[i] = 'a';
        if(value.feed('a') != action::changed || value.feed(1) != action::changed ||
           !state(value, ::fast_io::string_view{expected, i + 1u}, 0u)) { return false; }
    }
    // The newest thirty-two complete snapshots survive, never a partial text.
    for(::std::size_t i{}; i != 32u; ++i)
    {
        if(value.feed(31) != action::changed || !state(value, ::fast_io::string_view{expected, 39u - i}, 0u)) { return false; }
    }
    return value.feed(31) == action::unchanged && state(value, ::fast_io::string_view{expected, 8u}, 0u);
}

static bool whole_line_discard_and_prefix_refusal() noexcept
{
    editing::editor value{};
    value.begin();
    char full[editing::maximum_bytes]{};
    for(::std::size_t i{}; i != editing::maximum_bytes; ++i)
    {
        full[i] = 'x';
        if(value.feed('x') != action::changed) { return false; }
    }
    if(value.feed('z') != action::unchanged || !state(value, ::fast_io::string_view{full, sizeof(full)}, sizeof(full)) ||
       value.feed(31) != action::unchanged || value.feed(24) != action::unchanged ||
       value.feed(21) != action::unchanged || !state(value, ::fast_io::string_view{full, sizeof(full)}, sizeof(full)) ||
       value.feed('\n') != action::oversized || value.feed(-1) != action::oversized) { return false; }
    value.begin();
    if(!type(value, "whole") || value.feed(24) != action::unchanged ||
       !state(value, "whole", 5u) || value.feed(-1) != action::oversized) { return false; }
    value.begin();
    if(!type(value, "whole") || value.feed(24) != action::unchanged ||
       value.feed('\n') != action::oversized || !state(value, "whole", 5u)) { return false; }
    value.begin();
    if(!type(value, "whole") || value.feed(24) != action::unchanged ||
       value.feed('x') != action::unchanged || value.feed('!') != action::unchanged ||
       value.feed(31) != action::unchanged || !state(value, "whole", 5u) ||
       value.feed('\n') != action::oversized) { return false; }
    value.begin();
    return value.feed(31) == action::unchanged && state(value, "", 0u);
}

static bool ascii_word_commands() noexcept
{
    editing::editor value{};
    value.begin();
    if(!type(value, "one-two_33  four") || value.feed(1) != action::changed) { return false; }
    constexpr ::std::size_t forward[]{3u, 7u, 10u, 16u};
    for(auto cursor : forward)
    { if(meta(value, 'f') != action::changed || !state(value, "one-two_33  four", cursor)) { return false; } }
    constexpr ::std::size_t backward[]{12u, 8u, 4u, 0u};
    for(auto cursor : backward)
    { if(meta(value, 'b') != action::changed || !state(value, "one-two_33  four", cursor)) { return false; } }
    if(meta(value, 'd') != action::changed || !state(value, "-two_33  four", 0u) ||
       value.feed(25) != action::changed || !state(value, "one-two_33  four", 3u) ||
       value.feed(31) != action::changed || !state(value, "-two_33  four", 0u) ||
       value.feed(31) != action::changed || !state(value, "one-two_33  four", 0u)) { return false; }
    value.begin();
    if(!type(value, "one-two_33") || meta(value, 127) != action::changed ||
       !state(value, "one-two_", 8u) || value.feed(31) != action::changed ||
       !state(value, "one-two_33", 10u)) { return false; }
    // CtrlW uses whitespace boundaries, unlike ESC DEL's letters/digits.
    return value.feed(23) == action::changed && state(value, "", 0u) &&
           value.feed(31) == action::changed && state(value, "one-two_33", 10u);
}

static bool utf8_whole_character_commands() noexcept
{
    constexpr ::fast_io::string_view original{"ab\xF0\x9F\x99\x82-cd"};
    editing::editor value{};
    value.begin();
    if(!type(value, original)) { return false; }
    constexpr ::std::size_t backward[]{8u, 7u, 6u, 2u, 1u, 0u};
    for(auto cursor : backward)
    { if(value.feed(2) != action::changed || !state(value, original, cursor)) { return false; } }
    constexpr ::std::size_t forward[]{1u, 2u, 6u, 7u, 8u, 9u};
    for(auto cursor : forward)
    { if(value.feed(6) != action::changed || !state(value, original, cursor)) { return false; } }
    if(value.feed(1) != action::changed || meta(value, 'f') != action::changed ||
       !state(value, original, 2u) || meta(value, 'f') != action::changed ||
       !state(value, original, 9u) || meta(value, 'b') != action::changed ||
       !state(value, original, 7u) || meta(value, 'b') != action::changed ||
       !state(value, original, 0u)) { return false; }
    if(value.feed(6) != action::changed || value.feed(6) != action::changed ||
       meta(value, 'd') != action::changed || !state(value, "ab", 2u) ||
       value.feed(25) != action::changed || !state(value, original, 9u) ||
       value.feed(31) != action::changed || !state(value, "ab", 2u) ||
       value.feed(31) != action::changed || !state(value, original, 2u)) { return false; }
    if(value.feed(4) != action::changed || !state(value, "ab-cd", 2u) ||
       value.feed(31) != action::changed || !state(value, original, 2u) ||
       value.feed(6) != action::changed || !state(value, original, 6u) ||
       value.feed(8) != action::changed || !state(value, "ab-cd", 2u) ||
       value.feed(31) != action::changed || !state(value, original, 6u)) { return false; }
    return meta(value, 127) == action::changed && state(value, "-cd", 0u) &&
           value.feed(31) == action::changed && state(value, original, 6u);
}

static void history(editing::editor& value) noexcept
{
    value.remember("one needle", false);
    value.remember("noise only", false);
    value.remember("two needle", false);
    value.remember("three needle", false);
}

static bool search_direction_and_draft() noexcept
{
    editing::editor value{};
    history(value); value.begin();
    if(!type(value, "draft") || value.feed(1) != action::changed ||
       value.feed(6) != action::changed || value.feed(6) != action::changed) { return false; }
    if(value.feed(18) != action::changed || !type(value, "needle") ||
       !value.searching() || value.searching_forward() || !value.search_matched() ||
       value.search_query() != "needle" || !state(value, "three needle", 12u)) { return false; }
    if(value.feed(18) != action::changed || !state(value, "two needle", 10u)) { return false; }
    if(!type(value, "!") || value.search_matched() ||
       value.feed(127) != action::changed || value.search_query() != "needle" ||
       !value.search_matched() || !state(value, "two needle", 10u)) { return false; }
    if(value.feed(19) != action::changed || !value.searching_forward() ||
       !state(value, "three needle", 12u) || value.feed(18) != action::changed ||
       value.searching_forward() || !state(value, "two needle", 10u) ||
       value.feed(18) != action::changed || !state(value, "one needle", 10u) ||
       value.feed(19) != action::changed || !value.searching_forward() ||
       !state(value, "two needle", 10u)) { return false; }
    // Query changes stay anchored at this actual match after the direction
    // change, rather than restarting from the old draft's history-end index.
    if(!type(value, "!") || value.search_matched() ||
       value.feed(127) != action::changed || value.search_query() != "needle" ||
       !value.search_matched() || !value.searching_forward() ||
       !state(value, "two needle", 10u)) { return false; }
    return value.feed(7) == action::changed && !value.searching() && state(value, "draft", 2u);
}

static bool remembered_query_and_owned_history() noexcept
{
    editing::editor value{};
    history(value); value.begin();
    if(value.feed(18) != action::changed || !type(value, "needle") ||
       value.feed(7) != action::changed || !state(value, "", 0u)) { return false; }
    value.begin();
    if(value.feed(18) != action::changed || !value.search_query().empty() ||
       value.feed(18) != action::changed || value.search_query() != "needle" ||
       !value.search_matched() || value.searching_forward() ||
       !state(value, "three needle", 12u) || value.feed(18) != action::changed ||
       !state(value, "two needle", 10u) || value.feed(7) != action::changed) { return false; }
    value.begin();
    // A forward search starts from a real selected oldest history item.
    for(unsigned i{}; i != 4u; ++i)
    { if(value.feed(16) != action::changed) { return false; } }
    if(!state(value, "one needle", 10u) || value.feed(19) != action::changed ||
       !value.search_query().empty() || value.feed(19) != action::changed ||
       !value.searching_forward() || value.search_query() != "needle" ||
       !value.search_matched() || !state(value, "one needle", 10u) ||
       value.feed(19) != action::changed || !state(value, "two needle", 10u)) { return false; }
    if(value.feed(7) != action::changed || !state(value, "one needle", 10u)) { return false; }
    // Enter acceptance must persist the newly typed query, replacing needle.
    if(value.feed(18) != action::changed || !type(value, "one") ||
       value.feed('\n') != action::complete || value.searching()) { return false; }
    value.begin();
    if(value.feed(18) != action::changed || value.feed(18) != action::changed ||
       value.search_query() != "one" || !value.search_matched() ||
       !state(value, "one needle", 10u)) { return false; }
    value.begin();
    if(value.feed(18) != action::changed || !type(value, "two") ||
       !value.search_matched() || !state(value, "two needle", 10u) ||
       value.feed(-1) != action::complete || value.searching()) { return false; }
    value.begin();
    if(value.feed(18) != action::changed || value.feed(18) != action::changed ||
       value.search_query() != "two" || !value.search_matched() ||
       !state(value, "two needle", 10u)) { return false; }
    editing::editor owned{};
    char source[]{'o', 'w', 'n', 'e', 'd', '!'};
    owned.remember(::fast_io::string_view{source, sizeof(source)}, false);
    for(auto& byte : source) { byte = 'x'; }
    owned.begin();
    return owned.feed(18) == action::changed && type(owned, "owned") &&
           owned.search_matched() && state(owned, "owned!", 6u) &&
           owned.search_query() == "owned";
}

static bool failed_search_and_query_capacity() noexcept
{
    editing::editor value{};
    history(value); value.begin();
    if(!type(value, "draft") || value.feed(1) != action::changed || value.feed(6) != action::changed ||
       value.feed(18) != action::changed || !type(value, "needle") || !type(value, "missing") ||
       !value.searching() || value.search_matched() || value.feed('\n') != action::unchanged ||
       !value.searching() || value.feed(-1) != action::oversized ||
       value.feed(7) != action::changed || !state(value, "draft", 1u)) { return false; }
    value.begin();
    if(!type(value, "safe") || value.feed(19) != action::changed) { return false; }
    for(::std::size_t i{}; i != editing::maximum_bytes; ++i)
    { if(value.feed('q') != action::changed) { return false; } }
    if(value.search_query().size() != editing::maximum_bytes ||
       value.feed('q') != action::changed || value.search_matched() ||
       value.feed('\n') != action::unchanged || value.feed(-1) != action::oversized ||
       value.feed(7) != action::changed || !state(value, "safe", 4u)) { return false; }
    editing::editor utf{};
    utf.remember("x\xF0\x9F\x99\x82x", false); utf.begin();
    return utf.feed(18) == action::changed && type(utf, "\xF0\x9F\x99\x82") &&
           utf.search_query().size() == 4u && utf.feed(127) == action::changed &&
           utf.search_query().empty() && utf.feed(7) == action::changed && state(utf, "", 0u);
}

int main()
{
    // Stable group codes avoid NDEBUG-dependent assert removal.
    if(!undo_controls_and_owned_snapshots()) { return 1; }
    if(!undo_yank_pop_and_completion()) { return 2; }
    if(!undo_fixed_capacity()) { return 3; }
    if(!whole_line_discard_and_prefix_refusal()) { return 4; }
    if(!ascii_word_commands()) { return 5; }
    if(!utf8_whole_character_commands()) { return 6; }
    if(!search_direction_and_draft()) { return 7; }
    if(!remembered_query_and_owned_history()) { return 8; }
    if(!failed_search_and_query_capacity()) { return 9; }
    return 0;
}
