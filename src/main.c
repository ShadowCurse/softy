#include "game.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static Game game;

#ifdef __EMSCRIPTEN__
  void em_loop() {
    run(&game);
  }
#endif

int main() {
  init(&game);
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(em_loop, FPS, 1);
#else
  while (!game.stop) {
    run(&game);
  }
#endif
  destroy(&game);
}
