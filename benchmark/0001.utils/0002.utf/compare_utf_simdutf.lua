#!/usr/bin/env lua

-- compare_utf_simdutf.lua
--
-- Lua driver to:
--   - locate or clone a simdutf checkout;
--   - build and run per-implementation UTF-8 verification benchmarks;
--   - extract per-scenario timings and print simple ratios.
--
-- Usage (from project root):
--
--   lua benchmark/0001.utils/0002.utf/compare_utf_simdutf.lua
--   # or
--   xmake lua benchmark/0001.utils/0002.utf/compare_utf_simdutf.lua
--
-- Environment variables:
--
--   CXX               : C++ compiler (default: clang++)
--   CXXFLAGS_EXTRA    : extra C++ flags appended after the defaults
--   SIMDUTF_DIR       : path to an existing simdutf checkout
--                        (default: auto-clone into ./outputs/simdutf next to this file)
--   SIMDUTF_INCLUDE   : optional explicit include directory for simdutf.h
--                        (default: auto-detect singleheader/ or include/)
--   UTF_BENCH_BYTES   : approximate bytes per scenario buffer (default: 16 MiB)
--   ITERS             : number of benchmark iterations (default: 20, in C++)
--   WARMUP_ITERS      : warm-up iterations per implementation (not timed)
--                        (default: 1, in C++)
--   TRIALS            : number of measurement trials (default: 3, in C++)
--   RUSTFLAGS         : preserved (not used directly here, but forwarded untouched)
--   UWVM2_SIMD_LEVEL  : optional fixed SIMD level for C++.
--                        Recognized values:
--                          "native"    : use -march=native (default)
--                          "sse2"      : x86-64 + SSE2 only
--                          "sse3"      : x86-64 + SSE3
--                          "ssse3"     : x86-64 + SSSE3
--                          "sse4"      : x86-64 + SSE4.1/SSE4.2
--                          "avx"       : x86-64 + AVX
--                          "avx2"      : x86-64 + AVX2
--                          "avx512bw"  : x86-64 + AVX2 + AVX-512BW
--                          "avx512vbmi": x86-64 + AVX2 + AVX-512BW + AVX-512VBMI/VBMI2
--   SIMDUTF_FORCE_IMPLEMENTATION : optional simdutf implementation name to force
--                        (e.g. "fallback", "westmere", "haswell", "icelake").
--                        If not set and UWVM2_SIMD_LEVEL is pinned, this script
--                        will auto-force a comparable simdutf implementation.

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
    local ok, _, _ = os.rename(path, path)
    if ok then
        return true
    end
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

local gen_src = root_dir .. "/benchmark/0001.utils/0002.utf/UtfGen.cc"
local gen_bin = output_dir .. "/UtfGen"

local uwvm2_src = root_dir .. "/benchmark/0001.utils/0002.utf/UtfUwvm2Bench.cc"
local uwvm2_bin = output_dir .. "/UtfUwvm2Bench"

local simdutf_src = root_dir .. "/benchmark/0001.utils/0002.utf/UtfSimdutfBench.cc"
local simdutf_bin = output_dir .. "/UtfSimdutfBench"

local bench_log = output_dir .. "/utf_bench.log"
local bench_summary = output_dir .. "/utf_bench_summary.txt"

-- Ensure simdutf repository --------------------------------------------------

local function ensure_simdutf_repo()
    local env_dir = os.getenv("SIMDUTF_DIR")
    if env_dir and #env_dir > 0 then
        if not exists(env_dir) then
            error("SIMDUTF_DIR is set but directory does not exist: " .. env_dir)
        end
        return abspath(env_dir)
    end

    -- Default: clone into ./outputs/simdutf next to this script.
    local default_dir = output_dir .. "/simdutf"
    if exists(default_dir) then
        return default_dir
    end

    print("==> Cloning simdutf into " .. default_dir)
    local clone_cmd = string.format('git clone --depth 1 "https://github.com/simdutf/simdutf.git" %q', default_dir)
    run_or_die(clone_cmd)
    return default_dir
end

local function detect_simdutf_include(simdutf_dir)
    local env_inc = os.getenv("SIMDUTF_INCLUDE")
    if env_inc and #env_inc > 0 then
        if not exists(env_inc) then
            error("SIMDUTF_INCLUDE is set but directory does not exist: " .. env_inc)
        end
        return abspath(env_inc)
    end

    local single = simdutf_dir .. "/singleheader/simdutf.h"
    local include = simdutf_dir .. "/include/simdutf.h"

    if exists(single) then
        return simdutf_dir .. "/singleheader"
    end
    if exists(include) then
        return simdutf_dir .. "/include"
    end

    error("could not locate simdutf.h under " .. simdutf_dir .. " (tried singleheader/ and include/)")
