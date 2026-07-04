set_xmakever("2.9.8")

set_project("uwvm")
set_policy("check.auto_ignore_flags", false)

-- Version
set_version("2.0.3")
add_defines("UWVM_VERSION_X=2")
add_defines("UWVM_VERSION_Y=0")
add_defines("UWVM_VERSION_Z=3")
add_defines("UWVM_VERSION_S=0")
add_defines("UWVM_VERSION_DEV")

set_allowedplats("windows", "mingw", "cygwin", "linux", "djgpp", "unix", "bsd", "freebsd", "dragonflybsd", "netbsd",
	"openbsd", "macosx", "iphoneos", "watchos", "wasm-wasi", "wasm-wasip1", "wasm-wasip2", "wasm-wasip3",
	"wasm-wasi-threads", "wasm-wasip1-threads", "wasm-wasip2-threads", "wasm-wasip3-threads", "wasm-emscripten",
	"serenity", "sun", "cross", "none")

includes("xmake/impl.lua")
includes("xmake/platform/impl.lua")
add_moduledirs("xmake")

-- Currently, there are no plugins.
-- add_plugindirs("xmake/plugins")

set_defaultmode("release")
set_allowedmodes(support_rules_table)

function def_build(opt)
	opt = opt or {}
	if is_mode("debug") then
		add_rules("debug")
	elseif is_mode("release") then
		add_rules("release")
	elseif is_mode("minsizerel") then
		add_rules("minsizerel")
	elseif is_mode("releasedbg") then
		add_rules("releasedbg")
	end

	set_languages("c23", "cxx26")

	set_encodings("utf-8")
	set_warnings("all", "extra", "pedantic", "error")

	local enable_cxx_module = get_config("use-cxx-module")
	if enable_cxx_module then
		add_defines("UWVM_MODULE")
		set_policy("build.c++.modules", true)
		-- set_policy("build.c++.modules.std", true)
	end

	local use_stdlib = get_config("stdlib")
	if use_stdlib ~= "default" and use_stdlib then
		local flags = "-stdlib=" .. use_stdlib
		add_cxflags(flags)
		add_ldflags(flags)
	end

	local disable_cpp_exceptions = get_config("fno-exceptions")
	if disable_cpp_exceptions then
		set_exceptions("no-cxx")
	end

	local enable_debug_timer = get_config("debug-timer")
	if enable_debug_timer then
		add_defines("UWVM_TIMER")
	end

	local wasm_memory_model = get_config("wasm-memory-model") or "default"
	if wasm_memory_model == "default" then
		-- Keep the existing automatic backend selection.
	elseif wasm_memory_model == "mmap" then
		add_defines("UWVM_FORCE_USE_MMAP")
	elseif wasm_memory_model == "multi-thread-alloc" then
		add_defines("UWVM_FORCE_DISABLE_MMAP")
		add_defines("UWVM_USE_MULTITHREAD_ALLOCATOR")
	elseif wasm_memory_model == "single-thread-alloc" then
		add_defines("UWVM_FORCE_DISABLE_MMAP")
	else
		error("unsupported wasm-memory-model: " .. tostring(wasm_memory_model))
	end

	local disable_local_imported_wasip1 = get_config("disable-local-imported-wasip1")
	if disable_local_imported_wasip1 then
		add_defines("UWVM_DISABLE_LOCAL_IMPORTED_WASIP1")
	end

	local enable_local_imported_wasip1_wasm64 = get_config("enable-local-imported-wasip1-wasm64")
	if enable_local_imported_wasip1_wasm64 then
		add_defines("UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64")
	end

	local enable_local_imported_wasip1_socket = get_config("enable-local-imported-wasip1-socket")
	if not enable_local_imported_wasip1_socket or enable_local_imported_wasip1_socket == "none" then
		-- do nothing
	elseif enable_local_imported_wasip1_socket == "wasip1" then
		add_defines("UWVM_ENABLE_WASIP1_SOCKET")
	elseif enable_local_imported_wasip1_socket == "wasix" then
		add_defines("UWVM_ENABLE_WASIX_VER_WASIP1_SOCKET")
	end

	local execution_int = get_config("execution-int")
	if not execution_int or execution_int == "none" then
		add_defines("UWVM_DISABLE_INT")
	elseif execution_int == "default" then
		add_defines("UWVM_USE_DEFAULT_INT")
	elseif execution_int == "uwvm-int" then
		add_defines("UWVM_USE_UWVM_INT")
	end

	local execution_jit = get_config("execution-jit")
	if not execution_jit or execution_jit == "none" then
		add_defines("UWVM_DISABLE_JIT")
	elseif execution_jit == "default" then
		add_defines("UWVM_USE_DEFAULT_JIT")
		add_options("llvm-jit-env")
	elseif execution_jit == "llvm" then
		add_defines("UWVM_USE_LLVM_JIT")
		add_options("llvm-jit-env")
	end

	if execution_jit == "default" or execution_jit == "llvm" then
		add_cxxflags("-Wno-deprecated-declarations", { force = true })

		on_load(function(target)
			local utility = import("utility.utility", { anonymous = true })
			local llvm_jit_options = utility.get_llvm_jit_options()
			for _, field in ipairs({
				"linkdirs",
				"frameworkdirs",
				"frameworks",
				"links",
				"syslinks",
				"ldflags",
				"shflags"
			}) do
				for _, value in ipairs(llvm_jit_options[field] or {}) do
					if field == "ldflags" or field == "shflags" then
						target:add(field, value, { force = true })
					else
						target:add(field, value)
					end
				end
			end
			for _, value in ipairs(os.argv(llvm_jit_options.native_codegen_linkflags or "")) do
				target:add("ldflags", value, { force = true })
			end
		end)
	end

	if (execution_jit == "default" or execution_jit == "llvm") and (is_plat("macosx") or is_plat("iphoneos") or is_plat("watchos")) then
		on_load(function(target)
			local utility = import("utility.utility", { anonymous = true })
			-- Keep the runtime search path aligned with the LLVM installation
			-- selected by llvm-config, rather than inferring it from the C++ driver.
			local llvm_jit_options = utility.get_llvm_jit_options()
			for _, llvm_libdir in ipairs(llvm_jit_options.linkdirs or {}) do
				if llvm_libdir and llvm_libdir ~= "" then
					target:add("ldflags", "-Wl,-rpath," .. llvm_libdir, { force = true })
				end
			end
		end)
	end

	local debug_int = get_config("debug-int")
	if debug_int then
		add_defines("UWVM_ENABLE_DEBUG_INT")
	else
		add_defines("UWVM_DISABLE_DEBUG_INT")
	end

	local heavy_combine_ops_mode = get_config("enable-uwvm-int-combine-ops")

	if heavy_combine_ops_mode ~= "none" then
		-- Soft/light combine is enabled by default unless explicitly set to "none".
		add_defines("UWVM_ENABLE_UWVM_INT_COMBINE_OPS")

		if heavy_combine_ops_mode == "heavy" or heavy_combine_ops_mode == "extra" then
			add_defines("UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS")
			if heavy_combine_ops_mode == "extra" then
				add_defines("UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS")
			end
		end
	end

	local delay_local_mode = get_config("enable-uwvm-int-delay-local")
	if delay_local_mode == "soft" or delay_local_mode == "heavy" then
		add_defines("UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT")
		if delay_local_mode == "heavy" then
			add_defines("UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY")
		end
	end

	local enable_uwvm_int_instruction_reorder = get_config("enable-uwvm-int-instruction-reorder")
	if enable_uwvm_int_instruction_reorder then
		add_defines("UWVM_ENABLE_UWVM_INT_INSTRUCTION_REORDER")
	end

	local enable_uwvm_int_loop_unwind = get_config("enable-uwvm-int-loop-unwind")
	if enable_uwvm_int_loop_unwind then
		add_defines("UWVM_ENABLE_UWVM_INT_LOOP_UNWIND")
	end

	local use_thread_local = get_config("use-thread-local")
	if use_thread_local then
		add_defines("UWVM_USE_THREAD_LOCAL")
	end

	local detailed_debug_check = get_config("detailed-debug-check")
	if is_mode("debug") and detailed_debug_check then
		add_defines("UWVM_ENABLE_DETAILED_DEBUG_CHECK")
	end

	if is_plat("windows") then
		windows_target()
	elseif is_plat("mingw") then
		mingw_target()
	elseif is_plat("linux") then
		linux_target()
	elseif is_plat("macosx", "iphoneos", "watchos") then
		darwin_target(opt)
	elseif is_plat("djgpp") then
		djgpp_target()
	elseif is_plat("unix", "bsd", "freebsd", "dragonflybsd", "netbsd", "openbsd") then
		bsd_target()
	elseif is_plat("wasm-wasi", "wasm-wasip1", "wasm-wasip2", "wasm-wasip3", "wasm-wasi-threads", "wasm-wasip1-threads", "wasm-wasip2-threads", "wasm-wasip3-threads") then
		wasm_wasi_target()
	elseif is_plat("wasm-emscripten") then
		wasm_emscripten_target()
	elseif is_plat("cygwin") then
		cygwin_target()
	elseif is_plat("none") then
		none_target()
	end

	local use_llvm_compiler = get_config("use-llvm-compiler")
	if use_llvm_compiler then
		local llvm_target = get_config("llvm-target")
		if llvm_target ~= "detect" and llvm_target then
			local llvm_target_cvt = "--target=" .. llvm_target
			add_cxflags(llvm_target_cvt, { force = true })
			add_ldflags(llvm_target_cvt, { force = true })
		end

		-- Since neither LLVM nor Wextra supports this parameter by default, this addition prevents compilation.
		add_cxflags("-Wimplicit-fallthrough", { force = true })
	else
		-- Keep the non-LLVM toolchain path portable: some Clang builds do not
		-- recognize this warning class at all, which turns the suppression into a
		-- hard error under `-Werror`.
	end

	before_build(
		function(target)
			try
			{
				function()
					local function git_available()
						local result = os.iorun("git --version")
						return result and not result:find("fatal:") and not result:find("not found")
					end

					local function is_git_repo()
						if not git_available() then return false end
						local result = os.iorun("git rev-parse --is-inside-work-tree")
						return result and result:trim() == "true"
					end

					if not git_available() or not is_git_repo() then
						return
					end

					-- General Git commands execute functions and return results or nil
					local function git_command(cmd)
						local result = os.iorun(cmd)
						if result and not result:find("fatal:") then
							return result:trim()
						end
						return nil
					end

					-- Get Commit ID
					local commit_id = git_command("git rev-parse HEAD") or "unknown"

					-- Get the current branch name (may be empty, such as the separation HEAD status)
					local current_branch = git_command("git branch --show-current")

					-- Dynamically resolve the associated remote name
					local remote_name = nil
					if current_branch then
						-- Get the traced remote name through git config
						remote_name = git_command("git config branch." .. current_branch .. ".remote")
					end

					-- If the remote name is not found, try to resolve it through HEAD upstream
					local upstream_branch = git_command("git rev-parse --abbrev-ref --symbolic-full-name HEAD") or "unknown"
					if not remote_name then
						local upstream_branch_tmp = git_command("git rev-parse --abbrev-ref --symbolic-full-name @{u}")
						if upstream_branch_tmp then
							-- Resolve the remote name of the upstream branch (format: remote_name/branch_name)
							remote_name = upstream_branch_tmp:match("^(.-)/")
						end
					end

					-- If there is still no remote name, go back to the first remote or default value
					local remote_url = "unknown"
					if remote_name then
						remote_url = git_command("git remote get-url " .. remote_name) or "unknown"
					else
						-- Get all remote lists, the first one
						local remotes = git_command("git remote")
						if remotes then
							local first_remote = remotes:match("[^\r\n]+")
							if first_remote then
								remote_url = git_command("git remote get-url " .. first_remote) or "unknown"
							end
						end
					end

					-- Get submission timestamp (UTC)
					local timestamp_str = git_command("git log -1 --format=%ct HEAD")
					local timestamp = tonumber(timestamp_str) or 0

					-- Convert timestamp to date string (UTC time zone)
					local commit_date = "unknown"
					if timestamp > 0 then
						commit_date = os.date("!%Y-%m-%d", timestamp) -- Attention '! 'means forcing UTC
					end

					local is_dirty = false
					local status_output = git_command("git status --porcelain")
					if status_output and status_output ~= "" then
						is_dirty = true -- There are uncommitted modifications or untracked files
					end

					target:add("defines", "UWVM_GIT_COMMIT_ID=u8\"" .. commit_id .. "\"")
					target:add("defines", "UWVM_GIT_REMOTE_URL=u8\"" .. remote_url .. "\"")
					target:add("defines", "UWVM_GIT_COMMIT_DATA=u8\"" .. commit_date .. "\"")
					target:add("defines", "UWVM_GIT_UPSTREAM_BRANCH=u8\"" .. upstream_branch .. "\"")
					if is_dirty then
						target:add("defines", "UWVM_GIT_HAS_UNCOMMITTED_MODIFICATIONS")
					end
				end,
				catch
				{
					function()
						return nil
					end
				}
			}
		end
	)
