{
  description = "Flake for Clice";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }: 
    flake-utils.lib.eachDefaultSystem 
      (system: 
        let 
          pkgs = nixpkgs.legacyPackages.${system}; 
          tomlplusplus = pkgs.tomlplusplus.overrideAttrs (oldAttrs: {
            postInstall = oldAttrs.postInstall or "" + ''
              ln -s $out/lib/libtomlplusplus.so $out/lib/libtoml++.so              
            '';
          });

        in {
          devShell = pkgs.mkShell.override {
            stdenv = pkgs.llvmPackages_20.libcxxStdenv;
          } {
            buildInputs = [
              pkgs.pkg-config
              pkgs.xmake
              pkgs.cmake
              pkgs.ninja
              pkgs.python3
              pkgs.gtest
              pkgs.libuv
              pkgs.llvmPackages_20.libllvm
              pkgs.llvmPackages_20.bintools
              pkgs.llvmPackages_20.libcxx
              pkgs.llvmPackages_20.compiler-rt
              pkgs.llvmPackages_20.libunwind

              tomlplusplus
            ];   
          };
        }
      );
}
