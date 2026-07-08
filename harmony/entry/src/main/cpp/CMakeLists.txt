cmake_minimum_required(VERSION 3.5.0)

project(textcalc)

set(TEXTCALC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")

add_library(textcalc SHARED
    napi_init.cpp
    "${TEXTCALC_ROOT}/src/editor_logic.cpp"
    "${TEXTCALC_ROOT}/src/expression_engine.cpp"
    "${TEXTCALC_ROOT}/src/textcalc_c_api.cpp"
)

target_include_directories(textcalc PRIVATE
    "${TEXTCALC_ROOT}/src"
)

target_compile_features(textcalc PRIVATE cxx_std_20)
target_link_libraries(textcalc PUBLIC libace_napi.z.so)
