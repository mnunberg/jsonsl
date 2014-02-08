ADD_EXECUTABLE(json_test json_test.c)
TARGET_LINK_LIBRARIES(json_test jsonsl)

ADD_EXECUTABLE(failure_test fail-tests.c)
TARGET_LINK_LIBRARIES(failure_test jsonsl)

ADD_EXECUTABLE(jpr_test jpr_test.c)
TARGET_LINK_LIBRARIES(jpr_test jsonsl)
ADD_EXECUTABLE(unescape unescape.c)
TARGET_LINK_LIBRARIES(unescape jsonsl)

FILE(GLOB samples_ok ${CMAKE_BINARY_DIR}/share/*
                     ${CMAKE_BINARY_DIR}/jsc/pass*.json)
FILE(GLOB samples_bad ${CMAKE_BINARY_DIR}/share/jsc/fail*.json)
ADD_TEST(okparse json_test ${samples_ok})
ADD_TEST(badparse failure_test ${samples_bad})
ADD_TEST(jsonpointer jpr_test)
ADD_TEST(unescape unescape)
