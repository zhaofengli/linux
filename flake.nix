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
  in {
    devShell = pkgsCross.mkShell {
      inputsFrom = [ pkgsCross.linux_5_10 ];
      buildInputs = with pkgsCross; [ openssl ];
      depsBuildBuild = with pkgsCross; [ buildPackages.stdenv.cc ];

      ARCH = "arm64";
      CROSS_COMPILE = "aarch64-unknown-linux-gnu-";
    };
  });
}
