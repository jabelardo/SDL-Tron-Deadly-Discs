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

#pragma pack(push, 1)
struct bitmap_header {
  uint16 fileType;
  uint32 fileSize;
  uint16 reserved1;
  uint16 reserved2;
  uint32 bitmapOffset;
  uint32 size;
  int32 width;
  int32 height;
  uint16 planes;
  uint16 bitsPerPixel;
  uint32 compression;
  uint32 sizeOfBitmap;
  int32 horzResolution;
  int32 vertResolution;
  uint32 colorsUsed;
  uint32 colorsImportant;
  uint32 redMask;
  uint32 greenMask;
  uint32 blueMask;
};
#pragma pack(pop)

internal bitmap_buffer
loadBmp(memory_partition* partition, platform_functions functions, char *filename) {

  bitmap_buffer result = {};

  mem_buffer readResult = functions.readEntireFile(partition, filename);    

  if (readResult.size != 0) {

    bitmap_header *header = (bitmap_header *) readResult.base;
    result.pixels = (uint32 *) ((uint8 *) readResult.base + header->bitmapOffset);
    result.width = header->width;
    result.height = header->height;
    result.memory = readResult.base;
	  result.memorySize = readResult.size;

    ASSERT(header->compression == 3);

    // NOTE(casey): If you are using this generically for some reason,
    // please remember that BMP files CAN GO IN EITHER DIRECTION and
    // the height will be negative for top-down.
    // (Also, there can be compression, etc., etc... DON'T think this
    // is complete BMP loading code because it isn't!!)

    // NOTE(casey): Byte order in memory is determined by the Header itself,
    // so we have to read out the masks and convert the pixels ourselves.
    uint32 redMask = header->redMask;
    uint32 greenMask = header->greenMask;
    uint32 blueMask = header->blueMask;
    uint32 alphaMask = ~(redMask | greenMask | blueMask);        
    
    bit_scan_result redShift = leastSignificantSetBit(redMask);
    bit_scan_result greenShift = leastSignificantSetBit(greenMask);
    bit_scan_result blueShift = leastSignificantSetBit(blueMask);
    bit_scan_result alphaShift = leastSignificantSetBit(alphaMask);

    ASSERT(redShift.found);
    ASSERT(greenShift.found);
    ASSERT(blueShift.found);
    ASSERT(alphaShift.found);
    
    uint32 *sourceDest = result.pixels;

    for (int32 y = 0; y < header->height; ++y) {

      for(int32 x = 0; x < header->width; ++x) {

        uint32 c = *sourceDest;

        *sourceDest++ = ((((c >> alphaShift.index) & 0xff) << 24) |
                         (((c >> redShift.index) & 0xff) << 16) |
                         (((c >> greenShift.index) & 0xff) << 8) |
                         (((c >> blueShift.index) & 0xff) << 0));
      }
    }
  }

  return result;
}

