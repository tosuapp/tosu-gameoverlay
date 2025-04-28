# tosu-gameoverlay repository 

# Setup

First install some necessary tools and download the tosu-gameoverlay based project.

1\. Install [Python](https://www.python.org/downloads/). Version 3.9 to 3.11 is required.

2\. Install platform-specific build tools.

* Linux: Currently supported distributions include Debian 10 (Buster), Ubuntu 18 (Bionic Beaver), and related, with minimum GCC version 7.5.0. Ubuntu 22.04 64-bit with GCC 11+ is recommended. Newer versions will likely also work but may not have been tested. Required packages include: build-essential, libgtk-3-dev.
* MacOS: Xcode 12.2 to 15.0 building on MacOS 10.15.4 (Catalina) or newer. The Xcode command-line tools must also be installed.
* Windows: Visual Studio 2022 building on Windows 10 or newer. Windows 10/11 64-bit is recommended.

## Using CMake

[CMake](https://cmake.org/) can be used to generate project files in many different formats.

To build the tosu-gameoverlay example applications using CMake:

1\. Install [CMake](https://cmake.org/download/). Version 3.21 or newer is required.

2\. Set the `PYTHON_EXECUTABLE` environment variable if required (watch for errors during the CMake generation step below).

3\. Run CMake to download the CEF binary distribution from the [Spotify automated builder](https://cef-builds.spotifycdn.com/index.html) and generate build files for your platform. 

4\. Build using platform build tools. For example, using the most recent tool versions on each platform:

```
cd /path/to/tosu-gameoverlay

# Create and enter the build directory.
mkdir build
cd build

# Run specific commands for:

# X86
# For building main CEF backend (including windows, engine, etc)
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDESKTOP=1 -DA64=0 -B build

# For building main CEF injection dll (includes CEF main back)
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDESKTOP=1 -DA64=0 -B build

# X64
# For building main CEF backend (including windows, engine, etc)
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDESKTOP=1 -DA64=1 -B build

# For building main CEF injection dll (includes CEF main back)
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDESKTOP=1 -DA64=1 -B build
```

CMake supports different generators on each platform. Run `cmake --help` to list all supported generators. !!We're using Ninja as our primary generator!!

Ninja is a cross-platform open-source tool for running fast builds using pre-installed platform toolchains (GNU, clang, Xcode or MSVC). See comments in the "third_party/cef/cef_binary_*/CMakeLists.txt" file for Ninja usage instructions.

---
do whatever you want with this shit...