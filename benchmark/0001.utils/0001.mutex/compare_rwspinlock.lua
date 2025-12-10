#!/usr/bin/env lua

-- compare_rwspinlock.lua
--
-- Lua driver for the RWSpinLock benchmark in:
--   benchmark/0001.utils/0001.mutex/RWSpinLock.cc
--
-- It will:
--   - build the C++ benchmark binary with suitable optimization flags
--   - run it once
--   - capture the timer output into a log file and print a small summary
--
-- Usage (from project root):
--
--   lua benchmark/0001.utils/0001.mutex/compare_rwspinlock.lua
--   # or
--   xmake lua benchmark/0001.utils/0001.mutex/compare_rwspinlock.lua
--
-- Environment variables:
--
--   CXX               : C++ compiler (default: clang++)
--   CXXFLAGS_EXTRA    : extra C++ flags appended after the defaults
--   UWVM2_SIMD_LEVEL  : optional fixed SIMD level for the build.
--                        Recognized values:
--                          "native"    : use -march=native (default)
--                          "sse2"      : x86-64 + SSE2
--                          "sse3"      : x86-64 + SSE3
--                          "ssse3"     : x86-64 + SSSE3
--                          "sse4"      : x86-64 + SSE4.1/SSE4.2
--                          "avx"       : x86-64 + AVX
--                          "avx2"      : x86-64 + AVX2
--                          "avx512bw"  : x86-64 + AVX2 + AVX-512BW
--                          "avx512vbmi": x86-64 + AVX2 + AVX-512BW + AVX-512VBMI/VBMI2

