{
  description = "A simple project";

  inputs = {
    mars-std.url = "github:mars-research/mars-std";
  };

  outputs = { self, mars-std, ... }: let
    # System types to support.
    supportedSystems = [ "x86_64-linux" ];
  in mars-std.lib.eachSystem supportedSystems (system: let
    pkgs = mars-std.legacyPackages.${system};
    pkgsCross = pkgs.pkgsCross.aarch64-multiplatform;

    llvmSpec = "llvmPackages_12";

    mkShell = pkgsCross.mkShell.override {
      stdenv = pkgsCross.${llvmSpec}.stdenv;
    };
  in {
    devShell = mkShell {
      inputsFrom = [ pkgsCross.linux_5_10 ];
      buildInputs = with pkgsCross; [ openssl ];
      nativeBuildInputs = [
        pkgs.${llvmSpec}.bintools
      ];
      depsBuildBuild = with pkgsCross; [
        buildPackages.${llvmSpec}.stdenv.cc
      ];

      LLVM = "1";
      LLVM_IAS = "1"; # wow, required for LTO_CLANG -> CFI_CLANG
      ARCH = "arm64";
      CROSS_COMPILE = "aarch64-unknown-linux-gnu-";
      LOCALVERSION = "";
    };
  });
}
