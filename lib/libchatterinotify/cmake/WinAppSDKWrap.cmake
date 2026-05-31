if(NOT WIN32)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    NuGetCMakePackage
    GIT_REPOSITORY https://github.com/mschofie/NuGetCMakePackage
    GIT_TAG develop
)

FetchContent_MakeAvailable(NuGetCMakePackage)

add_nuget_packages(
    CONFIG_FILE ${CMAKE_CURRENT_LIST_DIR}/nuget/nuget.config
    LOCK_FILE ${CMAKE_CURRENT_LIST_DIR}/nuget/packages.lock.json
    FRAMEWORK native
    PACKAGES
        Microsoft.Windows.CppWinRT 2.0.250303.1
        Microsoft.WindowsAppSDK.Base 2.0.5-experimental2
        Microsoft.WindowsAppSDK.Foundation 2.0.19-experimental
)

find_package(Microsoft.WindowsAppSDK.Base CONFIG REQUIRED)
find_package(Microsoft.WindowsAppSDK.Foundation CONFIG REQUIRED)

add_library(WinAppSDKWrap INTERFACE)
target_link_libraries(WinAppSDKWrap
    INTERFACE
        Microsoft.WindowsAppSDK.Foundation_SelfContained
)

macro(setup_winappsdk_target target_name)
    set_target_properties(${target_name} PROPERTIES
        # MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        WindowsPackageType "None"
        # Bootstrap is on by default for unpackaged apps — setting explicitly as an example
        WindowsAppSdkBootstrapInitialize TRUE
    )
    get_target_property(_tt ${target_name} TYPE)
    if (_tt STREQUAL "MODULE_LIBRARY" OR _tt STREQUAL "SHARED_LIBRARY" OR _tt STREQUAL "EXECUTABLE")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "${CMAKE_COMMAND};-E;$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:${target_name}>>,copy;$<TARGET_RUNTIME_DLLS:${target_name}>;$<TARGET_FILE_DIR:${target_name}>,true>"
            COMMAND_EXPAND_LISTS
        )
    endif()
endmacro()
