#!/usr/bin/env lua

-- compare_varint_simd.lua
--
-- Lua driver to:
--   - build and run the uwvm2 function_section uleb128<u32> benchmark
--   - build and run a Rust varint-simd "fair" benchmark that decodes the
--     exact same LEB128 streams from shared data files
--   - extract per-value timings and compare them per scenario
--
-- All data streams are generated once on the C++ side and written to:
--
--   ${FS_BENCH_DATA_DIR}/${scenario}.bin
--
-- with the format:
--   - 8-byte little-endian u64: number of encoded values (FUNC_COUNT)
--   - raw LEB128 bytes for those values
--
-- Both the uwvm2 C++ benchmark and the Rust varint-simd fair benchmark read
-- these files into heap-allocated buffers (no mmap) before measuring decode
-- throughput. This ensures that:
--
--   - both sides run on the exact same bytes per scenario; and
--   - both sides perform heap allocation + memcpy from file into memory
--     before benchmarking, so there is no advantage from OS file mapping.
--
-- This script also includes some guidance on interpreting each scenario:
--
--   * u8_1b  : specialized fast path (SIMD + zero-copy u8 view).
--              It only validates and creates a view; it does not fully
--              materialize each value into an array. The numbers here are
--              "best-case upper bound" and are NOT a 1:1 fair comparison
--              against a general-purpose varint decoder like varint-simd.
--
--   * u8_2b  : main fair comparison. Both sides decode 1–2 byte LEB128<u32>
--              values into an array (u8-based storage). This is the most
--              representative scenario when comparing decoder throughput.
--
--   * u16_2b : 1–2 byte LEB128<u32> decoded into a u16 array. In real-world
--              wasm modules this range for typeidx is very rare; it mainly
--              serves as a completeness / stress scenario. A fully general
--              u16 decoder comparable to varint-u16/decode would correspond
--              more closely to an (unimplemented) u16_3b path.
--
-- Usage (from project root):
--
--   lua benchmark/0002.parser/0001.function_section/compare_varint_simd.lua
--   # or
--   xmake lua benchmark/0002.parser/0001.function_section/compare_varint_simd.lua
--
-- Environment variables:
--
--   CXX               : C++ compiler (default: clang++)
--   CXXFLAGS_EXTRA    : extra C++ flags appended after the defaults
--   VARINT_SIMD_DIR   : path to an existing varint-simd checkout
--                        (default: auto-clone into ./outputs/varint-simd next to this file)
--   FUNC_COUNT        : forwarded to uwvm2 benchmark (default: 1_000_000 in C++)
--   ITERS             : forwarded to both uwvm2 and Rust fair benchmarks (default: 20)
--   RUSTFLAGS         : existing flags will be preserved and extended with
--                        per-run CPU/feature flags
--   UWVM2_SIMD_LEVEL  : optional fixed SIMD level for both C++ and Rust.
--                        Recognized values:
--                          "native"    : use -march=native / -C target-cpu=native (default)
--                          "sse2"      : x86-64 + SSE2 only
--                          "sse3"      : x86-64 + SSE3
--                          "ssse3"     : x86-64 + SSSE3
--                          "sse4"      : x86-64 + SSE4.1/SSE4.2
--                          "avx"       : x86-64 + AVX
--                          "avx2"      : x86-64 + AVX2
--                          "avx512bw"  : x86-64 + AVX2 + AVX-512BW
--                          "avx512vbmi": x86-64 + AVX2 + AVX-512BW + AVX-512VBMI/VBMI2
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

local function trim(s)
    return (s:gsub("^%s+", ""):gsub("%s+$", ""))
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

-- All cloned repositories and benchmark outputs are kept under this directory
-- so that the project tree stays clean and this folder can be ignored easily.
local output_dir = script_dir .. "/outputs"
mkdir_p(output_dir)

local data_dir = output_dir .. "/data"
mkdir_p(data_dir)

local bench_src = root_dir .. "/benchmark/0002.parser/0001.function_section/FunctionSectionVarintSimd.cc"
local bench_bin = output_dir .. "/FunctionSectionVarintSimd"

local uwvm_log = output_dir .. "/uwvm2_uleb128_bench.log"
local uwvm_summary = output_dir .. "/uwvm2_uleb128_summary.txt"
local varint_log = output_dir .. "/varint_simd_bench.log"
local varint_summary = output_dir .. "/varint_simd_fair_summary.txt"

-- Build uwvm2 C++ benchmark --------------------------------------------------

local function build_uwvm_bench()
    print("==> Building uwvm2 FunctionSection uleb128 benchmark (C++)")

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

-- Run uwvm2 C++ benchmark ----------------------------------------------------

