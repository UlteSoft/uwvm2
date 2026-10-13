#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* Standalone Windows VM proof for a launcher-owned, single-instance message
 * pipe. Product adoption remains disabled until this inherited client-handle
 * identity and frame-boundary contract passes in the real Windows VM. */

static int fail(char const* operation)
{
    fprintf(stderr, "%s: win32=%lu\n", operation, (unsigned long)GetLastError());
    return 1;
}

static int direct_parent_is(DWORD expected)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry = {0};
    entry.dwSize = sizeof(entry);
    int result = 0;
    if(Process32FirstW(snapshot, &entry)) do {
        if(entry.th32ProcessID == GetCurrentProcessId()) {
            result = entry.th32ParentProcessID == expected;
            break;
        }
    } while(Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return result;
}

static int child_main(HANDLE pipe, DWORD expected_parent)
{
    if(pipe == NULL || pipe == INVALID_HANDLE_VALUE || GetFileType(pipe) != FILE_TYPE_PIPE)
        return fail("inherited client pipe type");
    ULONG server_pid = 0;
    DWORD flags = 0, max_instances = 0;
    if(!GetNamedPipeServerProcessId(pipe, &server_pid)) return fail("client server PID query");
    if(!GetNamedPipeInfo(pipe, &flags, NULL, NULL, &max_instances)) return fail("client pipe info");
    if(server_pid != expected_parent || !direct_parent_is(expected_parent) ||
       (flags & PIPE_TYPE_MESSAGE) == 0 || max_instances != 1)
        return fail("server/direct-parent/message/single-instance identity");
    HANDLE parent = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, expected_parent);
    if(parent == NULL) return fail("pin direct parent");
    if(WaitForSingleObject(parent, 0) != WAIT_TIMEOUT) return fail("direct parent exited");
    FILETIME created, exited, kernel, user;
    if(!GetProcessTimes(parent, &created, &exited, &kernel, &user)) return fail("parent creation time");
    DWORD read_mode = PIPE_READMODE_MESSAGE;
    if(!SetNamedPipeHandleState(pipe, &read_mode, NULL, NULL)) return fail("set client message read mode");
    DWORD observed_mode = 0;
    if(!GetNamedPipeHandleStateW(pipe, &observed_mode, NULL, NULL, NULL, NULL, 0) ||
       (observed_mode & PIPE_READMODE_MESSAGE) == 0) return fail("verify client message read mode");

    char message[512] = {0};
    DWORD length = 0;
    if(!ReadFile(pipe, message, sizeof(message), &length, NULL) || length != 4 ||
       memcmp(message, "PING", 4) != 0) return fail("receive exact launcher frame");
    if(WaitForSingleObject(parent, 0) != WAIT_TIMEOUT) return fail("parent liveness after frame");
    if(!WriteFile(pipe, "PONG", 4, &length, NULL) || length != 4) return fail("reply frame");

    length = 0;
    if(ReadFile(pipe, message, sizeof(message), &length, NULL) ||
       GetLastError() != ERROR_MORE_DATA || length != sizeof(message))
        return fail("reject oversized message without truncation ambiguity");
    char last = 0;
    if(!ReadFile(pipe, &last, 1, &length, NULL) || length != 1 || last != 'X')
        return fail("drain rejected message tail");
    if(!WriteFile(pipe, "OVER", 4, &length, NULL) || length != 4) return fail("oversize rejection reply");
    printf("server_pid=%lu direct_parent=1 type=message max_instances=1 frame_boundary=1 parent_live=1\n",
           (unsigned long)server_pid);
    CloseHandle(parent);
    CloseHandle(pipe);
    return 0;
}

int wmain(int argc, wchar_t** argv)
{
    if(argc == 4 && wcscmp(argv[1], L"--child") == 0) {
        HANDLE pipe = (HANDLE)(uintptr_t)_wcstoui64(argv[2], NULL, 10);
        DWORD parent = (DWORD)wcstoul(argv[3], NULL, 10);
        return child_main(pipe, parent);
    }
    if(argc != 1) return 2;
    wchar_t pipe_name[128], executable[MAX_PATH], command[1024];
    DWORD self = GetCurrentProcessId();
    if(swprintf(pipe_name, 128, L"\\\\.\\pipe\\uwvm-standalone-%lu-%llu", (unsigned long)self,
                (unsigned long long)GetTickCount64()) <= 0) return 3;
    HANDLE server = CreateNamedPipeW(pipe_name,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 65536, 65536, 0, NULL);
    if(server == INVALID_HANDLE_VALUE) return fail("create single-instance private server");
    HANDLE original_client = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE | FILE_WRITE_ATTRIBUTES,
                                         0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(original_client == INVALID_HANDLE_VALUE) return fail("connect launcher client endpoint");
    if(ConnectNamedPipe(server, NULL) || GetLastError() != ERROR_PIPE_CONNECTED)
        return fail("finish already-connected pipe handshake");
    HANDLE second_client = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(second_client != INVALID_HANDLE_VALUE || GetLastError() != ERROR_PIPE_BUSY)
        return fail("second client must be rejected");

    HANDLE inherited_client = NULL;
    if(!DuplicateHandle(GetCurrentProcess(), original_client, GetCurrentProcess(),
                        &inherited_client, 0, TRUE, DUPLICATE_SAME_ACCESS))
        return fail("create exact inheritable client handle");
    SIZE_T attribute_size = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_size);
    LPPROC_THREAD_ATTRIBUTE_LIST attributes = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(
        GetProcessHeap(), 0, attribute_size);
    if(attributes == NULL || !InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size))
        return fail("initialize exact handle inheritance list");
    if(!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                  &inherited_client, sizeof(inherited_client), NULL, NULL))
        return fail("restrict inherited handles");
    DWORD path_length = GetModuleFileNameW(NULL, executable, MAX_PATH);
    if(path_length == 0 || path_length == MAX_PATH) return fail("self path");
    if(swprintf(command, 1024, L"\"%ls\" --child %llu %lu", executable,
                (unsigned long long)(uintptr_t)inherited_client, (unsigned long)self) <= 0) return 4;
    STARTUPINFOEXW startup = {0};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION child = {0};
    if(!CreateProcessW(executable, command, NULL, NULL, TRUE,
                       EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW,
                       NULL, NULL, &startup.StartupInfo, &child)) return fail("launch child with only client handle");
    CloseHandle(inherited_client);
    CloseHandle(original_client);
    DeleteProcThreadAttributeList(attributes);
    HeapFree(GetProcessHeap(), 0, attributes);
    DWORD length = 0;
    if(!WriteFile(server, "PING", 4, &length, NULL) || length != 4) return fail("send launcher frame");
    char reply[4] = {0};
    if(!ReadFile(server, reply, sizeof(reply), &length, NULL) || length != 4 ||
       memcmp(reply, "PONG", 4) != 0) return fail("receive child reply");
    char oversized[513];
    memset(oversized, 'X', sizeof(oversized));
    if(!WriteFile(server, oversized, sizeof(oversized), &length, NULL) ||
       length != sizeof(oversized)) return fail("send oversized message");
    if(!ReadFile(server, reply, sizeof(reply), &length, NULL) || length != 4 ||
       memcmp(reply, "OVER", 4) != 0) return fail("receive oversized-message rejection");
    if(WaitForSingleObject(child.hProcess, 10000) != WAIT_OBJECT_0) return fail("child completion");
    DWORD child_status = 0;
    if(!GetExitCodeProcess(child.hProcess, &child_status) || child_status != 0)
        return fail("child identity checks");
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    CloseHandle(server);
    puts("windows_control_pipe_probe=pass");
    return 0;
}
