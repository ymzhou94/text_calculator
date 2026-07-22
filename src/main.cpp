#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef _RICHEDIT_VER
#define _RICHEDIT_VER 0x0800
#endif

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>

#include <algorithm>
#include <cstddef>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "editor_logic.h"
#include "expression_engine.h"

namespace {

constexpr wchar_t kWindowClass[] = L"TextCalculatorWindow";
constexpr int kAppIconResourceId = 1;
constexpr int kEditIdBase = 1;
constexpr int kOpenButtonId = 100;
constexpr int kClearButtonId = 101;
constexpr int kScientificButtonId = 102;
constexpr int kSaveButtonId = 103;
constexpr int kHelpButtonId = 104;
constexpr int kSaveAsCommandId = 105;
constexpr int kNewTabButtonId = 106;
constexpr int kCloseTabButtonId = 107;
constexpr int kNextTabCommandId = 108;
constexpr int kTabControlId = 109;
constexpr int kReferenceButtonIdBase = 2000;
constexpr UINT kEvaluateEnterMessage = WM_APP + 1;
constexpr UINT kRefreshReferencesMessage = WM_APP + 2;
constexpr wchar_t kInlineResultSeparator[] = L"    = ";
constexpr int kHeaderHeight = 132;
constexpr int kFooterHeight = 54;
constexpr int kOuterMargin = 16;
constexpr int kCardInset = 14;
constexpr int kMinWindowWidth = 680;
constexpr int kMinWindowHeight = 440;

const COLORREF kBackgroundColor = RGB(241, 243, 245);
const COLORREF kCardColor = RGB(255, 255, 255);
const COLORREF kTitleColor = RGB(24, 36, 49);
const COLORREF kMutedColor = RGB(75, 85, 99);
const COLORREF kSubtleColor = RGB(107, 114, 128);
const COLORREF kAccentColor = RGB(10, 89, 247);
const COLORREF kAccentSoftColor = RGB(232, 240, 255);
const COLORREF kButtonPressedColor = RGB(224, 230, 238);
const COLORREF kResultColor = RGB(10, 89, 247);
const COLORREF kErrorColor = RGB(196, 43, 28);
const IID kTextDocumentIid = {
    0x8cc497c0, 0xa1df, 0x11ce, {0x80, 0x98, 0x00, 0xaa, 0x00, 0x47, 0xbe, 0x5d}};

HWND g_edit = nullptr;
HWND g_tab_control = nullptr;
HMODULE g_richedit_module = nullptr;
WNDPROC g_original_edit_proc = nullptr;
ITextDocument* g_text_document = nullptr;
HFONT g_edit_font = nullptr;
HFONT g_title_font = nullptr;
HFONT g_meta_font = nullptr;
HFONT g_button_font = nullptr;
HBRUSH g_background_brush = nullptr;
HBRUSH g_card_brush = nullptr;
bool g_internal_edit = false;
bool g_styling_generated_lines = false;
UINT g_dpi = 96;
int g_toolbar_start_x = 0;
RECT g_edit_card = {};
std::wstring g_status_text;
HWND g_pending_reference_refresh_edit = nullptr;

struct GeneratedLine {
    size_t line_start = 0;
    size_t start = 0;
    size_t end = 0;
    bool is_result = false;
    std::wstring value;

    bool operator==(const GeneratedLine&) const = default;
};

struct EditorTab {
    HWND edit = nullptr;
    ITextDocument* text_document = nullptr;
    std::filesystem::path path;
    std::wstring saved_text;
    std::vector<GeneratedLine> generated_lines;
    bool dirty = false;
};

std::vector<EditorTab> g_tabs;
size_t g_active_tab = static_cast<size_t>(-1);
int g_next_edit_id = kEditIdBase;

struct ReferenceButton {
    HWND hwnd = nullptr;
    int id = 0;
    std::wstring value;
};

std::vector<ReferenceButton> g_reference_buttons;

struct LineRange {
    size_t start = 0;
    size_t end = 0;
    size_t after_line_break = 0;
    std::wstring text;
};

int Scale(int logical_pixels) {
    return MulDiv(logical_pixels, static_cast<int>(g_dpi), 96);
}

EditorTab& ActiveTab() {
    return g_tabs[g_active_tab];
}

std::wstring TabLabel(size_t index) {
    const EditorTab& tab = g_tabs[index];
    std::wstring label =
        tab.path.empty() ? L"Untitled " + std::to_wstring(index + 1) : tab.path.filename().wstring();
    if (tab.dirty) {
        label += L" *";
    }
    return label;
}

std::wstring GetWindowTextString(HWND hwnd) {
    GETTEXTLENGTHEX length_options{};
    length_options.flags = GTL_PRECISE | GTL_NUMCHARS;
    length_options.codepage = 1200;
    const LRESULT length =
        SendMessageW(hwnd, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&length_options), 0);
    if (length <= 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1);
    GETTEXTEX text_options{};
    text_options.cb = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
    text_options.flags = GT_DEFAULT;
    text_options.codepage = 1200;
    const LRESULT written = SendMessageW(hwnd, EM_GETTEXTEX,
                                         reinterpret_cast<WPARAM>(&text_options),
                                         reinterpret_cast<LPARAM>(buffer.data()));
    return std::wstring(buffer.data(), static_cast<size_t>(written));
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

std::vector<GeneratedLine> ScanGeneratedLines(std::wstring_view text) {
    std::vector<GeneratedLine> lines;
    size_t line_start = 0;

    while (line_start < text.size()) {
        size_t line_end = line_start;
        while (line_end < text.size() && text[line_end] != L'\r' && text[line_end] != L'\n') {
            ++line_end;
        }

        const std::wstring_view raw_line = text.substr(line_start, line_end - line_start);
        const size_t inline_result = raw_line.find(kInlineResultSeparator);
        const std::wstring line = Trim(raw_line);
        if (inline_result != std::wstring_view::npos &&
            !Trim(raw_line.substr(0, inline_result)).empty()) {
            GeneratedLine generated;
            generated.line_start = line_start;
            generated.start = line_start + inline_result;
            generated.end = line_end;
            generated.is_result = true;
            generated.value = Trim(raw_line.substr(
                inline_result + std::wstring_view(kInlineResultSeparator).size()));
            if (!generated.value.empty()) {
                lines.push_back(std::move(generated));
            }
        } else if (!line.empty() && (line.front() == L'=' || line.front() == L'!')) {
            GeneratedLine generated;
            generated.line_start = line_start;
            generated.start = line_start;
            generated.end = line_end;
            generated.is_result = line.front() == L'=';
            if (generated.is_result) {
                generated.value = Trim(std::wstring_view(line).substr(1));
            }
            lines.push_back(std::move(generated));
        }

        line_start = line_end;
        if (line_start < text.size() && text[line_start] == L'\r') {
            ++line_start;
        }
        if (line_start < text.size() && text[line_start] == L'\n') {
            ++line_start;
        }
    }

    return lines;
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
    const size_t inline_result = expression.find(kInlineResultSeparator);
    if (inline_result != std::wstring::npos) {
        expression = Trim(std::wstring_view(expression).substr(0, inline_result));
    }
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

bool Utf8ToWide(std::string_view text, std::wstring& output) {
    output.clear();
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }
    if (text.empty()) {
        return true;
    }
    if (text.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }

    const int size = static_cast<int>(text.size());
    const int required =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
    if (required <= 0) {
        return false;
    }

    output.resize(static_cast<size_t>(required));
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, output.data(),
                               required) == required;
}

