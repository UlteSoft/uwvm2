---@alias modifier_t fun(toolchain:unknown, opt:table<string, unknown>):nil
---@alias modifier_table_t table<string, modifier_t>
---@alias opt_t table<string, unknown>

---Placeholder, no effect
---@return nil
function noop_modifier(_, _) return end

---Customized partial flags for loongnix
---@return nil
function loongnix_modifier(toolchain, _)
    -- Loongnix has an older version of glibc and uses a different ld path than the newer compiler's default paths
    toolchain:add("ldflags", "-Wl,-dynamic-linker=/lib64/ld.so.1")
    -- The loongnix native gdb only supports debugging information in the dwarf4 format.
    toolchain:add("cxflags", "-gdwarf-4")
end

---Customize some of the flags for the standalone toolchain
---@return nil
function freestanding_modifier(toolchain, _)
    -- freestanding requires disabling the standard library
    toolchain:add("cxflags", "-ffreestanding")
    toolchain:add("ldflags", "-nodefaultlibs", "-lstdc++", "-lgcc")
end

local note_msg = "${color.warning}NOTE:${default} "

---Reset sysroot based on target
---@param target string
---@param opt opt_t
---@return nil
function _reset_sysroot(target, opt)
    import("common")
    local cache_info = common.get_cache()
    if cache_info["sysroot_set_by_user"] then
        return
    end
    local sysroot_option = opt.sysroot
    local sysroot = sysroot_option.ldflags:sub(11)
    local libcxx_option = #sysroot_option.cxflags == 2 and sysroot_option.cxflags[2] or nil
    local target_sysroot = path.join(sysroot, target)
    if path.filename(sysroot) ~= target and os.isdir(target_sysroot) then
        cprint(note_msg .. [[Reset "--sysroot" option to "%s".]], target_sysroot)
        sysroot = "--sysroot=" .. target_sysroot
        opt.sysroot.cxflags = { sysroot, libcxx_option }
        opt.sysroot.ldflags = sysroot
        opt.sysroot.shflags = sysroot
    end
end

---Reset march to the specified value
---@param march string
---@param opt opt_t
---@return nil
function _reset_march(march, opt)
    local march_option = "-march=" .. march
    if opt.march ~= march_option then
        cprint(note_msg .. [[Reset "-march" option to %s.]], march)
        opt.march = march_option
    end
end

---Customized partial flags for armv7m
---@param opt opt_t
---@return nil
function armv7m_modifier(_, opt)
    _reset_march("armv7-m", opt)
    _reset_sysroot("armv7m-none-eabi", opt)
end

---Customized partial flags for armv7m-fpv4
---@param opt opt_t
---@return nil
function armv7m_fpv4_modifier(toolchain, opt)
    _reset_march("armv7-m", opt)
    _reset_sysroot("armv7m-fpv4-none-eabi", opt)
    toolchain:add("cxflags", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard")
    toolchain:add("asflags", "-mfpu=fpv4-sp-d16")
end

---Targets supported only by clang
---@type modifier_table_t
clang_only_target_list = {
    ["x86_64-windows-msvc"] = noop_modifier,
    ["armv7m-none-eabi"] = armv7m_modifier,
    ["armv7m-fpv4-none-eabi"] = armv7m_fpv4_modifier,
}
---Targets supported only by gcc
---@type modifier_table_t
gcc_only_target_list = {
    ["arm-none-eabi"] = noop_modifier,
    ---The default options have already been specified when compiling gcc and will not be specified here.
    ["arm-fpv4-none-eabi"] = noop_modifier,
    ["arm-nonewlib-none-eabi"] = freestanding_modifier,
    ["riscv-none-elf"] = noop_modifier,
}
---Targets supported by both gcc and clang
---@type modifier_table_t
general_target_list = {
    ["x86_64-linux-gnu"] = noop_modifier,
    ["i686-linux-gnu"] = noop_modifier,
    ["x86_64-w64-mingw32"] = noop_modifier,
    ["i686-w64-mingw32"] = noop_modifier,
    ["loongarch64-linux-gnu"] = noop_modifier,
    ["loongarch64-loongnix-linux-gnu"] = loongnix_modifier,
    ["riscv64-linux-gnu"] = noop_modifier,
    ["aarch64-linux-gnu"] = noop_modifier,
    ["arm-linux-gnueabi"] = noop_modifier,
    ["arm-linux-gnueabihf"] = noop_modifier,
    ["powerpc-linux-gnu"] = noop_modifier,
    ["powerpc64-linux-gnu"] = noop_modifier,
    ["sparc64-linux-gnu"] = noop_modifier,
    ["x86_64-elf"] = freestanding_modifier,
    ["native"] = noop_modifier,
    ["target"] = noop_modifier
}

---Get a list of targets supported by clang
---@return modifier_table_t
function get_clang_target_list()
    return table.join(general_target_list, clang_only_target_list)
end

---Get a list of gcc supported targets
---@return modifier_table_t
function get_gcc_target_list()
    return table.join(general_target_list, gcc_only_target_list)
end

---Get a list of all supported targets
---@return modifier_table_t
function get_target_list()
    return table.join(general_target_list, clang_only_target_list, gcc_only_target_list)
end
