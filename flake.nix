{
  description = "Perentie is a Lua-based graphical adventure game engine. The design is heavily inspired by LucasArts' SCUMM and GrimE adventure game engines";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    systems.url = "github:nix-systems/x86_64-linux";
    flake-utils = {
      url = "github:numtide/flake-utils";
      inputs.systems.follows = "systems";
    };
  };

  outputs = {
    nixpkgs,
    flake-utils,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system};
      pname = "perentie";
      version = "0.6";
      src = ./.;
      buildInputs = with pkgs; [
        sdl3
      ];
      nativeBuildInputs = with pkgs; [
        sdl3
        meson
        ninja
        pkg-config
        lua5_4
      ];

      perentie = pkgs.stdenv.mkDerivation {
        inherit buildInputs nativeBuildInputs pname version src;

        mesonBuildType = "release";

        installPhase = ''
          mkdir -p $out/bin
          cp perentie $out/bin
        '';
      };

      perentie-debug = perentie.overrideAttrs (old:
        old
        // {
          pname = old.pname + "-debug";
          mesonBuildType = "debug";
          mesonFlags = [
            "-Doptimization=0"
            "-Db_sanitize=address"
          ];
        });

      perentie-dos = perentie.overrideAttrs (old:
        old
        // {
          pname = old.pname + "-dos";
          nativeBuildInputs = old.nativeBuildInputs ++ [pkgs.djgpp];
          mesonAutoFeatures = false;
          mesonFlags = [
            "--cross-file=i586-pc-msdosdjgpp.ini"
          ];
          installPhase = ''
            mkdir -p $out/bin
            cp perentie.exe $out/bin
          '';
        });
    in {
      devShells.default = pkgs.mkShell {
        inherit buildInputs;
        nativeBuildInputs = nativeBuildInputs ++ [pkgs.djgpp];
      };

      packages = {
        inherit perentie perentie-debug perentie-dos;
        default = perentie;
      };
    });
}