end

-- Build C++ benchmark --------------------------------------------------------

local function build_utf_bench(simdutf_dir)
    print("==> Building uwvm2 vs simdutf UTF-8 benchmarks (C++)")

    local cxx = os.getenv("CXX") or "clang++"

    local default_flags = {
        "-std=c++2c",
        "-O3",
        "-ffast-math",
        "-fno-rtti",
        "-fno-unwind-tables",
        "-fno-asynchronous-unwind-tables",
        -- Work around current arm64 backend constexpr issues in simdutf by
        -- forcing the generic/fallback implementation on all platforms.
        -- fix: "-DSIMDUTF_IMPLEMENTATION_ARM64=0",
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

    -- simdutf implementation source (non-header-only build).
    local simdutf_impl = simdutf_dir .. "/src/simdutf.cpp"
    if not exists(simdutf_impl) then
        error("simdutf implementation source not found: " .. simdutf_impl)
    end

    local simdutf_inc = detect_simdutf_include(simdutf_dir)

    local include_flags = {
        "-I", root_dir .. "/src",
        "-I", root_dir .. "/third-parties/fast_io/include",
        "-I", root_dir .. "/third-parties/bizwen/include",
        "-I", root_dir .. "/third-parties/boost_unordered/include",
    }

    local function compile_one(src_list, bin, extra_includes)
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
        if extra_includes then
            for _, f in ipairs(extra_includes) do
                table.insert(cmd_parts, f)
            end
        end
        for _, src in ipairs(src_list) do
            table.insert(cmd_parts, src)
        end
        table.insert(cmd_parts, "-o")
        table.insert(cmd_parts, bin)
        run_or_die(join(cmd_parts, " "))
    end

    compile_one({ gen_src }, gen_bin, nil)
    compile_one({ uwvm2_src }, uwvm2_bin, nil)

    -- simdutf currently uses a "simdutf_constexpr" macro that expands to
    -- constexpr for C++17+, and this can trip Clang's arm64 constexpr rules
    -- in some simdutf versions. Force-disable simdutf's C++17/20/23 feature
    -- toggles so simdutf_constexpr becomes empty (while we still compile with
    -- a C++2c dialect for our own benchmark sources).
    compile_one(
        { simdutf_src, simdutf_impl },
        simdutf_bin,
        {
            "-I", simdutf_inc,
            "-I", simdutf_dir .. "/src",
            "-DSIMDUTF_CPLUSPLUS17=0",
            "-DSIMDUTF_CPLUSPLUS20=0",
            "-DSIMDUTF_CPLUSPLUS23=0",
        })
end

-- Run benchmark and capture output ------------------------------------------

local function run_utf_bench()
    print("==> Running UTF-8 benchmark")

    local bench_bytes = os.getenv("UTF_BENCH_BYTES")
    local iters = os.getenv("ITERS")
    local warmup = os.getenv("WARMUP_ITERS")
    local trials = os.getenv("TRIALS")

    -- Truncate previous log so summaries stay clean.
    write_file(bench_log, "")

    local env_prefix = ""
    if bench_bytes and #bench_bytes > 0 then
        env_prefix = env_prefix .. "UTF_BENCH_BYTES=" .. bench_bytes .. " "
    end
    if iters and #iters > 0 then
        env_prefix = env_prefix .. "ITERS=" .. iters .. " "
    end
    if warmup and #warmup > 0 then
        env_prefix = env_prefix .. "WARMUP_ITERS=" .. warmup .. " "
    end
    if trials and #trials > 0 then
        env_prefix = env_prefix .. "TRIALS=" .. trials .. " "
    end

    -- simdutf does runtime ISA dispatch via CPUID. When we pin UWVM2_SIMD_LEVEL
    -- to a lower SIMD tier (e.g. "sse2"), force simdutf to a comparable
    -- implementation so the benchmark stays fair. Users can override this by
    -- setting SIMDUTF_FORCE_IMPLEMENTATION themselves.
    local simd_level = os.getenv("UWVM2_SIMD_LEVEL") or "native"
    local forced_impl = os.getenv("SIMDUTF_FORCE_IMPLEMENTATION")
    local simdutf_env_prefix = env_prefix
    if (not forced_impl or #forced_impl == 0) and simd_level ~= "native" then
        local map = {
            -- simdutf x86 implementations: fallback (scalar), westmere (SSE4.2),
            -- haswell (AVX2), icelake (AVX-512).
            sse2 = "fallback",
            sse3 = "fallback",
            ssse3 = "fallback",
            sse4 = "westmere",
            avx = "westmere", 
            avx2 = "haswell", -- haswell = avx2
            avx512bw = "icelake",
            avx512vbmi = "icelake",
        }
        local impl = map[simd_level]
        if impl then
            simdutf_env_prefix = simdutf_env_prefix .. "SIMDUTF_FORCE_IMPLEMENTATION=" .. impl .. " "
            print(string.format("  forcing simdutf implementation=%s (UWVM2_SIMD_LEVEL=%s)", impl, simd_level))
        end
    end

    local scenarios = { "ascii_only", "latin_mixed", "emoji_mixed" }
    for i, scenario in ipairs(scenarios) do
        local data_file = output_dir .. "/utf_data_" .. scenario .. ".bin"

        local common = env_prefix
            .. "SCENARIO=" .. string.format("%q", scenario) .. " "
            .. "DATA_FILE=" .. string.format("%q", data_file) .. " "
        local gen_cmd = string.format("cd %q && %s%q >> %q 2>&1", root_dir, common, gen_bin, bench_log)
        run_or_die(gen_cmd)

        local uw_cmd = string.format("cd %q && %s%q >> %q 2>&1", root_dir, common, uwvm2_bin, bench_log)
        local simd_env = simdutf_env_prefix
            .. "SCENARIO=" .. string.format("%q", scenario) .. " "
            .. "DATA_FILE=" .. string.format("%q", data_file) .. " "
        local simd_cmd = string.format("cd %q && %s%q >> %q 2>&1", root_dir, simd_env, simdutf_bin, bench_log)

        -- Alternate which implementation runs first per scenario to reduce
        -- cross-process warm-cache ordering bias.
        if (i % 2) == 1 then
            run_or_die(uw_cmd)
            run_or_die(simd_cmd)
        else
            run_or_die(simd_cmd)
            run_or_die(uw_cmd)
        end
    end
end

-- Parse timing lines ---------------------------------------------------------

local function parse_utf_results()
    local uwvm = {}
    local simd = {}

    local f = io.open(bench_log, "r")
    if not f then
        return uwvm, simd
    end

    for line in f:lines() do
        if line:match("^uwvm2_utf ") then
            local scenario = line:match("scenario=([^%s]+)")
            local impl = line:match("impl=([^%s]+)")
            local ns_str = line:match("ns_per_byte=([^%s]+)")
            if scenario and impl and ns_str then
                local ns = tonumber(ns_str)
                if impl == "uwvm2" then
                    uwvm[scenario] = ns
                elseif impl == "simdutf" then
                    simd[scenario] = ns
                end
            end
        end
    end

    f:close()

    local summary_lines = {}
    for scenario, ns in pairs(uwvm) do
        summary_lines[#summary_lines + 1] = string.format("scenario=%s impl=uwvm2 ns_per_byte=%.6f", scenario, ns)
    end
    for scenario, ns in pairs(simd) do
        summary_lines[#summary_lines + 1] = string.format("scenario=%s impl=simdutf ns_per_byte=%.6f", scenario, ns)
    end
    table.sort(summary_lines)
    if #summary_lines > 0 then
        write_file(bench_summary, table.concat(summary_lines, "\n") .. "\n")
    end

    return uwvm, simd
end

local function print_comparison(uwvm, simd)
    print()
    print(string.rep("=", 80))
    print("Per-scenario comparison (ns/byte, smaller is faster)")
    print(string.rep("=", 80))

    local scenarios = {}
    for k, _ in pairs(uwvm) do
        scenarios[#scenarios + 1] = k
    end
    table.sort(scenarios)

    for _, scenario in ipairs(scenarios) do
        local ns_uwvm = uwvm[scenario]
        local ns_simd = simd[scenario]

        print()
        print("Scenario: " .. scenario)
        print(string.format("  uwvm2  : ns_per_byte = %.6f", ns_uwvm))
        if ns_simd then
            print(string.format("  simdutf: ns_per_byte = %.6f", ns_simd))
            local ratio = ns_uwvm / ns_simd
            print(string.format("  ratio  : uwvm2 / simdutf ≈ %.3f  (<1 means uwvm2 is faster)", ratio))
        else
            print("  simdutf: <no result recorded>")
        end
    end

    print()
    print("Raw log saved to: " .. bench_log)
    print("Summary written to: " .. bench_summary)
end

-- Main -----------------------------------------------------------------------

local function main()
    print("uwvm2 UTF-8 verification benchmark (uwvm2 vs simdutf, Lua driver)")
    print(string.format("  script_dir = %s", script_dir))
    print(string.format("  root_dir   = %s", root_dir))
    print(string.format("  output_dir = %s", output_dir))

    local simdutf_dir = ensure_simdutf_repo()
    print(string.format("  simdutf_dir= %s", simdutf_dir))

    build_utf_bench(simdutf_dir)
    run_utf_bench()

    local uwvm, simd = parse_utf_results()
    print_comparison(uwvm, simd)

    print()
    print("Done.")
end

main()