end

local uwvm_uses_llvm_jit = (get_config("execution-jit") == "llvm") or (get_config("execution-jit") == "default")
local uwvm_has_runtime_backend = (get_config("execution-int") == "uwvm-int" or get_config("execution-int") == "default") or uwvm_uses_llvm_jit

if uwvm_uses_llvm_jit and get_config("openssl-root") == "default" then
	add_requires("openssl", {configs = {shared = false}})
end

function uwvm_add_llvm_jit_cache_openssl()
	add_defines("UWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519")
	local openssl_root = get_config("openssl-root")
	if openssl_root and openssl_root ~= "default" then
		local openssl_libdir = path.join(openssl_root, "lib")
		if not os.isdir(openssl_libdir) then
			openssl_libdir = openssl_root
		end
		add_includedirs(path.join(openssl_root, "include"))
		local static_mode = get_config("static")
		if static_mode == "non-system" or static_mode == "compiler" then
			local ssl_archive = path.join(openssl_libdir, "libssl.a")
			local crypto_archive = path.join(openssl_libdir, "libcrypto.a")
			if os.isfile(ssl_archive) and os.isfile(crypto_archive) then
				add_ldflags(ssl_archive, crypto_archive, { force = true })
				if is_plat("windows", "mingw") then
					add_ldflags("-lcrypt32", "-lgdi32", "-ladvapi32", "-luser32", "-lws2_32", { force = true })
				end
			else
				add_linkdirs(openssl_libdir)
				add_links("ssl", "crypto")
			end
		else
			add_linkdirs(openssl_libdir)
			add_links("ssl", "crypto")
		end
	else
		add_packages("openssl")
	end
