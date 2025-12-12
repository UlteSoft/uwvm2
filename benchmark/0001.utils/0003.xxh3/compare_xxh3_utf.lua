#!/usr/bin/env lua

-- compare_xxh3_utf.lua
--
-- Lua driver to:
--   - clone the upstream xxHash repository (if needed)
--   - build and run the uwvm2 vs xxHash UTF-8 buffer xxh3 benchmark
--   - extract throughput numbers and print a simple comparison
--
-- Usage (from project root):
--
--   lua benchmark/0001.utils/0003.xxh3/compare_xxh3_utf.lua
--   # or
--   xmake lua benchmark/0001.utils/0003.xxh3/compare_xxh3_utf.lua
--
-- Environment variables:
--
--   CXX               : C++ compiler (default: clang++)
--   CXXFLAGS_EXTRA    : extra C++ flags appended after the defaults
--   XXHASH_DIR        : path to an existing xxHash checkout
--                        (default: auto-clone into ./outputs/xxhash next to this file)
--   BYTES             : forwarded to C++ benchmark (default: 16MiB in C++)
--   ITERS             : forwarded to C++ benchmark (default: 50 in C++)
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
--                          "avx512vbmi": x86-64 + AVX-512BW + AVX-512VBMI/VBMI2
--

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

local gen_src = root_dir .. "/benchmark/0001.utils/0003.xxh3/Xxh3UtfGen.cc"
local gen_bin = output_dir .. "/Xxh3UtfGen"

local xxhash_src = root_dir .. "/benchmark/0001.utils/0003.xxh3/Xxh3UtfXxhashBench.cc"
local xxhash_bin = output_dir .. "/Xxh3UtfXxhashBench"

local uwvm2_src = root_dir .. "/benchmark/0001.utils/0003.xxh3/Xxh3UtfUwvm2Bench.cc"
local uwvm2_bin = output_dir .. "/Xxh3UtfUwvm2Bench"

local data_file = output_dir .. "/xxh3_utf_data.bin"

local bench_log = output_dir .. "/xxh3_utf_bench.log"
local bench_summary = output_dir .. "/xxh3_utf_summary.txt"

-- Ensure xxHash repository ---------------------------------------------------

local function ensure_xxhash_repo()
    local env_dir = os.getenv("XXHASH_DIR")
    if env_dir and #env_dir > 0 then
        if not exists(env_dir) then
            error("XXHASH_DIR is set but directory does not exist: " .. env_dir)
        end
        return abspath(env_dir)
    end

    local default_dir = output_dir .. "/xxhash"
    if exists(default_dir) then
        return default_dir
    end

    print("==> Cloning xxHash into " .. default_dir)
    local clone_cmd = string.format('git clone --depth 1 "https://github.com/Cyan4973/xxHash.git" %q', default_dir)
    run_or_die(clone_cmd)
    return default_dir
end

-- Build C++ benchmark --------------------------------------------------------

local function build_xxh3_bench(xxhash_dir)
    print("==> Building xxh3 UTF benchmarks (C++)")

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
        "-I", xxhash_dir,
    }

    -- We use the header-only style of xxHash via XXH_INLINE_ALL / XXH_STATIC_LINKING_ONLY.
    local define_flags = {
        "-DXXH_INLINE_ALL",
        "-DXXH_STATIC_LINKING_ONLY",
    }

    local function compile_one(src, bin, extra_defines)
        local cmd_parts = { cxx }
        for _, f in ipairs(default_flags) do
            table.insert(cmd_parts, f)
        end
        for _, f in ipairs(extra_flags) do
            table.insert(cmd_parts, f)
        end
        if extra_defines then
            for _, f in ipairs(extra_defines) do
                table.insert(cmd_parts, f)
            end
        end
        for _, f in ipairs(include_flags) do
            table.insert(cmd_parts, f)
        end
        table.insert(cmd_parts, src)
        table.insert(cmd_parts, "-o")
        table.insert(cmd_parts, bin)
        run_or_die(join(cmd_parts, " "))
    end

    -- Generator does not need xxHash defines, but using same flags is fine.
    compile_one(gen_src, gen_bin, nil)
    compile_one(xxhash_src, xxhash_bin, define_flags)
    compile_one(uwvm2_src, uwvm2_bin, nil)
