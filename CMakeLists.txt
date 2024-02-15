cmake_minimum_required(VERSION 3.18)

include(CMakePackageConfigHelpers)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

enable_language(CXX)
enable_language(C)

if (MSVC)
    add_compile_options(/MP) # Multi-core compilation (faster)
    add_compile_options("/Zc:__cplusplus") # Correct __cplusplus macro
endif()

find_package (Threads)
find_package (nngpp CONFIG REQUIRED)
#find_package (rapidjson CONFIG REQUIRED)


# Library project
project(telling)

add_library(telling STATIC)

file(GLOB_RECURSE telling.headers include/telling/*.h)
file(GLOB_RECURSE telling.sources src/telling/*.cpp src/telling/*.h)

target_sources(telling
    PRIVATE ${telling.sources}
    PUBLIC FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
        FILES ${telling.headers})

#target_compile_definitions(telling PUBLIC NNG_STATIC_LIB=1)
#add_library(nng STATIC IMPORTED)
#set_target_properties(nng PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_LIBDIR}/nng")

find_package(nng CONFIG REQUIRED)
target_link_libraries(telling PUBLIC nng::nng)
target_include_directories(telling PUBLIC ${NNGPP_INCLUDE_DIRS})

target_include_directories(telling PRIVATE "include")
target_include_directories(telling PRIVATE "thirdparty/include")

set_target_properties(telling PROPERTIES DEBUG_POSTFIX d)

set_property(TARGET telling PROPERTY EXPORT_NAME telling)
configure_package_config_file(
    telling-config.cmake.in
    telling-config.cmake
    INSTALL_DESTINATION lib/telling)
install(TARGETS telling
    EXPORT telling-targets
    FILE_SET HEADERS)
install(EXPORT telling-targets
    DESTINATION lib/telling
    NAMESPACE "telling::")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/telling-config.cmake
    DESTINATION lib/telling)


# TEST project
project(telling_test)
# add the executable
file(GLOB TEST_SOURCES test/*.*)
add_executable(telling_test ${TEST_SOURCES})

target_link_libraries(telling_test telling)

target_include_directories(telling_test PRIVATE "thirdparty/include")


# Solution name
project(telling)



# Installation
install(TARGETS telling DESTINATION lib)
install(FILES ${telling.headers} DESTINATION include)