internal void
copyBmp(memory_partition* memory, bitmap_buffer* dest, bitmap_buffer* src, 
        int32 srcX, int32 srcY, int32 width, int32 height, bool hflip = false) {

  dest->width = width;
  dest->height = height;
  dest->pitch = width * sizeof(uint32);
  dest->memorySize = dest->pitch * dest->height;
  dest->pixels = (uint32 *)((uint8*) memory->base + memory->used);
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
drawLine(game_screen_buffer *buffer, 
         real32 realX0, real32 realY0, 
         real32 realX1, real32 realY1,
         real32 R, real32 G, real32 B) {
  
  int32 x0 = (int32) roundf(realX0);
  int32 y0 = (int32) roundf(realY0);
  int32 x1 = (int32) roundf(realX1);
  int32 y1 = (int32) roundf(realY1);

  if (x0 < 0) {
    x0 = 0;
  }
  if (x0 > buffer->width) {
    x0 = buffer->width;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (y0 > buffer->height) {
    y0 = buffer->height;
  }
  if (x1 < 0) {
    x1 = 0;
  }
  if (x1 > buffer->width) {
    x1 = buffer->width;
  }
  if (y1 < 0) {
    y1 = 0;
  }
  if (y1 > buffer->height) {
    y1 = buffer->height;
  }

  int32 dx = abs(x1 - x0);
  int32 sx = (x0 < x1) ? 1 : -1;
  int32 dy = abs(y1 - y0);
  int32 sy = (y0 < y1) ? 1 : -1; 
  int32 err = (dx > dy ? dx : -dy) / 2, e2;

  uint32 color = (((int32) roundf(R * 255.0f) << 16) |
                  ((int32) roundf(G * 255.0f) << 8) |
                  ((int32) roundf(B * 255.0f) << 0));
    

 
  for (;;) {
    uint8 *row = ((uint8 *) buffer->memory + 
                 x0 * buffer->bytesPerPixel + 
                 (buffer->height - y0 - 1) * buffer->pitch);

    uint32 *pixel = (uint32 *) row;
    *pixel = color;

    if ((x0 == x1) && (y0 == y1)) { 
      break; 
    }
    e2 = err;
    if (e2 > -dx) { 
      err -= dy; 
      x0 += sx; 
    }
    if (e2 <  dy) { 
      err += dx; 
      y0 += sy; 
    }
  }
}

internal void
drawRectangle(game_screen_buffer *buffer,
              real32 realMinX, real32 realMinY, real32 realMaxX, real32 realMaxY,
              real32 R, real32 G, real32 B) {    
    int32 minX = (int32) roundf(realMinX);
    int32 minY = (int32) roundf(realMinY);
    int32 maxX = (int32) roundf(realMaxX);
    int32 maxY = (int32) roundf(realMaxY);

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
           int32 alignX = 0, int32 alignY = 0, real32 cAlpha = 1.f) {

    realX -= (real32) alignX;
    realY -= (real32) alignY;

    int32 minX = (int32) roundf(realX);
    int32 minY = (int32) roundf(realY);
    int32 maxX = (int32) roundf(realX + (real32) bitmap->width);
    int32 maxY = (int32) roundf(realY + (real32) bitmap->height);

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

          real32 A = (real32)((*source >> 24) & 0xFF) / 255.0f;
          A *= cAlpha;

          real32 SR = (real32)((*source >> 16) & 0xFF);
          real32 SG = (real32)((*source >> 8) & 0xFF);
          real32 SB = (real32)((*source >> 0) & 0xFF);

          real32 DR = (real32)((*dest >> 16) & 0xFF);
          real32 DG = (real32)((*dest >> 8) & 0xFF);
          real32 DB = (real32)((*dest >> 0) & 0xFF);

          // TODO(casey): Someday, we need to talk about premultiplied alpha!
          // (this is not premultiplied alpha)
          real32 R = (1.0f - A) * DR + A * SR;
          real32 G = (1.0f - A) * DG + A * SG;
          real32 B = (1.0f - A) * DB + A * SB;

          *dest = (((uint32)(R + 0.5f) << 16) |
                   ((uint32)(G + 0.5f) << 8) |
                   ((uint32)(B + 0.5f) << 0));          
          ++dest;
          ++source;
        }
        destRow -= buffer->pitch;
        sourceRow -= bitmap->width;
    }
}

internal bool
hitsWall(v2 wallP0, v2 wallP1, v2 oldPos, v2 newPos) {

  bool hit = false;

  // real32 tEpsilon = 0.00001f;
  // if (deltaPos.x != 0.0f) {
  //   real32 tResult = (wallX - oldPos.x) / deltaPos.x;
  //   real32 y = oldPos.y + tResult * deltaPos.y;
  
  //   if ((tResult >= 0.0f) && (*tMin > tResult))  {
  //     if ((y >= minY) && (y <= maxY)) {
  //       *tMin = MAXIMUM(0.0f, tResult - tEpsilon);
  //       hit = true;
  //     }
  //   }
  // }

  return hit;
}

