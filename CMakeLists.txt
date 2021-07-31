cmake_minimum_required(VERSION 3.10)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

enable_language(CXX)
enable_language(C)

find_package (Threads)


# Library project
project(telling)

file(GLOB TELLING_HEADERS include/telling/*.h)
file(GLOB TELLING_SOURCES src/telling/*.cpp)

add_library(telling STATIC ${TELLING_HEADERS} ${TELLING_SOURCES})

#target_compile_definitions(telling PUBLIC NNG_STATIC_LIB=1)
#add_library(nng STATIC IMPORTED)
#set_target_properties(nng PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_LIBDIR}/nng")

find_package(nng CONFIG REQUIRED)
target_link_libraries(telling PUBLIC nng::nng)


#find_package(MbedTLS CONFIG REQUIRED)
#target_link_libraries(telling PUBLIC MbedTLS::mbedtls)
#target_link_libraries(telling PUBLIC MbedTLS::mbedx509)
#target_link_libraries(telling PUBLIC MbedTLS::mbedcrypto)


#add_library(mbedtls STATIC IMPORTED)
#add_library(mbedx509 STATIC IMPORTED)
#add_library(mbedcrypto STATIC IMPORTED)
#set_target_properties(mbedtls PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_LIBDIR}/mbed TLS")
#set_target_properties(mbedx509 PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_LIBDIR}/mbed TLS")
#set_target_properties(mbedcrypto PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_LIBDIR}/mbed TLS")

#target_link_libraries(telling nng::nng mbedtls mbedx509 mbedcrypto)

target_include_directories(telling PUBLIC "include")
target_include_directories(telling PUBLIC "thirdparty/include")


# TEST project
project(telling_test)
# add the executable
file(GLOB TEST_SOURCES test/*.*)
add_executable(telling_test ${TEST_SOURCES})

target_link_libraries(telling_test telling)


# Solution name
project(telling)