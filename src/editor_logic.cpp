#include "editor_logic.h"

#include "expression_engine.h"

namespace textcalc {
namespace {

std::wstring TrimLine(std::wstring line) {
    while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n' ||
                             line.back() == L' ' || line.back() == L'\t')) {
        line.pop_back();
    }

    size_t first = 0;
    while (first < line.size() && (line[first] == L' ' || line[first] == L'\t')) {
        ++first;
    }

    if (first > 0) {
        line.erase(0, first);
    }

    return line;
}

bool IsGeneratedLine(const std::wstring& line) {
    return !line.empty() && (line.front() == L'=' || line.front() == L'!');
}

size_t ConsumeLineEnding(std::wstring_view text, size_t position) {
    if (position >= text.size()) {
        return position;
    }

    if (text[position] == L'\r') {
        ++position;
        if (position < text.size() && text[position] == L'\n') {
            ++position;
        }
    } else if (text[position] == L'\n') {
        ++position;
    }
    return position;
}

size_t ReplacementEndAfterCaret(std::wstring_view text, size_t caret) {
    size_t replacement_end = ConsumeLineEnding(text, caret);
    const size_t next_line_start = replacement_end;

    size_t next_line_end = next_line_start;
    while (next_line_end < text.size() && text[next_line_end] != L'\r' &&
           text[next_line_end] != L'\n') {
        ++next_line_end;
    }

    const std::wstring next_line =
        TrimLine(std::wstring(text.substr(next_line_start, next_line_end - next_line_start)));
    if (IsGeneratedLine(next_line)) {
        replacement_end = ConsumeLineEnding(text, next_line_end);
    }
    return replacement_end;
}

std::wstring LineBeforeCaret(std::wstring_view text, size_t caret) {
    if (caret == 0 || caret > text.size()) {
        return L"";
    }

    size_t line_end = caret;
    while (line_end > 0 && (text[line_end - 1] == L'\r' || text[line_end - 1] == L'\n')) {
        --line_end;
    }

    size_t line_start = line_end;
    while (line_start > 0 && text[line_start - 1] != L'\r' && text[line_start - 1] != L'\n') {
        --line_start;
    }

    return std::wstring(text.substr(line_start, line_end - line_start));
}

}  // namespace

EnterInsertion BuildEnterInsertion(std::wstring_view editor_text, size_t caret) {
    EnterInsertion insertion;
    if (caret == 0 || caret > editor_text.size() || editor_text[caret - 1] != L'\n') {
        return insertion;
    }

    const std::wstring line = TrimLine(LineBeforeCaret(editor_text, caret));
    if (line.empty() || IsGeneratedLine(line)) {
        return insertion;
    }

    const Evaluation evaluation = EvaluateExpression(line);
    if (!evaluation.ok) {
        return insertion;
    }

    insertion.should_insert = true;
    insertion.replace_start = caret;
    insertion.replace_end = ReplacementEndAfterCaret(editor_text, caret);
    insertion.text = L"= " + evaluation.output + L"\r\n";
    return insertion;
}

}  // namespace textcalc
