# Graphics Pipeline

A graphics pipeline is a sequence of operations that take the vertices and textures of your meshes all the way to the pixels in the render targets.

The graphics pipeline in Vulkan is almost completely immutable, so you must recreate the pipeline from scratch if you want to change shaders, bind different framebuffers or change the blend function.

The disadvantage is that you'll have to create a number of pipelines that represent all of the different combinations of states you want to use in your rendering operations. However, because all the operations you'll be doing in the pipeline are known in advance, the driver can optimize for it much better.