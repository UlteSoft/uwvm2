-- Generate the SDK's protected Mach exception server for native Mach stepping.
-- The checked-in marker is a source dependency; generated C and its object
-- remain in xmake's target-specific build directory.
rule("uwvm.mach_exc_protected")
	set_extensions(".uwvm_mig")
	on_build_file(function(target, marker, opt)
		import("core.project.depend")
		import("utils.progress")

		local sdk = assert(os.iorunv("xcrun", {"--show-sdk-path"}), "xcrun SDK lookup failed"):trim()
		local spec = path.join(sdk, "usr/include/mach/mach_exc.defs")
		assert(os.isfile(spec), "mach_exc.defs is missing from the selected macOS SDK")
		local outdir = path.join(target:autogendir(), "rules", "mach_exc_protected")
		local generated = path.join(outdir, "mach_exc_server.c")
		local generated_header = path.join(outdir, "mach_exc.h")
		local objectfile = target:objectfile(generated)
		table.insert(target:objectfiles(), objectfile)
		local dependfile = target:dependfile(objectfile)
		local dependinfo = target:is_rebuilt() and {} or (depend.load(dependfile) or {})
		local arch = target:is_arch("arm64", "aarch64") and "arm64" or "x86_64"
		assert(target:is_plat("macosx") and (arch == "arm64" or
			(target:is_arch("x86_64", "x64", "amd64") and get_config("macos-x64-native-step"))),
			"protected MIG server requires an enabled native macOS target")
		if not depend.is_changed(dependinfo, {
			lastmtime = os.mtime(objectfile), files = {marker, spec}, values = {sdk, arch}
		}) then
			return
		end

		progress.show(opt.progress, "${color.build.object}compiling.mach_exc %s", marker)
		os.mkdir(outdir)
		os.mkdir(path.directory(objectfile))
		os.vrunv("xcrun", {"mig", "-DMACH_EXC_SERVER_TASKIDTOKEN_STATE=1",
			"-server", generated, "-header", generated_header,
			"-user", "/dev/null", spec})
		os.vrunv("xcrun", {"clang", "-std=c17", "-O2", "-arch", arch,
			"-isysroot", sdk, "-c", generated, "-o", objectfile})
		dependinfo.files = {marker, spec}
		dependinfo.values = {sdk, arch}
		depend.save(dependinfo, dependfile)
	end)