local function run_uwvm_bench()
    print("==> Running uwvm2 benchmark")

    local func_count = os.getenv("FUNC_COUNT")
    local iters = os.getenv("ITERS")
    local fs_data_dir = data_dir

    -- We forward FUNC_COUNT / ITERS if present; otherwise the C++ binary will
    -- fall back to its built-in defaults (1_000_000 and 20).
    local env_prefix = ""
    if func_count and #func_count > 0 then
        env_prefix = env_prefix .. "FUNC_COUNT=" .. func_count .. " "
    end
    if iters and #iters > 0 then
        env_prefix = env_prefix .. "ITERS=" .. iters .. " "
    end
    env_prefix = env_prefix .. "FS_BENCH_DATA_DIR=" .. fs_data_dir .. " "

    local cmd = string.format("cd %q && %s%q > %q 2>&1", root_dir, env_prefix, bench_bin, uwvm_log)
    run_or_die(cmd)

    -- Extract machine-readable lines.
    local summary_lines = {}
    for line in io.lines(uwvm_log) do
        if line:match("^uwvm2_fs ") then
            table.insert(summary_lines, line)
        end
    end
    if #summary_lines == 0 then
        error("no 'uwvm2_fs ' lines found in " .. uwvm_log)
    end
    write_file(uwvm_summary, table.concat(summary_lines, "\n") .. "\n")

    print()
    print("uwvm2 per-scenario results (raw):")
    for _, line in ipairs(summary_lines) do
        print("  " .. line)
    end
end

-- Ensure varint-simd repository ----------------------------------------------

local function ensure_varint_repo()
    local env_dir = os.getenv("VARINT_SIMD_DIR")
    if env_dir and #env_dir > 0 then
        if not exists(env_dir) then
            error("VARINT_SIMD_DIR is set but directory does not exist: " .. env_dir)
        end
        return abspath(env_dir)
    end

    -- Default: clone into ./outputs/varint-simd next to this script.
    local default_dir = output_dir .. "/varint-simd"
    if exists(default_dir) then
        return default_dir
    end

    print("==> Cloning varint-simd into " .. default_dir)
    local clone_cmd = string.format('git clone --depth 1 "https://github.com/as-com/varint-simd.git" %q', default_dir)
    run_or_die(clone_cmd)
    return default_dir
end

-- Run varint-simd fair benchmark --------------------------------------------

local function run_varint_bench(varint_dir)
    print()
    print("==> Running Rust varint-simd fair benchmark (shared data files)")

    -- Extend RUSTFLAGS with an explicit CPU/feature set for this invocation.
    local rustflags = os.getenv("RUSTFLAGS") or ""

    local simd_level = os.getenv("UWVM2_SIMD_LEVEL") or "native"
    local extra
    if simd_level == "native" then
        extra = "-C target-cpu=native"
    else
        -- Map UWVM2_SIMD_LEVEL to an explicit x86-64 + feature set.
        local features
        if simd_level == "sse2" then
            features = "+sse2"
        elseif simd_level == "sse3" then
            features = "+sse2,+sse3"
        elseif simd_level == "ssse3" then
            features = "+sse2,+sse3,+ssse3"
        elseif simd_level == "sse4" then
            features = "+sse2,+sse3,+ssse3,+sse4.1,+sse4.2"
        elseif simd_level == "avx" then
            features = "+sse2,+sse3,+ssse3,+sse4.1,+sse4.2,+avx"
        elseif simd_level == "avx2" then
            features = "+sse2,+sse3,+ssse3,+sse4.1,+sse4.2,+avx,+avx2"
        elseif simd_level == "avx512bw" then
            features = "+sse2,+sse3,+ssse3,+sse4.1,+sse4.2,+avx,+avx2,+avx512bw"
        elseif simd_level == "avx512vbmi" then
            -- AVX-512VBMI level also enables AVX-512BW and both VBMI/VBMI2.
            features = "+sse2,+sse3,+ssse3,+sse4.1,+sse4.2,+avx,+avx2,+avx512bw,+avx512vbmi,+avx512vbmi2"
        else
            features = nil
        end

        if features then
            extra = "-C target-cpu=x86-64 -C target-feature=" .. features
        else
            extra = "-C target-cpu=native"
        end
    end

    if #rustflags > 0 then
        rustflags = rustflags .. " " .. extra
    else
        rustflags = extra
    end

    local fs_data_dir = data_dir

    -- The fair benchmark is a small Rust binary in ./varint-simd-fair that
    -- depends on the cloned varint-simd crate and reads the same LEB128
    -- streams as the C++ benchmark.
    local fair_dir = script_dir .. "/varint-simd-fair"

    local cmd = string.format(
        'cd %q && FS_BENCH_DATA_DIR=%q RUSTFLAGS=%q CARGO_TERM_COLOR=never cargo run --release > %q 2>&1',
        fair_dir,
        fs_data_dir,
        rustflags,
        varint_log
    )
    run_or_die(cmd)

    print("varint-simd raw output saved to:")
    print("  " .. varint_log)
