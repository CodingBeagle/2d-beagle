# cmake_minimum_required should be the first command in your CMakeLists file.
# This sets the minimum required version of cmake for this project.
cmake_minimum_required(VERSION 3.29)

# Set the name for the project.
# Also specifies that the supported language is C++
project(2dbeagle LANGUAGES CXX)

# Add executable target
# A target corresponds to an executable or a library.
# I specify WIN32 in order to create a WIN32 executable, which means that the application entry point becomes WinMain, instead of main.
# This is essentially the SUBSYSTEM linker option of MSVC.
add_executable(2dbeagle WIN32 src/main.cpp)