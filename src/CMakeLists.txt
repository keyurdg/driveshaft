set(CXX_SOURCES)
file(GLOB files ./*.cpp)
list(APPEND CXX_SOURCES ${files})
file(GLOB files ./dist/*.cpp)
list(APPEND CXX_SOURCES ${files})

add_executable(driveshaft ${CXX_SOURCES})
target_link_libraries(driveshaft ${DRIVESHAFT_LINK_LIBRARIES})

install(TARGETS driveshaft DESTINATION bin)
