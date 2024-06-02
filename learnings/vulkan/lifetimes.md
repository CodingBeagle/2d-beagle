# Lifetimes

Objects in Vulkan are created or allocated by *vkCreate** and *vkAllocate** commands.

Objects are destroyed or freed by *vkDestroy** and *vkFree** commands.

Objects that are allocated (rather than created) take resources from an existing pool object or memory heap. When freed, they return resources to that pool or heap.

**It is an application's responsibility to track the lifetime of Vulkan objects, and not to destroy them while they are still in use**.