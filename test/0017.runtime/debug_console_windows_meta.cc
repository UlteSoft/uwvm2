#include <windows.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <uwvm2/uwvm/debugger/console_keyboard.h>
#include <uwvm2/utils/control/win32_abi.h>

namespace kb = ::uwvm2::uwvm::debugger::console_keyboard;
namespace edit = ::uwvm2::uwvm::debugger::console_editing;
namespace abi = ::uwvm2::utils::control::win32_abi;

static_assert(LEFT_ALT_PRESSED == 0x0002u && RIGHT_ALT_PRESSED == 0x0001u);
static_assert(LEFT_CTRL_PRESSED == 0x0008u && RIGHT_CTRL_PRESSED == 0x0004u);
static_assert(SHIFT_PRESSED == 0x0010u && CAPSLOCK_ON == 0x0080u);

static bool inject(::HANDLE console, char16_t ch, ::DWORD controls,
                   ::WORD repeats = 1u, ::WORD virtual_key = 0u) noexcept
{
    ::INPUT_RECORD record{};
    record.EventType = KEY_EVENT;
    record.Event.KeyEvent.bKeyDown = TRUE;
    record.Event.KeyEvent.wRepeatCount = repeats;
    record.Event.KeyEvent.wVirtualKeyCode = virtual_key != 0u ? virtual_key : ch == u'y' ? 'Y' : 0u;
    record.Event.KeyEvent.uChar.UnicodeChar = static_cast<::WCHAR>(ch);
    record.Event.KeyEvent.dwControlKeyState = controls;
    ::DWORD written{};
    return ::fast_io::noexcept_call(::WriteConsoleInputW, console, &record, 1u, &written) != 0 && written == 1u;
}

static bool text(edit::editor const& editor, ::fast_io::string_view expected) noexcept
{ return editor.line().view() == expected; }

static void insert(edit::editor& editor, ::fast_io::string_view value) noexcept
{ for(auto const ch : value) { static_cast<void>(editor.feed(static_cast<unsigned char>(ch))); } }

static bool expect_meta(kb::native_session& session, ::HANDLE console,
                        char16_t ch, ::WORD virtual_key, int second) noexcept
{
    return inject(console, ch, LEFT_ALT_PRESSED, 1u, virtual_key) &&
        session.read_byte() == 27 && session.read_byte() == second;
}

