set_xmakever("2.9.8")

set_project("uwvm")

-- Version
set_version("2.0.0")
add_defines("UWVM_VERSION_X=2")
add_defines("UWVM_VERSION_Y=0")
add_defines("UWVM_VERSION_Z=0")
add_defines("UWVM_VERSION_S=0")

set_allowedplats("windows", "mingw", "cygwin", "linux", "djgpp", "unix", "bsd", "freebsd", "dragonflybsd", "netbsd", "openbsd", "macosx", "iphoneos", "watchos", "wasm-wasi", "wasm-wasip1", "wasm-wasip2", "wasm-wasi-threads", "wasm-wasip1-threads", "wasm-wasip2-threads", "serenity", "sun", "cross", "none")

includes("xmake/impl.lua")
includes("xmake/platform/impl.lua")

-- Currently, there are no plugins.
-- add_plugindirs("xmake/plugins")

set_defaultmode("releasedbg")
set_allowedmodes(support_rules_table)

function def_build()

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

	local disable_cpp_exceptions = get_config("fno-exceptions")
	if disable_cpp_exceptions then
		set_exceptions("no-cxx")
	end

    local enable_debug_timer = get_config("debug-timer")
	if enable_debug_timer then
		add_defines("UWVM_TIMER")
	end

    local enable_multithread_allocator_mem = get_config("use-multithread-allocator-memory")
	if enable_multithread_allocator_mem then
		add_defines("UWVM_USE_MULTITHREAD_ALLOCATOR")
	end

    local enable_int = get_config("enable-int")
	if not enable_int or enable_int == "none" then
		add_defines("UWVM_DISABLE_INT")
	elseif enable_int == "default" then
		add_defines("UWVM_USE_DEFAULT_INT")
	elseif enable_int == "uwvm-int" then
		add_defines("UWVM_USE_UWVM_INT")
	end

    local enable_jit = get_config("enable-jit")
	if not enable_jit or enable_jit == "none" then
		add_defines("UWVM_DISABLE_JIT")
	elseif enable_jit == "default" then
		add_defines("UWVM_USE_DEFAULT_JIT")
	elseif enable_jit == "llvm" then
		add_defines("UWVM_USE_LLVM_JIT")
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
        darwin_target()
	elseif is_plat("djgpp") then
        djgpp_target()
	elseif is_plat("unix", "bsd", "freebsd", "dragonflybsd", "netbsd", "openbsd") then
        bsd_target()
	elseif is_plat("wasm-wasi", "wasm-wasip1", "wasm-wasip2", "wasm-wasi-threads", "wasm-wasip1-threads", "wasm-wasip2-threads") then 
		wasm_wasi_target()
	elseif is_plat("cygwin") then
		cygwin_target()
	elseif is_plat("none") then
		none_target()
    end

	local use_llvm_toolchain = get_config("use-llvm")
    if use_llvm_toolchain then	
        local llvm_target = get_config("llvm-target")
		if llvm_target ~= "detect" and llvm_target then
			local llvm_target_cvt = "--target=" .. llvm_target
			add_cxflags(llvm_target_cvt, {force = true})
			add_ldflags(llvm_target_cvt, {force = true})
		end

		-- Since neither LLVM nor Wextra supports this parameter by default, this addition prevents compilation.
		add_cxflags("-Wimplicit-fallthrough", {force = true})
    end

	before_build(
		function (target)
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

target("uwvm")
	set_kind("binary")
	def_build()

	local is_debug_mode = is_mode("debug") -- public all modules in debug mode
	local enable_cxx_module = get_config("use-cxx-module")

	-- third-parties/fast_io
	add_includedirs("third-parties/fast_io/include")
	
	if enable_cxx_module then
		add_files("third-parties/fast_io/share/fast_io/fast_io.cppm", {public = is_debug_mode})
		add_files("third-parties/fast_io/share/fast_io/fast_io_crypto.cppm", {public = is_debug_mode})
	end 
		
	-- third-parties/bizwen
	add_includedirs("third-parties/bizwen/include")
	
	-- third-parties/boost
	add_includedirs("third-parties/boost_unordered/include")

	-- uwvm
	add_defines("UWVM=2")

	-- src
	add_includedirs("src/")

	add_headerfiles("src/**.h")

	if enable_cxx_module then
		-- uwvm predefine
		add_files("src/uwvm2/uwvm_predefine/**.cppm", {public = is_debug_mode})

		-- utils
		add_files("src/uwvm2/utils/**.cppm", {public = is_debug_mode})

		-- object
		add_files("src/uwvm2/object/**.cppm", {public = is_debug_mode})

		-- imported
		add_files("src/uwvm2/imported/**.cppm", {public = is_debug_mode})
		
        -- wasm parser
		add_files("src/uwvm2/parser/**.cppm", {public = is_debug_mode})

		-- uwvm
		add_files("src/uwvm2/uwvm/**.cppm", {public = is_debug_mode})
	end 

	if enable_cxx_module then
		-- uwvm cmd callback
		add_files("src/uwvm2/uwvm/cmdline/params/**.module.cpp")

		-- uwvm main
		add_files("src/uwvm2/uwvm/main.module.cpp")

	else
		-- uwvm cmd callback
		add_files("src/uwvm2/uwvm/cmdline/params/**.default.cpp")

		-- uwvm main
		add_files("src/uwvm2/uwvm/main.default.cpp")
	end

target_end()

-- test unit
for _, file in ipairs(os.files("test/**.cc")) do
    local name = path.basename(file)
    target(name)
        set_kind("binary")
		def_build()
        set_default(false)
			
		local enable_cxx_module = get_config("use-cxx-module")

		-- third-parties/fast_io
		add_includedirs("third-parties/fast_io/include")

		if enable_cxx_module then
			add_files("third-parties/fast_io/share/fast_io/fast_io.cppm", {public = is_debug_mode})
			add_files("third-parties/fast_io/share/fast_io/fast_io_crypto.cppm", {public = is_debug_mode})
		end
				
		-- third-parties/bizwen
		add_includedirs("third-parties/bizwen/include")

		-- third-parties/boost
		add_includedirs("third-parties/boost_unordered/include")

		-- uwvm
		add_defines("UWVM=2")
		-- uwvm test
		add_defines("UWVM_TEST=2")

		-- src
		add_includedirs("src/")

		if enable_cxx_module then
			-- uwvm predefine
			add_files("src/uwvm2/uwvm_predefine/**.cppm", {public = is_debug_mode})
			
			-- utils
			add_files("src/uwvm2/utils/**.cppm", {public = is_debug_mode})

			-- object
			add_files("src/uwvm2/object/**.cppm", {public = is_debug_mode})
            
            -- imported
		    add_files("src/uwvm2/imported/**.cppm", {public = is_debug_mode})
	
			-- wasm parser
			add_files("src/uwvm2/parser/**.cppm", {public = is_debug_mode})

			-- uwvm
			add_files("src/uwvm2/uwvm/**.cppm", {public = is_debug_mode})
		end 

		if false then -- Most cases do not need to use functions in cpp to speed up compilation
			if enable_cxx_module then
				-- uwvm cmd callback
				add_files("src/uwvm2/uwvm/cmdline/params/**.module.cpp")
			else
				-- uwvm cmd callback
				add_files("src/uwvm2/uwvm/cmdline/params/**.default.cpp")
			end
		end 

		set_warnings("all", "extra", "error")

		local is_libfuzzer = (string.find(file, "test/0000.libfuzzer/", 1, true) ~= nil) or (string.find(file, "test\\0000.libfuzzer\\", 1, true) ~= nil)
		local test_libfuzzer = get_config("test-libfuzzer")

		if is_libfuzzer then
			if get_config("use-llvm") and test_libfuzzer then
				add_cxflags("-fsanitize=fuzzer", {force = true})
				add_ldflags("-fsanitize=fuzzer", {force = true})
				-- change the env variables in ci to change the default values
				local rss      = os.getenv("FUZZ_RSS") or "512"
				local maxtime  = os.getenv("FUZZ_MAX_TIME") or "10"
				local maxlen   = os.getenv("FUZZ_MAX_LEN") or "1024"
				add_tests("fuzz", {group = "libfuzzer",runargs = { "-rss_limit_mb=" .. rss, "-max_total_time=" .. maxtime, "-max_len=" .. maxlen }}) -- xmake test -g libfuzzer
				add_files(file)
			end
		else
			add_tests("unit", {group = "default"}) -- xmake test -g default
			add_files(file)
		end

	target_end()
end
