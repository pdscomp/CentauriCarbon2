set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(KINEMATICS_SOURCES
    ${PROJECT_SOURCE_DIR}/kinematics/kinematics_factory.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/cartesian.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/corexy.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/corexz.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/delta.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/deltesian.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/hybrid_corexy.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/hybrid_corexz.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/none.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/polar.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/rotary_delta.cpp
    ${PROJECT_SOURCE_DIR}/kinematics/winch.cpp
)

add_library(kinematics SHARED ${KINEMATICS_SOURCES})

target_link_libraries(kinematics 
    elegoo_log
    elegoo_extras
)