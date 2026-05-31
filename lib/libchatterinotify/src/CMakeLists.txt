# include(WinAppSDKWrap)

set(SOURCE_FILES
    Log.cpp
)

if(WIN32)
    list(APPEND SOURCE_FILES
        windows/Manager.cpp
        windows/Notification.cpp
        windows/NotificationEvent.cpp
        windows/WinRTHelpers.cpp
    )
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt${MAJOR_QT_VERSION}::Core
        # WinAppSDKWrap
)

# set_target_properties(${PROJECT_NAME} PROPERTIES
#     # MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
# )

target_compile_definitions(${PROJECT_NAME} PUBLIC
    QT_NO_KEYWORDS
)

# post_build_runtime_dll_copy(${PROJECT_NAME})
# post_build_selfcontained_dll_copy(${PROJECT_NAME})

if (MSVC)
    # Add bigobj
    target_compile_options(${PROJECT_NAME} PRIVATE /EHsc /bigobj)

    # Change flags for RelWithDebInfo

    # Default: "/debug /INCREMENTAL"
    # Changes:
    #   - Disable incremental linking to reduce padding
    #   - Enable all optimizations - by default when /DEBUG is specified,
    #     these optimizations will be disabled. We need /DEBUG to generate a PDB.
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/20812 for more details.
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/DEBUG /INCREMENTAL:NO /OPT:REF,ICF,LBR")

    # Use the function inlining level from 'Release' mode (2).
    string(REPLACE "/Ob1" "/Ob2" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

    # Configure warnings
    target_compile_options(${PROJECT_NAME} PUBLIC /W4)
    # Disable min/max macros from Windows.h
    target_compile_definitions(${PROJECT_NAME} PUBLIC NOMINMAX)
else ()
    # TODO
endif ()

