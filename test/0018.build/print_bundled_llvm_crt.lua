-- The COFF regression consumes the production mapping, not a second recipe.
function main(crt)
    local bundled = import("utility.bundled_llvm", {rootdir = path.join(os.projectdir(), "xmake"), anonymous = true})
    print(bundled.msvc_runtime_library(crt))
end
