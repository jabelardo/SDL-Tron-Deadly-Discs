#include <stdint.h>
#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int32 bool32;

typedef float real32;
typedef double real64;

// TODO: Implement sine ourselved
#include <math.h>

#include "tron.h"
#include "tron.cpp"

#include <SDL.h>
#include <sys/stat.h> 
#include <fcntl.h>

#if _MSC_VER
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#include <x86intrin.h>
#endif

#include "sdl_tron.h"

// NOTE: MAP_ANONYMOUS is not defined on Mac OS X and some other UNIX systems.
// On the vast majority of those systems, one can use MAP_ANON instead.
// Huge thanks to Adam Rosenfield for investigating this, and suggesting this
// workaround:
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

global_variable game_screen_buffer G_backbuffer;

global_variable char G_dataPath[FILENAME_MAX];

#define BITES_PER_PIXEL 4

internal void* 
sdlPlatformAlloc(memory_partition* partition, size_t size) {
  ASSERT(partition && partition->base && partition->size);
  ASSERT(size);
  ASSERT(size <= partition->size - partition->used);

  uint8* result = partition->base + partition->used;
  partition->used += size;

  return result;
}

internal void
sdlPlatformFree(memory_partition* partition, void* base, size_t size) {
  // ASSERT(partition && partition->base && partition->size && partition->used);
  // ASSERT(base && size);
  // ASSERT(partition->base <= (uint8*) base - size);
  // ASSERT(partition->used >= size);
  // partition->used -= size;
}

internal mem_buffer 
sdlPlatformReadEntireFile(memory_partition* partition, char *filename) {
  
  ASSERT(strlen(G_dataPath) + strlen(filename) < FILENAME_MAX);
  char filepath[FILENAME_MAX];
  filepath[FILENAME_MAX - 1] = 0;
  strcpy(filepath, G_dataPath);
  strcat(filepath, filename);

  mem_buffer result = {};

#if _MSC_VER
  
  HANDLE fileHandle = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  
  if (fileHandle != INVALID_HANDLE_VALUE) {

    LARGE_INTEGER fileSize;

    if (GetFileSizeEx(fileHandle, &fileSize)) {

      uint32 fileSize32 = safeTruncateUInt64(fileSize.QuadPart);

      result.base = (uint8 *) sdlAlloc(partition, fileSize32);

      if (result.base) {

        DWORD bytesRead;
        if (ReadFile(fileHandle, result.base, fileSize32, &bytesRead, 0) && 
            (fileSize32 == bytesRead)) {

          // NOTE(casey): File read successfully
          result.size = fileSize32;

        } else {                    
          // TODO(casey): Logging
          sdlPlatformFree(partition, result.base, result.size);
          result.base = 0;
          result.size = 0;
        }

      } else {
          // TODO(casey): Logging
      }

    } else {
      // TODO(casey): Logging
    }

    CloseHandle(fileHandle);

  } else {
      // TODO(casey): Logging
  }

#else
  int fileHandle = open(filepath, O_RDONLY);
  if (fileHandle == -1) {
    return result;
  }

  struct stat fileStatus;
  if (fstat(fileHandle, &fileStatus) == -1) {
    close(fileHandle);
    return result;
  }
  result.size = fileStatus.st_size;

  result.base = (uint8 *) sdlPlatformAlloc(partition, result.size);

  if (!result.base) {
    close(fileHandle);
    result.size = 0;
    return result;
  }

  uint32 bytesToRead = result.size;
  uint8 *nextByteLocation = (uint8 *) result.base;
  while (bytesToRead) {
    ssize_t bytesRead = read(fileHandle, nextByteLocation, bytesToRead);
    if (bytesRead == -1) {
        sdlPlatformFree(partition, result.base, result.size);
        result.base = 0;
        result.size = 0;
        close(fileHandle);
        return result;
    }
    bytesToRead -= bytesRead;
    nextByteLocation += bytesRead;
  }

  close(fileHandle);
  #endif

  return(result);
}