std::wstring NormalizeLineEndings(std::wstring_view text) {
    std::wstring normalized;
    normalized.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') {
                ++i;
            }
            normalized += L"\r\n";
        } else if (text[i] == L'\n') {
            normalized += L"\r\n";
        } else {
            normalized.push_back(text[i]);
        }
    }
    return normalized;
}

HFONT CreateUiFont(int point_size, int weight, const wchar_t* family) {
    const int height = -MulDiv(point_size, static_cast<int>(g_dpi), 72);

    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, family);
}

void CreateUiResources() {
    g_background_brush = CreateSolidBrush(kBackgroundColor);
    g_card_brush = CreateSolidBrush(kCardColor);
    g_edit_font = CreateUiFont(14, FW_NORMAL, L"Consolas");
    g_title_font = CreateUiFont(22, FW_BOLD, L"Segoe UI");
    g_meta_font = CreateUiFont(10, FW_NORMAL, L"Segoe UI");
    g_button_font = CreateUiFont(10, FW_NORMAL, L"Segoe UI");
}

void DeleteFont(HFONT& font) {
    if (font != nullptr) {
        DeleteObject(font);
        font = nullptr;
    }
}

void DeleteBrush(HBRUSH& brush) {
    if (brush != nullptr) {
        DeleteObject(brush);
        brush = nullptr;
    }
}

void DestroyUiResources() {
    DeleteFont(g_edit_font);
    DeleteFont(g_title_font);
    DeleteFont(g_meta_font);
    DeleteFont(g_button_font);
    DeleteBrush(g_background_brush);
    DeleteBrush(g_card_brush);
}

void ApplyUiFonts() {
    for (const EditorTab& tab : g_tabs) {
        SendMessageW(tab.edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_edit_font), TRUE);
        SendMessageW(tab.edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(Scale(4), Scale(84)));
    }

    if (g_tab_control != nullptr) {
        SendMessageW(g_tab_control, WM_SETFONT, reinterpret_cast<WPARAM>(g_button_font), TRUE);
        TabCtrl_SetItemSize(g_tab_control, Scale(150), Scale(32));
    }

    const int button_ids[] = {kOpenButtonId, kSaveButtonId, kScientificButtonId,
                              kClearButtonId, kHelpButtonId, kNewTabButtonId,
                              kCloseTabButtonId};
    for (int id : button_ids) {
        HWND button = GetDlgItem(GetParent(g_edit), id);
        if (button != nullptr) {
            SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_button_font), TRUE);
        }
    }
}

void UpdateTabLabels() {
    if (g_tab_control == nullptr) {
        return;
    }

    for (size_t i = 0; i < g_tabs.size(); ++i) {
        std::wstring label = TabLabel(i);
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = label.data();
        TabCtrl_SetItem(g_tab_control, static_cast<int>(i), &item);
    }
    InvalidateRect(g_tab_control, nullptr, TRUE);
}

void UpdateWindowTitle(HWND hwnd) {
    const EditorTab& tab = ActiveTab();
    std::wstring title = tab.path.empty() ? L"Untitled" : tab.path.filename().wstring();
    if (tab.dirty) {
        title += L" *";
    }
    title += L" - Text Calculator";
    SetWindowTextW(hwnd, title.c_str());
    UpdateTabLabels();
}

void RefreshDirtyState(HWND hwnd) {
    EditorTab& tab = ActiveTab();
    const bool dirty = GetWindowTextString(g_edit) != tab.saved_text;
    if (dirty == tab.dirty) {
        return;
    }
    tab.dirty = dirty;
    UpdateWindowTitle(hwnd);
}

void SetStatus(HWND hwnd, std::wstring text) {
    g_status_text = std::move(text);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void ClearEditor(HWND hwnd) {
    if (GetWindowTextLengthW(g_edit) == 0) {
        SetFocus(g_edit);
        return;
    }

    g_internal_edit = true;
    SendMessageW(g_edit, EM_SETSEL, 0, -1);
    SendMessageW(g_edit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L""));
    g_internal_edit = false;
    SetStatus(hwnd, L"Cleared - Ctrl+Z to undo");
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
        SetStatus(hwnd, L"Converted selection to scientific notation");
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
    if (!evaluation.ok) {
        MessageBoxW(hwnd, evaluation.error.c_str(), L"Scientific notation", MB_ICONERROR);
        SetFocus(g_edit);
        return;
    }

    ReplaceRange(line.start, line.end,
                 expression + kInlineResultSeparator + evaluation.output);
    SetStatus(hwnd, L"Scientific result updated");
    SetFocus(g_edit);
}

bool ChooseSavePath(HWND hwnd, std::filesystem::path& selected_path) {
    wchar_t path[MAX_PATH] = {};
    const EditorTab& tab = ActiveTab();
    const std::wstring initial =
        tab.path.empty() ? L"text-calculator.txt" : tab.path.wstring();
    const size_t length = (std::min)(initial.size(), static_cast<size_t>(MAX_PATH - 1));
    std::copy_n(initial.data(), length, path);

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd;
    dialog.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"txt";
    dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&dialog)) {
        return false;
    }
    selected_path = std::filesystem::path(path);
    return true;
}

