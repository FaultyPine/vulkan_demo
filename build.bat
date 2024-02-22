@echo off

python build.py %*

set SHADERS_OUT=src/shaders/built
glslc src/shaders/main.vert -o %SHADERS_OUT%/vert.spv
glslc src/shaders/main.frag -o %SHADERS_OUT%/frag.spv
echo Built shaders!