end

-- Parse varint-simd fair decode timings --------------------------------------

local function parse_varint_fair()
    local unsafe = {}

    for line in io.lines(varint_log) do
        -- Lines look like:
        --   varint_simd_fs scenario=u8_2b impl=unsafe values=... total_ns=... \
        --                   ns_per_value=... avg_bytes_value=... gib_per_s=...
        local scenario = line:match("scenario=([^%s]+)")
        local impl = line:match("impl=([^%s]+)")
        local ns_str = line:match("ns_per_value=([^%s]+)")
        if scenario and impl == "unsafe" and ns_str then
            unsafe[scenario] = tonumber(ns_str)
        end
    end

    local summary_lines = {}
    for scenario, ns in pairs(unsafe) do
        summary_lines[#summary_lines + 1] = string.format("scenario=%s impl=unsafe ns_per_value=%.6f", scenario, ns)
    end
    table.sort(summary_lines)
    write_file(varint_summary, table.concat(summary_lines, "\n") .. (next(summary_lines) and "\n" or ""))

    return unsafe
end

-- Parse uwvm2 SIMD timings per scenario --------------------------------------

local function parse_uwvm_simd()
    local simd = {}
    for line in io.lines(uwvm_summary) do
        local scenario = line:match("scenario=([^%s]+)")
        local impl = line:match("impl=([^%s]+)")
        local ns_str = line:match("ns_per_value=([^%s]+)")
        if scenario and impl == "simd" and ns_str then
            simd[scenario] = tonumber(ns_str)
        end
    end
    return simd
end

-- Pretty comparison per scenario ---------------------------------------------

local function compare_one(scenario, desc, uwvm_simd_ns, varint_unsafe, bench_unsafe)
    print()
    print(string.rep("=", 80))
    print("Scenario: " .. scenario)
    print("  " .. desc)

    local ns_uwvm = uwvm_simd_ns[scenario]
    if not ns_uwvm then
        print("  uwvm2 : <no SIMD result found>")
        return
    end
    print(string.format("  uwvm2 : ns_per_value = %.6f", ns_uwvm))

    local ns_varint_u = varint_unsafe[scenario]
    if not ns_varint_u then
        print(string.format("  varint-simd (unsafe) : <bench %s not found in varint-simd output>", bench_unsafe))
    else
        print(string.format("  varint-simd (unsafe, fair-file) : ns_per_value ≈ %.6f",
                            ns_varint_u))
        local ratio = ns_uwvm / ns_varint_u
        print(string.format("  ratio (unsafe)       : uwvm2 / varint-simd ≈ %.3f  ( <1 means uwvm2 is faster )", ratio))
    end
end

-- Main -----------------------------------------------------------------------

local function main()
    print("uwvm2 function_section uleb128<u32> SIMD benchmark (varint-simd style, Lua driver)")
    print(string.format("  script_dir = %s", script_dir))
    print(string.format("  root_dir   = %s", root_dir))
    print(string.format("  data_dir   = %s", data_dir))

    build_uwvm_bench()
    run_uwvm_bench()

    local varint_dir = ensure_varint_repo()
    run_varint_bench(varint_dir)

    -- Parse results.
    local uwvm_simd_ns = parse_uwvm_simd()
    local varint_unsafe = parse_varint_fair()

    print()
    print(string.rep("=", 80))
    print("Per-scenario comparison (ns/value, smaller is faster)")
    print(string.rep("=", 80))

    compare_one(
        "u8_1b",
        "typeidx < 2^7, always 1-byte encoding (fast path, zero-copy u8 view)",
        uwvm_simd_ns,
        varint_unsafe,
        "unused"
    )

    compare_one(
        "u8_2b",
        "2^7 ≤ typeidx < 2^8, 1–2 byte encodings, main fair comparison against varint-u8/decode",
        uwvm_simd_ns,
        varint_unsafe,
        "unused"
    )

    compare_one(
        "u16_2b",
        "2^8 ≤ typeidx < 2^14, 1–2 byte encodings, rare in real-world wasm but useful as a stress case",
        uwvm_simd_ns,
        varint_unsafe,
        "unused"
    )

    print()
    print(string.rep("=", 80))
    print("Notes")
    print(string.rep("=", 80))
    print("  u8_1b  : specialized fast path (SIMD + zero-copy view); not a 1:1 fair decoder comparison.")
    print("  u8_2b  : main fair-comparison scenario (1–2 byte decode into array, vs varint-u8/decode).")
    print("  u16_2b : decodes into u16 array; rare in real wasm, closer to a stress/completeness case.")

    print()
    print("Done.")
end

main()


