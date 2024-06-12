# Shaders

Unlike earlier APIs, shader code in Vulkan has to be specified in a bytecode format, as opposed to human-readable syntax like GLSL and HLSL.

This bytecode format is called SPIR-V.

Khronos has released a vender-independent compiler that compiles GLSL to SPIR-V.

GLSL is a shading language with a C-style syntax. Programs written in it have a main function that is invoked for every object.
Instead of using parameters for input and a return value as output, GLSL uses global variables to handle input and output.