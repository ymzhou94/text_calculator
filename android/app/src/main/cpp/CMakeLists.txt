cmake_minimum_required(VERSION 3.22.1)

project(TextCalculatorAndroid LANGUAGES CXX)

set(TEXTCALC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")

add_library(textcalc_jni SHARED
    textcalc_jni.cpp
    "${TEXTCALC_ROOT}/src/editor_logic.cpp"
    "${TEXTCALC_ROOT}/src/expression_engine.cpp"
    "${TEXTCALC_ROOT}/src/textcalc_c_api.cpp"
)

target_include_directories(textcalc_jni PRIVATE
    "${TEXTCALC_ROOT}/src"
)

target_compile_features(textcalc_jni PRIVATE cxx_std_20)
