cmake_minimum_required(VERSION 3.16)

project(daisy-tt-rt VERSION 0.0.1 DESCRIPTION "A library providing ELLPACK-based MatVec")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CPM)

add_library(daisy_tt_rt SHARED
    lib/ellpack_matVec.cpp
)
target_include_directories(daisy_tt_rt PUBLIC
    lib/
)
target_compile_features(daisy_tt_rt PUBLIC cxx_std_20)
target_compile_options(daisy_tt_rt PRIVATE
    -fPIC
)
target_compile_definitions(daisy_tt_rt PUBLIC
    SPDLOG_FMT_EXTERNAL
)

if(NOT $ENV{TT_METAL_INSTALL_DIR} STREQUAL "")
    message(STATUS "Building against Tenstorrent Metalium installation from TT_METAL_INSTALL_DIR: $ENV{TT_METAL_INSTALL_DIR}")
    target_include_directories(daisy_tt_rt PUBLIC
        $ENV{TT_METAL_INSTALL_DIR}/include
        $ENV{TT_METAL_INSTALL_DIR}/include/metalium-thirdparty
    )
    target_link_directories(daisy_tt_rt PUBLIC
        $ENV{TT_METAL_INSTALL_DIR}/lib
    )
elseif(NOT $ENV{TT_METAL_HOME} STREQUAL "")
    message(STATUS "Building against Tenstorrent Metalium installation from TT_METAL_HOME: $ENV{TT_METAL_HOME}")
    target_include_directories(daisy_tt_rt PUBLIC
        $ENV{TT_METAL_HOME}/build/include
    )
    target_link_directories(daisy_tt_rt PUBLIC
        $ENV{TT_METAL_HOME}/build/lib
    )
endif()

find_package(Boost 1.83.0 REQUIRED container)
############################################################################################################################
# Boost
############################################################################################################################

# function(ensureboosttarget boostTarget)
#     if(NOT TARGET Boost::${boostTarget})
#         add_library(Boost::${boostTarget} INTERFACE IMPORTED GLOBAL)
#         target_link_libraries(Boost::${boostTarget} INTERFACE Boost::headers)
#         message(STATUS "Defined Boost::${boostTarget} as an INTERFACE target.")
#     endif()
# endfunction()

# CPMAddPackage(
#     NAME Boost
#     VERSION 1.86.0
#     URL
#         https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.tar.xz
#         URL_HASH
#         SHA256=2c5ec5edcdff47ff55e27ed9560b0a0b94b07bd07ed9928b476150e16b0efc57
#     OPTIONS
#         "BOOST_ENABLE_CMAKE ON"
#         "BOOST_SKIP_INSTALL_RULES ON"
#         "BUILD_SHARED_LIBS OFF"
#         "BOOST_INCLUDE_LIBRARIES core\\\;container\\\;smart_ptr\\\;interprocess\\\;asio\\\;lockfree"
#     FIND_PACKAGE_ARGUMENTS "CONFIG REQUIRED"
# )

# ensureboosttarget(algorithm)
# ensureboosttarget(asio)
# ensureboosttarget(lockfree)
# ensureboosttarget(core)
# ensureboosttarget(container)
# ensureboosttarget(smart_ptr)
# ensureboosttarget(interprocess)

############################################################################################################################
# boost-ext reflect : https://github.com/boost-ext/reflect
############################################################################################################################

CPMAddPackage(NAME reflect GITHUB_REPOSITORY boost-ext/reflect GIT_TAG v1.2.6)
if(reflect_ADDED)
    add_library(reflect INTERFACE)
    add_library(Reflect::Reflect ALIAS reflect)
    target_include_directories(reflect SYSTEM INTERFACE "$<BUILD_INTERFACE:${reflect_SOURCE_DIR}>")

    target_sources(
        reflect
        INTERFACE
            FILE_SET api
            TYPE HEADERS
            BASE_DIRS ${reflect_SOURCE_DIR}
            FILES ${reflect_SOURCE_DIR}/reflect
    )
endif()

# #############################################################################################################################

target_link_libraries(daisy_tt_rt PUBLIC
    tt_metal
    fmt
    Boost::headers
    # Boost::algorithm # what tt_metal builds against in addition to just Boost::headers
    # Boost::asio
    # Boost::lockfree
    # Boost::core
    Boost::container
    # Boost::smart_ptr
    # Boost::interprocess
    Reflect::Reflect
)

# #############################################################################################################################

target_link_libraries(daisy_tt_rt PUBLIC
    tt_metal
    fmt
    Boost::headers
    # Boost::algorithm # what tt_metal builds against in addition to just Boost::headers
    # Boost::asio
    # Boost::lockfree
    # Boost::core
    Boost::container
    # Boost::smart_ptr
    # Boost::interprocess
    Reflect::Reflect
)


set(KERNELS_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/kernels")
set(KERNELS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen_kernels")
file(GLOB tenstorrent_kernel_subdirs RELATIVE "${KERNELS_BASE_DIR}" "${KERNELS_BASE_DIR}/*")

set(gen_cpp_files "")
foreach(subdir ${tenstorrent_kernel_subdirs})
    if(IS_DIRECTORY "${KERNELS_BASE_DIR}/${subdir}")
        message(VERBOSE "Processing tenstorrent kernel subdir: ${subdir}")
        file(GLOB cpp_files RELATIVE "${KERNELS_BASE_DIR}/${subdir}" "${KERNELS_BASE_DIR}/${subdir}/*.cpp")
        set(gen_cpp "${KERNELS_OUTPUT_DIR}/gen_kernels_${subdir}.hpp")
        file(WRITE "${gen_cpp}" "// Auto-generated kernel source file for ${subdir}\n")
        foreach(cpp_file ${cpp_files})
            get_filename_component(var_name "${cpp_file}" NAME_WE)
            set(var_name "tt_kernel_${subdir}_${var_name}")
            # Read file contents
            file(READ "${KERNELS_BASE_DIR}/${subdir}/${cpp_file}" file_content)
            # Write as a C++ const char* variable
            file(APPEND "${gen_cpp}" "const inline char* ${var_name} = R\"MULTI(\n${file_content}\n)MULTI\";\n")
        endforeach()
        list(APPEND gen_cpp_files "${gen_cpp}")
    endif()
endforeach()

add_custom_target(generate_gen_cpp_files ALL DEPENDS ${gen_cpp_files})
add_dependencies(daisy_tt_rt generate_gen_cpp_files)
target_include_directories(daisy_tt_rt PRIVATE "${KERNELS_OUTPUT_DIR}")
