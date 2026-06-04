if(EXISTS "/home/keaner/Downloads/golde/release/GOLTest/GOLTest")
  if(NOT EXISTS "/home/keaner/Downloads/golde/release/GOLTest/GOLTest[1]_tests.cmake" OR
     NOT "/home/keaner/Downloads/golde/release/GOLTest/GOLTest[1]_tests.cmake" IS_NEWER_THAN "/home/keaner/Downloads/golde/release/GOLTest/GOLTest" OR
     NOT "/home/keaner/Downloads/golde/release/GOLTest/GOLTest[1]_tests.cmake" IS_NEWER_THAN "${CMAKE_CURRENT_LIST_FILE}")
    include("/usr/share/cmake-3.28/Modules/GoogleTestAddTests.cmake")
    gtest_discover_tests_impl(
      TEST_EXECUTABLE [==[/home/keaner/Downloads/golde/release/GOLTest/GOLTest]==]
      TEST_EXECUTOR [==[]==]
      TEST_WORKING_DIR [==[/home/keaner/Downloads/golde/release/GOLTest]==]
      TEST_EXTRA_ARGS [==[]==]
      TEST_PROPERTIES [==[]==]
      TEST_PREFIX [==[]==]
      TEST_SUFFIX [==[]==]
      TEST_FILTER [==[]==]
      NO_PRETTY_TYPES [==[FALSE]==]
      NO_PRETTY_VALUES [==[FALSE]==]
      TEST_LIST [==[GOLTest_TESTS]==]
      CTEST_FILE [==[/home/keaner/Downloads/golde/release/GOLTest/GOLTest[1]_tests.cmake]==]
      TEST_DISCOVERY_TIMEOUT [==[90]==]
      TEST_XML_OUTPUT_DIR [==[]==]
    )
  endif()
  include("/home/keaner/Downloads/golde/release/GOLTest/GOLTest[1]_tests.cmake")
else()
  add_test(GOLTest_NOT_BUILT GOLTest_NOT_BUILT)
endif()
