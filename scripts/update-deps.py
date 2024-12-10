import os
import subprocess


submodules = [
    ("https://github.com/llvm/llvm-project.git", "./deps/llvm"),
    ("https://github.com/libuv/libuv.git", "./deps/libuv"),
    ("https://github.com/marzer/tomlplusplus.git", "./deps/toml"),
    ("https://github.com/google/googletest.git", "./deps/googletest"),
]

def run_command(command):
    result = subprocess.run(command, shell=True, text=True, capture_output=True)
    if result.returncode != 0:
        print(f"Error running command: {command}")
        print(result.stderr)
    else:
        print(result.stdout)

def update_or_clone_submodule(repo_url, submodule_dir):
    if os.path.isdir(os.path.join(submodule_dir, ".git")):
        print(f"Directory {submodule_dir} already exists, updating...")
        os.chdir(submodule_dir)
        run_command("git fetch --depth=1")
        run_command("git pull --ff-only")
        os.chdir("../..")
    else:
        print(f"Directory {submodule_dir} does not exist, cloning...")
        run_command(f"git clone --depth=1 {repo_url} {submodule_dir}")

if __name__ == "__main__":
    for repo_url, submodule_dir in submodules:
        update_or_clone_submodule(repo_url, submodule_dir)
