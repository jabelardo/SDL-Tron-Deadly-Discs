#include "tron.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

inline game_controller_input *
getController(game_input *input, unsigned int controllerIndex) {
  ASSERT(controllerIndex < ARRAY_COUNT(input->controllers));
  
  game_controller_input *result = &input->controllers[controllerIndex];
  return(result);
}

internal void
copyBmp(memory_partition* memory, bitmap_buffer* dest, bitmap_buffer* src, 
        int32 srcX, int32 srcY, int32 width, int32 height, bool hflip = false) {

  dest->width = width;
  dest->height = height;
  dest->pitch = width * sizeof(uint32);
  dest->memorySize = dest->pitch * dest->height;
  dest->pixels = (uint32 *)(memory->base + memory->used);
  memory->used += dest->memorySize;

  uint32 *sourceRow = ((uint32 *) src->pixels) + srcY * src->width + srcX;
  uint32 *destRow = dest->pixels;

  for (int y = 0; y < height; ++y) { 
    if (hflip) {
      for (int x = 0; x < dest->width; ++x) {
        destRow[dest->width - 1 - x] = sourceRow[x];
      }
    } else {
      memcpy(destRow, sourceRow, dest->pitch);
    }
    destRow += dest->width;
    sourceRow += src->width;
  }
}

internal void
drawRectangle(game_screen_buffer *buffer,
              real32 realMinX, real32 realMinY, real32 realMaxX, real32 realMaxY,
              real32 R, real32 G, real32 B) {    
    int32 minX = roundf(realMinX);
    int32 minY = roundf(realMinY);
    int32 maxX = roundf(realMaxX);
    int32 maxY = roundf(realMaxY);

    if (minX < 0) {
      minX = 0;
    }
    if (maxX > buffer->width) {
      maxX = buffer->width;
    }
    if (minY < 0) {
      minY = 0;
    }
    if (maxY > buffer->height) {
      maxY = buffer->height;
    }

    uint32 color = (((int32) roundf(R * 255.0f) << 16) |
                    ((int32) roundf(G * 255.0f) << 8) |
                    ((int32) roundf(B * 255.0f) << 0));
    
    uint8 *row = ((uint8 *) buffer->memory + 
                  minX * buffer->bytesPerPixel + 
                  (buffer->height - minY - 1) * buffer->pitch);

    for (int y = minY; y < maxY; ++y) {
      uint32 *pixel = (uint32 *) row;
      for (int x = minX; x < maxX; ++x) {          
        *pixel++ = color;
      }
      row -= buffer->pitch;
    }

}
internal void
drawBitmap(game_screen_buffer *buffer, bitmap_buffer *bitmap, real32 realX, real32 realY,
           real32 filterR = 1, real32 filterG = 1, real32 filterB = 1, 
           int32 alignX = 0, int32 alignY = 0) {

    realX -= (real32) alignX;
    realY -= (real32) alignY;

    int32 minX = roundf(realX);
    int32 minY = roundf(realY);
    int32 maxX = roundf(realX + (real32) bitmap->width);
    int32 maxY = roundf(realY + (real32) bitmap->height);

    int32 sourceOffsetX = 0;
    if (minX < 0) {
        sourceOffsetX = -minX;
        minX = 0;
    }
    if (maxX > buffer->width) {
        maxX = buffer->width;
    }

    int32 sourceOffsetY = 0;
    if (minY < 0) {
        sourceOffsetY = -minY;
        minY = 0;
    }
    if (maxY > buffer->height) {
        maxY = buffer->height;
    }

    uint32 *sourceRow = bitmap->pixels + bitmap->width * (bitmap->height - 1);
    sourceRow -= -sourceOffsetY * bitmap->width + sourceOffsetX;

    uint8 *destRow = ((uint8 *) buffer->memory +
                      minX * buffer->bytesPerPixel +
                      (buffer->height - minY - 1) * buffer->pitch);

    for (int y = minY; y < maxY; ++y) {
        uint32 *dest = (uint32 *) destRow;
        uint32 *source = sourceRow;
        for (int x = minX; x < maxX; ++x) { 
          real32 SR = (real32)((*source >> 24) & 0xFF) / 255.f;
          real32 SG = (real32)((*source >> 16) & 0xFF) / 255.f;
          real32 SB = (real32)((*source >> 8) & 0xFF) / 255.f;
          real32 A = 1.0f - (real32)((*source >> 0) & 0xFF) / 255.f;

          // real32 A = (real32)((*source >> 24) & 0xFF) / 255.f;
          // real32 SR = (real32)((*source >> 16) & 0xFF) / 255.f;
          // real32 SG = (real32)((*source >> 8) & 0xFF) / 255.f;
          // real32 SB = (real32)((*source >> 0) & 0xFF) / 255.f;

          real32 DR = (real32)((*dest >> 16) & 0xFF) / 255.f;
          real32 DG = (real32)((*dest >> 8) & 0xFF) / 255.f;
          real32 DB = (real32)((*dest >> 0) & 0xFF) / 255.f;

          real32 R = ((1.0f - A) * DR + A * SR) * filterR;
          real32 G = ((1.0f - A) * DG + A * SG) * filterG;
          real32 B = ((1.0f - A) * DB + A * SB) * filterB;

          *dest = (((uint32)(R * 255) << 16) |
                   ((uint32)(G * 255) << 8) |
                   ((uint32)(B * 255) << 0));          
          ++dest;
          ++source;
        }
        destRow -= buffer->pitch;
        sourceRow -= bitmap->width;
    }
}

