// Small host-owned child for the Windows broker VM qualification. It speaks the
// bounded debugger wire protocol without linking the Wasm runtime.
#if defined(_WIN32) && !defined(__CYGWIN__)
# define _WIN32_WINNT 0x0A00
# include <windows.h>
# include <stdint.h>
# include <stdlib.h>
# include <string.h>
# include <wchar.h>

static int pipe_io(HANDLE pipe, char* bytes, DWORD capacity, DWORD* size, int writing)
{
    HANDLE event = CreateEventW(0, TRUE, FALSE, 0);
    if(!event) { return 0; }
    OVERLAPPED request = {0};
    request.hEvent = event;
    *size = 0;
    BOOL started = writing ? WriteFile(pipe, bytes, capacity, size, &request)
                           : ReadFile(pipe, bytes, capacity, size, &request);
    if(!started && GetLastError() == ERROR_IO_PENDING)
    {
        if(WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0)
        {
            CancelIoEx(pipe, &request);
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
            return 0;
        }
        started = GetOverlappedResult(pipe, &request, size, FALSE);
    }
    CloseHandle(event);
    return started && *size != 0 && *size <= capacity;
}

static int hex_digit(wchar_t c)
{
    if(c >= L'0' && c <= L'9') { return c - L'0'; }
    if(c >= L'a' && c <= L'f') { return c - L'a' + 10; }
    return -1;
}

static int attempt_host_connection(wchar_t const* pipe_name, wchar_t const* token)
{
    if(wcslen(token) != 64) { return 40; }
    unsigned char binary_token[32];
    for(unsigned i = 0; i != 32; ++i)
    {
        int high = hex_digit(token[i * 2]);
        int low = hex_digit(token[i * 2 + 1]);
        if(high < 0 || low < 0) { return 40; }
        binary_token[i] = (unsigned char)((high << 4) | low);
    }
    // The authorized client closes before this guest descendant connects, so
    // the broker is accepting again. A positive host response is a test failure.
    Sleep(800);
    HANDLE external = INVALID_HANDLE_VALUE;
    for(unsigned retry = 0; retry != 100; ++retry)
    {
        external = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, 0,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if(external != INVALID_HANDLE_VALUE) { break; }
        if(GetLastError() != ERROR_PIPE_BUSY) { return 41; }
        WaitNamedPipeW(pipe_name, 50);
    }
    if(external == INVALID_HANDLE_VALUE) { return 41; }
    DWORD state = PIPE_READMODE_MESSAGE;
    DWORD count = 0;
    char response[6];
    BOOL authenticated = SetNamedPipeHandleState(external, &state, 0, 0) &&
        WriteFile(external, binary_token, sizeof(binary_token), &count, 0) && count == sizeof(binary_token) &&
        ReadFile(external, response, sizeof(response), &count, 0) && count == sizeof(response) &&
        memcmp(response, "ready\n", sizeof(response)) == 0;
    CloseHandle(external);
    return authenticated ? 0 : 42;
}

int wmain(int argc, wchar_t** argv)
{
    if(argc == 4 && wcscmp(argv[1], L"--attempt") == 0)
    { return attempt_host_connection(argv[2], argv[3]); }
    uintptr_t inherited = 0;
    for(int i = 1; i + 1 < argc; ++i)
    {
        if(wcscmp(argv[i], L"--debug-jit-control-handle") != 0) { continue; }
        wchar_t* end = 0;
        unsigned long long parsed = wcstoull(argv[++i], &end, 10);
        if(parsed == 0 || end == argv[i] || *end != L'\0' || (uintptr_t)parsed != parsed)
        { return 20; }
        inherited = (uintptr_t)parsed;
        break;
    }
    if(inherited == 0) { return 21; }
    HANDLE pipe = (HANDLE)inherited;
    DWORD server_pid = 0;
    if(GetFileType(pipe) != FILE_TYPE_PIPE ||
       !GetNamedPipeServerProcessId(pipe, &server_pid) || server_pid == 0)
    { return 22; }
    HANDLE attempt = 0;
    char message[513];
    for(;;)
    {
        DWORD size = 0;
        if(!pipe_io(pipe, message, 512, &size, 0)) { return 23; }
        if(size == 0 || size > 512) { return 24; }
        message[size] = 0;
        char const* reply = "child-ok\n";
        if(size >= 19 && memcmp(message, "attempt-descendant ", 19) == 0)
        {
            char* separator = strchr(message + 19, ' ');
            if(separator == 0 || attempt != 0) { reply = "schedule-error\n"; }
            else
            {
                *separator = 0;
                wchar_t pipe_name[256];
                wchar_t token[65];
                size_t pipe_length = strlen(message + 19);
                size_t token_length = strlen(separator + 1);
                if(pipe_length == 0 || pipe_length >= 256 || token_length != 64)
                { reply = "schedule-error\n"; }
                else
                {
                    for(size_t i = 0; i != pipe_length; ++i) { pipe_name[i] = (unsigned char)message[i + 19]; }
                    pipe_name[pipe_length] = 0;
                    for(size_t i = 0; i != 64; ++i) { token[i] = (unsigned char)separator[i + 1]; }
                    token[64] = 0;
                    wchar_t self[MAX_PATH];
                    wchar_t command[1024];
                    DWORD self_length = GetModuleFileNameW(0, self, MAX_PATH);
                    if(self_length == 0 || self_length >= MAX_PATH ||
                       swprintf(command, 1024, L"\"%ls\" --attempt \"%ls\" %ls", self, pipe_name, token) < 0)
                    { reply = "schedule-error\n"; }
                    else
                    {
                        STARTUPINFOW startup = {0};
                        startup.cb = sizeof(startup);
                        PROCESS_INFORMATION child = {0};
                        if(!CreateProcessW(self, command, 0, 0, FALSE, CREATE_NO_WINDOW, 0, 0,
                                           &startup, &child))
                        { reply = "schedule-error\n"; }
                        else
                        {
                            attempt = child.hProcess;
                            CloseHandle(child.hThread);
                            reply = "scheduled\n";
                        }
                    }
                }
            }
        }
        else if(size == 6 && memcmp(message, "status", 6) == 0)
        {
            DWORD result = 0;
            if(attempt == 0 || WaitForSingleObject(attempt, 0) != WAIT_OBJECT_0 ||
               !GetExitCodeProcess(attempt, &result)) { reply = "guest-pending\n"; }
            else { reply = result == 42 ? "guest-denied\n" : result == 0 ? "guest-accepted\n" : "guest-error\n"; }
        }
        else if(size == 4 && memcmp(message, "quit", 4) == 0)
        { reply = "detached\n"; }
        DWORD written = 0;
        DWORD const reply_size = (DWORD)strlen(reply);
        if(!pipe_io(pipe, (char*)reply, reply_size, &written, 1) || written != reply_size)
        { return 25; }
        if(size == 4 && memcmp(message, "quit", 4) == 0)
        { if(attempt != 0) { CloseHandle(attempt); } return 0; }
    }
}
#else
int main(void) { return 2; }
#endif
