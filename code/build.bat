@echo off

set SDL_INCLUDE="C:\Users\jabel\Documents\GitHub\SDL2-2.0.3\include"
set SDL_LIB="C:\Users\jabel\Documents\GitHub\SDL2-2.0.3\lib\x64"

set CommonCompilerFlags=-I%SDL_INCLUDE% -MDd -nologo -fp:fast -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4456 -wd4201 -wd4100 -wd4189 -wd4505 -DTRON_INTERNAL=1 -DTRON_SLOW=1 -DTRON_WIN32=1 -FC -Z7
set CommonLinkerFlags=/LIBPATH:%SDL_LIB% -incremental:no -opt:ref user32.lib SDL2.lib SDL2main.lib  /SUBSYSTEM:WINDOWS /NODEFAULTLIB:msvcrt.lib

REM TODO - can we just build both with one exe?

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM 32-bit build
REM cl %CommonCompilerFlags% ..\code\sdl_tron.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
REM del *.pdb > NUL 2> NUL
REM Optimization switches /O2
REM echo WAITING FOR PDB > lock.tmp
REM cl %CommonCompilerFlags% ..\code\tron.cpp -Fmhtron.map -LD /link -incremental:no -opt:ref -PDB:tron_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender
REM del lock.tmp
cl %CommonCompilerFlags% ..\code\sdl_tron.cpp -Fmsdl_tron.map /link %CommonLinkerFlags% 
popd