extern "C" void
gameUpdateAndRender(game_memory* memory, game_input *input, game_screen_buffer *buffer) {
  
  ASSERT(sizeof(game_state) <= memory->permanentStorage.size);

  game_state *gameState = (game_state *) memory->permanentStorage.base;

  if (!memory->isInitialized) {
    memory->permanentStorage.used = sizeof(game_state);
    bitmap_buffer runningManBmp = memory->platformLoadBmp("running_man.bmp");
    runner_bitmaps* runnerBitmaps = &gameState->runnerBitmaps;
    copyBmp(&memory->permanentStorage, &runnerBitmaps->down[0], &runningManBmp, 209, 78, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->down[1], &runningManBmp, 242, 78, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->down[2], &runningManBmp, 209, 78, 28, 37, true);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->down[3], &runningManBmp, 175, 78, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->up[0], &runningManBmp, 209, 39, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->up[1], &runningManBmp, 242, 39, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->up[2], &runningManBmp, 209, 39, 28, 37, true);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->up[3], &runningManBmp, 175, 39, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->right[0], &runningManBmp, 5, 2, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->left [0], &runningManBmp, 5, 2, 28, 37, true);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->right[1], &runningManBmp, 39, 2, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->left [1], &runningManBmp, 39, 2, 28, 37, true);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->right[2], &runningManBmp, 73, 2, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->left [2], &runningManBmp, 73, 2, 28, 37, true);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->right[3], &runningManBmp, 106, 2, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->left [3], &runningManBmp, 106, 2, 28, 37, true);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->right[4], &runningManBmp, 141, 2, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->left [4], &runningManBmp, 141, 2, 28, 37, true);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->right[5], &runningManBmp, 175, 2, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->left [5], &runningManBmp, 175, 2, 28, 37, true);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->right[6], &runningManBmp, 208, 2, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->left [6], &runningManBmp, 208, 2, 28, 37, true);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->right[7], &runningManBmp, 240, 2, 28, 37);
    copyBmp(&memory->permanentStorage, &runnerBitmaps->left [7], &runningManBmp, 240, 2, 28, 37, true);
    memory->platformFreeBmp(&runningManBmp);
    gameState->runnerState.bitmap = &runnerBitmaps->down[0];
    gameState->runnerState.direction = DIRECTION_DOWN;

    gameState->runnerState.position.x = 0;
    gameState->runnerState.position.y = 0;

    gameState->maxAccel = 9.0f;
    gameState->maxSpeed = 12.0f;
    gameState->pixelsPerMt = 20.0f;
    gameState->dAccel = .1f;
    gameState->dDeaccel = 4.f;

    memory->isInitialized = true;
  }

  game_controller_input *input0 = &input->controllers[0];    
  
  runner_state *runner = &gameState->runnerState;

  real32 dAccel = gameState->maxAccel * gameState->dAccel;
  if (input0->actionDown.endedDown) {
    runner->direction = DIRECTION_DOWN;
    runner->accel.y -= dAccel;
  }
  if (input0->actionUp.endedDown) {
    runner->direction = DIRECTION_UP;
    runner->accel.y += dAccel;
  }
  if (!input0->actionDown.endedDown && !input0->actionUp.endedDown) {
    if (runner->accel.y > 0) {
      runner->accel.y -= gameState->dDeaccel * dAccel;
      if (runner->accel.y < 0) {
        runner->accel.y = 0;
      }
    } else if (runner->accel.y < 0) {
      runner->accel.y += gameState->dDeaccel * dAccel;
      if (runner->accel.y > 0) {
        runner->accel.y = 0;
      }
    }
  }
  if (input0->actionLeft.endedDown) {
    runner->direction = DIRECTION_LEFT;
    runner->accel.x -= dAccel;
  }
  if (input0->actionRight.endedDown) {
    runner->direction = DIRECTION_RIGHT;
    runner->accel.x += dAccel;
  }
  if (!input0->actionLeft.endedDown && !input0->actionRight.endedDown) {
    if (runner->accel.x > 0) {
      runner->accel.x -= gameState->dDeaccel * dAccel;
      if (runner->accel.x < 0) {
        runner->accel.x = 0;
      }
    } else if (runner->accel.x < 0) {
      runner->accel.x += gameState->dDeaccel * dAccel;
      if (runner->accel.x > 0) {
        runner->accel.x = 0;
      }
    }
  }
  real32 squareLenAccel = squareLen(runner->accel);
  if (squareLenAccel > square(gameState->maxAccel)) {
    real32 ratio = gameState->maxAccel / sqrtf(squareLenAccel);
    runner->accel.x *= ratio;
    runner->accel.y *= ratio;
  }

  runner->speed += runner->accel - (.2f * runner->speed);

  real32 squareLenSpped = squareLen(runner->speed);
  if (squareLenSpped > square(gameState->maxSpeed)) {
    real32 ratio = gameState->maxSpeed / sqrtf(squareLenSpped);
    runner->speed.x *= ratio;
    runner->speed.y *= ratio;
  }

  runner->position.x += runner->speed.x * input->dtForFrame;
  runner->position.y += runner->speed.y * input->dtForFrame;

  if (runner->position.x < 0) {
    runner->position.x = 0;
  }
  real32 maxXPos = (buffer->width - 28) / gameState->pixelsPerMt;
  if (runner->position.x > maxXPos) {
    runner->position.x = maxXPos;
  }
  if (runner->position.y < 0) {
    runner->position.y = 0;
  }
  real32 maxYPos = (buffer->height - 37) / gameState->pixelsPerMt;
  if (runner->position.y > maxYPos) {
    runner->position.y = maxYPos;
  }
  int framesPerBitmap = 3;
  int pixelSpeedX = runner->speed.x * gameState->pixelsPerMt;
  int pixelSpeedY = runner->speed.y * gameState->pixelsPerMt;

  if (pixelSpeedX > 1 || pixelSpeedX < -1 || pixelSpeedY > 1 || pixelSpeedY < -1) {
    runner->bitmapIndex = (runner->bitmapIndex < 8 * framesPerBitmap - 1) 
                        ? ++runner->bitmapIndex : 0;

    if (runner->direction == DIRECTION_RIGHT) {
      runner->bitmap = &gameState->runnerBitmaps.right[runner->bitmapIndex/framesPerBitmap];

    } else if (runner->direction == DIRECTION_LEFT) { 
      runner->bitmap = &gameState->runnerBitmaps.left[runner->bitmapIndex/framesPerBitmap];

    } else if (runner->direction == DIRECTION_DOWN) {
      runner->bitmap = &gameState->runnerBitmaps.down[runner->bitmapIndex/framesPerBitmap/2];

    } else if (runner->direction == DIRECTION_UP) {
      runner->bitmap = &gameState->runnerBitmaps.up[runner->bitmapIndex/framesPerBitmap/2];
    }
  } else {
    runner->bitmapIndex = 0;
    if (runner->direction == DIRECTION_RIGHT) {
      runner->bitmap = &gameState->runnerBitmaps.right[0];

    } else if (runner->direction == DIRECTION_LEFT) { 
      runner->bitmap = &gameState->runnerBitmaps.left[0];

    } else if (runner->direction == DIRECTION_DOWN) {
      runner->bitmap = &gameState->runnerBitmaps.down[0];

    } else if (runner->direction == DIRECTION_UP) {
      runner->bitmap = &gameState->runnerBitmaps.up[0];
    }
  }

  drawRectangle(buffer, 0, 0, buffer->width, buffer->height, 0, 0, 0);

  drawRectangle(buffer, 0, 0, 10, 10, 0, 0, 1);
  drawRectangle(buffer, buffer->width - 10, 0, buffer->width, 10, 1, 0, 0);
  drawRectangle(buffer, buffer->width - 10, buffer->height - 10, buffer->width, buffer->height, 0, 1, 0);
  drawRectangle(buffer, 0, buffer->height - 10, 10, buffer->height, 1, 1, 0);

  int x = gameState->runnerState.position.x * gameState->pixelsPerMt;
  int y = gameState->runnerState.position.y * gameState->pixelsPerMt;

  drawBitmap(buffer, gameState->runnerState.bitmap, x, y, 0, .8, 1);
}