internal void
sdlInitDataPath() {
  char *basepath = SDL_GetBasePath();
  printf("base_path %s\n", basepath);

  char* datapath = "../data/";

  size_t basepathLen = strlen(basepath);
  size_t datapathLen = strlen(datapath);

  ASSERT(basepathLen + datapathLen + 1 <= FILENAME_MAX);

  memcpy(G_dataPath, basepath, basepathLen);
  memcpy(G_dataPath + basepathLen, datapath, datapathLen);
  G_dataPath[basepathLen + datapathLen] = 0;

  SDL_free(basepath);
}

internal SDL_Surface * 
sdlCreateRGBSurfaceFrom(game_screen_buffer* buffer) {
  int depth = buffer->pitch / buffer->width * 8;
  SDL_Surface* result = SDL_CreateRGBSurfaceFrom(buffer->memory, buffer->width, 
    buffer->height, depth, buffer->pitch, 0, 0, 0, 0);

  return result;
}

window_dimension 
sdlGetWindowDimension(SDL_Window *window) {
    window_dimension result;
    SDL_GetWindowSize(window, &result.width, &result.height);
    return result;
}

internal int
sdlGetWindowRefreshRate(SDL_Window *window) {
  SDL_DisplayMode mode;
  int displayIndex = SDL_GetWindowDisplayIndex(window);
  // If we can't find the refresh rate, we'll return this:
  int defaultRefreshRate = 60;
  if (SDL_GetDesktopDisplayMode(displayIndex, &mode) != 0) {
    return defaultRefreshRate;
  }
  if (mode.refresh_rate == 0) {
    return defaultRefreshRate;
  }
  return mode.refresh_rate;
}

