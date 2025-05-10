task("clang-tidy")

    set_category("plugin")

    on_run(function ()
        import("core.base.task")
        import("core.base.json")
        import("core.base.scheduler")
        import("async.runjobs")
		task.run("project", {kind = "compile_commands", outputdir = "build"})
        local files = {}
		for _, v in ipairs(json.loadfile("build/compile_commands.json")) do 
            table.insert(files, v["file"])
        end
        local results = {}
        runjobs("clang-tidy",function (index) 
            function run(filepath) 
                try
                {
                    function ()
                        local tmp = os.tmpfile()
                        os.execv("clang-tidy", {"-p", "build", "-header-filter=.*", filepath}, {stdout = tmp, stderr = tmp})
                        scheduler.co_lock("clang-tidy")
                        results[filepath] = io.readfile(tmp)
                        scheduler.co_unlock("clang-tidy")
                        os.rm(tmp)
                    end,
                    catch
                    {
                        function (errors)
                            print(errors)
                        end
                    }
                }
            end
            run(files[index])
        end, {total = #files, comax = os.default_njob()})
        for filepath, content in pairs(results) do 
            printf("[clang-tidy] %s\n%s\n", filepath, content)
        end
    end)

    set_menu {usage = "xmake clang-tidy", description = "Run clang-tidy on all code.", options = {}} 
