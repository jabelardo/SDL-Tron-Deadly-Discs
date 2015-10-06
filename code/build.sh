#!/bin/bash

mkdir -p ../build

pushd ../build

CommonFlags="-Wall -Werror -Wno-unused-variable -Wno-unused-function -Wno-writable-strings \
	-std=gnu++11 -fno-rtti -fno-exceptions -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 \
	-DHANDMADE_SDL=1"

c++ $CommonFlags ../code/sdl_tron.cpp -o sdl_tron -g \
	`/usr/local/bin/sdl2-config --cflags --libs`

popd