internal void 
sdlResizeScreenBuffer(game_screen_buffer *buffer, int width, int height) {

  if (buffer->memory) {
#if _MSC_VER
    VirtualFree(buffer->memory, 0, MEM_RELEASE);
#else
    munmap(buffer->memory,
          buffer->width * buffer->height * BITES_PER_PIXEL);
#endif
  }
  buffer->width = width;
  buffer->height = height;
  buffer->pitch = width * BITES_PER_PIXEL;
  buffer->bytesPerPixel = BITES_PER_PIXEL;
#if _MSC_VER
  buffer->memory = (uint32 *) VirtualAlloc(0, width * height * BITES_PER_PIXEL, 
                                            MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
#else
  buffer->memory = mmap(0,
                        width * height * BITES_PER_PIXEL,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
#endif
}

internal void 
sdlUpdateWindow(SDL_Window *window, SDL_Renderer *renderer, SDL_Surface *surface) {
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (texture) {
    if (SDL_RenderCopy(renderer, texture, 0, 0) == 0) {
      SDL_RenderPresent(renderer);

    } else {

    }
    SDL_DestroyTexture(texture);
  } else {

  }
}

internal void
sdlProcessKeyPress(game_button_state *newState, bool32 isDown) {
  ASSERT(newState->endedDown != isDown);
  newState->endedDown = isDown;
  ++newState->halfTransitionCount;
}

internal bool 
sdlHandleEvent(SDL_Event *event, game_controller_input *keyboardController) {
  bool shouldQuit = false;

  switch (event->type) {

  	case SDL_QUIT: {
  	  printf("SDL_QUIT\n");
  	  shouldQuit = true;
  	} break;

    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      SDL_Keycode keyCode = event->key.keysym.sym;
      bool isDown = (event->key.state == SDL_PRESSED);
      bool wasDown = (event->key.state == SDL_RELEASED) ||
                     (event->key.repeat != 0);

      if (event->key.repeat == 0) {
        switch (keyCode) {
          case SDLK_w: {
            sdlProcessKeyPress(&keyboardController->moveUp, isDown);
          } break;
          case SDLK_a: {
            sdlProcessKeyPress(&keyboardController->moveLeft, isDown);
          } break;
          case SDLK_s: {
            sdlProcessKeyPress(&keyboardController->moveDown, isDown);
          } break;
          case SDLK_d: {
            sdlProcessKeyPress(&keyboardController->moveRight, isDown);
          } break;
          case SDLK_q: {
            sdlProcessKeyPress(&keyboardController->leftShoulder, isDown);
          } break;
          case SDLK_e: {
            sdlProcessKeyPress(&keyboardController->rightShoulder, isDown);
          } break;
          case SDLK_UP: {
            sdlProcessKeyPress(&keyboardController->actionUp, isDown);
          } break;
          case SDLK_LEFT: {
            sdlProcessKeyPress(&keyboardController->actionLeft, isDown);
          } break;
          case SDLK_DOWN: {
            sdlProcessKeyPress(&keyboardController->actionDown, isDown);
          } break;
          case SDLK_RIGHT: {
            sdlProcessKeyPress(&keyboardController->actionRight, isDown);
          } break;
          case SDLK_ESCAPE: {
            sdlProcessKeyPress(&keyboardController->back, isDown);
          } break;
          case SDLK_SPACE: {
            sdlProcessKeyPress(&keyboardController->start, isDown);
          } break;
        }
      }
        
    } break;

  	case SDL_WINDOWEVENT: {

  	  switch (event->window.event) {

  	  	case SDL_WINDOWEVENT_SIZE_CHANGED: {
          SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
          printf("SDL_WINDOWEVENT_SIZE_CHANGED (%d, %d)\n", event->window.data1, 
            event->window.data2);
  	  	} break;

        case SDL_WINDOWEVENT_FOCUS_GAINED: {
          printf("SDL_WINDOWEVENT_FOCUS_GAINED\n");
        } break;

        case SDL_WINDOWEVENT_EXPOSED: {
          printf("SDL_WINDOWEVENT_EXPOSED\n");
          SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
          SDL_Renderer *renderer = SDL_GetRenderer(window);
          SDL_Surface *surface = sdlCreateRGBSurfaceFrom(&G_backbuffer);
          if (surface) {
            sdlUpdateWindow(window, renderer, surface);
            SDL_FreeSurface(surface);
          }
        } break;
  	  }
  	} break;
  }

  return shouldQuit;
}

internal real32
sdlGetSecondsElapsed(uint64 oldCounter, uint64 currentCounter) {
    return ( (real32) (currentCounter - oldCounter) 
           / (real32) (SDL_GetPerformanceFrequency()) );
}

int 
main(int argc, char *argv[]) {
  int result = 0;
  if ((result = SDL_Init(SDL_INIT_VIDEO)) != 0) {
  	return result;
  }

  sdl_state sdlState = {};

  sdlInitDataPath();

  uint64 perfCountFrequency = SDL_GetPerformanceFrequency();

  SDL_Window *window = SDL_CreateWindow("Tron", SDL_WINDOWPOS_UNDEFINED,
  	SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_RESIZABLE);

  if (window) {

    int monitorRefreshHz = sdlGetWindowRefreshRate(window);
    int gameUpdateHz = (int) (monitorRefreshHz / 2.0f);
    printf("Refresh rate is %d Hz\n", gameUpdateHz);
    real32 targetSecondsPerFrame = 1.0f / (real32) gameUpdateHz;

    // Create a "Renderer" for our window.
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

    if (renderer) {
      SDL_RenderClear(renderer);
      bool running = true;

      window_dimension dimension = sdlGetWindowDimension(window);
      sdlResizeScreenBuffer(&G_backbuffer, dimension.width, dimension.height);

      SDL_Surface *surface = sdlCreateRGBSurfaceFrom(&G_backbuffer);

      if (surface) {
        game_input input[2] = {};
        game_input *newInput = &input[0];
        game_input *oldInput = &input[1];

#if TRON_INTERNAL
        void *baseAddress = (void *) TERABYTES(2);
#else
        void *baseAddress = (void *) 0;
#endif
        game_memory gameMemory = {};
        gameMemory.permanentStorage.size = MEGABYTES(64);
        gameMemory.transientStorage.size = MEGABYTES(64);
        gameMemory.platformFunctions.readEntireFile = sdlPlatformReadEntireFile;
        gameMemory.platformFunctions.free = sdlPlatformFree;
        gameMemory.platformFunctions.alloc = sdlPlatformAlloc;

        sdlState.totalSize = gameMemory.permanentStorage.size 
                           + gameMemory.transientStorage.size;

#if _MSC_VER
        sdlState.gameMemoryBlock = VirtualAlloc(0, sdlState.totalSize, 
                                                MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
#else
        sdlState.gameMemoryBlock = mmap(0, sdlState.totalSize,
                                        PROT_READ | PROT_WRITE,
                                        MAP_ANON | MAP_PRIVATE,
                                        -1, 0);
#endif

        gameMemory.permanentStorage.base = (uint8*) sdlState.gameMemoryBlock;
        gameMemory.transientStorage.base = (uint8*) (gameMemory.permanentStorage.base) 
                                                   + gameMemory.permanentStorage.size;

        if (sdlState.gameMemoryBlock) {

          game_input input[2] = {};
          game_input *newInput = &input[0];
          game_input *oldInput = &input[1];

          uint64 lastCounter = SDL_GetPerformanceCounter();
          // uint64 lastCycleCount = _rdtsc();

          while (running) {

            newInput->dtForFrame = targetSecondsPerFrame;

            game_controller_input *oldKeyboardController = getController(oldInput, 0);
            game_controller_input *newKeyboardController = getController(newInput, 0);
            *newKeyboardController = {};
            newKeyboardController->isConnected = true;
            for (int buttonIndex = 0; 
              buttonIndex < ARRAY_COUNT(newKeyboardController->buttons);
              ++buttonIndex) {
              newKeyboardController->buttons[buttonIndex].endedDown =
                oldKeyboardController->buttons[buttonIndex].endedDown;
            }

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
              if (sdlHandleEvent(&event, newKeyboardController)) {
                running = false;
              }
            }
            
            int lockSurface = 0;   
            if (SDL_MUSTLOCK(surface) != 0) {
              lockSurface = SDL_LockSurface(surface);
            }
            if (lockSurface == 0) {
              game_screen_buffer buffer = {};
              buffer.memory = surface->pixels;
              buffer.width = surface->w;
              buffer.height = surface->h;
              buffer.pitch = surface->pitch;
              buffer.bytesPerPixel = buffer.pitch / buffer.width;
              gameUpdateAndRender(&gameMemory, newInput, &buffer);
              if (SDL_MUSTLOCK(surface) != 0) {
                SDL_UnlockSurface(surface);
              }
            }

            real32 secondsElapsedForFrame = sdlGetSecondsElapsed(lastCounter, SDL_GetPerformanceCounter());
            if (secondsElapsedForFrame < targetSecondsPerFrame) {
                int32 timeToSleep = (int32) ((targetSecondsPerFrame - secondsElapsedForFrame) * 1000) - 1;
                if (timeToSleep > 0) {
                  //printf("timeToSleep %d\n", timeToSleep);
                  SDL_Delay(timeToSleep);
                }
                while (secondsElapsedForFrame < targetSecondsPerFrame) {
                  secondsElapsedForFrame = sdlGetSecondsElapsed(lastCounter, SDL_GetPerformanceCounter());
                }
            } else {
              printf("MISSED FRAME RATE!\n");
            }

            uint64 endCounter = SDL_GetPerformanceCounter();
#if 0
            uint64 endCycleCount = _rdtsc();
            uint64 counterElapsed = endCounter - lastCounter;
            uint64 cyclesElapsed = endCycleCount - lastCycleCount;

            real64 msPerFrame = (((1000.0f * (real64) counterElapsed) / 
                                             (real64) perfCountFrequency));
            real64 fps = (real64) perfCountFrequency / (real64) counterElapsed;
            real64 mcpf = ((real64) cyclesElapsed / (1000.0f * 1000.0f));

            printf("%.02fms/f, %.02ff/s, %.02fmc/f\n", msPerFrame, fps, mcpf);

            lastCycleCount = endCycleCount;
#endif
            lastCounter = endCounter; 

            sdlUpdateWindow(window, renderer, surface);

            game_input *temp = newInput;
            newInput = oldInput;
            oldInput = temp;

          }
          SDL_FreeSurface(surface);
        
        } else {

        }
      } else {
          
      }
    } else {

    }
  } else {

  }
  
  SDL_Quit();
  return result;
}