end

target("uwvm")
	set_kind("binary")
	def_build({ skip_static_libcxx = uwvm_uses_llvm_jit })

	-- uwvm uses precise floating-point model to ensure determinism.
	set_fpmodels("precise") 

	local is_debug_mode = is_mode("debug")  -- public all modules in debug mode
	local enable_cxx_module = get_config("use-cxx-module")

	-- third-parties/fast_io
	add_includedirs("third-parties/fast_io/include")

	if enable_cxx_module then
		add_files("third-parties/fast_io/share/fast_io/fast_io.cppm", { public = is_debug_mode })
		add_files("third-parties/fast_io/share/fast_io/fast_io_crypto.cppm", { public = is_debug_mode })
	end

	-- third-parties/bizwen
	add_includedirs("third-parties/bizwen/include")

	-- third-parties/boost
	add_includedirs("third-parties/boost_unordered/include")

	if uwvm_uses_llvm_jit then
		uwvm_add_llvm_jit_cache_openssl()
	end

	-- uwvm
	add_defines("UWVM=2")

	-- src
	add_includedirs("src/")

	add_headerfiles("src/**.h")

	if enable_cxx_module then
		-- uwvm predefine
		add_files("src/uwvm2/uwvm_predefine/**.cppm", { public = is_debug_mode })

		-- utils
		add_files("src/uwvm2/utils/**.cppm", { public = is_debug_mode })

		-- object
		add_files("src/uwvm2/object/**.cppm", { public = is_debug_mode })

		-- imported
		add_files("src/uwvm2/imported/**.cppm", { public = is_debug_mode })

		-- wasm parser
		add_files("src/uwvm2/parser/**.cppm", { public = is_debug_mode })

		-- validation
		add_files("src/uwvm2/validation/**.cppm", { public = is_debug_mode })

		-- uwvm
		add_files("src/uwvm2/uwvm/**.cppm", { public = is_debug_mode })
	end

	if enable_cxx_module then
		-- uwvm main
		add_files("src/uwvm2/uwvm/main.module.cpp")
		add_files("src/uwvm2/uwvm/host_api.module.cpp")
	else
		-- uwvm main
		add_files("src/uwvm2/uwvm/main.default.cpp")
		add_files("src/uwvm2/uwvm/host_api.default.cpp")
	end

	-- uwvm_runtime also provides non-backend host API shims used by uwvm.
	add_deps("uwvm_runtime")

