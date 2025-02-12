import("net.http")

function main()
    local hash = "fac46469977da9c4e9c6eeaac21103c971190577" -- 2025.01.04
    local github_url = format("https://raw.githubusercontent.com/llvm/llvm-project/%s/clang/lib/Sema/", hash)

    local headers = {
        "CoroutineStmtBuilder.h",
        "TypeLocBuilder.h",
        "TreeTransform.h",
    }

    local install_clang_include_dir = path.join(path.directory(path.absolute(os.scriptdir())), "include/clang/Sema")

    for _, header in ipairs(headers) do
        http.download(github_url .. header, path.join(install_clang_include_dir, header))
    end
end
