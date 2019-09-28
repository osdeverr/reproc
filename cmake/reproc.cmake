### Installation options ###

get_directory_property(REPROC_IS_SUBDIRECTORY PARENT_DIRECTORY)

# Don't add libraries to the install target by default if the project is built
# from within another project as a static library.
if(REPROC_IS_SUBDIRECTORY AND NOT BUILD_SHARED_LIBS)
  option(REPROC_INSTALL "Generate installation rules." OFF)
else()
  option(REPROC_INSTALL "Generate installation rules." ON)
endif()

mark_as_advanced(REPROC_INSTALL)

include(GNUInstallDirs)

option(REPROC_INSTALL_PKGCONFIG "Install pkg-config files." ON)

set(REPROC_INSTALL_CMAKECONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake
    CACHE STRING "CMake config files installation directory.")
set(REPROC_INSTALL_PKGCONFIGDIR ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    CACHE STRING "pkg-config files installation directory.")

mark_as_advanced(
  REPROC_INSTALL
  REPROC_INSTALL_PKGCONFIG
  REPROC_INSTALL_CMAKECONFIGDIR
  REPROC_INSTALL_PKGCONFIGDIR
)

### Developer options ###

option(REPROC_TIDY "Run clang-tidy when building.")
option(REPROC_SANITIZERS "Build with sanitizers.")
option(REPROC_WARNINGS_AS_ERRORS "Add -Werror or equivalent to the compile flags and clang-tidy.")

mark_as_advanced(
  REPROC_TIDY
  REPROC_SANITIZERS
  REPROC_WARNINGS_AS_ERRORS
)

### clang-tidy ###

if(REPROC_TIDY)
  find_program(REPROC_TIDY_PROGRAM clang-tidy)
  mark_as_advanced(REPROC_TIDY_PROGRAM)

  if(REPROC_TIDY_PROGRAM)
    if(REPROC_WARNINGS_AS_ERRORS)
      set(REPROC_TIDY_PROGRAM ${REPROC_TIDY_PROGRAM} -warnings-as-errors=*)
    endif()
  else()
    message(FATAL_ERROR "clang-tidy not found")
  endif()
endif()

### Global Setup ###

get_property(REPROC_ENABLED_LANGUAGES GLOBAL PROPERTY ENABLED_LANGUAGES)

foreach(LANGUAGE IN ITEMS C CXX)
  if(NOT LANGUAGE IN_LIST REPROC_ENABLED_LANGUAGES)
    continue()
  endif()

  if(MSVC)
    if(LANGUAGE STREQUAL "C")
      include(CheckCCompilerFlag)
      check_c_compiler_flag(/permissive- REPROC_${LANGUAGE}_HAVE_PERMISSIVE)
    else()
      include(CheckCXXCompilerFlag)
      check_cxx_compiler_flag(/permissive- REPROC_${LANGUAGE}_HAVE_PERMISSIVE)
    endif()
  endif()

  if(REPROC_SANITIZERS)
    if(MSVC)
      message(FATAL_ERROR "Building with sanitizers is not supported when using the Visual C++ toolchain.")
    endif()

    if(NOT ${CMAKE_${LANGUAGE}_COMPILER_ID} MATCHES GNU|Clang)
      message(FATAL_ERROR "Building with sanitizers is not supported when using the ${CMAKE_${LANGUAGE}_COMPILER_ID} compiler.")
    endif()
  endif()
endforeach()

### Includes ###

include(GenerateExportHeader)
include(CMakePackageConfigHelpers)

### Functions ###

