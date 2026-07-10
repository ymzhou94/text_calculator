#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commdlg.h>

#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "editor_logic.h"
#include "expression_engine.h"

namespace {

constexpr wchar_t kWindowClass[] = L"TextCalculatorWindow";
constexpr int kEditId = 1;
constexpr int kClearButtonId = 101;
constexpr int kScientificButtonId = 102;
constexpr int kSaveButtonId = 103;
constexpr int kHelpButtonId = 104;
constexpr int kToolbarHeight = 44;

HWND g_edit = nullptr;
HFONT g_font = nullptr;
bool g_internal_edit = false;

struct LineRange {
    size_t start = 0;
    size_t end = 0;
    size_t after_line_break = 0;
    std::wstring text;
};

std::wstring GetWindowTextString(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1);
    GetWindowTextW(hwnd, buffer.data(), static_cast<int>(buffer.size()));
    return std::wstring(buffer.data(), static_cast<size_t>(length));
}

void InsertAtCaret(const std::wstring& text) {
    g_internal_edit = true;
    SendMessageW(g_edit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
    g_internal_edit = false;
}

void ReplaceRange(size_t start, size_t end, const std::wstring& text) {
    SendMessageW(g_edit, EM_SETSEL, static_cast<WPARAM>(start), static_cast<LPARAM>(end));
    InsertAtCaret(text);
}

std::wstring Trim(std::wstring_view text) {
    size_t first = 0;
    while (first < text.size() && std::iswspace(text[first])) {
        ++first;
    }

    size_t last = text.size();
    while (last > first && std::iswspace(text[last - 1])) {
        --last;
    }

    return std::wstring(text.substr(first, last - first));
}

LineRange GetActiveLine(std::wstring_view text, size_t caret) {
    if (caret > text.size()) {
        caret = text.size();
    }

    LineRange line;
    line.start = caret;
    while (line.start > 0 && text[line.start - 1] != L'\r' && text[line.start - 1] != L'\n') {
        --line.start;
    }

    line.end = caret;
    while (line.end < text.size() && text[line.end] != L'\r' && text[line.end] != L'\n') {
        ++line.end;
    }

    line.after_line_break = line.end;
    if (line.after_line_break < text.size() && text[line.after_line_break] == L'\r') {
        ++line.after_line_break;
    }
    if (line.after_line_break < text.size() && text[line.after_line_break] == L'\n') {
        ++line.after_line_break;
    }

    line.text = std::wstring(text.substr(line.start, line.end - line.start));
    return line;
}

std::wstring ExpressionFromLine(const std::wstring& line) {
    std::wstring expression = Trim(line);
    if (!expression.empty() && expression.front() == L'=') {
        expression = Trim(std::wstring_view(expression).substr(1));
    }
    if (!expression.empty() && expression.front() == L'!') {
        return L"";
    }
    return expression;
}

bool WideToUtf8(std::wstring_view text, std::string& output) {
    output.clear();
    if (text.empty()) {
        return true;
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                            nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return false;
    }

    output.assign(static_cast<size_t>(required), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           output.data(), required, nullptr, nullptr);
    return written == required;
}

void ClearEditor() {
    g_internal_edit = true;
    SetWindowTextW(g_edit, L"");
    g_internal_edit = false;
    SetFocus(g_edit);
}

void InsertScientificResult(HWND hwnd) {
    DWORD selection_start = 0;
    DWORD selection_end = 0;
    SendMessageW(g_edit, EM_GETSEL, reinterpret_cast<WPARAM>(&selection_start),
                 reinterpret_cast<LPARAM>(&selection_end));

    const std::wstring text = GetWindowTextString(g_edit);
    if (selection_start != selection_end) {
        const std::wstring expression =
            Trim(std::wstring_view(text).substr(selection_start, selection_end - selection_start));
        const textcalc::Evaluation evaluation = textcalc::EvaluateExpressionScientific(expression);
        if (!evaluation.ok) {
            MessageBoxW(hwnd, evaluation.error.c_str(), L"Scientific notation", MB_ICONERROR);
            SetFocus(g_edit);
            return;
        }

        ReplaceRange(selection_start, selection_end, evaluation.output);
        SetFocus(g_edit);
        return;
    }

    const LineRange line = GetActiveLine(text, selection_start);
    const std::wstring expression = ExpressionFromLine(line.text);
    if (expression.empty()) {
        SetFocus(g_edit);
        return;
    }

    const textcalc::Evaluation evaluation = textcalc::EvaluateExpressionScientific(expression);
    const std::wstring result =
        evaluation.ok ? L"= " + evaluation.output + L"\r\n" : L"! " + evaluation.error + L"\r\n";
    const bool line_has_break = line.after_line_break > line.end;
    ReplaceRange(line_has_break ? line.after_line_break : line.end,
                 line_has_break ? line.after_line_break : line.end,
                 line_has_break ? result : L"\r\n" + result);
    SetFocus(g_edit);
}

void SaveEditor(HWND hwnd) {
    wchar_t path[MAX_PATH] = L"text-calculator.txt";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd;
    dialog.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"txt";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&dialog)) {
        SetFocus(g_edit);
        return;
    }

    std::string utf8_text;
    if (!WideToUtf8(GetWindowTextString(g_edit), utf8_text)) {
        MessageBoxW(hwnd, L"Failed to encode the editor text as UTF-8.", L"Save", MB_ICONERROR);
        SetFocus(g_edit);
        return;
    }

    std::ofstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) {
        MessageBoxW(hwnd, L"Failed to open the selected file for writing.", L"Save",
                    MB_ICONERROR);
        SetFocus(g_edit);
        return;
    }

    file.write(utf8_text.data(), static_cast<std::streamsize>(utf8_text.size()));
    if (!file) {
        MessageBoxW(hwnd, L"Failed to write the selected file.", L"Save", MB_ICONERROR);
        SetFocus(g_edit);
        return;
    }

    MessageBoxW(hwnd, L"Saved.", L"Save", MB_OK);
    SetFocus(g_edit);
}

