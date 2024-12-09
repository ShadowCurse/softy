{ pkgs ? import <nixpkgs> { } }:
pkgs.mkShell {
  SDL2_INCLUDE_PATH = "${pkgs.lib.makeIncludePath [pkgs.SDL2]}";
  LIBC_INCLUDE_PATH="${pkgs.lib.makeIncludePath [pkgs.glibc]}";
  CLANG_INCLUDE_PATH="${pkgs.clang}/resource-root/include";
  EM_CACHE="/home/antaraz/.emscripten_cache";

  buildInputs = with pkgs; [
    SDL2
    clang
    pkg-config
    emscripten
  ];
}
