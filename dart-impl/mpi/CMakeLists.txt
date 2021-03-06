project(project_dash_dart_impl_mpi C)

# Library name
set(DASH_DART_IMPL_MPI_LIBRARY dart-mpi)

set(DASH_DART_BASE_LIBRARY dart-base)

include(${CMAKE_SOURCE_DIR}/CMakeExt/MPI.cmake)

if (MPI_NOTFOUND)
  message(FATAL_ERROR, "No MPI implementation found for dart-mpi")
endif()

# Source- and header files to be compiled (OBJ):
file(GLOB_RECURSE DASH_DART_IMPL_MPI_SOURCES "src/*.c" "src/*.h" "src/*.cc")
file(GLOB_RECURSE DASH_DART_IMPL_MPI_HEADERS "include/*.h")

set(DASH_DART_IF_INCLUDE_DIR ${DASH_DART_IF_INCLUDE_DIR}
    PARENT_SCOPE)
set(ENABLE_DART_LOGGING ${ENABLE_DART_LOGGING}
    PARENT_SCOPE)
set(ENABLE_UNIFIED_MEMORY_MODEL ${ENABLE_UNIFIED_MEMORY_MODEL}
    PARENT_SCOPE)
set(ENABLE_SHARED_WINDOWS ${ENABLE_SHARED_WINDOWS}
    PARENT_SCOPE)

message(STATUS "MPI include path:         " ${MPI_INCLUDE_PATH})
message(STATUS "MPI libraries:")
foreach (MPI_C_LIB ${MPI_C_LIBRARIES})
  message(STATUS "    " ${MPI_C_LIB})
endforeach()
message(STATUS "MPI link flags:")
foreach (MPI_LINK_FLAG ${MPI_LINK_FLAGS})
  message(STATUS "   " ${MPI_LINK_FLAG})
endforeach()

## Configure compile flags

set (ADDITIONAL_COMPILE_FLAGS "-DDART -DDART_MPI_IMPL_ID=\"${MPI_IMPL_ID}\"")
if (${MPI_IMPL_IS_MPICH})
  set (ADDITIONAL_COMPILE_FLAGS "-DDART_MPI_IMPL_IS_MPICH")
endif()
if (${MPI_IMPL_IS_INTEL})
  set (ADDITIONAL_COMPILE_FLAGS "-DDART_MPI_IMPL_IS_INTEL")
endif()
if (${MPI_IMPL_IS_MVAPICH})
  set (ADDITIONAL_COMPILE_FLAGS "-DDART_MPI_IMPL_IS_MVAPICH")
endif()
if (${MPI_IMPL_IS_OPENMPI})
  set (ADDITIONAL_COMPILE_FLAGS "-DDART_MPI_IMPL_IS_OPENMPI")
endif()

if (ENABLE_DART_LOGGING)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDASH_ENABLE_LOGGING")
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDART_ENABLE_LOGGING")
endif()

if (ENABLE_UNIFIED_MEMORY_MODEL)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} -DDART_MPI_ENABLE_UNIFIED_MEMORY_MODEL")
endif()

if (NOT ENABLE_SHARED_WINDOWS)
  set (ADDITIONAL_COMPILE_FLAGS
    "${ADDITIONAL_COMPILE_FLAGS} -DDART_MPI_DISABLE_SHARED_WINDOWS")
endif()

if(MPI_COMPILE_FLAGS)
  set (ADDITIONAL_COMPILE_FLAGS
       "${ADDITIONAL_COMPILE_FLAGS} ${MPI_COMPILE_FLAGS}")
endif()

## Build targets

# Directories containing the implementation of the library (-I):
set(DASH_DART_IMPL_MPI_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
# Includes
include_directories(
  ${DASH_DART_IMPL_MPI_INCLUDE_DIRS}
  ${MPI_INCLUDE_PATH}
  ${DASH_DART_IF_INCLUDE_DIR}
  ${DASH_DART_BASE_INCLUDE_DIR}
)
# Library compilation sources
add_library(
  ${DASH_DART_IMPL_MPI_LIBRARY} # library name
  ${DASH_DART_IMPL_MPI_SOURCES} # sources
  ${DASH_DART_IMPL_MPI_HEADERS} # headers
)
# Link dependencies
target_link_libraries(
  ${DASH_DART_IMPL_MPI_LIBRARY}
  ${MPI_C_LIBRARIES}
  ${DASH_DART_BASE_LIBRARY}
)

if(MPI_LINK_FLAGS)
  set_target_properties(
    ${DASH_DART_IMPL_MPI_LIBRARY} PROPERTIES
    LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()

# Compile flags
set_target_properties(
  ${DASH_DART_IMPL_MPI_LIBRARY} PROPERTIES
  COMPILE_FLAGS ${ADDITIONAL_COMPILE_FLAGS}
)

## Installation

DeployLibrary(${DASH_DART_IMPL_MPI_LIBRARY})

# Headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/dash
        DESTINATION include FILES_MATCHING PATTERN "*.h")
# Generated headers, if any
# install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/dash
#         DESTINATION include FILES_MATCHING PATTERN "*.h")
# Library
install(TARGETS ${DASH_DART_IMPL_MPI_LIBRARY} DESTINATION lib)