target_end()

-- uwvm_runtime: build the shared runtime unit separately so it can use its own FP flags.
target("uwvm_runtime")
	set_kind("object")
	def_build({ skip_static_libcxx = true })

	-- Interpreter/runtime execution unit: disable observable floating-point side effects
	-- (errno, traps, dynamic rounding, and FMA contraction) to preserve WebAssembly FP semantics.
	add_cxflags("-fno-math-errno", "-fno-trapping-math", "-fno-rounding-math", "-ffp-contract=off")

	-- third-parties/fast_io
	add_includedirs("third-parties/fast_io/include")

	if enable_cxx_module then
		add_files("third-parties/fast_io/share/fast_io/fast_io.cppm", { public = is_debug_mode })
		add_files("third-parties/fast_io/share/fast_io/fast_io_crypto.cppm", { public = is_debug_mode })
	end

	-- third-parties/bizwen
	add_includedirs("third-parties/bizwen/include")

	-- third-parties/boost
	add_includedirs("third-parties/boost_unordered/include")

	if uwvm_uses_llvm_jit then
		uwvm_add_llvm_jit_cache_openssl()
	end

	-- src
	add_includedirs("src/")

	add_defines("UWVM=2")

	if enable_cxx_module then
		-- uwvm predefine
		add_files("src/uwvm2/uwvm_predefine/**.cppm", { public = is_debug_mode })

		-- utils
		add_files("src/uwvm2/utils/**.cppm", { public = is_debug_mode })

		-- object
		add_files("src/uwvm2/object/**.cppm", { public = is_debug_mode })

		-- imported
		add_files("src/uwvm2/imported/**.cppm", { public = is_debug_mode })

		-- wasm parser
		add_files("src/uwvm2/parser/**.cppm", { public = is_debug_mode })

		-- validation
		add_files("src/uwvm2/validation/**.cppm", { public = is_debug_mode })

		-- uwvm
		add_files("src/uwvm2/uwvm/**.cppm", { public = is_debug_mode })

		-- runtime
		add_files("src/uwvm2/runtime/**.cppm", { public = is_debug_mode })
	end

	if uwvm_has_runtime_backend then
		if enable_cxx_module then
			add_files("src/uwvm2/runtime/lib/uwvm_runtime.module.cpp")
		else
			add_files("src/uwvm2/runtime/lib/uwvm_runtime.default.cpp")
		end
	else
		if enable_cxx_module then
			add_files("src/uwvm2/runtime/lib/uwvm_stub_runtime.module.cpp")
		else
			add_files("src/uwvm2/runtime/lib/uwvm_stub_runtime.default.cpp")
		end
	end

target_end()