local function join(list, sep)
    sep = sep or " "
    local buf = {}
    for _, v in ipairs(list) do
        if v ~= nil and v ~= "" then
            buf[#buf + 1] = v
        end
    end
    return table.concat(buf, sep)
end

local function dirname(path)
    return path:match("^(.*)/[^/]+$") or "."
end

local function abspath(path)
    if path:sub(1, 1) == "/" then
        return path
    end
    local pwd = assert(io.popen("pwd")):read("*l")
    return pwd .. "/" .. path
end

local function exists(path)
    local ok, _, code = os.rename(path, path)
    if ok then
        return true
    end
    -- code 2: no such file or directory (POSIX)
    return false
end

local function run_or_die(cmd)
    print(">> " .. cmd)
    local ok, why, code = os.execute(cmd)
    if ok == true or ok == 0 then
        return
    end
    error(string.format("command failed (%s, %s): %s", tostring(why), tostring(code), cmd))
end

local function write_file(path, content)
    local f, err = io.open(path, "w")
    if not f then
        error("failed to open file for write: " .. path .. " : " .. tostring(err))
    end
    f:write(content)
    f:close()
end

local function mkdir_p(path)
    if exists(path) then
        return
    end
    local cmd = string.format('mkdir -p %q', path)
    run_or_die(cmd)
end

-- Resolve important paths ----------------------------------------------------

local script_path = abspath(assert(arg[0], "arg[0] is not set"))
local script_dir = dirname(script_path)
local root_dir = script_dir .. "/../../.."

local output_dir = script_dir .. "/outputs"
mkdir_p(output_dir)

local bench_src = root_dir .. "/benchmark/0001.utils/0001.mutex/RWSpinLock.cc"
local bench_bin = output_dir .. "/RWSpinLock"
local bench_log = output_dir .. "/rwspinlock_bench.log"
local bench_summary = output_dir .. "/rwspinlock_summary.txt"

-- Build RWSpinLock C++ benchmark --------------------------------------------

local function build_rwspinlock_bench()
    print("==> Building RWSpinLock benchmark (C++)")

    local cxx = os.getenv("CXX") or "clang++"

    local default_flags = {
        "-std=c++2c",
        "-O3",
        "-ffast-math",
        "-fno-rtti",
        "-fno-unwind-tables",
        "-fno-asynchronous-unwind-tables",
    }

    local simd_level = os.getenv("UWVM2_SIMD_LEVEL") or "native"
    if simd_level == "native" then
        table.insert(default_flags, "-march=native")
    elseif simd_level == "sse2" then
        table.insert(default_flags, "-msse2")
    elseif simd_level == "sse3" then
        table.insert(default_flags, "-msse3")
    elseif simd_level == "ssse3" then
        table.insert(default_flags, "-mssse3")
    elseif simd_level == "sse4" then
        table.insert(default_flags, "-msse4.1")
        table.insert(default_flags, "-msse4.2")
    elseif simd_level == "avx" then
        table.insert(default_flags, "-mavx")
    elseif simd_level == "avx2" then
        table.insert(default_flags, "-mavx2")
    elseif simd_level == "avx512bw" then
        table.insert(default_flags, "-mavx512bw")
    elseif simd_level == "avx512vbmi" then
        table.insert(default_flags, "-mavx512bw")
        table.insert(default_flags, "-mavx512vbmi")
        table.insert(default_flags, "-mavx512vbmi2")
    else
        -- Fallback: native
        table.insert(default_flags, "-march=native")
    end

    local extra = os.getenv("CXXFLAGS_EXTRA")
    local extra_flags = {}
    if extra and #extra > 0 then
        for flag in extra:gmatch("%S+") do
            table.insert(extra_flags, flag)
        end
    end

    local include_flags = {
        "-I", root_dir .. "/src",
        "-I", root_dir .. "/third-parties/fast_io/include",
        "-I", root_dir .. "/third-parties/bizwen/include",
        "-I", root_dir .. "/third-parties/boost_unordered/include",
    }

    local cmd_parts = { cxx }
    for _, f in ipairs(default_flags) do
        table.insert(cmd_parts, f)
    end
    for _, f in ipairs(extra_flags) do
        table.insert(cmd_parts, f)
    end
    for _, f in ipairs(include_flags) do
        table.insert(cmd_parts, f)
    end
    table.insert(cmd_parts, bench_src)
    table.insert(cmd_parts, "-o")
    table.insert(cmd_parts, bench_bin)

    local cmd = join(cmd_parts, " ")
    run_or_die(cmd)
end

-- Run C++ benchmark and capture output --------------------------------------

local function run_rwspinlock_bench()
    print("==> Running RWSpinLock benchmark")

    local cmd = string.format("cd %q && %q > %q 2>&1", root_dir, bench_bin, bench_log)
    run_or_die(cmd)
end

-- Parse timer lines from log ------------------------------------------------

local function parse_timer_log()
    local timers = {}

    local f = io.open(bench_log, "r")
    if not f then
        return timers
    end

    for line in f:lines() do
        -- Lines look roughly like:
        --   uwvm: [debug] timer "mylock single-thread read": 0.123456789s [2025-04-01T12:34:56Z]
        local name, seconds = line:match('timer "([^"]+)":%s*([%d%.]+)s')
        if name and seconds then
            timers[#timers + 1] = { name = name, seconds = tonumber(seconds) or seconds }
        end
    end

    f:close()

    -- Sort by name for stable output
    table.sort(timers, function(a, b)
        return a.name < b.name
    end)

    local summary_lines = {}
    for _, t in ipairs(timers) do
        summary_lines[#summary_lines + 1] = string.format("%s: %s s", t.name, tostring(t.seconds))
    end
    if #summary_lines > 0 then
        write_file(bench_summary, table.concat(summary_lines, "\n") .. "\n")
    end

    return timers
end

-- Main ----------------------------------------------------------------------

local function main()
    print("uwvm2 utils::mutex rwlock_t vs bench_folly::RWSpinLock benchmark (Lua driver)")
    print(string.format("  script_dir = %s", script_dir))
    print(string.format("  root_dir   = %s", root_dir))
    print(string.format("  output_dir = %s", output_dir))

    build_rwspinlock_bench()
    run_rwspinlock_bench()

    local timers = parse_timer_log()

    print()
    print("Timer results (seconds):")
    if #timers == 0 then
        print("  <no timer lines parsed; see log file for raw output>")
        print(string.format("  log: %s", bench_log))
    else
        for _, t in ipairs(timers) do
            print(string.format("  %-32s %s", t.name, tostring(t.seconds)))
        end
        print()
        print(string.format("Summary written to: %s", bench_summary))
        print(string.format("Raw log saved to : %s", bench_log))
    end

    print()
    print("Done.")
end

main()

