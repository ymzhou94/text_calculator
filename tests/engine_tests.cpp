#include "editor_logic.h"
#include "expression_engine.h"
#include "textcalc_c_api.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Case {
    std::wstring expression;
    double expected;
    std::wstring expected_output;
};

bool CloseEnough(double actual, double expected) {
    return std::abs(actual - expected) < 1e-9;
}

std::string ToNarrow(const std::wstring& text) {
    std::string result;
    result.reserve(text.size());
    for (wchar_t ch : text) {
        result.push_back(static_cast<char>(ch));
    }
    return result;
}

}  // namespace

int main() {
    const std::vector<Case> cases = {
        {L"1+2*3", 7.0, L"7"},
        {L"9-4", 5.0, L"5"},
        {L"8/4", 2.0, L"2"},
        {L"(1+2)*3", 9.0, L"9"},
        {L"100", 100.0, L"100"},
        {L"2^8", 256.0, L"256"},
        {L"2^3^2", 512.0, L"512"},
        {L"-2^2", -4.0, L"-4"},
        {L"sqrt(16)", 4.0, L"4"},
        {L"root(3,27)", 3.0, L"3"},
        {L"root(3,-8)", -2.0, L"-2"},
        {L"log(100)", 2.0, L"2"},
        {L"log(2,8)", 3.0, L"3"},
        {L"ln(e)", 1.0, L"1"},
        {L"2pi", 2.0 * 3.14159265358979323846, L""},
        {L"\\frac{1}{2}+\\sqrt{9}", 3.5, L"3.5"},
        {L"2^{3}+\\sqrt[3]{27}", 11.0, L"11"},
        {L"\\log_{2}{8}", 3.0, L"3"},
        {L"\\ln(e)", 1.0, L"1"},
        {L"\\frac{1+1}{\\sqrt{16}}", 0.5, L"0.5"},
    };

    bool ok = true;
    for (const Case& test : cases) {
        const textcalc::Evaluation result = textcalc::EvaluateExpression(test.expression);
        if (!result.ok) {
            std::cerr << "FAIL " << ToNarrow(test.expression) << ": "
                      << ToNarrow(result.error) << "\n";
            ok = false;
            continue;
        }
        if (!CloseEnough(result.value, test.expected)) {
            std::cerr << "FAIL " << ToNarrow(test.expression) << ": expected " << test.expected
                      << ", got " << result.value << "\n";
            ok = false;
        }
        if (!test.expected_output.empty() && result.output != test.expected_output) {
            std::cerr << "FAIL " << ToNarrow(test.expression) << ": expected output "
                      << ToNarrow(test.expected_output) << ", got " << ToNarrow(result.output)
                      << "\n";
            ok = false;
        }
    }

    const textcalc::Evaluation invalid = textcalc::EvaluateExpression(L"\\sin{1}");
    if (invalid.ok) {
        std::cerr << "FAIL unsupported LaTeX command should be rejected\n";
        ok = false;
    }

    char buffer[64] = {};
    TextCalcResult c_result{};
    const TextCalcStatus c_status =
        textcalc_evaluate_utf8("\\frac{1}{2}+\\sqrt{9}", buffer, sizeof(buffer), &c_result);
    if (c_status != TEXTCALC_STATUS_OK || c_result.ok != 1 || std::string(buffer) != "3.5") {
        std::cerr << "FAIL C API LaTeX evaluation\n";
        ok = false;
    }

    const TextCalcStatus utf8_status =
        textcalc_evaluate_utf8("\xF0\x28\x8C\x28", buffer, sizeof(buffer), &c_result);
    if (utf8_status != TEXTCALC_STATUS_INVALID_UTF8) {
        std::cerr << "FAIL C API invalid UTF-8 should be rejected\n";
        ok = false;
    }

    const textcalc::EnterInsertion insertion =
        textcalc::BuildEnterInsertion(L"1+2*3\r\n", 7);
    if (!insertion.should_insert || insertion.text != L"= 7\r\n") {
        std::cerr << "FAIL editor enter insertion should append result line\n";
        ok = false;
    }

    const textcalc::EnterInsertion generated =
        textcalc::BuildEnterInsertion(L"= 7\r\n", 5);
    if (generated.should_insert) {
        std::cerr << "FAIL generated result lines should not be evaluated again\n";
        ok = false;
    }

    char insertion_buffer[64] = {};
    TextCalcInsertion c_insertion{};
    const TextCalcStatus insertion_status = textcalc_build_enter_insertion_utf8(
        "1+2*3\r\n", insertion_buffer, sizeof(insertion_buffer), &c_insertion);
    if (insertion_status != TEXTCALC_STATUS_OK || c_insertion.should_insert != 1 ||
        std::string(insertion_buffer) != "= 7\r\n") {
        std::cerr << "FAIL C API editor insertion should append result line\n";
        ok = false;
    }

    const TextCalcStatus lf_insertion_status = textcalc_build_enter_insertion_utf8(
        "1+2*3\n", insertion_buffer, sizeof(insertion_buffer), &c_insertion);
    if (lf_insertion_status != TEXTCALC_STATUS_OK || c_insertion.should_insert != 1 ||
        std::string(insertion_buffer) != "= 7\n") {
        std::cerr << "FAIL C API editor insertion should preserve LF line endings\n";
        ok = false;
    }

    const TextCalcStatus generated_status = textcalc_build_enter_insertion_utf8(
        "= 7\r\n", insertion_buffer, sizeof(insertion_buffer), &c_insertion);
    if (generated_status != TEXTCALC_STATUS_OK || c_insertion.should_insert != 0) {
        std::cerr << "FAIL C API generated result lines should not be evaluated again\n";
        ok = false;
    }

    return ok ? 0 : 1;
}
