#!/usr/bin/env lua

-- compare_varint_simd.lua
--
-- Lua driver to:
--   - build and run the uwvm2 function_section uleb128<u32> benchmark
--   - fetch (if necessary) and run the Rust varint-simd benchmark
--   - extract per-value timings and compare them per scenario
--
-- This is the Lua equivalent of the shell script compare_varint_simd.sh, but
-- with some extra explanation about how to interpret each scenario:
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
--   CXX              : C++ compiler (default: clang++)
--   CXXFLAGS_EXTRA   : extra C++ flags appended after the defaults
--   VARINT_SIMD_DIR  : path to an existing varint-simd checkout
--                       (default: auto-clone into ./outputs/varint-simd next to this file)
--   FUNC_COUNT       : forwarded to uwvm2 benchmark (default: 1_000_000 in C++)
--   ITERS            : forwarded to uwvm2 benchmark (default: 20 in C++)
--   RUSTFLAGS        : existing flags will be preserved and extended with
--                      "-C target-cpu=native" for the varint-simd bench run.
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

local bench_src = root_dir .. "/benchmark/0002.parser/0001.function_section/FunctionSectionVarintSimd.cc"
local bench_bin = output_dir .. "/FunctionSectionVarintSimd"

local uwvm_log = output_dir .. "/uwvm2_uleb128_bench.log"
local uwvm_summary = output_dir .. "/uwvm2_uleb128_summary.txt"
local varint_log = output_dir .. "/varint_simd_bench.log"
local varint_decode_unsafe = output_dir .. "/varint_simd_decode_unsafe.txt"
local varint_decode_safe = output_dir .. "/varint_simd_decode_safe.txt"

-- Build uwvm2 C++ benchmark --------------------------------------------------

local function build_uwvm_bench()
    print("==> Building uwvm2 FunctionSection uleb128 benchmark (C++)")

    local cxx = os.getenv("CXX") or "clang++"

    local default_flags = {
        "-std=c++2c",
        "-O3",
        "-ffast-math",
        "-march=native",
        "-fno-rtti",
        "-fno-unwind-tables",
        "-fno-asynchronous-unwind-tables",
    }

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

    -- We forward FUNC_COUNT / ITERS if present; otherwise the C++ binary will
    -- fall back to its built-in defaults (1_000_000 and 20).
    local env_prefix = ""
    if func_count and #func_count > 0 then
        env_prefix = env_prefix .. "FUNC_COUNT=" .. func_count .. " "
    end
    if iters and #iters > 0 then
        env_prefix = env_prefix .. "ITERS=" .. iters .. " "
    end

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

-- Run varint-simd benchmarks -------------------------------------------------

local function run_varint_bench(varint_dir)
    print()
    print("==> Running Rust varint-simd benchmarks")

    -- Extend RUSTFLAGS with -C target-cpu=native for this invocation.
    local rustflags = os.getenv("RUSTFLAGS")
    if rustflags and #rustflags > 0 then
        rustflags = rustflags .. " -C target-cpu=native"
    else
        rustflags = "-C target-cpu=native"
    end

    -- NOTE: varint-simd's Criterion-based bench harness does not currently
    -- accept arbitrary positional filters the way some other setups do, so we
    -- invoke the full varint_bench suite here and later extract only the
    -- decode benchmarks we care about from the log. This keeps the script
    -- robust even if the upstream CLI changes.
    local cmd = string.format('cd %q && RUSTFLAGS=%q CARGO_TERM_COLOR=never cargo bench --bench varint_bench > %q 2>&1',
                              varint_dir, rustflags, varint_log)
    print(">> " .. cmd)
    local ok, why, code = os.execute(cmd)
    if not (ok == true or ok == 0) then
        print()
        print("WARNING: failed to run Rust varint-simd benchmarks.")
        print("         The varint-simd crate currently targets x86/x86_64")
        print("         with SIMD extensions (e.g. SSE2/AVX2). On non-x86")
        print("         platforms these intrinsics are unavailable, so the")
        print("         Rust benchmark build will fail and cross-project")
        print("         comparisons cannot be produced.")
        print()
        print("         uwvm2 native SIMD/scalar results above are still valid,")
        print("         but no varint-simd baseline is available on this host.")
        return false
    end

    print("varint-simd raw output saved to:")
    print("  " .. varint_log)
    return true
end

-- Parse varint-simd decode timings -------------------------------------------

