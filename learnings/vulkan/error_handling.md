# Error Handling

The Vulkan API is designed for minimal driver overhead.

One consequence of this is that there is very limited error checking in the API by default.

Mistakes such as setting enumerations to incorrect values or passing null pointers to required parameters are not explicitly handled and will result in crashes or undefined behaviour.

## Validation Layers

One way to add debugging checks and error handling is by using *validation layers*.

These are optional components that hook into Vulkan function calls to apply additional operations. Such as:

- Checking the values of parameters against the specification to detect misuse.
- Tracking creation and destruction of objects to find resource leaks.
- Checking thread safety.
- Tracing Vulkan calls for profiling and replaying.

The LunarG SDK comes with a set of layers that check for common errors.