C:\VulkanSDK\1.3.283.0\Bin\glslc.exe shader.vert -o vert.spv
C:\VulkanSDK\1.3.283.0\Bin\glslc.exe shader.frag -o frag.spv

IF NOT EXIST C:\projects\2d-beagle\build\Debug\Shaders (
    mkdir C:\projects\2d-beagle\build\Debug\Shaders
)

copy vert.spv C:\projects\2d-beagle\build\Debug\Shaders
copy frag.spv C:\projects\2d-beagle\build\Debug\Shaders