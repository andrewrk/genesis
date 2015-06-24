with import <nixpkgs> {}; {
  genesisEnv = stdenv.mkDerivation {
    name = "genesis";
    buildInputs = [
      alsaLib
      cmake
      epoxy
      freetype
      gcc49
      glfw
      glm
      libav
      libjack2
      liblaxjson
      libpng
      libpulseaudio
      mesa_noglu
      rhash
      rucksack
      xlibs.libX11
    ];
  };
}
