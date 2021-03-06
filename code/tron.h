#if !defined(TRON_H)
#define TRON_H

#include <stddef.h>

#if _MSC_VER
#include <intrin.h>
#endif

/*
  NOTE:

  TRON_INTERNAL:
    0 - Build for public release
    1 - Build for developer only

  TRON_SLOW:
    0 - Not slow code allowed!
    1 - Slow code welcome.
*/

#if TRON_SLOW
#define ASSERT(expression) if (!(expression)) { *(volatile int *) 0 = 0; }
#else
#define ASSERT(expression)
#endif

#define KILOBYTES(value) ((value) * 1024LL)
#define MEGABYTES(value) (KILOBYTES(value) * 1024LL)
#define GIGABYTES(value) (MEGABYTES(value) * 1024LL)
#define TERABYTES(value) (GIGABYTES(value) * 1024LL)

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define MINIMUM(A, B) ((A < B) ? (A) : (B))
#define MAXIMUM(A, B) ((A > B) ? (A) : (B))

inline uint32
safeTruncateUInt64(uint64 value) {
  ASSERT(value <= 0xFFFFFFFF);
  uint32 result = (uint32) value;
  return result;
}

inline bool32 sign(int32 x) {
  // assumes 32-bit int and 2s complement signed shifts work (implementation defined by C spec)
  return (x >> 31) | ((uint32) -x >> 31);
}

struct game_screen_buffer {
  void *memory;
  int width;
  int height;
  int pitch;
  int bytesPerPixel;
};

struct bitmap_buffer { 
  void *memory;
  size_t memorySize;
  uint32 *pixels;
  int width;
  int height;
  int pitch;
};

struct game_button_state {
  int halfTransitionCount;
  bool32 endedDown;
};

struct game_controller_input {
  bool32 isConnected;
  bool32 isAnalog;
  real32 stickAverageX;
  real32 stickAverageY;

  union {
    game_button_state buttons[12];
    struct {
      game_button_state moveUp;
      game_button_state moveDown;
      game_button_state moveLeft;
      game_button_state moveRight;
      
      game_button_state actionUp;
      game_button_state actionDown;
      game_button_state actionLeft;
      game_button_state actionRight;
      
      game_button_state leftShoulder;
      game_button_state rightShoulder;

      game_button_state back;
      game_button_state start;

      // NOTE(casey): All buttons must be added above this line
      
      game_button_state terminator;
    };
  };
};

struct game_input {
  real32 dtForFrame;
  game_controller_input controllers[4];
};

// TODO: this is a stack for the moment, could change in the future.
struct memory_partition {
  size_t size;
  void *base;
  size_t used;
};

struct mem_buffer {
  size_t size;
  void *base;
};

typedef mem_buffer (platform_read_entire_file) (memory_partition* partition, char *filename);

typedef void (platform_free) (memory_partition* partition, void* base, size_t size);

typedef void*  (platform_alloc) (memory_partition* partition, size_t size);

struct platform_functions {
  platform_read_entire_file* readEntireFile;
  platform_free* free;
  platform_alloc* alloc;
};

struct game_memory {
  bool32 isInitialized;

  memory_partition permanentStorage;
  memory_partition transientStorage;

  platform_functions platformFunctions;
};

struct runner_bitmaps {
  bitmap_buffer right[8];
  bitmap_buffer left[8];
  bitmap_buffer up[4];
  bitmap_buffer down[4];
};

struct v2 {
  real32 x; 
  real32 y; 
};

inline bool operator==(v2 a, v2 b) {
  bool result = (a.x == b.x) && (a.y == b.y);
  return result;
}

inline bool operator!=(v2 a, v2 b) {
  bool result = ! (a == b);
  return result;
}

inline real32 signReal32(real32 x) {
  real32 result = (real32) (x > 0.f) - (real32) (x < 0.f);
  return result;
}

inline real32 squareLen(v2 a) {
  real32 result = (a.x * a.x) + (a.y * a.y);
  return result;
}

inline real32 square(real32 a) {
  real32 result = a * a;
  return result;
}

inline v2 operator*(real32 a, v2 b) {
  v2 result;
  result.x = a * b.x;
  result.y = a * b.y;
  return result; 
}

inline v2 operator*(v2 b, real32 a) {
  v2 result;
  result.x = a * b.x;
  result.y = a * b.y;
  return result; 
}

inline v2 operator+(v2 a, v2 b) {
  v2 result;
  result.x = a.x + b.x;
  result.y = a.y + b.y;
  return result;
}

inline v2 operator-(v2 a, v2 b) {
  v2 result;
  result.x = a.x - b.x;
  result.y = a.y - b.y;
  return result;
}

inline v2 &operator+=(v2 &a, v2 b) {
  a = a + b;
  return a;
}

inline real32 len(v2 a) {
  real32 result = square(squareLen(a));
  return result;
}

enum directions {  
  DIRECTION_UP, DIRECTION_RIGHT, DIRECTION_DOWN, DIRECTION_LEFT
};

struct runner_state {
  v2 position;
  v2 speed;
  v2 accel;
  real32 deltaDrag;
  real32 deltaAccel;
  real32 maxAccel;
  real32 maxSpeed;
  int bitmapIndex;
  bitmap_buffer* bitmap;
  directions direction;
};

struct wall {
  v2 p1, p2;
  real32 r, g, b;
};

struct game_state { 
  real32 pixelsPerMt;
  runner_bitmaps runnerBitmaps;
  int runnersCount;
  runner_state runners[2];
  wall walls[4];
};

struct bit_scan_result {
  bool32 found;
  uint32 index;
};

inline bit_scan_result
leastSignificantSetBit(uint32 value) {

  bit_scan_result result = {};

#if _MSC_VER
  result.found = _BitScanForward((unsigned long *) &result.index, value);
#else

  for (uint32 test = 0; test < 32; ++test) {

    if (value & (1 << test)) {
      result.index = test;
      result.found = true;
      break;
    }
  }
#endif  

  return result;
}


#endif /* TRON_H */