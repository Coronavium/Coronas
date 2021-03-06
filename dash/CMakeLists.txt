project(project_dash_libdash CXX)

# Load global build settings
set(DASH_DART_IF_INCLUDE_DIR ${DASH_DART_IF_INCLUDE_DIR}
    PARENT_SCOPE)
set(DART_IMPLEMENTATIONS_LIST ${DART_IMPLEMENTATIONS_LIST}
    PARENT_SCOPE)
set(BUILD_EXAMPLES ${BUILD_EXAMPLES}
    PARENT_SCOPE)
set(BUILD_TESTS ${BUILD_TESTS}
    PARENT_SCOPE)
set(ENABLE_LOGGING ${ENABLE_LOGGING}
    PARENT_SCOPE)
set(ENABLE_TRACE_LOGGING ${ENABLE_TRACE_LOGGING}
    PARENT_SCOPE)
set(ENABLE_UNIFIED_MEMORY_MODEL ${ENABLE_UNIFIED_MEMORY_MODEL}
    PARENT_SCOPE)
set(ENABLE_DEFAULT_INDEX_TYPE_LONG ${ENABLE_DEFAULT_INDEX_TYPE_LONG}
    PARENT_SCOPE)
set(ENABLE_SHARED_WINDOWS ${ENABLE_SHARED_WINDOWS}
    PARENT_SCOPE)
set(ENABLE_SCALAPACK ${ENABLE_SCALAPACK}
    PARENT_SCOPE)
set(ENABLE_LIBNUMA ${ENABLE_LIBNUMA}
    PARENT_SCOPE)
set(ENABLE_HWLOC ${ENABLE_HWLOC}
    PARENT_SCOPE)
set(ENABLE_PAPI ${ENABLE_PAPI}
    PARENT_SCOPE)

# include(${CMAKE_SOURCE_DIR}/CMakeExt/MPI.cmake)
# include(${CMAKE_SOURCE_DIR}/CMakeExt/PAPI.cmake)
# include(${CMAKE_SOURCE_DIR}/CMakeExt/Hwloc.cmake)
# include(${CMAKE_SOURCE_DIR}/CMakeExt/NUMA.cmake)
# include(${CMAKE_SOURCE_DIR}/CMakeExt/MKL.cmake)
# include(${CMAKE_SOURCE_DIR}/CMakeExt/IPM.cmake)
# include(${CMAKE_SOURCE_DIR}/CMakeExt/PLASMA.cmake)

# Source- and header files to be compiled (OBJ):
file(GLOB_RECURSE DASH_LIBRARY_SOURCES
     "src/*.c" "src/*.h" "src/*.cc" "src/*.cpp" )
file(GLOB_RECURSE DASH_LIBRARY_HEADERS
     "include/*.h")
file(GLOB_RECURSE DASH_TEST_SOURCES
     "test/*.c" "test/*.h" "test/*.cc" "src/*.cpp")

# Directories containing the implementation of the library (-I):
set(DASH_LIBRARY_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
include_directories(
  ${DASH_LIBRARY_INCLUDE_DIRS}
  ${DASH_DART_IF_INCLUDE_DIR}
)

set (ADDITIONAL_COMPILE_FLAGS "-DDASH")
set (ADDITIONAL_INCLUDES      "")

if (ENABLE_DEFAULT_INDEX_TYPE_LONG)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_DEFAULT_INDEX_TYPE_LONG")
endif()
if (ENABLE_UNIFIED_MEMORY_MODEL)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_UNIFIED_MEMORY_MODEL")
endif()
if (ENABLE_LOGGING)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_LOGGING")
endif()
if (ENABLE_TRACE_LOGGING)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_TRACE_LOGGING")
endif()
if (PAPI_FOUND AND ENABLE_PAPI)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_PAPI")
  set (ADDITIONAL_INCLUDES ${ADDITIONAL_INCLUDES}
       ${PAPI_INCLUDE_DIRS})
  set (ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES}
       ${PAPI_LIBRARIES})
else()
  set (ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES}
       rt)
endif()
if (HWLOC_FOUND AND ENABLE_HWLOC)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_HWLOC")
  set (ADDITIONAL_INCLUDES ${ADDITIONAL_INCLUDES}
       ${HWLOC_INCLUDE_DIRS})
  set (ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES}
       ${HWLOC_LIBRARIES})