-- test unit
for _, file in ipairs(os.files("test/**.cc")) do
	local is_0013_uwvm_int = (string.find(file, "test/0013.uwvm_int/", 1, true) ~= nil) or (string.find(file, "test\\0013.uwvm_int\\", 1, true) ~= nil)
	local is_0013_uwvm_int_lazy = (string.find(file, "test/0013.uwvm_int/lazy/", 1, true) ~= nil) or (string.find(file, "test\\0013.uwvm_int\\lazy\\", 1, true) ~= nil)
	local is_0014_llvm_jit = (string.find(file, "test/0014.llvm_jit/", 1, true) ~= nil) or (string.find(file, "test\\0014.llvm_jit\\", 1, true) ~= nil)
	local is_0015_backend_fuzzer = (string.find(file, "test/0015.backend_fuzzer/", 1, true) ~= nil) or (string.find(file, "test\\0015.backend_fuzzer\\", 1, true) ~= nil)
	local is_libfuzzer = (string.find(file, "test/0009.libfuzzer/", 1, true) ~= nil) or
		(string.find(file, "test\\0009.libfuzzer\\", 1, true) ~= nil)
	local is_llvm_jit_test = is_0014_llvm_jit or (string.find(file, "llvm_jit", 1, true) ~= nil)
	local test_libfuzzer = get_config("test-libfuzzer")
	local is_int_backend = get_config("execution-int") == "uwvm-int" or get_config("execution-int") == "default"

	if not ((is_0013_uwvm_int and not get_config("enable-test-uwvm-int")) or
		(is_0013_uwvm_int_lazy and not is_int_backend) or
		(is_0014_llvm_jit and not get_config("enable-test-llvm-jit")) or
		is_0015_backend_fuzzer) then
		local name = path.basename(file)
		target(name)
		local group = path.directory(file):gsub("\\\\", "/")
		set_group(group)
		set_kind("binary")
		def_build({ skip_static_libcxx = (is_libfuzzer and test_libfuzzer) or is_llvm_jit_test })

		if ((get_config("execution-jit") == "llvm") or (get_config("execution-jit") == "default")) and
			is_llvm_jit_test then
			add_deps("uwvm_runtime")
			add_deps("uwvm")
		end

		if is_0013_uwvm_int_lazy then
			add_deps("uwvm_runtime")
			add_deps("uwvm")
		end

		-- uwvm uses precise floating-point model to ensure determinism.
		set_fpmodels("precise")

		set_default(false)

		local enable_cxx_module = get_config("use-cxx-module")

		-- third-parties/fast_io
		add_includedirs("third-parties/fast_io/include")

		if enable_cxx_module then
			add_files("third-parties/fast_io/share/fast_io/fast_io.cppm", { public = is_debug_mode })
			add_files("third-parties/fast_io/share/fast_io/fast_io_crypto.cppm", { public = is_debug_mode })
		end
		-- third-parties/bizwen
		add_includedirs("third-parties/bizwen/include")

		-- third-parties/boost
		add_includedirs("third-parties/boost_unordered/include")

		if is_llvm_jit_test then
			uwvm_add_llvm_jit_cache_openssl()
		end

		-- uwvm
		add_defines("UWVM=2")
		-- uwvm test
		add_defines("UWVM_TEST=2")

		-- src
		add_includedirs("src/")

		if enable_cxx_module then
			-- uwvm predefine
			add_files("src/uwvm2/uwvm_predefine/**.cppm", { public = is_debug_mode })

			-- utils
			add_files("src/uwvm2/utils/**.cppm", { public = is_debug_mode })

			-- object
			add_files("src/uwvm2/object/**.cppm", { public = is_debug_mode })

			-- imported
			add_files("src/uwvm2/imported/**.cppm", { public = is_debug_mode })

			-- wasm parser
			add_files("src/uwvm2/parser/**.cppm", { public = is_debug_mode })

			-- validation
			add_files("src/uwvm2/validation/**.cppm", { public = is_debug_mode })

			-- uwvm
			add_files("src/uwvm2/uwvm/**.cppm", { public = is_debug_mode })
		end

		set_warnings("all", "extra", "error")
		
		if get_config("use-llvm-compiler") then
			-- Test targets include CLI glue headers that currently trigger
			-- `-Wundefined-inline` under LLVM. Keep src/* at full warning-as-error
			-- strictness and only downgrade this diagnostic for tests.
			add_cxxflags("-Wno-error=undefined-inline")
			add_cxxflags("-Wno-undefined-inline")
		end

		if is_libfuzzer then
			if get_config("use-llvm-compiler") and test_libfuzzer then
				local need_wabt = (string.find(file, "wabt", 1, true) ~= nil)
				if need_wabt then
					before_build(function(target)
						local utility = import("utility.utility", { anonymous = true })
						local require_static_wabt = utility.is_static_non_system_mode()
						local llvm_jit_options = utility.get_llvm_jit_options()

						local function append_unique(values, seen, value)
							if not value or value == "" or seen[value] then
								return
							end

							seen[value] = true
							table.insert(values, value)
						end

						local function append_cmake_cache_value(cache_values, line)
							local key, value = line:match("^([%w_]+):[^=]*=(.*)$")
							if key and value then
								cache_values[key] = value
							end
						end

						local function read_cmake_cache(cache_file)
							if not os.isfile(cache_file) then
								return {}
							end

							local cache_values = {}
							local cache_text = io.readfile(cache_file) or ""
							cache_text = cache_text:gsub("\r\n", "\n")
							for line in (cache_text .. "\n"):gmatch("(.-)\n") do
								append_cmake_cache_value(cache_values, line)
							end
							return cache_values
						end

						local function wabt_uses_compatible_llvm_abi(wabt_root)
							local wabt_build = path.join(wabt_root, "build")
							local cache_values = read_cmake_cache(path.join(wabt_build, "CMakeCache.txt"))
							if cache_values["CMAKE_CXX_COMPILER"] == nil and cache_values["CMAKE_CXX_FLAGS"] == nil then
								return false
							end

							local expected_cxx = target:tool("cxx")
							local actual_cxx = cache_values["CMAKE_CXX_COMPILER"]
							if not expected_cxx or not actual_cxx then
								return false
							end

							if path.normalize(expected_cxx) ~= path.normalize(actual_cxx) then
								return false
							end

							local expected_stdlib = nil
							for _, flag in ipairs(llvm_jit_options.cxxflags or {}) do
								if flag:startswith("-stdlib=") then
									expected_stdlib = flag
									break
								end
							end

							if expected_stdlib then
								local actual_cxx_flags = cache_values["CMAKE_CXX_FLAGS"] or ""
								if actual_cxx_flags:find(expected_stdlib, 1, true) == nil then
									return false
								end
							end

							return true
						end

						local function wabt_uses_external_crypto(wabt_config)
							if not os.isfile(wabt_config) then
								return false
							end

							local config = io.readfile(wabt_config)
							return config and config:find("#define HAVE_OPENSSL_SHA_H 1", 1, true) ~= nil
						end

						local function has_wabt(wabt_root)
							local wabt_include = path.join(wabt_root, "include")
							local wabt_build = path.join(wabt_root, "build")
							local wabt_config = path.join(wabt_build, "include/wabt/config.h")
							local has_static_lib = os.isfile(path.join(wabt_build, "libwabt.a")) or os.isfile(path.join(wabt_build, "wabt.lib"))
							local has_lib = has_static_lib or
								(not require_static_wabt and (os.isfile(path.join(wabt_build, "libwabt.so")) or os.isfile(path.join(wabt_build, "libwabt.dylib"))))

							return os.isdir(wabt_include)
								and os.isfile(wabt_config)
								and has_lib
								and not wabt_uses_external_crypto(wabt_config)
								and wabt_uses_compatible_llvm_abi(wabt_root)
						end

						local function make_wabt_cmake_args(wabt_root)
							local build_dir = path.join(wabt_root, "build")
							local cmake_args = {
								"-S", wabt_root,
								"-B", build_dir,
								"-DCMAKE_BUILD_TYPE=Release",
								"-DBUILD_TESTS=OFF",
								"-DBUILD_TOOLS=OFF",
								"-DBUILD_LIBWASM=OFF",
								"-DUSE_INTERNAL_SHA256=ON",
								"-DHAVE_OPENSSL_SHA_H=OFF"
							}

							local cc = target:tool("cc")
							local cxx = target:tool("cxx")
							if cc then
								table.insert(cmake_args, "-DCMAKE_C_COMPILER=" .. cc)
							end
							if cxx then
								table.insert(cmake_args, "-DCMAKE_CXX_COMPILER=" .. cxx)
							end

							local cflags = {}
							local cflags_seen = {}
							local cxxflags = {}
							local cxxflags_seen = {}
							local linkerflags = {}
							local linkerflags_seen = {}

							local function append_common_compile_flag(flag)
								append_unique(cflags, cflags_seen, flag)
								append_unique(cxxflags, cxxflags_seen, flag)
							end

							local function append_cxx_system_include(dir)
								if not dir or dir == "" then
									return
								end

								table.insert(cxxflags, "-isystem")
								table.insert(cxxflags, dir)
							end

							local sysroot_para = get_config("sysroot")
							if sysroot_para ~= "detect" and sysroot_para ~= "none" and sysroot_para ~= "no" and sysroot_para then
								local sysroot_flag = "--sysroot=" .. sysroot_para
								append_common_compile_flag(sysroot_flag)
								append_unique(linkerflags, linkerflags_seen, sysroot_flag)
							end

							local target_para = get_config("target")
							if target_para ~= "detect" and target_para then
								local target_flag = "--target=" .. target_para
								append_common_compile_flag(target_flag)
								append_unique(linkerflags, linkerflags_seen, target_flag)
							end

							local llvm_target = get_config("llvm-target")
							if llvm_target ~= "detect" and llvm_target then
								local llvm_target_flag = "--target=" .. llvm_target
								append_common_compile_flag(llvm_target_flag)
								append_unique(linkerflags, linkerflags_seen, llvm_target_flag)
							end

							for _, include_dir in ipairs(llvm_jit_options.sysincludedirs or {}) do
								append_cxx_system_include(include_dir)
							end

							for _, flag in ipairs(llvm_jit_options.cxxflags or {}) do
								append_unique(cxxflags, cxxflags_seen, flag)
							end

							if target:is_plat("linux") or target:is_plat("macosx") or target:is_plat("iphoneos") or target:is_plat("watchos") then
								append_unique(linkerflags, linkerflags_seen, "-fuse-ld=lld")
							end
							for _, linkdir in ipairs(llvm_jit_options.linkdirs or {}) do
								append_unique(linkerflags, linkerflags_seen, "-L" .. linkdir)
							end
							for _, flag in ipairs(llvm_jit_options.ldflags or {}) do
								append_unique(linkerflags, linkerflags_seen, flag)
							end
							for _, syslink in ipairs(llvm_jit_options.syslinks or {}) do
								append_unique(linkerflags, linkerflags_seen, "-l" .. syslink)
							end

							if #cflags ~= 0 then
								table.insert(cmake_args, "-DCMAKE_C_FLAGS=" .. os.args(cflags))
							end
							if #cxxflags ~= 0 then
								table.insert(cmake_args, "-DCMAKE_CXX_FLAGS=" .. os.args(cxxflags))
							end
							if #linkerflags ~= 0 then
								local linkerflags_str = os.args(linkerflags)
								table.insert(cmake_args, "-DCMAKE_EXE_LINKER_FLAGS=" .. linkerflags_str)
								table.insert(cmake_args, "-DCMAKE_SHARED_LINKER_FLAGS=" .. linkerflags_str)
							end

							return cmake_args
						end

						local function with_wabt_prepare_lock(action)
							local scheduler = import("core.base.scheduler")
							local lock_name = "uwvm2.wabt.prepare"
							local lock_dir = "build/test/third-parties"

							scheduler.co_lock(lock_name)
							local lock = nil
							local action_errors = nil
							try {
								function()
									os.mkdir(lock_dir)
									lock = io.openlock(path.join(lock_dir, "wabt.prepare.lock"))
									lock:lock()
									action()
								end,
								catch {
									function(errors)
										action_errors = errors
									end
								},
								finally {
									function()
										if lock then
											lock:unlock()
											lock:close()
										end
										scheduler.co_unlock(lock_name)
									end
								}
							}

							if action_errors then
								raise(action_errors)
							end
						end

						if not (has_wabt("build/test/third-parties/wabt") or has_wabt("wabt")) then
							with_wabt_prepare_lock(function()
								if has_wabt("build/test/third-parties/wabt") or has_wabt("wabt") then
									return
								end

								local wabt_root = "build/test/third-parties/wabt"
								-- Check if directory exists AND contains CMakeLists.txt to ensure it's not just an empty placeholder
								if not os.isdir(wabt_root) or not os.isfile(path.join(wabt_root, "CMakeLists.txt")) then
									print("wabt source not found or incomplete. Attempting to pull...")
									if not os.isdir(wabt_root) then
										os.mkdir(wabt_root)
									end
									-- Use git clone/pull instead of submodule
									if not os.isdir(path.join(wabt_root, ".git")) then
										os.vrunv("git", {"clone", "https://github.com/WebAssembly/wabt.git", wabt_root, "--recursive"})
									else
										os.vrunv("git", {"-C", wabt_root, "pull"})
										os.vrunv("git", {"-C", wabt_root, "submodule", "update", "--init", "--recursive"})
									end
								end

								if not os.isdir(wabt_root) then
									wabt_root = "wabt"
								end

								if os.isdir(wabt_root) then
									print("wabt is required for " .. target:name() .. " but no built artifacts were found. Building wabt...")
									local build_dir = path.join(wabt_root, "build")
									os.tryrm(build_dir)
									-- Build WABT in Release so libwabt is compiled with NDEBUG and won't abort on debug assertions
									-- when fed malformed inputs (important for fuzzing/differential validation).
									-- Use WABT's internal SHA-256 implementation so Darwin cross sysroots do not need OpenSSL libcrypto.
									os.vrunv("cmake", make_wabt_cmake_args(wabt_root))
									os.vrunv("cmake", {"--build", build_dir, "--target", "wabt", "--config", "Release"})
								else
									raise("wabt is required for " .. target:name() .. " but neither source nor built artifacts were found.")
								end
							end)
						end
					end)
				end

				after_load(function(target)
					if need_wabt then
						local utility = import("utility.utility", { anonymous = true })
						local require_static_wabt = utility.is_static_non_system_mode()

						local function try_add_wabt(wabt_root, force)
							local wabt_include = path.join(wabt_root, "include")
							local wabt_build = path.join(wabt_root, "build")
							local wabt_config = path.join(wabt_build, "include/wabt/config.h")
							local has_lib = os.isfile(path.join(wabt_build, "libwabt.a")) or
								os.isfile(path.join(wabt_build, "libwabt.so")) or
								os.isfile(path.join(wabt_build, "libwabt.dylib")) or
								os.isfile(path.join(wabt_build, "wabt.lib"))

							if force or (os.isdir(wabt_include) and os.isfile(wabt_config) and has_lib) then
								target:add("includedirs", wabt_include, path.join(wabt_build, "include"))
								target:add("linkdirs", wabt_build)
								if require_static_wabt then
									local wabt_archive = utility.find_static_library("wabt", wabt_build)
									if not wabt_archive and force then
										wabt_archive = path.join(wabt_build, target:is_plat("windows") and "wabt.lib" or "libwabt.a")
									end
									if not wabt_archive then
										return false
									end
									target:add("ldflags", wabt_archive, { force = true })
								else
									target:add("links", "wabt")
								end
								return true
							end

							return false
						end

						-- Try to find existing wabt, if not found, force configure build/third-parties/wabt
						-- assuming before_build will handle the build.
						if not try_add_wabt("build/test/third-parties/wabt", false) and not try_add_wabt("wabt", false) then
							try_add_wabt("build/test/third-parties/wabt", true)
						end
					end

					if target:is_plat("macosx") or target:is_plat("iphoneos") or target:is_plat("watchos") then
						local clang_runtime_dir = os.iorunv("clang", {"--print-runtime-dir"})
						clang_runtime_dir = clang_runtime_dir and string.trim(clang_runtime_dir) or nil
						if clang_runtime_dir and clang_runtime_dir ~= "" and os.isdir(clang_runtime_dir) then
							target:add("runenvs", "DYLD_LIBRARY_PATH", clang_runtime_dir)
						end
					end
				end)

				add_cxflags("-fsanitize=fuzzer", { force = true })
				add_ldflags("-fsanitize=fuzzer", { force = true })
				-- change the env variables in ci to change the default values
				local rss     = os.getenv("FUZZ_RSS") or "512"
				local maxtime = os.getenv("FUZZ_MAX_TIME") or "10"
				local maxlen  = os.getenv("FUZZ_MAX_LEN") or "1024"
				local timeout = os.getenv("FUZZ_TIMEOUT") or "5"
				add_tests("fuzz",
					{ group = "libfuzzer", runargs = { "-rss_limit_mb=" .. rss, "-max_total_time=" .. maxtime, "-max_len=" .. maxlen, "-timeout=" .. timeout } })    -- xmake test -g libfuzzer
				add_files(file)
			elseif not get_config("use-llvm-compiler") and test_libfuzzer then
				raise("Libfuzzer is not supported on this platform, please use llvm toolchain.")
			end
		else
			add_tests("unit", { group = "default" }) -- xmake test -g default
			add_files(file)
		end

		target_end()
	end
end

if get_config("enable-test-backend-fuzzer") then
	local backend_fuzzer_has_int = get_config("execution-int") == "uwvm-int" or get_config("execution-int") == "default"
	local backend_fuzzer_has_jit = get_config("execution-jit") == "llvm" or get_config("execution-jit") == "default"
	if not backend_fuzzer_has_int or not backend_fuzzer_has_jit then
		raise("test/0015.backend_fuzzer requires --execution-int=uwvm-int/default and --execution-jit=llvm/default.")
	end

	target("backend_fuzzer")
		set_group("test/0015.backend_fuzzer")
		set_kind("phony")
		set_default(false)

		on_run(function(target)
			local root = os.projectdir()
			os.execv("bash", {path.join(root, "test/0015.backend_fuzzer/run_backend_fuzzer.sh")}, {curdir = root})
		end)

		on_test(function(target, opt)
			local root = os.projectdir()
			local status, errors = os.execv("bash", {path.join(root, "test/0015.backend_fuzzer/run_backend_fuzzer.sh")},
				{try = true, curdir = root})
			if status ~= 0 then
				opt.errors = errors or ("backend fuzzer failed with exit code: " .. tostring(status))
				return false
			end
			return true
		end)

		local timeout = tonumber(os.getenv("UWVM_BACKEND_FUZZ_TIMEOUT")) or 600
		add_tests("run", { group = "backend_fuzzer", realtime_output = true, run_timeout = timeout })
	target_end()
end

-- LLVM JIT mirror of the 0013 strict uwvm-int suites. These targets compile the
-- original 0013 source files with a runner macro that routes Runner::run through
-- llvm_jit_call_raw_host_api, so the LLVM coverage stays aligned with 0013.
if get_config("enable-test-llvm-jit") and ((get_config("execution-jit") == "llvm") or (get_config("execution-jit") == "default")) then
	local llvm_jit_strict_files = os.files("test/0013.uwvm_int/strict/**.cc")
	table.sort(llvm_jit_strict_files)
	for index, file in ipairs(llvm_jit_strict_files) do
		local rel = file:gsub("\\", "/")
		rel = rel:gsub("^test/0013%.uwvm_int/strict/", "")
		local name = string.format("lj13s_%03d", index)

		target(name)
			set_group("test/0014.llvm_jit")
			set_kind("binary")
			def_build({ skip_static_libcxx = true })

			add_deps("uwvm_runtime")
			add_deps("uwvm")

			-- uwvm uses precise floating-point model to ensure determinism.
			set_fpmodels("precise")

			set_default(false)

			local enable_cxx_module = get_config("use-cxx-module")

			-- third-parties/fast_io
			add_includedirs("third-parties/fast_io/include")

			if enable_cxx_module then
				add_files("third-parties/fast_io/share/fast_io/fast_io.cppm", { public = is_debug_mode })
				add_files("third-parties/fast_io/share/fast_io/fast_io_crypto.cppm", { public = is_debug_mode })
			end
			-- third-parties/bizwen
			add_includedirs("third-parties/bizwen/include")

			-- third-parties/boost
			add_includedirs("third-parties/boost_unordered/include")

			uwvm_add_llvm_jit_cache_openssl()

			-- uwvm
			add_defines("UWVM=2")
			-- uwvm test
			add_defines("UWVM_TEST=2")
			add_defines("UWVM2TEST_RUNNER_USE_LLVM_JIT")

			-- src
			add_includedirs("src/")

			if enable_cxx_module then
				-- uwvm predefine
				add_files("src/uwvm2/uwvm_predefine/**.cppm", { public = is_debug_mode })

				-- utils
				add_files("src/uwvm2/utils/**.cppm", { public = is_debug_mode })

				-- object
				add_files("src/uwvm2/object/**.cppm", { public = is_debug_mode })

				-- imported
				add_files("src/uwvm2/imported/**.cppm", { public = is_debug_mode })

				-- wasm parser
				add_files("src/uwvm2/parser/**.cppm", { public = is_debug_mode })

				-- validation
				add_files("src/uwvm2/validation/**.cppm", { public = is_debug_mode })

				-- uwvm
				add_files("src/uwvm2/uwvm/**.cppm", { public = is_debug_mode })
			end

			set_warnings("all", "extra", "error")

			if get_config("use-llvm-compiler") then
				-- Test targets include CLI glue headers that currently trigger
				-- `-Wundefined-inline` under LLVM. Keep src/* at full warning-as-error
				-- strictness and only downgrade this diagnostic for tests.
				add_cxxflags("-Wno-error=undefined-inline")
				add_cxxflags("-Wno-undefined-inline")
			end

			add_tests("unit", { group = "default" }) -- xmake test -g default
			add_files(file)
		target_end()
	end

	local llvm_jit_lazy_files = os.files("test/0013.uwvm_int/lazy/**.cc")
	table.sort(llvm_jit_lazy_files)
	local llvm_jit_lazy_index = 0
	for _, file in ipairs(llvm_jit_lazy_files) do
		local normalized = file:gsub("\\", "/")
		if normalized:find("/uwvm_int_lazy_split.cc", 1, true) or normalized:find("/uwvm_int_lazy_strategy_matrix.cc", 1, true) then
			goto continue_llvm_jit_lazy
		end
		local rel = normalized
		rel = rel:gsub("^test/0013%.uwvm_int/lazy/", "")
		llvm_jit_lazy_index = llvm_jit_lazy_index + 1
		local name = string.format("lj13l_%03d", llvm_jit_lazy_index)

		target(name)
			set_group("test/0014.llvm_jit/lazy")
			set_kind("binary")
			def_build({ skip_static_libcxx = true })

			add_deps("uwvm_runtime")
			add_deps("uwvm")

			-- uwvm uses precise floating-point model to ensure determinism.
			set_fpmodels("precise")

			set_default(false)

			local enable_cxx_module = get_config("use-cxx-module")

			-- third-parties/fast_io
			add_includedirs("third-parties/fast_io/include")

			if enable_cxx_module then
				add_files("third-parties/fast_io/share/fast_io/fast_io.cppm", { public = is_debug_mode })
				add_files("third-parties/fast_io/share/fast_io/fast_io_crypto.cppm", { public = is_debug_mode })
			end
			-- third-parties/bizwen
			add_includedirs("third-parties/bizwen/include")

			-- third-parties/boost
			add_includedirs("third-parties/boost_unordered/include")

			uwvm_add_llvm_jit_cache_openssl()

			-- uwvm
			add_defines("UWVM=2")
			-- uwvm test
			add_defines("UWVM_TEST=2")
			add_defines("UWVM2TEST_RUNNER_USE_LLVM_JIT")

			-- src
			add_includedirs("src/")

			if enable_cxx_module then
				-- uwvm predefine
				add_files("src/uwvm2/uwvm_predefine/**.cppm", { public = is_debug_mode })

				-- utils
				add_files("src/uwvm2/utils/**.cppm", { public = is_debug_mode })

				-- object
				add_files("src/uwvm2/object/**.cppm", { public = is_debug_mode })

				-- imported
				add_files("src/uwvm2/imported/**.cppm", { public = is_debug_mode })

				-- wasm parser
				add_files("src/uwvm2/parser/**.cppm", { public = is_debug_mode })

				-- validation
				add_files("src/uwvm2/validation/**.cppm", { public = is_debug_mode })

				-- uwvm
				add_files("src/uwvm2/uwvm/**.cppm", { public = is_debug_mode })
			end

			set_warnings("all", "extra", "error")

			if get_config("use-llvm-compiler") then
				add_cxxflags("-Wno-error=undefined-inline")
				add_cxxflags("-Wno-undefined-inline")
			end

			add_tests("unit", { group = "default" }) -- xmake test -g default
			add_files(file)
		target_end()
		::continue_llvm_jit_lazy::
	end
end
