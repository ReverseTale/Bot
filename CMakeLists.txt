# ---[ Create project
project(ReverseTale-Bot)

# ---[ Parse all prefixes
include(cmake-common/Utils.cmake)
String2List(${CMAKE_PREFIX_PATH} CMAKE_PREFIX_PATH)
String2List(${CMAKE_MODULE_PATH} CMAKE_MODULE_PATH)

# ---[ Find all source and header files
file(GLOB_RECURSE sources_Bot       src/*.cpp     src/*.h)
set(Bot_SOURCES
    ${sources_Bot}
)

# ---[ Load dependencies
if (${ReverseTale_DEV})
    include_directories(${CMAKE_SOURCE_DIR}/src/xHacking/include)
else()
    find_package(xHacking   REQUIRED)
endif()

# ---[ Add definitions and DLL
if (NOT ${DEBUG})
    add_definitions(-DNDEBUG)
endif()
add_library(ReverseTale-Bot SHARED ${Bot_SOURCES})

# ---[ Link dependencies libraries
target_link_libraries(ReverseTale-Bot ws2_32 xHacking ReverseTale-Library)

# ---[ Set C++11 standard
set_property(TARGET ReverseTale-Bot PROPERTY CXX_STANDARD 11)
target_compile_features(ReverseTale-Bot PRIVATE cxx_decltype cxx_nullptr cxx_variadic_templates)

# ---[ Setup output directory and include directories
set_target_properties(ReverseTale-Bot PROPERTIES
    OUTPUT_NAME Bot
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}/bin)

include(CMakePackageConfigHelpers)
set(config_install_dir lib/cmake/ReverseTale-Bot)
set(version_config ${PROJECT_BINARY_DIR}/ReverseTale-Bot-config-version.cmake)
set(project_config ${PROJECT_BINARY_DIR}/ReverseTale-Bot-config.cmake)
set(targets_export_name ReverseTale-Bot-targets)

# Generate the version, config and target files into the build directory.
write_basic_package_version_file(
  ${version_config}
  VERSION 0.0.1
  COMPATIBILITY AnyNewerVersion)
configure_package_config_file(
  ${PROJECT_SOURCE_DIR}/config.cmake.in
  ${project_config}
  INSTALL_DESTINATION ${config_install_dir})
export(TARGETS ReverseTale-Bot FILE ${PROJECT_BINARY_DIR}/${targets_export_name}.cmake)

install(TARGETS ReverseTale-Bot
	EXPORT ${targets_export_name}
	RUNTIME DESTINATION bin/
  	LIBRARY DESTINATION lib/
  	ARCHIVE DESTINATION lib/)

install(FILES ${project_config} ${version_config} DESTINATION ${config_install_dir})
install(EXPORT ${targets_export_name} DESTINATION ${config_install_dir})
