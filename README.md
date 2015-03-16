Various synchronization primitives for multithreaded applications in C++11.

Used in the blog post, "Semaphores are Surprisingly Versatile".

Code is released under the [zlib license](http://en.wikipedia.org/wiki/Zlib_License). See the `LICENSE` file.

## How to Build the Tests

First, you must generate the projects using [CMake](http://www.cmake.org/). Open a command prompt in the `tests` folder and do the following.

    mkdir build
    cd build
    cmake .. 

`cmake` takes an optional `-G` argument to specify which project generator to use. For example, the following command will use the Visual Studio 2012 generator. A complete list of available generators can be found by running `cmake` with no arguments.

    cmake -G "Visual Studio 11" ..

On a Unix-like OS, to generate a makefile that builds the release configuration:

    cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ..

To generate projects for iOS devices, use the following.

    cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/iOS.cmake -G "Xcode" ..

To build the project, simply use the generated project files as you would normally. On some platforms, you can use CMake to perform the build step, too. For example, on Windows, you can use the command:

    cmake --build . --config Release
