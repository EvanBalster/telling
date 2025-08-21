vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO evanbalster/nngpp
    REF b8737140105bca3dd00f66d32d162e512c16aa4a
    SHA512 0d4d7cb523c0e76fafcdb42602cf69bb4ce183f684d60e31a75ced62b984e994b641404af17abf9e99f9cdb1cad5cba23bcb71d7702513ebb5e8de03e023418e
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DNNGPP_BUILD_DEMOS=OFF
        -DNNGPP_BUILD_TESTS=OFF
)

vcpkg_cmake_install()

# Move CMake config files to the right place
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/${PORT})

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib")

# Handle copyright
file(INSTALL "${SOURCE_PATH}/license.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)