bool SaveEditor(HWND hwnd, bool save_as = false) {
    EditorTab& tab = ActiveTab();
    std::filesystem::path path = tab.path;
    if (save_as || path.empty()) {
        if (!ChooseSavePath(hwnd, path)) {
            SetFocus(g_edit);
            return false;
        }
    }

    const std::wstring editor_text = GetWindowTextString(g_edit);
    std::string utf8_text;
    if (!WideToUtf8(NormalizeLineEndings(editor_text), utf8_text)) {
        MessageBoxW(hwnd, L"Failed to encode the editor text as UTF-8.", L"Save", MB_ICONERROR);
        SetFocus(g_edit);
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        MessageBoxW(hwnd, L"Failed to open the selected file for writing.", L"Save",
                    MB_ICONERROR);
        SetFocus(g_edit);
        return false;
    }

    file.write(utf8_text.data(), static_cast<std::streamsize>(utf8_text.size()));
    if (!file) {
        MessageBoxW(hwnd, L"Failed to write the selected file.", L"Save", MB_ICONERROR);
        SetFocus(g_edit);
        return false;
    }

    tab.path = path;
    tab.saved_text = editor_text;
    RefreshDirtyState(hwnd);
    SetStatus(hwnd, L"Saved " + path.filename().wstring());
    SetFocus(g_edit);
    return true;
}

bool ConfirmSaveChanges(HWND hwnd) {
    if (!ActiveTab().dirty) {
        return true;
    }

    const int choice = MessageBoxW(hwnd, L"Save changes before continuing?", L"Text Calculator",
                                   MB_ICONWARNING | MB_YESNOCANCEL);
    if (choice == IDCANCEL) {
        return false;
    }
    if (choice == IDYES) {
        return SaveEditor(hwnd);
    }
    return true;
}

void OpenEditor(HWND hwnd) {
    if (!ConfirmSaveChanges(hwnd)) {
        SetFocus(g_edit);
        return;
    }

    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd;
    dialog.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&dialog)) {
        SetFocus(g_edit);
        return;
    }

    const std::filesystem::path selected_path(path);
    std::ifstream file(selected_path, std::ios::binary);
    if (!file) {
        MessageBoxW(hwnd, L"Failed to open the selected file.", L"Open", MB_ICONERROR);
        SetFocus(g_edit);
        return;
    }

    const std::string bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    if (file.bad()) {
        MessageBoxW(hwnd, L"Failed to read the selected file.", L"Open", MB_ICONERROR);
        SetFocus(g_edit);
        return;
    }

    std::wstring decoded;
    if (!Utf8ToWide(bytes, decoded)) {
        MessageBoxW(hwnd, L"The selected file is not valid UTF-8.", L"Open", MB_ICONERROR);
        SetFocus(g_edit);
        return;
    }

    const std::wstring normalized = NormalizeLineEndings(decoded);
    g_internal_edit = true;
    SetWindowTextW(g_edit, normalized.c_str());
    SendMessageW(g_edit, EM_EMPTYUNDOBUFFER, 0, 0);
    g_internal_edit = false;

    EditorTab& tab = ActiveTab();
    tab.path = selected_path;
    tab.saved_text = GetWindowTextString(g_edit);
    RefreshDirtyState(hwnd);
    SetStatus(hwnd, L"Opened " + selected_path.filename().wstring());
    SetFocus(g_edit);
}

void ShowHelp(HWND hwnd) {
    MessageBoxW(hwnd,
                L"Enter always inserts a normal text newline. If the completed line contains "
                L"only a supported expression, its result is added to the same line. Other text "
                L"is left unchanged.\n\n"
                L"Examples:\n"
                L"1+2*3, 2^8, sqrt(16), root(3,27)\n"
                L"log(100), log(2,8), ln(e)\n"
                L"sin(pi/2), cos(0), tan(pi/4)\n"
                L"10%3, mod(-1,5), rem(-5,3)\n"
                L"3.3k/1.1k, 2M, 2m, 1u+500n\n\n"
                L"LaTeX:\n"
                L"\\frac{1}{2}, \\sqrt[3]{27}, \\sin{\\pi/2}, 1\\mu, 5 \\bmod 2\n\n"
                L"Files and shortcuts:\n"
                L"Ctrl+O Open, Ctrl+S Save, Ctrl+Shift+S Save As\n"
                L"Ctrl+T New tab, Ctrl+W Close tab, Ctrl+Tab Next tab\n"
                L"Ctrl+E Scientific notation, Ctrl+Shift+Delete Clear, F1 Help\n"
                L"Each tab keeps its own text, file and undo history. Clear can be undone with "
                L"Ctrl+Z. Files are read and written as UTF-8.\n\n"
                L"Click 引用 beside a result to insert that value at the current caret. The "
                L"button is not written into the text file.\n\n"
                L"Trigonometric functions use radians. Unit suffixes are case-insensitive except "
                L"M is mega and m is milli. Supported suffixes: t, g, k, M, m, u, n, p, f.",
                L"Text Calculator Help", MB_OK);
    SetFocus(g_edit);
}

HWND CreateButton(HWND hwnd, HINSTANCE instance, int id, const wchar_t* text,
                  bool visible = true) {
    const DWORD visibility_style = visible ? WS_VISIBLE : 0;
    HWND button =
        CreateWindowExW(0, L"BUTTON", text,
                        WS_CHILD | visibility_style | WS_TABSTOP | WS_CLIPSIBLINGS |
                            BS_PUSHBUTTON | BS_OWNERDRAW,
                        0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_button_font), TRUE);
    return button;
}

void SetTextColorForRange(size_t start, size_t end, COLORREF color) {
    CHARRANGE range{static_cast<LONG>(start), static_cast<LONG>(end)};
    SendMessageW(g_edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));

    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = color;
    SendMessageW(g_edit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
}

