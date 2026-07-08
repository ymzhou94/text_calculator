#pragma once

#include <string>
#include <string_view>

namespace textcalc {

struct EnterInsertion {
    bool should_insert = false;
    std::wstring text;
};

EnterInsertion BuildEnterInsertion(std::wstring_view editor_text, size_t caret);

}  // namespace textcalc
