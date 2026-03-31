/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      OpenAI
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

// std
#include <coroutine>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

// macro
#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
// import
# include <uwvm2/utils/thread/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{

    [[nodiscard]] ::uwvm2::utils::thread::scheduled_task make_mid_suspend_task()
    {
        co_await ::std::suspend_always{};
        co_return;
    }

    int run_child_case()
    {
        ::uwvm2::utils::thread::scheduled_task_batch task_batch{1uz};

        auto task{make_mid_suspend_task()};
        ::std::construct_at(task_batch.handles.buffer, task.release());
        task_batch.handle_count = 1uz;

        ::uwvm2::utils::thread::native_thread_pool thread_pool{};
        thread_pool.run(task_batch, 0uz);

        return 0;
    }

    [[nodiscard]] int run_parent_case(char const* argv0)
    {
        auto const executable{::std::filesystem::absolute(argv0)};
        auto const command{::std::string{"\""} + executable.string() + "\" --child"};
        auto const status{::std::system(command.c_str())};

        if(status == -1) [[unlikely]] { return 1; }
        if(status == 0) [[unlikely]] { return 2; }
        return 0;
    }

}  // namespace

int main(int argc, char** argv)
{
    if(argc > 1 && ::std::string_view{argv[1]} == "--child") { return run_child_case(); }
    return run_parent_case(argv[0]);
}
