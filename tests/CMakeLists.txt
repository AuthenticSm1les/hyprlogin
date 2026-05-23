find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(
  deps
  REQUIRED
  IMPORTED_TARGET
  wayland-client
  hyprutils>=0.11.0
  hyprgraphics>=0.1.6)

enable_testing()

add_executable(test_Color test_Color.cpp ../src/helpers/Color.cpp)
target_link_libraries(test_Color PRIVATE GTest::GTest GTest::Main PkgConfig::deps)

add_executable(test_Math test_Math.cpp ../src/helpers/Math.cpp)
target_link_libraries(test_Math PRIVATE GTest::GTest GTest::Main PkgConfig::deps)

add_executable(test_ConfigDataValues test_ConfigDataValues.cpp ../src/helpers/Color.cpp)
target_link_libraries(test_ConfigDataValues PRIVATE GTest::GTest GTest::Main PkgConfig::deps)

add_executable(test_Timer test_Timer.cpp ../src/core/Timer.cpp)
target_link_libraries(test_Timer PRIVATE GTest::GTest GTest::Main PkgConfig::deps)

add_executable(test_MiscFunctions test_MiscFunctions.cpp)
target_link_libraries(test_MiscFunctions PRIVATE GTest::GTest GTest::Main)

add_executable(test_Greetd test_Greetd.cpp)
target_link_libraries(test_Greetd PRIVATE GTest::GTest GTest::Main)

add_executable(test_MiscFunctionsExtra test_MiscFunctions_extra.cpp)
target_link_libraries(test_MiscFunctionsExtra PRIVATE GTest::GTest GTest::Main)

add_executable(test_Log test_Log.cpp)
target_link_libraries(test_Log PRIVATE GTest::GTest GTest::Main PkgConfig::deps)

add_executable(test_StringUtils test_StringUtils.cpp)
target_link_libraries(test_StringUtils PRIVATE GTest::GTest GTest::Main)

add_test(NAME Color COMMAND test_Color)
add_test(NAME Math COMMAND test_Math)
add_test(NAME ConfigDataValues COMMAND test_ConfigDataValues)
add_test(NAME Timer COMMAND test_Timer)
add_test(NAME MiscFunctions COMMAND test_MiscFunctions)
add_test(NAME Greetd COMMAND test_Greetd)
add_test(NAME MiscFunctionsExtra COMMAND test_MiscFunctionsExtra)
add_test(NAME Log COMMAND test_Log)
add_test(NAME StringUtils COMMAND test_StringUtils)
