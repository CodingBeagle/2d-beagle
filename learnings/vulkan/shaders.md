# Shaders

Unlike earlier APIs, shader code in Vulkan has to be specified in a bytecode format, as opposed to human-readable syntax like GLSL and HLSL.

This bytecode format is called SPIR-V.

Khronos has released a vender-independent compiler that compiles GLSL to SPIR-V.

GLSL is a shading language with a C-style syntax. Programs written in it have a main function that is invoked for every object.
Instead of using parameters for input and a return value as output, GLSL uses global variables to handle input and output.

## Vertex Shader

The vertex shader processes each incoming vertex.

It takes its attributes, like world position, color, normal, and texture coordinates as input.

The output is the final position in clip coordinates and the attributes that need to be passed on to the fragment shader, like color and texture coordiantes.

These values will then be interpolated over the fragments by the rasterizer to produce a smooth gradient.

**Clip Coordinate** refers to a four dimensional vector from the vertex shader that is subsequently turned into a *normalized device coordinate* by dividing the whole vector by its last component.

These normalized device coordinates are homogeneous coordinates that map the framebuffer to a [-1, 1] by [-1, 1] coordinate system.

## Fragment Shader

The fragment shader is invoked for every fragment, like the vertex shader is invoked for every vertex.

A fragment shader produces color and depth for these fragments for the framebuffer.