end

-- Run benchmark and capture output -------------------------------------------

local function run_xxh3_bench()
    print("==> Running xxh3 UTF benchmarks")

    local bytes = os.getenv("BYTES")
    local iters = os.getenv("ITERS")

    -- Truncate previous log so summaries stay clean.
    write_file(bench_log, "")

    local env_prefix = "DATA_FILE=" .. data_file .. " "
    if bytes and #bytes > 0 then
        env_prefix = env_prefix .. "BYTES=" .. bytes .. " "
    end
    if iters and #iters > 0 then
        env_prefix = env_prefix .. "ITERS=" .. iters .. " "
    end

    -- 1) Generate data file.
    local gen_cmd = string.format("cd %q && %s%q >> %q 2>&1", root_dir, env_prefix, gen_bin, bench_log)
    run_or_die(gen_cmd)

    -- 2) Benchmark xxHash.
    local xx_cmd = string.format("cd %q && %s%q >> %q 2>&1", root_dir, env_prefix, xxhash_bin, bench_log)
    run_or_die(xx_cmd)

    -- 3) Benchmark uwvm2.
    local uw_cmd = string.format("cd %q && %s%q >> %q 2>&1", root_dir, env_prefix, uwvm2_bin, bench_log)
    run_or_die(uw_cmd)

    local summary_lines = {}
    for line in io.lines(bench_log) do
        if line:match("^xxh3_utf ") then
            table.insert(summary_lines, line)
        end
    end
    if #summary_lines == 0 then
        error("no 'xxh3_utf ' lines found in " .. bench_log)
    end
    write_file(bench_summary, table.concat(summary_lines, "\n") .. "\n")

    print()
    print("Per-implementation throughput results (raw):")
    for _, line in ipairs(summary_lines) do
        print("  " .. line)
    end
end

-- Parse summary and show comparison ------------------------------------------

local function parse_summary()
    local res = {}
    for line in io.lines(bench_summary) do
        local impl = line:match("impl=([^%s]+)")
        local gib_str = line:match("gib_per_s=([^%s]+)")
        if impl and gib_str then
            res[impl] = tonumber(gib_str)
        end
    end
    return res
end

local function show_comparison(results)
    local uwvm = results["uwvm2_xxh3"]
    local xxh = results["xxhash_xxh3"]

    print()
    print(string.rep("=", 80))
    print("Throughput comparison (GiB/s, larger is better)")
    print(string.rep("=", 80))

    if not uwvm then
        print("  uwvm2_xxh3 : <no result found>")
    else
        print(string.format("  uwvm2_xxh3 : %.6f GiB/s", uwvm))
    end

    if not xxh then
        print("  xxhash_xxh3: <no result found>")
    else
        print(string.format("  xxhash_xxh3: %.6f GiB/s", xxh))
    end

    if uwvm and xxh then
        local ratio = uwvm / xxh
        print()
        print(string.format("  ratio (uwvm2 / xxHash): %.3f  ( >1 means uwvm2 is faster )", ratio))
    end
end

-- Main -----------------------------------------------------------------------

local function main()
    print("uwvm2 xxh3 vs upstream xxHash UTF benchmark (Lua driver)")
    print(string.format("  script_dir = %s", script_dir))
    print(string.format("  root_dir   = %s", root_dir))
    print(string.format("  output_dir = %s", output_dir))

    local xxhash_dir = ensure_xxhash_repo()
    build_xxh3_bench(xxhash_dir)
    run_xxh3_bench()

    local results = parse_summary()
    show_comparison(results)

    print()
    print("Done.")
end

main()
