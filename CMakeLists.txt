cmake_minimum_required(VERSION 3.20)

project(TextCalculator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_library(calc_engine
    src/editor_logic.cpp
    src/expression_engine.cpp
    src/textcalc_c_api.cpp
)
target_include_directories(calc_engine PUBLIC src)

add_executable(text_calculator WIN32
    src/main.cpp
)
target_link_libraries(text_calculator PRIVATE calc_engine user32 gdi32)
target_link_libraries(text_calculator PRIVATE comdlg32)
if(MINGW)
    target_link_options(text_calculator PRIVATE -municode)
endif()

add_executable(engine_tests
    tests/engine_tests.cpp
)
target_link_libraries(engine_tests PRIVATE calc_engine)

enable_testing()
add_test(NAME engine_tests COMMAND engine_tests)
