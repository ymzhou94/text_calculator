#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>

#include <string>
#include <vector>

#include "editor_logic.h"

namespace {

constexpr wchar_t kWindowClass[] = L"TextCalculatorWindow";

HWND g_edit = nullptr;
HFONT g_font = nullptr;
bool g_internal_edit = false;

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
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(1),
                reinterpret_cast<LPCREATESTRUCTW>(lparam)->hInstance, nullptr);

            g_font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 FIXED_PITCH | FF_MODERN, L"Consolas");
            SendMessageW(g_edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
            SetFocus(g_edit);
            return 0;
        }

        case WM_SIZE:
            if (g_edit != nullptr) {
                MoveWindow(g_edit, 0, 0, LOWORD(lparam), HIWORD(lparam), TRUE);
            }
            return 0;

        case WM_COMMAND:
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
