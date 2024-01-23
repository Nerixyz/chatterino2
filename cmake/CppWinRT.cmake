include(ExternalProject)

ExternalProject_Add(
  cppwinrtBuild
  URL "https://github.com/microsoft/cppwinrt/releases/download/2.0.240111.5/Microsoft.Windows.CppWinRT.2.0.240111.5.nupkg"
  URL_HASH "SHA256=e0e9076b0c8d5a85212015a8abcfa7c92fda28ef5fd9a25be4de221fce65446d"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND "<SOURCE_DIR>/bin/cppwinrt.exe" -in local -output "<BINARY_DIR>/include"
  INSTALL_COMMAND ""
  DOWNLOAD_EXTRACT_TIMESTAMP On

  EXCLUDE_FROM_ALL
)

add_library(cppwinrt INTERFACE)
add_dependencies(cppwinrt cppwinrtBuild)
target_link_libraries(cppwinrt INTERFACE WindowsApp.lib)

ExternalProject_Get_property(cppwinrtBuild BINARY_DIR)
target_include_directories(cppwinrt INTERFACE "${BINARY_DIR}/include")

ExternalProject_Get_property(cppwinrtBuild SOURCE_DIR)