extern "C" void
gameUpdateAndRender(game_memory* memory, game_input *input, game_screen_buffer *buffer) {
  
  ASSERT(sizeof(game_state) <= memory->permanentStorage.size);

  game_state *gameState = (game_state *) memory->permanentStorage.base;

  if (!memory->isInitialized) {
    memory->permanentStorage.used = sizeof(game_state);
    bitmap_buffer runningManBmp = loadBmp(&memory->transientStorage, memory->platformFunctions, "running_man.bmp");
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
    memory->platformFunctions.free(&memory->transientStorage, runningManBmp.memory, runningManBmp.memorySize);

    gameState->pixelsPerMt = 20.0f;
    gameState->runnersCount = 2;

    gameState->walls[0] = {v2{1, 1}, v2{1, 22}, 1, 0, 0};
    gameState->walls[1] = {v2{1, 1}, v2{31, 1}, 1, 0, 0};
    gameState->walls[2] = {v2{31, 1}, v2{31, 22}, 1, 0, 0};
    gameState->walls[3] = {v2{1, 22}, v2{31, 22}, 1, 0, 0};

    gameState->runners[0].bitmap = &runnerBitmaps->down[0];
    gameState->runners[0].direction = DIRECTION_DOWN;

    gameState->runners[0].maxAccel = 9.0f;
    gameState->runners[0].maxSpeed = 12.0f;
    gameState->runners[0].deltaAccel = 1.f;
    gameState->runners[0].deltaDrag = 4.f;

    gameState->runners[0].position.x = 15;
    gameState->runners[0].position.y = 10.5f;

    memory->isInitialized = true;
  }

  game_controller_input *input0 = &input->controllers[0];    
  
  runner_state *runner = &gameState->runners[0];

  if (input0->actionDown.endedDown) {
    runner->direction = DIRECTION_DOWN;
    runner->accel.y -= runner->deltaAccel;
  }
  if (input0->actionUp.endedDown) {
    runner->direction = DIRECTION_UP;
    runner->accel.y += runner->deltaAccel;
  }
  if (!input0->actionDown.endedDown && !input0->actionUp.endedDown) {
    if (runner->accel.y > 0) {
      runner->accel.y -= runner->deltaDrag * runner->deltaAccel;
      if (runner->accel.y < 0) {
        runner->accel.y = 0;
      }
    } else if (runner->accel.y < 0) {
      runner->accel.y += runner->deltaDrag * runner->deltaAccel;
      if (runner->accel.y > 0) {
        runner->accel.y = 0;
      }
    }
  }
  if (input0->actionLeft.endedDown) {
    runner->direction = DIRECTION_LEFT;
    runner->accel.x -= runner->deltaAccel;
  }
  if (input0->actionRight.endedDown) {
    runner->direction = DIRECTION_RIGHT;
    runner->accel.x += runner->deltaAccel;
  }
  if (!input0->actionLeft.endedDown && !input0->actionRight.endedDown) {
    if (runner->accel.x > 0) {
      runner->accel.x -= runner->deltaDrag * runner->deltaAccel;
      if (runner->accel.x < 0) {
        runner->accel.x = 0;
      }
    } else if (runner->accel.x < 0) {
      runner->accel.x += runner->deltaDrag * runner->deltaAccel;
      if (runner->accel.x > 0) {
        runner->accel.x = 0;
      }
    }
  }
  real32 squareLenAccel = squareLen(runner->accel);
  if (squareLenAccel > square(runner->maxAccel)) {
    real32 ratio = runner->maxAccel / sqrtf(squareLenAccel);
    runner->accel.x *= ratio;
    runner->accel.y *= ratio;
  }

  runner->speed += runner->accel - (.2f * runner->speed);

  real32 squareLenSpped = squareLen(runner->speed);
  if (squareLenSpped > square(runner->maxSpeed)) {
    real32 ratio = runner->maxSpeed / sqrtf(squareLenSpped);
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
  int pixelSpeedX = (int) (runner->speed.x * gameState->pixelsPerMt);
  int pixelSpeedY = (int) (runner->speed.y * gameState->pixelsPerMt);

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

  drawRectangle(buffer, 0.f, 0.f, (real32) buffer->width, (real32) buffer->height, 0.f, 0.f, 0.f);

  for (int wallIndex = 0; wallIndex < ARRAY_COUNT(gameState->walls); ++wallIndex) {
    real32 x1 = gameState->walls[wallIndex].p1.x * gameState->pixelsPerMt; 
    real32 y1 = gameState->walls[wallIndex].p1.y * gameState->pixelsPerMt;
    real32 x2 = gameState->walls[wallIndex].p2.x * gameState->pixelsPerMt; 
    real32 y2 = gameState->walls[wallIndex].p2.y * gameState->pixelsPerMt; 
    real32 r = gameState->walls[wallIndex].r;
    real32 g = gameState->walls[wallIndex].g;
    real32 b = gameState->walls[wallIndex].b;
    drawLine(buffer, x1, y1, x2, y2, r, g, b);
  }

  int x0 = (int) (gameState->runners[0].position.x * gameState->pixelsPerMt);
  int y0 = (int) (gameState->runners[0].position.y * gameState->pixelsPerMt);

  drawBitmap(buffer, gameState->runners[0].bitmap, (real32) x0, (real32) y0);

}