void StyleGeneratedLines(const std::wstring& text, const std::vector<GeneratedLine>& lines,
                         const std::vector<GeneratedLine>& previous_lines) {
    if (g_edit == nullptr || g_text_document == nullptr || g_styling_generated_lines) {
        return;
    }

    g_styling_generated_lines = true;
    long undo_state = 0;
    if (FAILED(g_text_document->Undo(tomSuspend, &undo_state))) {
        OutputDebugStringW(L"Text Calculator: failed to suspend RichEdit formatting undo.\n");
        g_styling_generated_lines = false;
        return;
    }

    long freeze_count = 0;
    if (FAILED(g_text_document->Freeze(&freeze_count))) {
        OutputDebugStringW(L"Text Calculator: failed to freeze RichEdit display.\n");
        if (FAILED(g_text_document->Undo(tomResume, &undo_state))) {
            OutputDebugStringW(L"Text Calculator: failed to resume RichEdit formatting undo.\n");
        }
        g_styling_generated_lines = false;
        return;
    }

    CHARRANGE selection{};
    SendMessageW(g_edit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const LRESULT first_visible_line = SendMessageW(g_edit, EM_GETFIRSTVISIBLELINE, 0, 0);

    size_t first_changed = 0;
    while (first_changed < previous_lines.size() && first_changed < lines.size() &&
           previous_lines[first_changed] == lines[first_changed]) {
        ++first_changed;
    }

    size_t affected_start = text.size();
    if (previous_lines.empty()) {
        affected_start = 0;
    } else {
        if (first_changed < previous_lines.size()) {
            affected_start = (std::min)(affected_start, previous_lines[first_changed].start);
        }
        if (first_changed < lines.size()) {
            affected_start = (std::min)(affected_start, lines[first_changed].start);
        }
    }
    affected_start = (std::min)(affected_start, text.size());
    if (affected_start < text.size()) {
        SetTextColorForRange(affected_start, text.size(), kTitleColor);
    }
    for (size_t i = first_changed; i < lines.size(); ++i) {
        const GeneratedLine& line = lines[i];
        SetTextColorForRange(line.start, line.end,
                             line.is_result ? kResultColor : kErrorColor);
    }

    SendMessageW(g_edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    if (selection.cpMin == selection.cpMax) {
        COLORREF insertion_color = kTitleColor;
        for (const GeneratedLine& line : lines) {
            if (selection.cpMin >= static_cast<LONG>(line.start) &&
                selection.cpMin <= static_cast<LONG>(line.end)) {
                insertion_color = line.is_result ? kResultColor : kErrorColor;
                break;
            }
        }

        CHARFORMAT2W format{};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR;
        format.crTextColor = insertion_color;
        SendMessageW(g_edit, EM_SETCHARFORMAT, SCF_SELECTION,
                     reinterpret_cast<LPARAM>(&format));
    }

    const LRESULT new_first_visible_line = SendMessageW(g_edit, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (new_first_visible_line != first_visible_line) {
        SendMessageW(g_edit, EM_LINESCROLL, 0, first_visible_line - new_first_visible_line);
    }
    if (FAILED(g_text_document->Unfreeze(&freeze_count))) {
        OutputDebugStringW(L"Text Calculator: failed to unfreeze RichEdit display.\n");
    }
    if (FAILED(g_text_document->Undo(tomResume, &undo_state))) {
        OutputDebugStringW(L"Text Calculator: failed to resume RichEdit formatting undo.\n");
    }
    g_styling_generated_lines = false;
}

void DestroyReferenceButtons() {
    for (const ReferenceButton& button : g_reference_buttons) {
        if (button.hwnd != nullptr) {
            DestroyWindow(button.hwnd);
        }
    }
    g_reference_buttons.clear();
}

void UpdateReferenceButtons(HWND hwnd, const std::vector<GeneratedLine>& lines) {
    if (g_edit == nullptr) {
        return;
    }

    RECT edit_rect{};
    GetWindowRect(g_edit, &edit_rect);
    MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT*>(&edit_rect), 2);

    RECT edit_client{};
    GetClientRect(g_edit, &edit_client);
    const int button_width = Scale(58);
    const int button_height = Scale(20);
    const int button_x = edit_rect.right - button_width - Scale(20);

    struct DesiredReferenceButton {
        int y = 0;
        std::wstring value;
    };
    std::vector<DesiredReferenceButton> desired_buttons;

    for (const GeneratedLine& line : lines) {
        if (!line.is_result || line.value.empty()) {
            continue;
        }

        POINTL position{};
        SendMessageW(g_edit, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&position),
                     static_cast<LPARAM>(line.line_start));
        if (position.y < 0 || position.y + button_height > edit_client.bottom) {
            continue;
        }

        desired_buttons.push_back(
            {edit_rect.top + static_cast<int>(position.y) + Scale(2), line.value});
    }

    while (g_reference_buttons.size() < desired_buttons.size()) {
        const int id = kReferenceButtonIdBase + static_cast<int>(g_reference_buttons.size());
        HWND button = CreateButton(hwnd, GetModuleHandleW(nullptr), id, L"引用", false);
        if (button == nullptr) {
            OutputDebugStringW(L"Text Calculator: failed to create a reference button.\n");
            break;
        }
        g_reference_buttons.push_back({button, id, L""});
    }

    const size_t active_button_count =
        (std::min)(g_reference_buttons.size(), desired_buttons.size());
    for (size_t i = 0; i < active_button_count; ++i) {
        ReferenceButton& button = g_reference_buttons[i];
        const DesiredReferenceButton& desired = desired_buttons[i];
        button.value = desired.value;

        RECT current{};
        GetWindowRect(button.hwnd, &current);
        MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT*>(&current), 2);
        if (current.left != button_x || current.top != desired.y ||
            current.right - current.left != button_width ||
            current.bottom - current.top != button_height) {
            SetWindowPos(button.hwnd, nullptr, button_x, desired.y, button_width, button_height,
                         SWP_NOACTIVATE | SWP_NOZORDER);
        }
        if (!IsWindowVisible(button.hwnd)) {
            SetWindowPos(button.hwnd, HWND_TOP, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    }

    while (g_reference_buttons.size() > desired_buttons.size()) {
        ReferenceButton& button = g_reference_buttons.back();
        DestroyWindow(button.hwnd);
        g_reference_buttons.pop_back();
    }
}

void UpdateReferenceButtons(HWND hwnd) {
    if (g_active_tab < g_tabs.size()) {
        UpdateReferenceButtons(hwnd, ActiveTab().generated_lines);
    }
}

void RefreshGeneratedLineUi(HWND hwnd) {
    if (g_edit == nullptr || g_active_tab >= g_tabs.size() || g_styling_generated_lines) {
        return;
    }

    const std::wstring text = GetWindowTextString(g_edit);
    std::vector<GeneratedLine> lines = ScanGeneratedLines(text);
    EditorTab& tab = ActiveTab();
    if (lines != tab.generated_lines) {
        StyleGeneratedLines(text, lines, tab.generated_lines);
        tab.generated_lines = std::move(lines);
        UpdateReferenceButtons(hwnd, tab.generated_lines);
    }
}

void QueueReferenceButtonRefresh(HWND edit) {
    if (g_pending_reference_refresh_edit == edit) {
        return;
    }
    g_pending_reference_refresh_edit = edit;
    PostMessageW(GetParent(edit), kRefreshReferencesMessage,
                 reinterpret_cast<WPARAM>(edit), 0);
}

void InsertReferencedResult(HWND hwnd, int id) {
    for (const ReferenceButton& button : g_reference_buttons) {
        if (button.id != id) {
            continue;
        }

        const std::wstring value = button.value;
        InsertAtCaret(value);
        SetStatus(hwnd, L"Referenced " + value);
        SetFocus(g_edit);
        return;
    }
}

void LayoutChildren(HWND hwnd, int width, int height) {
    const int button_width = Scale(68);
    const int button_height = Scale(36);
    const int button_gap = Scale(8);
    const int button_y = Scale(24);
    const int button_count = 5;
    const int button_total = button_count * button_width + (button_count - 1) * button_gap;

    int x = width - Scale(kOuterMargin) - button_total;
    g_toolbar_start_x = x;
    const int button_ids[] = {kOpenButtonId, kSaveButtonId, kScientificButtonId,
                              kClearButtonId, kHelpButtonId};
    for (int id : button_ids) {
        HWND button = GetDlgItem(hwnd, id);
        if (button != nullptr) {
            MoveWindow(button, x, button_y, button_width, button_height, TRUE);
        }
        x += button_width + button_gap;
    }

    const int margin = Scale(kOuterMargin);
    const int tab_button_size = Scale(32);
    const int tab_gap = Scale(6);
    const int tab_y = Scale(88);
    const int tab_height = Scale(34);
    const int close_x = width - margin - tab_button_size;
    const int add_x = close_x - tab_gap - tab_button_size;
    if (g_tab_control != nullptr) {
        MoveWindow(g_tab_control, margin, tab_y,
                   (std::max)(0, add_x - tab_gap - margin), tab_height, TRUE);
    }
    HWND add_button = GetDlgItem(hwnd, kNewTabButtonId);
    if (add_button != nullptr) {
        MoveWindow(add_button, add_x, tab_y, tab_button_size, tab_button_size, TRUE);
    }
    HWND close_button = GetDlgItem(hwnd, kCloseTabButtonId);
    if (close_button != nullptr) {
        MoveWindow(close_button, close_x, tab_y, tab_button_size, tab_button_size, TRUE);
    }

    const int card_inset = Scale(kCardInset);
    const int card_bottom =
        (std::max)(Scale(kHeaderHeight + 80), height - Scale(kFooterHeight));
    g_edit_card = {margin, Scale(kHeaderHeight), (std::max)(margin, width - margin), card_bottom};

    if (!g_tabs.empty()) {
        const LONG raw_edit_width = g_edit_card.right - g_edit_card.left - card_inset * 2;
        const LONG raw_edit_height = g_edit_card.bottom - g_edit_card.top - card_inset * 2;
        const int edit_width = raw_edit_width > 0 ? static_cast<int>(raw_edit_width) : 0;
        const int edit_height = raw_edit_height > 0 ? static_cast<int>(raw_edit_height) : 0;
        for (const EditorTab& tab : g_tabs) {
            MoveWindow(tab.edit, g_edit_card.left + card_inset, g_edit_card.top + card_inset,
                       edit_width, edit_height, TRUE);
        }
    }

    UpdateReferenceButtons(hwnd);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void FillRoundRect(HDC hdc, RECT rect, COLORREF fill, COLORREF border, int radius) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_brush = SelectObject(hdc, brush);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawTextInRect(HDC hdc, RECT rect, const wchar_t* text, HFONT font, COLORREF color,
                    UINT format) {
    HGDIOBJ old_font = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, text, -1, &rect, format);
    SelectObject(hdc, old_font);
}

void DrawStatusPill(HDC hdc, int x, int y, int width, const wchar_t* text) {
    RECT rect{x, y, x + width, y + Scale(28)};
    FillRoundRect(hdc, rect, kCardColor, kCardColor, Scale(28));
    DrawTextInRect(hdc, rect, text, g_meta_font, kMutedColor,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void PaintWindow(HWND hwnd) {
    PAINTSTRUCT paint{};
    HDC hdc = BeginPaint(hwnd, &paint);

    RECT client{};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, g_background_brush);

    RECT title_rect{Scale(kOuterMargin + 8), Scale(18), g_toolbar_start_x - Scale(16),
                    Scale(48)};
    DrawTextInRect(hdc, title_rect, L"Text Calculator", g_title_font, kTitleColor,
                   DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    RECT subtitle_rect{Scale(kOuterMargin + 8), Scale(52), g_toolbar_start_x - Scale(16),
                       Scale(72)};
    DrawTextInRect(hdc, subtitle_rect, L"Plain text / LaTeX", g_meta_font, kSubtleColor,
                   DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    FillRoundRect(hdc, g_edit_card, kCardColor, kCardColor, Scale(24));

    const int pill_y = (std::max)(g_edit_card.bottom + Scale(12), client.bottom - Scale(42));
    int pill_x = Scale(kOuterMargin);
    DrawStatusPill(hdc, pill_x, pill_y, Scale(88), L"Angle: RAD");
    pill_x += Scale(96);
    DrawStatusPill(hdc, pill_x, pill_y, Scale(210), L"Units: t g k M/m u n p f");
    pill_x += Scale(218);
    DrawStatusPill(hdc, pill_x, pill_y, Scale(90), L"Input: Auto");

    RECT status_rect{pill_x + Scale(102), pill_y, client.right - Scale(kOuterMargin),
                     pill_y + Scale(28)};
    DrawTextInRect(hdc, status_rect, g_status_text.c_str(), g_meta_font, kSubtleColor,
                   DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    EndPaint(hwnd, &paint);
}

void DrawOwnerButton(const DRAWITEMSTRUCT& item) {
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool primary = item.CtlID == kSaveButtonId;
    const bool reference = item.CtlID >= kReferenceButtonIdBase;
    const COLORREF background =
        pressed ? kButtonPressedColor : ((primary || reference) ? kAccentSoftColor : kCardColor);
    const COLORREF text_color = (primary || reference) ? kAccentColor : kMutedColor;

    RECT rect = item.rcItem;
    FillRoundRect(item.hDC, rect, background, background, Scale(18));

    wchar_t text[32] = {};
    GetWindowTextW(item.hwndItem, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
    if (pressed) {
        OffsetRect(&rect, Scale(1), Scale(1));
    }
    DrawTextInRect(item.hDC, rect, text, g_button_font, text_color,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if ((item.itemState & ODS_FOCUS) != 0) {
        RECT focus = item.rcItem;
        InflateRect(&focus, -Scale(4), -Scale(4));
        DrawFocusRect(item.hDC, &focus);
    }
}

void DrawOwnerTab(const DRAWITEMSTRUCT& item) {
    if (item.itemID == static_cast<UINT>(-1)) {
        return;
    }

    const bool selected = static_cast<int>(item.itemID) == TabCtrl_GetCurSel(item.hwndItem);
    RECT rect = item.rcItem;
    InflateRect(&rect, -Scale(2), -Scale(2));
    const COLORREF background = selected ? kAccentSoftColor : kCardColor;
    const COLORREF text_color = selected ? kAccentColor : kMutedColor;
    FillRoundRect(item.hDC, rect, background, background, Scale(14));

    wchar_t text[128] = {};
    TCITEMW tab_item{};
    tab_item.mask = TCIF_TEXT;
    tab_item.pszText = text;
    tab_item.cchTextMax = static_cast<int>(sizeof(text) / sizeof(text[0]));
    TabCtrl_GetItem(item.hwndItem, static_cast<int>(item.itemID), &tab_item);
    InflateRect(&rect, -Scale(12), 0);
    DrawTextInRect(item.hDC, rect, text, g_button_font, text_color,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if ((item.itemState & ODS_FOCUS) != 0) {
        RECT focus = item.rcItem;
        InflateRect(&focus, -Scale(5), -Scale(5));
        DrawFocusRect(item.hDC, &focus);
    }
}

size_t ConsumeLineEndingAt(std::wstring_view text, size_t position) {
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

void EvaluateCompletedLineAfterEnter() {
    DWORD selection_start = 0;
    DWORD selection_end = 0;
    SendMessageW(g_edit, EM_GETSEL, reinterpret_cast<WPARAM>(&selection_start),
                 reinterpret_cast<LPARAM>(&selection_end));

    if (selection_start != selection_end || selection_start == 0) {
        return;
    }

    const std::wstring text = GetWindowTextString(g_edit);
    size_t line_end = static_cast<size_t>(selection_start);
    while (line_end > 0 && (text[line_end - 1] == L'\r' || text[line_end - 1] == L'\n')) {
        --line_end;
    }
    if (line_end == static_cast<size_t>(selection_start)) {
        return;
    }

    size_t line_start = line_end;
    while (line_start > 0 && text[line_start - 1] != L'\r' && text[line_start - 1] != L'\n') {
        --line_start;
    }

    const std::wstring_view line(text.data() + line_start, line_end - line_start);
    const size_t separator = line.find(kInlineResultSeparator);
    const std::wstring expression = Trim(separator == std::wstring_view::npos
                                             ? line
                                             : line.substr(0, separator));
    if (expression.empty()) {
        return;
    }

    const textcalc::Evaluation evaluation = textcalc::EvaluateExpression(expression);
    if (!evaluation.ok) {
        return;
    }

    const std::wstring line_ending =
        text.substr(line_end, static_cast<size_t>(selection_start) - line_end);
    size_t replace_end = static_cast<size_t>(selection_start);
    replace_end = ConsumeLineEndingAt(text, replace_end);
    const std::wstring completed_line =
        expression + kInlineResultSeparator + evaluation.output + line_ending;
    ReplaceRange(line_start, replace_end, completed_line);
}

LRESULT CALLBACK RichEditWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    const LRESULT result = CallWindowProcW(g_original_edit_proc, hwnd, message, wparam, lparam);

    if (message == WM_KEYDOWN && wparam == VK_RETURN) {
        PostMessageW(GetParent(hwnd), kEvaluateEnterMessage, reinterpret_cast<WPARAM>(hwnd), 0);
    } else if (message == WM_VSCROLL || message == WM_MOUSEWHEEL) {
        QueueReferenceButtonRefresh(hwnd);
    }
    return result;
}

bool CreateEditorTabControl(HWND hwnd, EditorTab& tab, int id) {
    tab.edit = CreateWindowExW(
        0, MSFTEDIT_CLASS, L"",
        WS_CHILD | WS_VSCROLL | WS_CLIPSIBLINGS | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_WANTRETURN | ES_NOHIDESEL,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    if (tab.edit == nullptr) {
        return false;
    }

    SetLastError(0);
    WNDPROC original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        tab.edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(RichEditWindowProc)));
    if (original == nullptr && GetLastError() != 0) {
        DestroyWindow(tab.edit);
        tab.edit = nullptr;
        return false;
    }
    if (g_original_edit_proc == nullptr) {
        g_original_edit_proc = original;
    }

    SendMessageW(tab.edit, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
    SendMessageW(tab.edit, EM_SETBKGNDCOLOR, 0, kCardColor);
    const LRESULT event_mask = SendMessageW(tab.edit, EM_GETEVENTMASK, 0, 0);
    SendMessageW(tab.edit, EM_SETEVENTMASK, 0, event_mask | ENM_CHANGE | ENM_SCROLL);
    SendMessageW(tab.edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_edit_font), TRUE);
    SendMessageW(tab.edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELPARAM(Scale(4), Scale(84)));

    IRichEditOle* rich_edit_ole = nullptr;
    SendMessageW(tab.edit, EM_GETOLEINTERFACE, 0,
                 reinterpret_cast<LPARAM>(&rich_edit_ole));
    if (rich_edit_ole == nullptr ||
        FAILED(rich_edit_ole->QueryInterface(
            kTextDocumentIid, reinterpret_cast<void**>(&tab.text_document)))) {
        if (rich_edit_ole != nullptr) {
            rich_edit_ole->Release();
        }
        DestroyWindow(tab.edit);
        tab.edit = nullptr;
        return false;
    }
    rich_edit_ole->Release();
    return true;
}

void SwitchToTab(HWND hwnd, size_t index) {
    if (index >= g_tabs.size()) {
        return;
    }

    if (g_active_tab < g_tabs.size()) {
        ShowWindow(g_tabs[g_active_tab].edit, SW_HIDE);
    }
    DestroyReferenceButtons();

    g_active_tab = index;
    g_edit = g_tabs[index].edit;
    g_text_document = g_tabs[index].text_document;
    TabCtrl_SetCurSel(g_tab_control, static_cast<int>(index));
    ShowWindow(g_edit, SW_SHOW);

    RECT client{};
    GetClientRect(hwnd, &client);
    LayoutChildren(hwnd, client.right, client.bottom);
    RefreshGeneratedLineUi(hwnd);
    UpdateWindowTitle(hwnd);
    SetFocus(g_edit);
}

bool AddEditorTab(HWND hwnd) {
    EditorTab tab;
    if (!CreateEditorTabControl(hwnd, tab, g_next_edit_id++)) {
        MessageBoxW(hwnd, L"Failed to create a new editor tab.", L"Text Calculator",
                    MB_ICONERROR);
        return false;
    }

    g_tabs.push_back(std::move(tab));
    const size_t index = g_tabs.size() - 1;
    std::wstring label = TabLabel(index);
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = label.data();
    TabCtrl_InsertItem(g_tab_control, static_cast<int>(index), &item);
    SwitchToTab(hwnd, index);
    SetStatus(hwnd, L"New tab");
    return true;
}

void CloseActiveTab(HWND hwnd) {
    if (!ConfirmSaveChanges(hwnd)) {
        return;
    }

    const size_t closing = g_active_tab;
    DestroyReferenceButtons();
    EditorTab& tab = g_tabs[closing];
    if (tab.text_document != nullptr) {
        tab.text_document->Release();
        tab.text_document = nullptr;
    }
    DestroyWindow(tab.edit);
    TabCtrl_DeleteItem(g_tab_control, static_cast<int>(closing));
    g_tabs.erase(g_tabs.begin() + static_cast<std::ptrdiff_t>(closing));

    g_edit = nullptr;
    g_text_document = nullptr;
    g_active_tab = static_cast<size_t>(-1);
    if (g_tabs.empty()) {
        AddEditorTab(hwnd);
    } else {
        SwitchToTab(hwnd, (std::min)(closing, g_tabs.size() - 1));
    }
    UpdateTabLabels();
}

void SelectNextTab(HWND hwnd) {
    if (g_tabs.size() > 1) {
        SwitchToTab(hwnd, (g_active_tab + 1) % g_tabs.size());
    }
}

bool ConfirmAllTabs(HWND hwnd) {
    for (size_t i = 0; i < g_tabs.size(); ++i) {
        if (!g_tabs[i].dirty) {
            continue;
        }
        SwitchToTab(hwnd, i);
        if (!ConfirmSaveChanges(hwnd)) {
            return false;
        }
    }
    return true;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            g_dpi = GetDpiForWindow(hwnd);
            g_richedit_module = LoadLibraryW(L"Msftedit.dll");
            if (g_richedit_module == nullptr) {
                MessageBoxW(hwnd, L"Failed to load the Windows RichEdit control.",
                            L"Text Calculator", MB_ICONERROR);
                return -1;
            }
            CreateUiResources();

            HINSTANCE instance = reinterpret_cast<LPCREATESTRUCTW>(lparam)->hInstance;
            g_tab_control = CreateWindowExW(
                0, WC_TABCONTROLW, L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | TCS_OWNERDRAWFIXED |
                    TCS_FIXEDWIDTH | TCS_SINGLELINE | TCS_BUTTONS | TCS_FLATBUTTONS,
                0, 0, 0, 0, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTabControlId)), instance, nullptr);
            if (g_tab_control == nullptr) {
                MessageBoxW(hwnd, L"Failed to create the tab control.", L"Text Calculator",
                            MB_ICONERROR);
                return -1;
            }

            CreateButton(hwnd, instance, kOpenButtonId, L"Open");
            CreateButton(hwnd, instance, kSaveButtonId, L"Save");
            CreateButton(hwnd, instance, kScientificButtonId, L"Sci");
            CreateButton(hwnd, instance, kClearButtonId, L"Clear");
            CreateButton(hwnd, instance, kHelpButtonId, L"Help");
            CreateButton(hwnd, instance, kNewTabButtonId, L"+");
            CreateButton(hwnd, instance, kCloseTabButtonId, L"\u00d7");

            if (!AddEditorTab(hwnd)) {
                return -1;
            }

            ApplyUiFonts();
            UpdateWindowTitle(hwnd);
            SetFocus(g_edit);
            return 0;
        }

        case WM_SIZE:
            LayoutChildren(hwnd, LOWORD(lparam), HIWORD(lparam));
            return 0;

        case WM_COMMAND:
            if (LOWORD(wparam) >= kReferenceButtonIdBase && HIWORD(wparam) == BN_CLICKED) {
                InsertReferencedResult(hwnd, LOWORD(wparam));
                return 0;
            }

            switch (LOWORD(wparam)) {
                case kOpenButtonId:
                    if (HIWORD(wparam) == BN_CLICKED || lparam == 0) {
                        OpenEditor(hwnd);
                    }
                    return 0;
                case kClearButtonId:
                    if (HIWORD(wparam) == BN_CLICKED || lparam == 0) {
                        ClearEditor(hwnd);
                    }
                    return 0;
                case kScientificButtonId:
                    if (HIWORD(wparam) == BN_CLICKED || lparam == 0) {
                        InsertScientificResult(hwnd);
                    }
                    return 0;
                case kSaveButtonId:
                    if (HIWORD(wparam) == BN_CLICKED || lparam == 0) {
                        SaveEditor(hwnd);
                    }
                    return 0;
                case kSaveAsCommandId:
                    if (lparam == 0) {
                        SaveEditor(hwnd, true);
                    }
                    return 0;
                case kHelpButtonId:
                    if (HIWORD(wparam) == BN_CLICKED || lparam == 0) {
                        ShowHelp(hwnd);
                    }
                    return 0;
                case kNewTabButtonId:
                    if (HIWORD(wparam) == BN_CLICKED || lparam == 0) {
                        AddEditorTab(hwnd);
                    }
                    return 0;
                case kCloseTabButtonId:
                    if (HIWORD(wparam) == BN_CLICKED || lparam == 0) {
                        CloseActiveTab(hwnd);
                    }
                    return 0;
                case kNextTabCommandId:
                    if (lparam == 0) {
                        SelectNextTab(hwnd);
                    }
                    return 0;
                default:
                    break;
            }

            if (reinterpret_cast<HWND>(lparam) == g_edit && HIWORD(wparam) == EN_CHANGE) {
                RefreshDirtyState(hwnd);
                RefreshGeneratedLineUi(hwnd);
            } else if (reinterpret_cast<HWND>(lparam) == g_edit &&
                       HIWORD(wparam) == EN_VSCROLL) {
                QueueReferenceButtonRefresh(g_edit);
            }
            return 0;

        case kEvaluateEnterMessage:
            if (reinterpret_cast<HWND>(wparam) == g_edit) {
                EvaluateCompletedLineAfterEnter();
                RefreshGeneratedLineUi(hwnd);
            }
            return 0;

        case kRefreshReferencesMessage:
            if (reinterpret_cast<HWND>(wparam) == g_pending_reference_refresh_edit) {
                g_pending_reference_refresh_edit = nullptr;
            }
            if (reinterpret_cast<HWND>(wparam) == g_edit) {
                UpdateReferenceButtons(hwnd);
            }
            return 0;

        case WM_NOTIFY: {
            const auto* notification = reinterpret_cast<const NMHDR*>(lparam);
            if (notification->hwndFrom == g_tab_control && notification->code == TCN_SELCHANGE) {
                const int selected = TabCtrl_GetCurSel(g_tab_control);
                if (selected >= 0) {
                    SwitchToTab(hwnd, static_cast<size_t>(selected));
                }
            }
            return 0;
        }

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize.x = Scale(kMinWindowWidth);
            info->ptMinTrackSize.y = Scale(kMinWindowHeight);
            return 0;
        }

        case WM_DPICHANGED: {
            g_dpi = HIWORD(wparam);
            const auto* suggested = reinterpret_cast<const RECT*>(lparam);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOACTIVATE | SWP_NOZORDER);

            DestroyUiResources();
            CreateUiResources();
            ApplyUiFonts();

            RECT client{};
            GetClientRect(hwnd, &client);
            LayoutChildren(hwnd, client.right, client.bottom);
            return 0;
        }

        case WM_SETFOCUS:
            if (g_edit != nullptr) {
                SetFocus(g_edit);
            }
            return 0;

        case WM_CTLCOLOREDIT:
            SetTextColor(reinterpret_cast<HDC>(wparam), kTitleColor);
            SetBkColor(reinterpret_cast<HDC>(wparam), kCardColor);
            return reinterpret_cast<LRESULT>(g_card_brush);

        case WM_DRAWITEM:
            if (reinterpret_cast<DRAWITEMSTRUCT*>(lparam)->CtlID == kTabControlId) {
                DrawOwnerTab(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
            } else {
                DrawOwnerButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
            }
            return TRUE;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT:
            PaintWindow(hwnd);
            return 0;

        case WM_CLOSE:
            if (ConfirmAllTabs(hwnd)) {
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_DESTROY:
            DestroyReferenceButtons();
            for (EditorTab& tab : g_tabs) {
                if (tab.text_document != nullptr) {
                    tab.text_document->Release();
                    tab.text_document = nullptr;
                }
            }
            g_tabs.clear();
            g_edit = nullptr;
            g_text_document = nullptr;
            g_tab_control = nullptr;
            DestroyUiResources();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int command_show) {
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        OutputDebugStringW(L"Text Calculator: failed to enable per-monitor DPI awareness.\n");
    }
    g_dpi = GetDpiForSystem();

    INITCOMMONCONTROLSEX common_controls{};
    common_controls.dwSize = sizeof(common_controls);
    common_controls.dwICC = ICC_TAB_CLASSES;
    if (!InitCommonControlsEx(&common_controls)) {
        MessageBoxW(nullptr, L"Failed to initialize Windows common controls.",
                    L"Text Calculator", MB_ICONERROR);
        return 1;
    }

    WNDCLASSW window_class{};
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClass;
    window_class.hCursor = LoadCursor(nullptr, IDC_IBEAM);
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(kAppIconResourceId));
    window_class.hbrBackground = nullptr;

    if (RegisterClassW(&window_class) == 0) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Text Calculator", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, kWindowClass, L"Text Calculator",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                CW_USEDEFAULT, CW_USEDEFAULT, Scale(820), Scale(600), nullptr,
                                nullptr, instance, nullptr);
    if (hwnd == nullptr) {
        if (g_richedit_module != nullptr) {
            FreeLibrary(g_richedit_module);
            g_richedit_module = nullptr;
        }
        MessageBoxW(nullptr, L"Failed to create the main window.", L"Text Calculator", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, command_show);
    UpdateWindow(hwnd);

    ACCEL shortcut_table[] = {
        {FVIRTKEY | FCONTROL, 'O', kOpenButtonId},
        {FVIRTKEY | FCONTROL, 'S', kSaveButtonId},
        {FVIRTKEY | FCONTROL | FSHIFT, 'S', kSaveAsCommandId},
        {FVIRTKEY | FCONTROL, 'E', kScientificButtonId},
        {FVIRTKEY | FCONTROL, 'T', kNewTabButtonId},
        {FVIRTKEY | FCONTROL, 'W', kCloseTabButtonId},
        {FVIRTKEY | FCONTROL, VK_TAB, kNextTabCommandId},
        {FVIRTKEY | FCONTROL | FSHIFT, VK_DELETE, kClearButtonId},
        {FVIRTKEY, VK_F1, kHelpButtonId},
    };
    HACCEL shortcuts = CreateAcceleratorTableW(
        shortcut_table, static_cast<int>(sizeof(shortcut_table) / sizeof(shortcut_table[0])));

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (shortcuts != nullptr && TranslateAcceleratorW(hwnd, shortcuts, &message)) {
            continue;
        }
        if (IsDialogMessageW(hwnd, &message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (shortcuts != nullptr) {
        DestroyAcceleratorTable(shortcuts);
    }
    if (g_richedit_module != nullptr) {
        FreeLibrary(g_richedit_module);
        g_richedit_module = nullptr;
    }

    return static_cast<int>(message.wParam);
}