else()
endif()
if (NUMA_FOUND AND ENABLE_LIBNUMA)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_NUMA")
  set (ADDITIONAL_INCLUDES ${ADDITIONAL_INCLUDES}
       ${NUMA_INCLUDE_DIRS})
  set (ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES}
       ${NUMA_LIBRARIES})
else()
endif()

if (ENABLE_PLASMA AND PLASMA_FOUND)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_PLASMA")
  set (ADDITIONAL_INCLUDES ${ADDITIONAL_INCLUDES}
       ${PLASMA_INCLUDE_DIRS})
  set (ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES}
       ${PLASMA_LIBRARIES})
else()
endif()

if (MKL_FOUND)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_MKL")
  set (ADDITIONAL_INCLUDES ${ADDITIONAL_INCLUDES}
       ${MKL_INCLUDE_DIRS})
  set (ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES}
       ${MKL_LIBRARIES})
else()
endif()

if (ENABLE_SCALAPACK AND MKL_SCALAPACK_FOUND)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_SCALAPACK")
  set (ADDITIONAL_LIBRARIES ${ADDITIONAL_LIBRARIES}
       ${MKL_SCALAPACK_LIBRARIES})
else()
endif()

if (DASH_ENV_HOST_SYSTEM_ID)
  set (HOST_ID ${DASH_ENV_HOST_SYSTEM_ID})
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENV_HOST_SYSTEM_ID=\"${HOST_ID}\"")
endif()

## Build targets

# DASH library for every enabled DART variant:
#
foreach(dart_variant ${DART_IMPLEMENTATIONS_LIST})
  set (DART_LIBRARY "dart-${dart_variant}")
  set (DASH_LIBRARY "dash-${dart_variant}")
  set (VARIANT_ADDITIONAL_COMPILE_FLAGS ${ADDITIONAL_COMPILE_FLAGS})
  set (VARIANT_ADDITIONAL_COMPILE_FLAGS
       "${VARIANT_ADDITIONAL_COMPILE_FLAGS} -Wno-sign-compare")
  if (${dart_variant} STREQUAL "mpi")
    if (IPM_FOUND)
      set (VARIANT_ADDITIONAL_COMPILE_FLAGS
           "${VARIANT_ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_IPM")
    endif()
    if (NOT ENABLE_SHARED_WINDOWS)
      set (VARIANT_ADDITIONAL_COMPILE_FLAGS
           "${VARIANT_ADDITIONAL_COMPILE_FLAGS} -DDART_MPI_DISABLE_SHARED_WINDOWS")
    endif()
    set (VARIANT_ADDITIONAL_COMPILE_FLAGS
         "${VARIANT_ADDITIONAL_COMPILE_FLAGS} -DMPI_IMPL_ID=\"${MPI_IMPL_ID}\"")
  endif()
  message(STATUS "Building DASH library    " ${DASH_LIBRARY})

  # Library compilation sources
  add_library(
    ${DASH_LIBRARY}         # library name
    ${DASH_LIBRARY_SOURCES} # sources
    ${DASH_LIBRARY_HEADERS} # headers
  )
  target_link_libraries(
    ${DASH_LIBRARY}           # library name
    ${DASH_LINK_DART_LIBRARY} # dependency: DART lib
    ${ADDITIONAL_LIBRARIES}
  )
  set_target_properties(
    ${DASH_LIBRARY} PROPERTIES
    COMPILE_FLAGS ${VARIANT_ADDITIONAL_COMPILE_FLAGS}
  )
  set_target_properties(
    ${DASH_LIBRARY} PROPERTIES
    CXX_STANDARD 11
  )
  set_target_properties(
    ${DASH_LIBRARY} PROPERTIES
    CXX_STANDARD_REQUIRED 11
  )
  include_directories(
    ${ADDITIONAL_INCLUDES}
  )
  if (${dart_variant} STREQUAL "mpi")
    include_directories(
      ${MPI_INCLUDE_PATH})
    target_link_libraries(
      ${DASH_LIBRARY}
      ${MPI_C_LIBRARIES})
  endif()

  DeployLibrary(${DASH_LIBRARY})

  # Install library
  install(TARGETS ${DASH_LIBRARY} DESTINATION lib)

  # Examples
  if (BUILD_EXAMPLES)
    SubDirList(
      DASH_TEST_DIRECTORIES
      ${CMAKE_CURRENT_SOURCE_DIR}/examples)
    foreach(examplename ${DASH_TEST_DIRECTORIES})
      set(exampletarget ${examplename}.${dart_variant})
      if (EXISTS
         ${CMAKE_CURRENT_SOURCE_DIR}/examples/${examplename}/main.cpp)

        # Build example for single DART implementation
        add_executable(
          ${exampletarget}
          examples/${examplename}/main.cpp
        )
        include_directories(
          ${CMAKE_SOURCE_DIR}/dash/include
          ${ADDITIONAL_INCLUDES}
        )
        target_link_libraries(
          ${exampletarget}
          ${DASH_LIBRARY}
          ${DART_LIBRARY}
          ${ADDITIONAL_LIBRARIES}
        )
        set_target_properties(
          ${exampletarget} PROPERTIES
          COMPILE_FLAGS
          "${VARIANT_ADDITIONAL_COMPILE_FLAGS} -Wno-unused"
        )
        set_target_properties(
          ${exampletarget} PROPERTIES
          CXX_STANDARD 11
        )
        set_target_properties(
          ${exampletarget} PROPERTIES
          CXX_STANDARD_REQUIRED 11
        )
        if (${dart_variant} STREQUAL "mpi")
          include_directories(
            ${MPI_INCLUDE_PATH})
          target_link_libraries(
            ${exampletarget}
            ${MPI_C_LIBRARIES}
            ${MPI_CXX_LIBRARIES})
          if (IPM_FOUND)
            include_directories(
              ${IPM_INCLUDE_DIRS})
            target_link_libraries(
              ${exampletarget}
              ${IPM_LIBRARIES})
          endif()
          if (PLASMA_FOUND)
            target_link_libraries(
              ${exampletarget}
              ${PLASMA_LIBRARIES})
          endif()
        endif()

        # Installation
        DeployBinary(${exampletarget})
        install(
          TARGETS ${exampletarget}
          DESTINATION bin/dash/examples/${dart_variant})
      endif()
    endforeach(examplename ${DASH_TEST_DIRECTORIES})
  endif()
