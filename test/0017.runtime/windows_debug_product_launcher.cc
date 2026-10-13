#include <windows.h>
#include <cwchar>

// Test-only Windows host: feed commands through a private anonymous pipe while
// writing product output to a disk file. Both handles are selected before the
// VM starts, so the debug input isolation path sees the real launch topology.
int wmain(int argc, wchar_t** argv)
{
    if(argc != 6 && argc != 7) { return 2; }
    if(argc == 7 && ::wcscmp(argv[6], L"exceptions") != 0) { return 13; }
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE input_read{}, input_write{};
    if(!::CreatePipe(&input_read, &input_write, &security, 0u)) { return 3; }
    bool const alias_output{::wcscmp(argv[4], L"alias") == 0};
    HANDLE inherited_alias{};
    if(alias_output && !::DuplicateHandle(::GetCurrentProcess(), input_write, ::GetCurrentProcess(),
                                           &inherited_alias, 0u, TRUE, DUPLICATE_SAME_ACCESS)) { return 12; }
    if(!::SetHandleInformation(input_write, HANDLE_FLAG_INHERIT, 0u)) { return 4; }
    HANDLE output{::CreateFileW(argv[3], GENERIC_WRITE, FILE_SHARE_READ, &security,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(output == INVALID_HANDLE_VALUE) { return 5; }
    HANDLE commands{::CreateFileW(argv[5], GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(commands == INVALID_HANDLE_VALUE) { return 6; }
    wchar_t command_line[8192]{};
    int const count{::swprintf(command_line, sizeof(command_line) / sizeof(command_line[0]),
        L"\"%ls\" -m debug-jit -Rct 0 -Rllvm-call-stack %ls -Rllvm-cache-path disable %ls --run \"%ls\"",
        argv[1], alias_output ? L"instruction" : argv[4],
        argc == 7 ? L"-WFE-exceptions" : L"", argv[2])};
    if(count <= 0 || count >= static_cast<int>(sizeof(command_line) / sizeof(command_line[0]))) { return 7; }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input_read;
    startup.hStdOutput = alias_output ? inherited_alias : output;
    startup.hStdError = output;
    PROCESS_INFORMATION process{};
    if(!::CreateProcessW(nullptr, command_line, nullptr, nullptr, TRUE,
                         CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) { return 8; }
    ::CloseHandle(input_read);
    if(inherited_alias != nullptr) { ::CloseHandle(inherited_alias); }
    ::CloseHandle(output);
    char buffer[4096];
    for(;;)
    {
        DWORD bytes{};
        if(!::ReadFile(commands, buffer, sizeof(buffer), &bytes, nullptr)) { return 9; }
        if(bytes == 0u) { break; }
        DWORD written{};
        if(!::WriteFile(input_write, buffer, bytes, &written, nullptr) || written != bytes) { return 10; }
    }
    ::CloseHandle(commands);
    ::CloseHandle(input_write);
    if(::WaitForSingleObject(process.hProcess, 90000u) != WAIT_OBJECT_0)
    {
        ::TerminateProcess(process.hProcess, 124u);
        return 124;
    }
    DWORD child_status{};
    if(!::GetExitCodeProcess(process.hProcess, &child_status)) { return 11; }
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    return child_status <= 255u ? static_cast<int>(child_status) : 125;
}
