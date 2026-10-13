#include <fast_io.h>
#include <uwvm2/utils/control/sealed_input.h>
#include <windows.h>

using namespace uwvm2::utils::control;

int main()
{
    auto const saved_input{::GetStdHandle(STD_INPUT_HANDLE)};
    auto const saved_output{::GetStdHandle(STD_OUTPUT_HANDLE)};
    ::HANDLE read_end{}, write_end{};
    if(!::CreatePipe(&read_end, &write_end, nullptr, 0u)) { return 1; }
    auto const output{::CreateFileW(L"C:\\uwvm-sealed-output.tmp", GENERIC_WRITE, FILE_SHARE_READ,
                                    nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(output == INVALID_HANDLE_VALUE) { return 2; }
    if(!::SetStdHandle(STD_INPUT_HANDLE, read_end) || !::SetStdHandle(STD_OUTPUT_HANDLE, output)) { return 3; }
    if(seal_console_input_host_api(0) != sealed_input_status::ok) { return 4; }
    if(!console_input_sealed() || inspect_guest_file_host_api(0) != sealed_input_decision::denied) { return 5; }
    if(inspect_guest_output_host_api(1) != sealed_input_decision::allow) { return 6; }
    if(inspect_guest_native_handle_host_api(read_end) != sealed_input_decision::denied ||
       inspect_guest_native_handle_host_api(write_end) != sealed_input_decision::denied) { return 7; }
    if(inspect_guest_native_handle_host_api(output) != sealed_input_decision::allow) { return 8; }
    if(seal_console_input_host_api(0) != sealed_input_status::already_sealed) { return 9; }
    unseal_console_input_after_guest_drain_host_api();
    if(console_input_sealed()) { return 10; }
    ::SetStdHandle(STD_INPUT_HANDLE, saved_input);
    ::SetStdHandle(STD_OUTPUT_HANDLE, saved_output);
    ::CloseHandle(read_end);
    ::CloseHandle(write_end);
    ::CloseHandle(output);
    ::DeleteFileW(L"C:\\uwvm-sealed-output.tmp");
    return 0;
}
