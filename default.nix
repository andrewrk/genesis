with import <nixpkgs> {}; {
  genesisEnv = stdenv.mkDerivation {
    name = "genesis";
    buildInputs = [
      cmake
      gcc49
      glfw
      epoxy
      glm
      freetype
      libpng
      libav
      alsaLib
      liblaxjson
      rucksack
      libpulseaudio
      rhash
      xlibs.libX11
      mesa_noglu
    ];
  };
}
