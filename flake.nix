{
  description = "Flake for Clice";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        tomlplusplus = pkgs.tomlplusplus.overrideAttrs (
          oldAttrs: {
            mesonFlags = oldAttrs.mesonFlags or [] ++ [
              "-Dbuild_examples=false"
              "-Dbuild_tests=false"
              "-Dcompile_library=false"
            ];

            postInstall = oldAttrs.postInstall or "" + ''
              cat > "$out/share/pkgconfig/toml++.pc" <<EOF
              Name: toml++
              Description: Alias for tomlplusplus
              Version: ${oldAttrs.version}
              Requires: tomlplusplus
              EOF
            '';
          }
        );
      in
      {
        devShell = pkgs.mkShell {
          buildInputs = [
            pkgs.gcc
            pkgs.pkg-config
            pkgs.xmake
            pkgs.cmake
            pkgs.ninja
            pkgs.python3
            pkgs.gtest
            pkgs.libuv
            pkgs.llvmPackages_20.libstdcxxClang
            # pkgs.llvmPackages_20.libllvm
            pkgs.llvmPackages_20.bintools
            pkgs.llvmPackages_20.compiler-rt
            pkgs.llvmPackages_20.libunwind
            pkgs.zlib 
            tomlplusplus
          ];

          shellHook =
            let
              gcc = pkgs.gcc-unwrapped;
              gccInclude = "${gcc}/include";
              gccCxxInclude = "${gccInclude}/c++/${gcc.lib.version}";
            in
            ''
              export NIX_CFLAGS_COMPILE+=" -isystem ${gccInclude}"
              export NIX_CFLAGS_COMPILE+=" -isystem ${gccCxxInclude}"
              export NIX_CFLAGS_COMPILE+=" -isystem ${gccCxxInclude}/$(gcc -dumpmachine)"
            '';
        };
      }
    );
}
