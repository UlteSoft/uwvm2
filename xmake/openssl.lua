-- Rules run before UWVM compilation; generated headers and archives are private
-- to the selected platform, ABI, compiler, sysroot and dependency recipe.
rule("uwvm.bundled-openssl")
    on_config(function(target)
        import("utility.bundled_openssl", {anonymous = true}).prepare(target)
    end)
    before_build(function(target)
        import("utility.bundled_openssl", {anonymous = true}).build(target)
    end)
rule_end()

rule("uwvm.prebuilt-openssl")
    on_config(function(target)
        import("utility.bundled_openssl", {anonymous = true}).prepare_prebuilt(target)
    end)
rule_end()

-- Opt-in dependency qualification target: it exercises the exact same provider
-- and linker integration as the real product without requesting a VM rebuild.
target("wsea_dependency_smoke")
    set_kind("binary")
    set_default(false)
    set_languages("cxx20")
    add_files(path.join(os.projectdir(), "tests/security_wsea/openssl_dependency_smoke.cpp"))
    uwvm_add_llvm_jit_cache_openssl()
target_end()