function(reproc_add_common TARGET LANGUAGE STANDARD OUTPUT_DIRECTORY)
  if(LANGUAGE STREQUAL "C")
    target_compile_features(${TARGET} PUBLIC c_std_${STANDARD})
  else()
    target_compile_features(${TARGET} PUBLIC cxx_std_${STANDARD})
  endif()

  set_target_properties(${TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}"
  )

  if(REPROC_TIDY AND REPROC_TIDY_PROGRAM)
    set_target_properties(${TARGET} PROPERTIES
      # `REPROC_TIDY_PROGRAM` is a list so we surround it with quotes to pass it
      # as a single argument.
      ${LANGUAGE}_CLANG_TIDY "${REPROC_TIDY_PROGRAM}"
    )
  endif()

  ### Common development flags (warnings + sanitizers + colors) ###

  if(MSVC)
    target_compile_options(${TARGET} PRIVATE
      /nologo # Silence MSVC compiler version output.
      $<$<BOOL:${REPROC_WARNINGS_AS_ERRORS}>:/WX> # -Werror
      $<$<BOOL:${REPROC_${LANGUAGE}_HAVE_PERMISSIVE}>:/permissive->
    )

    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.15.0")
      # CMake 3.15 does not add /W3 to the compiler flags by default anymore
      # so we add /W4 instead.
      target_compile_options(${TARGET} PRIVATE /W4)
    endif()

    if(NOT STANDARD STREQUAL "90")
      # MSVC reports non-constant initializers as a nonstandard extension but
      # they've been standardized in C99 so we disable it if we're targeting at
      # least C99.
      target_compile_options(${TARGET} PRIVATE /wd4204)
    endif()

    target_link_options(${TARGET} PRIVATE
      /nologo # Silence MSVC linker version output.
    )
  else()
    target_compile_options(${TARGET} PRIVATE
      -Wall
      -Wextra
      -pedantic
      -Wconversion
      -Wsign-conversion
      $<$<BOOL:${REPROC_WARNINGS_AS_ERRORS}>:-Werror>
      $<$<BOOL:${REPROC_WARNINGS_AS_ERRORS}>:-pedantic-errors>
    )

    if(LANGUAGE STREQUAL "C" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      target_compile_options(${TARGET} PRIVATE -Wmissing-prototypes)
    endif()
  endif()

  if(REPROC_SANITIZERS)
    set_property(
      TARGET ${TARGET}
      PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded
    )
    target_compile_options(
      ${TARGET}
      PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(${TARGET} PRIVATE -fsanitize=address,undefined)
  endif()

  target_compile_options(${TARGET} PRIVATE
    $<$<${LANGUAGE}_COMPILER_ID:GNU>:-fdiagnostics-color>
    $<$<${LANGUAGE}_COMPILER_ID:Clang>:-fcolor-diagnostics>
  )
endfunction()

function(reproc_add_library TARGET LANGUAGE STANDARD)
  add_library(${TARGET})

  reproc_add_common(${TARGET} ${LANGUAGE} ${STANDARD} lib)
  # Enable -fvisibility=hidden and -fvisibility-inlines-hidden (if applicable).
  set_target_properties(${TARGET} PROPERTIES
    ${LANGUAGE}_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN true
  )

  # clang-tidy errors with error: unknown argument: '-fno-keep-inline-dllexport'
  # when enabling `VISIBILITY_INLINES_HIDDEN` on MinGW so we disable it when
  # running clang-tidy on MinGW.
  if(MINGW AND REPROC_TIDY)
    set_target_properties(${TARGET} PROPERTIES
      VISIBILITY_INLINES_HIDDEN false
    )
  endif()

  # A preprocesor macro cannot contain + so we replace it with x.
  string(REPLACE + x EXPORT_MACRO ${TARGET})
  string(TOUPPER ${EXPORT_MACRO} EXPORT_MACRO_UPPER)

  if(LANGUAGE STREQUAL "C")
    set(HEADER_EXT h)
  else()
    set(HEADER_EXT hpp)
  endif()

  # Generate export headers. We generate export headers using CMake since
  # different export files are required depending on whether a library is shared
  # or static and we can't determine whether a library is shared or static from
  # the export header without requiring the user to add a #define which we want
  # to avoid.
  generate_export_header(${TARGET}
    BASE_NAME ${EXPORT_MACRO_UPPER}
    EXPORT_FILE_NAME
      ${CMAKE_CURRENT_BINARY_DIR}/include/${TARGET}/export.${HEADER_EXT}
  )

  # Make sure we follow the popular naming convention for shared libraries on
  # UNIX systems.
  set_target_properties(${TARGET} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
  )

  # Only use the headers from the repository when building. When installing we
  # want to use the install location of the headers (e.g. /usr/include) as the
  # include directory instead.
  target_include_directories(${TARGET} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  )

  # Adapted from https://codingnest.com/basic-cmake-part-2/.
  # Each library is installed separately (with separate config files).

  if(REPROC_INSTALL)

    ## Config files

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}-config.cmake.in)

      # CMake

      ## Headers

      install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/${TARGET}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.${HEADER_EXT}"
      )

      install(
        DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/${TARGET}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      )

      target_include_directories(${TARGET} PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
      )

      ## Libraries

      install(
        TARGETS ${TARGET}
        EXPORT ${TARGET}-targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      )

      install(
        EXPORT ${TARGET}-targets
        FILE ${TARGET}-targets.cmake
        DESTINATION ${REPROC_INSTALL_CMAKECONFIGDIR}/${TARGET}
      )

      write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
      )

      configure_package_config_file(
          ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}-config.cmake.in
          ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config.cmake
        INSTALL_DESTINATION
          ${REPROC_INSTALL_CMAKECONFIGDIR}/${TARGET}
      )

      install(
        FILES
          ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config-version.cmake
        DESTINATION
          ${REPROC_INSTALL_CMAKECONFIGDIR}/${TARGET}
      )
    endif()

    # pkg-config

    if(REPROC_INSTALL_PKGCONFIG AND
       EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}.pc.in)
      configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc
        @ONLY
      )

      install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc
        DESTINATION ${REPROC_INSTALL_PKGCONFIGDIR}
      )
    endif()
  endif()
endfunction()

function(reproc_add_example TARGET LANGUAGE STANDARD)
  add_executable(reproc-${TARGET})

  if(LANGUAGE STREQUAL "C")
    set(SOURCE_EXT c)
  else()
    set(SOURCE_EXT cpp)
  endif()

  reproc_add_common(reproc-${TARGET} ${LANGUAGE} ${STANDARD} examples)
  target_sources(reproc-${TARGET} PRIVATE examples/${TARGET}.${SOURCE_EXT})
  target_link_libraries(reproc-${TARGET} PRIVATE ${ARGN})
  set_target_properties(reproc-${TARGET} PROPERTIES OUTPUT_NAME ${TARGET})

  if(LANGUAGE STREQUAL "C" AND REPROC_SANITIZERS)
    set_target_properties(reproc-${TARGET} PROPERTIES
      # Hack to avoid UBSan undefined reference errors.
      LINKER_LANGUAGE CXX
    )
  endif()
endfunction()
