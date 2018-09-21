include_directories(../../src gtest-1.7.0/include ${COMMON_INCLUDES})

add_subdirectory(gtest-1.7.0)

add_executable(
    driveshaft_unit_tests
    test_driveshaft_config.cpp
    test_gearman_client.cpp
    tests.cpp
)

target_link_libraries(
    driveshaft_unit_tests
    driveshaft
    gtest
    log4cxx
    network-uri 
    ${Boost_LIBRARIES}
)

add_test(AllTestsInDriveShaft driveshaft_unit_tests)