int main()
{
    // The job-owned child starts with STARTF_USESTDHANDLES and private pipes.
    // Microsoft says AllocConsole need not replace redirected std handles.
    // Save them, then install handles explicitly opened on our fresh console.
    auto const inherited_input{::fast_io::win32::GetStdHandle(static_cast<::std::uint_least32_t>(STD_INPUT_HANDLE))};
    auto const inherited_output{::fast_io::win32::GetStdHandle(static_cast<::std::uint_least32_t>(STD_OUTPUT_HANDLE))};
    static_cast<void>(::fast_io::noexcept_call(::FreeConsole));
    if(::fast_io::noexcept_call(::AllocConsole) == 0) { return 2; }
    auto const input_raw{abi::uwvm_CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0u, nullptr)};
    if(input_raw == INVALID_HANDLE_VALUE)
    { static_cast<void>(::fast_io::noexcept_call(::FreeConsole)); return 3; }
    ::fast_io::native_file input_owner{input_raw};
    auto const output_raw{abi::uwvm_CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0u, nullptr)};
    if(output_raw == INVALID_HANDLE_VALUE)
    { static_cast<void>(::fast_io::noexcept_call(::FreeConsole)); return 3; }
    ::fast_io::native_file output_owner{output_raw};
    auto const input{static_cast<::HANDLE>(input_owner.native_handle())};
    auto const output{static_cast<::HANDLE>(output_owner.native_handle())};
    struct restore_standard_handles final
    {
        ::HANDLE input{}, output{};
        ~restore_standard_handles() noexcept
        {
            // Restore inherited pipes before detaching/closing this console.
            // Both calls run even if one fails; no failed cleanup can PASS.
            auto const in_ok{::fast_io::noexcept_call(::SetStdHandle, STD_INPUT_HANDLE, input)};
            auto const out_ok{::fast_io::noexcept_call(::SetStdHandle, STD_OUTPUT_HANDLE, output)};
            auto const free_ok{::fast_io::noexcept_call(::FreeConsole)};
            if(in_ok == 0 || out_ok == 0 || free_ok == 0)
            { ::fast_io::fast_terminate(); }
        }
    } restore{inherited_input, inherited_output};
    if(::fast_io::noexcept_call(::SetStdHandle, STD_INPUT_HANDLE, input) == 0 ||
       ::fast_io::noexcept_call(::SetStdHandle, STD_OUTPUT_HANDLE, output) == 0) { return 4; }
    ::DWORD input_before{}, output_before{};
    if(::fast_io::noexcept_call(::GetConsoleMode, input, &input_before) == 0 ||
       ::fast_io::noexcept_call(::GetConsoleMode, output, &output_before) == 0 ||
       ::fast_io::noexcept_call(::FlushConsoleInputBuffer, input) == 0) { return 4; }
    int failure{};
    {
        kb::native_session session{};
        if(!session.ready() || !session.interactive()) { failure = 5; }
        else
        {
            edit::editor editor{};
            editor.begin(); insert(editor, "first"); static_cast<void>(editor.feed(21));
            editor.begin(); insert(editor, "second"); static_cast<void>(editor.feed(21));
            editor.begin(); static_cast<void>(editor.feed(25));
            if(!text(editor, "second")) { failure = 6; }
            // One WriteConsoleInputW KEY_EVENT with packed repeat=3 yields one
            // editor action. Three separate records would yield three actions.
            if(failure == 0 && !inject(input, u'y', LEFT_ALT_PRESSED, 3u)) { failure = 7; }
            if(failure == 0)
            {
                auto const first{session.read_byte()}, second{session.read_byte()};
                if(first != 27 || second != 'y') { failure = 8; }
                else
                {
                    static_cast<void>(editor.feed(first)); static_cast<void>(editor.feed(second));
                    if(!text(editor, "first")) { failure = 9; }
                }
            }
            if(failure == 0 && (!inject(input, u'z', 0u) || session.read_byte() != 'z')) { failure = 10; }
            // Right Alt + left Ctrl is AltGr on common layouts. Never mint
            // editor ESC,y from that translated Unicode character.
            if(failure == 0 && (!inject(input, u'y', RIGHT_ALT_PRESSED | LEFT_CTRL_PRESSED) ||
                                session.read_byte() != 'y')) { failure = 11; }
            if(failure == 0 && (!inject(input, u'y', 0u) || session.read_byte() != 'y')) { failure = 12; }
            // A pending high surrogate remains governed by the old UTF-16
            // conversion path. This branch has no modifier special case.
            if(failure == 0 && (!inject(input, static_cast<char16_t>(0xd83du), 0u) ||
                                !inject(input, static_cast<char16_t>(0xde42u), 0u))) { failure = 13; }
            if(failure == 0)
            {
                constexpr unsigned char expected[]{0xf0u, 0x9fu, 0x99u, 0x82u};
                for(auto byte : expected) { if(session.read_byte() != byte) { failure = 14; break; } }
            }
            // This calls the already installed callback directly to test only
            // queue cancellation. It is NOT evidence of real console Ctrl+C.
            if(failure == 0 && !inject(input, u'y', LEFT_ALT_PRESSED)) { failure = 15; }
            if(failure == 0 && session.read_byte() != 27) { failure = 16; }
            if(failure == 0 && kb::details::interrupt_handler(CTRL_C_EVENT) == FALSE) { failure = 17; }
            if(failure == 0 && session.read_byte() != kb::interrupted) { failure = 18; }
            if(failure == 0 && (!inject(input, u'z', 0u) || session.read_byte() != 'z')) { failure = 19; }
            // These are actual console records consumed by native_session.
            // A zero UnicodeChar with the matching letter VK is a separate
            // Windows representation of an Alt-modified letter key.
            if(failure == 0 && !expect_meta(session, input, u'f', 'F', 'f')) { failure = 21; }
            if(failure == 0 && !expect_meta(session, input, 0u, 'B', 'b')) { failure = 22; }
            if(failure == 0 && !expect_meta(session, input, u'd', 'D', 'd')) { failure = 23; }
            if(failure == 0 && !expect_meta(session, input, 0u, 'Y', 'y')) { failure = 24; }
            if(failure == 0 && !expect_meta(session, input, u'\b', VK_BACK, 127)) { failure = 25; }
            if(failure == 0 && !expect_meta(session, input, 0u, VK_BACK, 127)) { failure = 26; }
            // Ctrl+Alt/AltGr must remain ordinary translated Unicode input.
            if(failure == 0 && (!inject(input, u'b', RIGHT_ALT_PRESSED | LEFT_CTRL_PRESSED, 1u, 'B') ||
                                session.read_byte() != 'b')) { failure = 27; }
            // Shift+Alt uppercase and Alt+NumPad Unicode are not editor Meta.
            if(failure == 0 && (!inject(input, u'F', LEFT_ALT_PRESSED | SHIFT_PRESSED, 1u, 'F') ||
                                session.read_byte() != 'F')) { failure = 28; }
            if(failure == 0 && (!inject(input, u'f', LEFT_ALT_PRESSED, 1u, VK_NUMPAD6) ||
                                session.read_byte() != 'f')) { failure = 29; }
            if(failure == 0 && (!inject(input, u'\b', RIGHT_ALT_PRESSED | LEFT_CTRL_PRESSED, 1u, VK_BACK) ||
                                session.read_byte() != 8)) { failure = 30; }
            if(failure == 0 && !expect_meta(session, input, static_cast<char16_t>(127u), VK_BACK, 127))
            { failure = 31; }
            // Three separate records are three editor actions, unlike one
            // KEY_EVENT with packed wRepeatCount=3 tested above.
            if(failure == 0)
            {
                for(unsigned n{}; n != 3u; ++n)
                { if(!expect_meta(session, input, u'f', 'F', 'f')) { failure = 32; break; } }
            }
            // A zero translated UnicodeChar plus Shift or CapsLock cannot
            // prove a lower-case letter. The following sentinel must be the
            // next editor byte; neither record may leave a queued ESC/meta.
            if(failure == 0 && (!inject(input, 0u, LEFT_ALT_PRESSED | SHIFT_PRESSED, 1u, 'F') ||
                                !inject(input, u'z', 0u) || session.read_byte() != 'z'))
            { failure = 33; }
            if(failure == 0 && (!inject(input, 0u, LEFT_ALT_PRESSED | CAPSLOCK_ON, 1u, 'B') ||
                                !inject(input, u'z', 0u) || session.read_byte() != 'z'))
            { failure = 34; }
        }
    }
    ::DWORD input_after{}, output_after{};
    if(::fast_io::noexcept_call(::GetConsoleMode, input, &input_after) == 0 ||
       ::fast_io::noexcept_call(::GetConsoleMode, output, &output_after) == 0 ||
       input_after != input_before || output_after != output_before) { failure = failure == 0 ? 20 : failure; }
    return failure;
}
