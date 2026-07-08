#include "textcalc_c_api.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

#include "editor_logic.h"
#include "expression_engine.h"

namespace {

bool AppendCodePoint(std::wstring& output, unsigned int code_point) {
    if (code_point > 0x10FFFF || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
        return false;
    }

    if constexpr (sizeof(wchar_t) == 2) {
        if (code_point <= 0xFFFF) {
            output.push_back(static_cast<wchar_t>(code_point));
            return true;
        }

        code_point -= 0x10000;
        output.push_back(static_cast<wchar_t>(0xD800 + (code_point >> 10)));
        output.push_back(static_cast<wchar_t>(0xDC00 + (code_point & 0x3FF)));
        return true;
    } else {
        output.push_back(static_cast<wchar_t>(code_point));
        return true;
    }
}

bool DecodeUtf8(std::string_view input, std::wstring& output) {
    output.clear();

    for (size_t i = 0; i < input.size();) {
        const unsigned char first = static_cast<unsigned char>(input[i]);
        unsigned int code_point = 0;
        size_t length = 0;

        if (first <= 0x7F) {
            code_point = first;
            length = 1;
        } else if (first >= 0xC2 && first <= 0xDF) {
            code_point = first & 0x1F;
            length = 2;
        } else if (first >= 0xE0 && first <= 0xEF) {
            code_point = first & 0x0F;
            length = 3;
        } else if (first >= 0xF0 && first <= 0xF4) {
            code_point = first & 0x07;
            length = 4;
        } else {
            return false;
        }

        if (i + length > input.size()) {
            return false;
        }

        for (size_t j = 1; j < length; ++j) {
            const unsigned char next = static_cast<unsigned char>(input[i + j]);
            if ((next & 0xC0) != 0x80) {
                return false;
            }
            code_point = (code_point << 6) | (next & 0x3F);
        }

        if ((length == 3 && code_point < 0x800) || (length == 4 && code_point < 0x10000)) {
            return false;
        }

        if (!AppendCodePoint(output, code_point)) {
            return false;
        }
        i += length;
    }

    return true;
}

void AppendUtf8(std::string& output, unsigned int code_point) {
    if (code_point <= 0x7F) {
        output.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else if (code_point <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
}

std::string EncodeUtf8(std::wstring_view input) {
    std::string output;

    for (size_t i = 0; i < input.size(); ++i) {
        unsigned int code_point = static_cast<unsigned int>(input[i]);

        if constexpr (sizeof(wchar_t) == 2) {
            if (code_point >= 0xD800 && code_point <= 0xDBFF && i + 1 < input.size()) {
                const unsigned int trail = static_cast<unsigned int>(input[i + 1]);
                if (trail >= 0xDC00 && trail <= 0xDFFF) {
                    code_point = 0x10000 + (((code_point - 0xD800) << 10) | (trail - 0xDC00));
                    ++i;
                }
            }
        }

        AppendUtf8(output, code_point);
    }

    return output;
}

void CopyOutput(const std::string& message, char* output, size_t output_size) {
    if (output == nullptr || output_size == 0) {
        return;
    }

    const size_t bytes_to_copy = std::min(output_size - 1, message.size());
    std::memcpy(output, message.data(), bytes_to_copy);
    output[bytes_to_copy] = '\0';
}

std::wstring MatchInputLineEnding(std::wstring text, std::wstring_view input) {
    if (input.empty() || input.back() != L'\n') {
        return text;
    }

    const bool input_uses_crlf = input.size() >= 2 && input[input.size() - 2] == L'\r';
    if (input_uses_crlf) {
        return text;
    }

    std::wstring normalized;
    normalized.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') {
            continue;
        }
        normalized.push_back(text[i]);
    }
    return normalized;
}

}  // namespace

extern "C" TextCalcStatus textcalc_evaluate_utf8(const char* expression,
                                                 char* output,
                                                 size_t output_size,
                                                 TextCalcResult* result) {
    if (result == nullptr || expression == nullptr) {
        return TEXTCALC_STATUS_INVALID_ARGUMENT;
    }

    result->ok = 0;
    result->value = 0.0;
    result->required_size = 0;

    std::wstring wide_expression;
    if (!DecodeUtf8(expression, wide_expression)) {
        return TEXTCALC_STATUS_INVALID_UTF8;
    }

    const textcalc::Evaluation evaluation = textcalc::EvaluateExpression(wide_expression);
    result->ok = evaluation.ok ? 1 : 0;
    result->value = evaluation.ok ? evaluation.value : 0.0;

    const std::string message = EncodeUtf8(evaluation.ok ? evaluation.output : evaluation.error);
    result->required_size = message.size() + 1;
    CopyOutput(message, output, output_size);
    return TEXTCALC_STATUS_OK;
}

extern "C" TextCalcStatus textcalc_build_enter_insertion_utf8(const char* text_before_caret,
                                                              char* output,
                                                              size_t output_size,
                                                              TextCalcInsertion* result) {
    if (result == nullptr || text_before_caret == nullptr) {
        return TEXTCALC_STATUS_INVALID_ARGUMENT;
    }

    result->should_insert = 0;
    result->required_size = 0;

    std::wstring wide_text;
    if (!DecodeUtf8(text_before_caret, wide_text)) {
        return TEXTCALC_STATUS_INVALID_UTF8;
    }

    const textcalc::EnterInsertion insertion =
        textcalc::BuildEnterInsertion(wide_text, wide_text.size());
    result->should_insert = insertion.should_insert ? 1 : 0;

    const std::string message = EncodeUtf8(MatchInputLineEnding(insertion.text, wide_text));
    result->required_size = message.size() + 1;
    CopyOutput(message, output, output_size);
    return TEXTCALC_STATUS_OK;
}