endforeach(dart_variant ${DART_IMPLEMENTATIONS_LIST})

## Tests
#
if (BUILD_TESTS)
  include(${CMAKE_SOURCE_DIR}/CMakeExt/GoogleTest.cmake)
  foreach(dart_variant ${DART_IMPLEMENTATIONS_LIST})
    set(DASH_LIBRARY "dash-${dart_variant}")
    set(DART_LIBRARY "dart-${dart_variant}")
    set(DASH_TEST "dash-test-${dart_variant}")
    include_directories(
      ${GTEST_INCLUDES}
      ${CMAKE_SOURCE_DIR}/dash/include
      ${ADDITIONAL_INCLUDES}
    )
    add_executable(
      ${DASH_TEST}
      ${DASH_TEST_SOURCES}
    )
    target_link_libraries(
      ${DASH_TEST}
      GTest
      ${DASH_LIBRARY}
      ${DART_LIBRARY}
      ${ADDITIONAL_LIBRARIES}
    )
    if (${dart_variant} STREQUAL "mpi")
      if (IPM_FOUND)
        include_directories(
          ${IPM_INCLUDE_DIRS})
        target_link_libraries(
          ${exampletarget}
          ${IPM_LIBRARIES})
      endif()
    endif()
    set_target_properties(
      ${DASH_TEST} PROPERTIES
      COMPILE_FLAGS
      "${VARIANT_ADDITIONAL_COMPILE_FLAGS} -Wno-unused -Wno-sign-compare"
    )
    set_target_properties(
      ${DASH_TEST} PROPERTIES
      CXX_STANDARD 11
    )
    set_target_properties(
      ${DASH_TEST} PROPERTIES
      CXX_STANDARD_REQUIRED 11
    )

    # Installation
    DeployBinary(${DASH_TEST})
    install(
      TARGETS ${DASH_TEST}
      DESTINATION bin/dash/test/${dart_variant})
  endforeach(dart_variant ${DART_IMPLEMENTATIONS_LIST})
endif()

## Installation

# Headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/dash
        DESTINATION include FILES_MATCHING PATTERN "*.h")

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/libdash.h
        DESTINATION include)
