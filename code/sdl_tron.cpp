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
#include <stdio.h>
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

#define BITES_PER_PIXEL 4

internal void
debugSdlPlatformFreeFileMemory(debug_read_file_result* memory) {
  if (memory && memory->contents && memory->contentsSize) {
#if _MSC_VER
    VirtualFree(memory->contents, 0, MEM_RELEASE);
#else
    munmap(memory->contents, memory->contentsSize);
#endif
    memory->contents = 0;
    memory->contentsSize = 0;
  }
}

internal debug_read_file_result 
debugSdlPlatformReadEntireFile(char *filename) {
  debug_read_file_result result = {};
  #if 0
  int fileHandle = open(filename, O_RDONLY);
  if (fileHandle == -1) {
    return result;
  }

  struct stat fileStatus;
  if (fstat(fileHandle, &fileStatus) == -1) {
    close(fileHandle);
    return result;
  }
  result.contentsSize = fileStatus.st_size;

  result.contents = mmap(0,
                         result.contentsSize,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1,
                         0);

  if (!result.contents) {
    close(fileHandle);
    result.contentsSize = 0;
    return result;
  }

  uint32 bytesToRead = result.contentsSize;
  uint8 *nextByteLocation = (uint8 *) result.contents;
  while (bytesToRead) {
    ssize_t bytesRead = read(fileHandle, nextByteLocation, bytesToRead);
    if (bytesRead == -1)
    {
        munmap(result.contents, result.contentsSize);
        result.contents = 0;
        result.contentsSize = 0;
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
sdlPlatformFreeBmp(bitmap_buffer* memory) {
  if (memory && memory->pixels && memory->memorySize) {
#if _MSC_VER
    VirtualFree(memory->pixels, 0, MEM_RELEASE);
#else
    munmap(memory->pixels, memory->memorySize);
#endif
    memory->pixels = 0;
    memory->memorySize = 0;
  }
}

internal bitmap_buffer 
sdlPlatformLoadBmp(char *filename) {
  bitmap_buffer result = {};
  char *basepath = SDL_GetBasePath();
  printf("base_path %s\n", basepath);

  char* datapath = "../data/";

  char filePath[FILENAME_MAX];

  size_t basepathLen = strlen(basepath);
  size_t datapathLen = strlen(datapath);
  size_t filenameLen = strlen(filename);

  ASSERT(basepathLen + datapathLen + filenameLen + 1 <= FILENAME_MAX);

  memcpy(filePath, basepath, basepathLen);
  memcpy(filePath + basepathLen, datapath, datapathLen);
  memcpy(filePath + basepathLen + datapathLen, filename, filenameLen);
  filePath[basepathLen + datapathLen + filenameLen] = 0;

  SDL_Surface* surface = SDL_LoadBMP(filePath);
  if (surface) {
    result.memorySize = surface->pitch * surface->h * sizeof(uint32);
    result.width = surface->w;
    result.height = surface->h;
    result.pitch = surface->pitch;

#if _MSC_VER
    result.pixels = (uint32 *) VirtualAlloc(0, result.memorySize, 
                                            MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
#else
    result.pixels = (uint32 *) mmap(0,
                                    result.memorySize,
                                    PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS,
                                    -1,
                                    0);
#endif

    memcpy(result.pixels, surface->pixels, result.memorySize);

    printf("loaded %s\n", filePath);
    printf("memorySize %d width %d height %d pitch %d\n", result.memorySize,
      result.width, result.height, result.pitch);

    SDL_FreeSurface(surface);
  } else {
    printf("can't load %s\n", filePath);
  }
  SDL_free(basepath);
  return result;
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
        gameMemory.transientStorage.size = GIGABYTES(1);
        gameMemory.platformLoadBmp = sdlPlatformLoadBmp;
        gameMemory.platformFreeBmp = sdlPlatformFreeBmp;

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

