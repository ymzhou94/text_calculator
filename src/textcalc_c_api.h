#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum TextCalcStatus {
    TEXTCALC_STATUS_OK = 0,
    TEXTCALC_STATUS_INVALID_ARGUMENT = 1,
    TEXTCALC_STATUS_INVALID_UTF8 = 2,
};

struct TextCalcResult {
    int ok;
    double value;
    size_t required_size;
};

struct TextCalcInsertion {
    int should_insert;
    size_t required_size;
};

TextCalcStatus textcalc_evaluate_utf8(const char* expression,
                                      char* output,
                                      size_t output_size,
                                      TextCalcResult* result);

TextCalcStatus textcalc_build_enter_insertion_utf8(const char* text_before_caret,
                                                   char* output,
                                                   size_t output_size,
                                                   TextCalcInsertion* result);

#ifdef __cplusplus
}
#endif
