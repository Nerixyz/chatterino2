if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # See https://gcc.gnu.org/wiki/LibstdcxxDebugMode
    # FIXME: use -fhardened once we can require GCC 14+
    add_compile_definitions(_GLIBCXX_ASSERTIONS=1)
elseif(WIN32)
    # See https://github.com/microsoft/STL/wiki/STL-Hardening
    add_compile_definitions(_MSVC_STL_HARDENING=1)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # See https://libcxx.llvm.org/Hardening.html
    if(CHATTERINO_DEBUG_HARDENING)
        set(_hardening_mode _LIBCPP_HARDENING_MODE_DEBUG)
    else()
        set(_hardening_mode _LIBCPP_HARDENING_MODE_FAST)
    endif()
    add_compile_definitions("LIBCPP_HARDENING_MODE=${_hardening_mode}")
else()
    message(FATAL_ERROR "Hardened mode requested, but compiler (${CMAKE_CXX_COMPILER_ID}) is not supported")
endif()
