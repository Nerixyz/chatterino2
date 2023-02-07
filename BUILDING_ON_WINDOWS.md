# Building on Windows

This guide shows how to compile Chatterino from the command line.
For a detailed walkthrough and instructions for specific editors [consult the wiki](https://wiki.chatterino.com/).

## Requirements

You need to have [git](https://git-scm.com/), a recent (14.20 - Visual Studio 2019 and later) version of MSVC, and a Windows SDK (both through [Visual Studio](https://visualstudio.microsoft.com/downloads/)) for your system installed. Additionally, you need to have [CMake](https://cmake.org/) installed - this is done by default when you're installing Visual Studio with "Desktop development with C++" enabled.

## Dependencies

### Qt

If you're using vcpkg as your package manager, you don't need to have Qt installed, since it will build Qt from source.

Your Qt installation must be version `5.15.2` or a newer Qt5 version with `MSVC 2019 64-bit`.
You can download Qt on the [Qt Open Source Page](https://www.qt.io/download-open-source).

Qt can be installed anywhere, but make sure CMake's `find_package` can find your installation.
To do this, set `CMAKE_PREFIX_PATH` either as an environment variable or when configuring your build with CMake
to your installation location (for example `C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5`).
You can also set `Qt5_DIR` like described in the [Qt docs](https://doc.qt.io/qt-5/cmake-get-started.html).

### Boost and OpenSSL

Chatterino depends on [Boost](https://www.boost.org) and [OpenSSL](https://www.openssl.org).

There are several ways of getting these dependencies:

- [Using conan](#using-conan)
- [Using vcpkg](#using-vcpkg)
- [Without a package manager](#without-a-package-manager)

#### Using conan

Install conan through pip (`pip install conan`) or through the installer from [conan.io](https://conan.io/downloads.html) and make sure it's on your PATH.
Create a new folder `build` in the cloned repository.

If you haven't set up your profile yet, run `conan profile new --detect --force default`.
To avoid CMake conflicts, set the default generator: `conan profile update conf.tools.cmake.cmaketoolchain:generator=Ninja default`.

Create a `build` folder and inside it, run `conan install .. -b missing -s build_type=Release` (`build_type` is any [CMake build type](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html)).

When configuring your build with CMake, you need to set the `CMAKE_TOOLCHAIN_FILE` to `conan_toolchain.cmake`.

**Useful links:** [Conan documentation](https://docs.conan.io), [`CMakeToolchain` workflow](https://docs.conan.io/en/latest/reference/conanfile/tools/cmake/cmaketoolchain.html#using-the-toolchain-in-developer-flow), [profile reference](https://docs.conan.io/en/latest/reference/profiles.html), [`conan.conf` reference](https://docs.conan.io/en/latest/reference/config_files/conan.conf.html).

#### Using vcpkg

Install [vcpkg](https://vcpkg.io/). Set the `VCPKG_DEFAULT_TRIPLET` to `x64-windows`.

Inside your cloned repository, run `vcpkg install`.

When configuring your build with CMake, you need to set the `CMAKE_TOOLCHAIN_FILE` to `$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake` (`$VCPKG_ROOT` is the path to your vcpkg installation).

**Useful links:** [Installing vcpkg](https://vcpkg.io/en/getting-started.html), [vcpkg documentation](https://vcpkg.io/en/docs/README.html), [CMake integration](https://vcpkg.io/en/docs/users/buildsystems/cmake-integration.html), [vcpkg configuration/environment](https://vcpkg.io/en/docs/users/config-environment.html).

#### Without a package manager

##### Boost

Install boost from [SourceForge](https://sourceforge.net/projects/boost/files/boost-binaries/).
There, select your version of MSVC (`14.3` corresponds to MSVC 2022, `14.2` to 2019).
**Convenience link for Visual Studio 2022: [boost_1_81_0-msvc-14.3-64.exe](https://sourceforge.net/projects/boost/files/boost-binaries/1.81.0/boost_1_81_0-msvc-14.3-64.exe/download)**

When prompted where to install Boost, leave the location as is (`C:\local\boost_<version>`).
After the installation finishes, rename the `C:\local\boost_<version>\lib64-msvc-14.3` (or similar) directory to `lib` (`C:\local\boost_<version>\lib`).

**Useful links:** [`FindBoost` documentation](https://cmake.org/cmake/help/latest/module/FindBoost.html) (how CMake searches for your Boost installation).

##### OpenSSL

Install OpenSSL from [slproweb.com](https://slproweb.com/download/Win64OpenSSL-1_1_1s.exe).
OpenSSL should be installed to `C:\local\openssl`. When prompted, copy the OpenSSL DLLs to "The OpenSSL binaries (/bin) directory".

**Useful links:** [`FindOpenSSL` documentation](https://cmake.org/cmake/help/latest/module/FindOpenSSL.html) (how CMake searches for your OpenSSL installation).

## Building

Chatterino uses CMake as its build system. For CMake to detect your compiler, you need to set the Visual Studio environment variables in your shell.
In `cmd`, run `vcvars64.bat` from `<visual-studio-path>\VC\Auxiliary\Build\vcvars64.bat` (default location: `C:\Program Files\Microsoft Visual Studio\2022\VC\Auxiliary\Build\vcvars64.bat`).
In PowerShell, use the script provided [here](https://gist.github.com/Nerixyz/4b75418a3fc9be504a281dcd71875c0c).

If you haven't done already, create a folder called `build` inside the cloned repository and `cd` inside it.

From there, configure the build with `cmake ..`. Here you need to set any CMake flags using `-D<name>=<value>` (e.g. `cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake`).

Now you can use `cmake --build .` to build the project. The final executable will be stored inside `bin/`.

If you're not using vcpkg, you need to deploy the Qt libraries with `windeployqt bin/chatterino.exe --release --no-compiler-runtime --no-translations --no-opengl-sw --dir bin/`. `windeployqt` is from your Qt installation (by default `C:\Qt\5.15.2\msvc2019_64\bin`).

## Installing

To install Chatterino in your system or any folder, configure your build with `CMAKE_INSTALL_PREFIX` set to your desired folder.
Then run `cmake --install .`. If you're using vcpkg, you need to additionally set `X_VCPKG_APPLOCAL_DEPS_INSTALL` to `On`.
