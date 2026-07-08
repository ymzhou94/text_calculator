#pragma once

#include <string>
#include <string_view>

namespace textcalc {

struct Evaluation {
    bool ok = false;
    double value = 0.0;
    std::wstring output;
    std::wstring error;
};

Evaluation EvaluateExpression(std::wstring_view line);

}  // namespace textcalc
