#if !defined(SDL_TRON_H)
#define SDL_TRON_H

struct window_dimension {
  int width;
  int height;
};

struct sdl_state {
  uint64 totalSize;
  void *gameMemoryBlock;
};

#endif /* SDL_TRON_H */