{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = { self, nixpkgs }:
  let
    pkgs = nixpkgs.legacyPackages.aarch64-linux;
  in {
    devShells.aarch64-linux.default = pkgs.mkShell {
      packages = with pkgs; [
        python3
        pkg-config
        bison
        flex
        stdenv
        openssl
        ncurses
        elfutils
        llvmPackages_latest.clang
        cargo
        rustc
        pahole
        dtc
        dt-schema
      ];
      buildInputs = with pkgs; [
        zlib
      ];
    };
  };
}