local function parse_varint_decode()
    local unsafe = {}
    local safe = {}

    local current_unsafe = nil
    local current_safe = nil

    for line in io.lines(varint_log) do
        -- Match anywhere in the line to be robust against possible prefixes
        -- (indentation, ANSI color codes, etc.).
        local name_u = line:match("(varint%-u%d+/decode/varint%-simd/unsafe)")
        if name_u then
            current_unsafe = name_u
        elseif current_unsafe and line:match("time:%s*%[") then
            -- Example line:
            --   time:   [730.59 ns 735.41 ns 741.03 ns]
            --   time:   [1.2588 µs 1.2642 µs 1.2690 µs]
            -- Capture only the first numeric value and its unit (ns/µs/...),
            -- without insisting on a trailing ']' immediately after the unit.
            local val_str, unit = line:match("%[%s*([%d%.]+)%s*(%S+)")
            if val_str and unit then
                local val = tonumber(val_str)
                if unit:find("µs", 1, true) then
                    val = val * 1000.0
                end
                -- Each iteration decodes SEQUENCE_LEN (=256) elements in varint-simd.
                unsafe[current_unsafe] = val / 256.0
            end
            current_unsafe = nil
        end

        local name_s = line:match("(varint%-u%d+/decode/varint%-simd/safe)")
        if name_s then
            current_safe = name_s
        elseif current_safe and line:match("time:%s*%[") then
            local val_str, unit = line:match("%[%s*([%d%.]+)%s*(%S+)")
            if val_str and unit then
                local val = tonumber(val_str)
                if unit:find("µs", 1, true) then
                    val = val * 1000.0
                end
                safe[current_safe] = val / 256.0
            end
            current_safe = nil
        end
    end

    -- Persist small summaries (optional, mainly for debugging / inspection).
    local buf_unsafe = {}
    for name, ns in pairs(unsafe) do
        buf_unsafe[#buf_unsafe + 1] = string.format("%s %.6f", name, ns)
    end
    table.sort(buf_unsafe)
    write_file(varint_decode_unsafe, table.concat(buf_unsafe, "\n") .. (next(buf_unsafe) and "\n" or ""))

    local buf_safe = {}
    for name, ns in pairs(safe) do
        buf_safe[#buf_safe + 1] = string.format("%s %.6f", name, ns)
    end
    table.sort(buf_safe)
    write_file(varint_decode_safe, table.concat(buf_safe, "\n") .. (next(buf_safe) and "\n" or ""))

    return unsafe, safe
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

    local ns_varint_u = varint_unsafe[bench_unsafe]
    if not ns_varint_u then
        print(string.format("  varint-simd (unsafe) : <bench %s not found in varint-simd output>", bench_unsafe))
    else
        print(string.format("  varint-simd (unsafe) : %s, ns_per_value ≈ %.6f",
                            bench_unsafe, ns_varint_u))
        local ratio = ns_uwvm / ns_varint_u
        print(string.format("  ratio (unsafe)       : uwvm2 / varint-simd ≈ %.3f  ( <1 means uwvm2 is faster )", ratio))
    end
end

-- Main -----------------------------------------------------------------------

local function main()
    print("uwvm2 function_section uleb128<u32> SIMD benchmark (varint-simd style, Lua driver)")
    print(string.format("  script_dir = %s", script_dir))
    print(string.format("  root_dir   = %s", root_dir))

    build_uwvm_bench()
    run_uwvm_bench()

    local varint_dir = ensure_varint_repo()
    local ok_varint = run_varint_bench(varint_dir)
    if not ok_varint then
        print()
        print(string.rep("=", 80))
        print("Skipping varint-simd decode comparison because varint-simd")
        print("benches could not be built on this non-x86 SIMD platform.")
        print(string.rep("=", 80))
        print()
        print("Done.")
        return
    end

    -- Parse results.
    local uwvm_simd_ns = parse_uwvm_simd()
    local varint_unsafe, varint_safe = parse_varint_decode()

    print()
    print(string.rep("=", 80))
    print("Per-scenario comparison (ns/value, smaller is faster)")
    print(string.rep("=", 80))

    -- Mapping to varint-simd benchmark names:
    --   - u8_1b / u8_2b -> varint-u8/decode/varint-simd/{unsafe,safe}
    --   - u16_2b        -> varint-u16/decode/varint-simd/{unsafe,safe}

    compare_one(
        "u8_1b",
        "typeidx < 2^7, always 1-byte encoding (fast path, zero-copy u8 view)",
        uwvm_simd_ns,
        varint_unsafe,
        "varint-u8/decode/varint-simd/unsafe"
    )

    compare_one(
        "u8_2b",
        "2^7 ≤ typeidx < 2^8, 1–2 byte encodings, main fair comparison against varint-u8/decode",
        uwvm_simd_ns,
        varint_unsafe,
        "varint-u8/decode/varint-simd/unsafe"
    )

    compare_one(
        "u16_2b",
        "2^8 ≤ typeidx < 2^14, 1–2 byte encodings, rare in real-world wasm but useful as a stress case",
        uwvm_simd_ns,
        varint_unsafe,
        "varint-u16/decode/varint-simd/unsafe"
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

