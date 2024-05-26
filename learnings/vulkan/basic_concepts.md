# Basic Concepts

## Usual Patterns

A lot of information in Vulkan is passed through structs instead of function parameters.

## Window System Integration (WSI) Platform

A platform is an abstraction for a window system, OS... etc.

The Vulkan API does not define any type of platform object. Platform-specific WSI extensions are defined, each containing platform-specific functions for using WSI.

In order for an application to be compiled to use WSI with a given platform, it must either:

- #define the appropriate preprocessor symbol prior to including the vulkna.h header file or,
- include vulkan_core.h and any native platform headers, followed by the appropriate platform-specific header.