@echo off
setlocal enabledelayedexpansion

if not exist build mkdir build

set "INCLUDES=-I./include -I%PROGRAMFILES%/SDL2/include -I%PROGRAMFILES%/SDL2_image/include -I%PROGRAMFILES%/SDL2_ttf/include"
set "LDFLAGS=-L%PROGRAMFILES%/SDL2/lib -L%PROGRAMFILES%/SDL2_image/lib -L%PROGRAMFILES%/SDL2_ttf/lib"
set "LIBS=-lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf"

set "OBJECTS="
for %%f in (src\*.cpp) do (
    set "obj=build\%%~nf.o"
    set "OBJECTS=!OBJECTS! !obj!"
    
    echo Compiling %%f -^> !obj!
    g++ -std=c++20 %INCLUDES% -c %%f -o !obj!
)

g++ %OBJECTS% %LDFLAGS% %LIBS% -o build\engine.exe 