void ShowHelp(HWND hwnd) {
    MessageBoxW(hwnd,
                L"Type one expression per line and press Enter to insert a result.\n\n"
                L"Examples:\n"
                L"1+2*3, 2^8, sqrt(16), root(3,27)\n"
                L"log(100), log(2,8), ln(e)\n"
                L"sin(pi/2), cos(0), tan(pi/4)\n"
                L"10%3, mod(-1,5), rem(-5,3)\n"
                L"3.3k/1.1k, 2M, 2m, 1u+500n\n\n"
                L"LaTeX:\n"
                L"\\frac{1}{2}, \\sqrt[3]{27}, \\sin{\\pi/2}, 1\\mu, 5 \\bmod 2\n\n"
                L"Buttons:\n"
                L"Clear deletes all text. Sci inserts scientific notation for the active line "
                L"or replaces the selection. Save writes the editor text to a UTF-8 file.\n\n"
                L"Trigonometric functions use radians. Unit suffixes are case-insensitive except "
                L"M is mega and m is milli.",
                L"Text Calculator Help", MB_OK);
    SetFocus(g_edit);
}

void CreateButton(HWND hwnd, HINSTANCE instance, int id, const wchar_t* text) {
    CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0,
                    0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance,
                    nullptr);
}

void LayoutChildren(HWND hwnd, int width, int height) {
    const int margin = 8;
    const int button_width = 82;
    const int button_height = 28;
    const int y = 8;

    int x = margin;
    const int button_ids[] = {kClearButtonId, kScientificButtonId, kSaveButtonId, kHelpButtonId};
    for (int id : button_ids) {
        HWND button = GetDlgItem(hwnd, id);
        if (button != nullptr) {
            MoveWindow(button, x, y, button_width, button_height, TRUE);
        }
        x += button_width + margin;
    }

    if (g_edit != nullptr) {
        const int editor_height = height > kToolbarHeight ? height - kToolbarHeight : 0;
        MoveWindow(g_edit, 0, kToolbarHeight, width, editor_height, TRUE);
    }
}

void EvaluateLineAfterEnter() {
    DWORD selection_start = 0;
    DWORD selection_end = 0;
    SendMessageW(g_edit, EM_GETSEL, reinterpret_cast<WPARAM>(&selection_start),
                 reinterpret_cast<LPARAM>(&selection_end));

    if (selection_start != selection_end || selection_start == 0) {
        return;
    }

    const std::wstring text = GetWindowTextString(g_edit);
    const textcalc::EnterInsertion insertion =
        textcalc::BuildEnterInsertion(text, static_cast<size_t>(selection_start));
    if (insertion.should_insert) {
        InsertAtCaret(insertion.text);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            g_edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL |
                    ES_WANTRETURN,
                0, 0, 0, 0, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditId)),
                reinterpret_cast<LPCREATESTRUCTW>(lparam)->hInstance, nullptr);

            HINSTANCE instance = reinterpret_cast<LPCREATESTRUCTW>(lparam)->hInstance;
            CreateButton(hwnd, instance, kClearButtonId, L"Clear");
            CreateButton(hwnd, instance, kScientificButtonId, L"Sci");
            CreateButton(hwnd, instance, kSaveButtonId, L"Save");
            CreateButton(hwnd, instance, kHelpButtonId, L"Help");

            g_font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 FIXED_PITCH | FF_MODERN, L"Consolas");
            SendMessageW(g_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
            SetFocus(g_edit);
            return 0;
        }

        case WM_SIZE:
            LayoutChildren(hwnd, LOWORD(lparam), HIWORD(lparam));
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case kClearButtonId:
                    if (HIWORD(wparam) == BN_CLICKED) {
                        ClearEditor();
                    }
                    return 0;
                case kScientificButtonId:
                    if (HIWORD(wparam) == BN_CLICKED) {
                        InsertScientificResult(hwnd);
                    }
                    return 0;
                case kSaveButtonId:
                    if (HIWORD(wparam) == BN_CLICKED) {
                        SaveEditor(hwnd);
                    }
                    return 0;
                case kHelpButtonId:
                    if (HIWORD(wparam) == BN_CLICKED) {
                        ShowHelp(hwnd);
                    }
                    return 0;
                default:
                    break;
            }

            if (reinterpret_cast<HWND>(lparam) == g_edit && HIWORD(wparam) == EN_CHANGE &&
                !g_internal_edit) {
                EvaluateLineAfterEnter();
            }
            return 0;

        case WM_SETFOCUS:
            if (g_edit != nullptr) {
                SetFocus(g_edit);
            }
            return 0;

        case WM_DESTROY:
            if (g_font != nullptr) {
                DeleteObject(g_font);
                g_font = nullptr;
            }
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int command_show) {
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClass;
    window_class.hCursor = LoadCursor(nullptr, IDC_IBEAM);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (RegisterClassW(&window_class) == 0) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Text Calculator", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, kWindowClass, L"Text Calculator", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 760, 520, nullptr, nullptr, instance,
                                nullptr);
    if (hwnd == nullptr) {
        MessageBoxW(nullptr, L"Failed to create the main window.", L"Text Calculator", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, command_show);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
