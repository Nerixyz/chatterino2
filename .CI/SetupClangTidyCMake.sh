#!/bin/sh

set -ev;

# pip3 install -U pip;
# pip3 install aqtinstall;
# aqt install-qt linux desktop $1 gcc_64 -m qtimageformats qt5compat --outputdir .qtinstall;

# # aqt installs into .qtinstall/<version>/gcc_64
# # This is doing the same as jurplel/install-qt-action
# # See https://github.com/jurplel/install-qt-action/blob/74ca8cd6681420fc8894aed264644c7a76d7c8cb/action/src/main.ts#L52-L74
# qtpath=${$(echo .qtinstall/[0-9]*/*/bin/qmake*)%/bin/qmake*};
# export LD_LIBRARY_PATH="$qtpath/lib";
# export QT_ROOT_DIR=$qtpath;
# export QT_PLUGIN_PATH="$qtpath/plugins";

cmake -S. -Bbuild-clang-tidy \
    -DCMAKE_BUILD_TYPE=Debug \
    -DPAJLADA_SETTINGS_USE_BOOST_FILESYSTEM=On \
    -DUSE_PRECOMPILED_HEADERS=OFF \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=On \
    -DCHATTERINO_LTO=Off \
    -DCHATTERINO_PLUGINS=On \
    -DBUILD_WITH_QT6=On \
    -DBUILD_TESTS=On \
    -DBUILD_BENCHMARKS=On;

# Run MOC and UIC
# This will compile the version project (1 file)
# Get the targets using `ninja -t targets | grep autogen`
cmake --build build-clang-tidy -t \
    Core_autogen \
    LibCommuni_autogen \
    Model_autogen \
    Util_autogen \
    chatterino-lib_autogen;
