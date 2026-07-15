# Text Calculator

Text-editor style calculator for Windows, Android, and HarmonyOS NEXT. The
calculation core is C++ and is shared through a small UTF-8 C ABI for mobile
bridges.

## Windows exe

MinGW/GCC:

```powershell
.\scripts\build-mingw.ps1
```

```bat
scripts\build-mingw.bat
```

MSVC:

```powershell
.\scripts\build-msvc.ps1
# Or pass a specific generator:
.\scripts\build-msvc.ps1 -Generator "Visual Studio 17 2022"
```

```bat
scripts\build-msvc.bat
scripts\build-msvc.bat build-msvc x64 "Visual Studio 17 2022"
```

Manual CMake build:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

The Windows executable is written to:

```text
build/text_calculator.exe
```

## Android APK

The Android app uses a Java text editor UI and a JNI bridge to the C++ core.

```powershell
.\scripts\build-android.ps1
```

```bat
scripts\build-android.bat
```

The debug APK is written to:

```text
android/app/build/outputs/apk/debug/app-debug.apk
```

## HarmonyOS NEXT HAP

The Harmony app uses ArkTS UI and a NAPI bridge to the same C++ core.

```powershell
.\scripts\build-harmony.ps1
```

```bat
scripts\build-harmony.bat
```

The debug HAP is written to:

```text
harmony/entry/build/default/outputs/default/entry-default-unsigned.hap
```

Run all available local builds:

```powershell
.\scripts\build-all.ps1
```

```bat
scripts\build-all.bat
```

## Behavior

Enter always inserts a normal text newline. In the Windows exe, if the completed
line contains only a supported mathematical expression, the result is added to
that same line:

```text
1+2*3    = 7
```

Other text is left unchanged without an error line. The result portion uses a
different color, and each visible result has a `引用` button that inserts the
result value at the current caret. The button itself is not part of the text and
is not saved to the file.

The Windows exe can open and save UTF-8 text files. Its toolbar also includes
actions for clearing the editor, inserting scientific notation for the active
expression, and showing a short help dialog. Clear can be undone with `Ctrl+Z`.
Each tab keeps its own text, file path, dirty state, and undo history.

Windows shortcuts:

- `Ctrl+O`: open a UTF-8 text file.
- `Ctrl+S`: save the current file.
- `Ctrl+Shift+S`: save as a different file.
- `Ctrl+T`: create a new editor tab.
- `Ctrl+W`: close the current tab.
- `Ctrl+Tab`: switch to the next tab.
- `Ctrl+E`: insert scientific notation.
- `Ctrl+Shift+Delete`: clear the editor.
- `F1`: show help.

Supported plain text examples:

```text
1+2*3
2^8
sqrt(16)
root(3,27)
log(100)
log(2,8)
ln(e)
sin(pi/2)
cos(0)
tan(pi/4)
10%3
mod(-1,5)
rem(-5,3)
3.3k/1.1k
2M
2m
1u+500n
```

Supported LaTeX examples:

```text
\frac{1}{2}+\sqrt{9}
2^{3}
\sqrt[3]{27}
\log_{2}{8}
\ln(e)
\sin{\pi/2}
1\mu
5 \bmod 2
```

MVP assumptions:

- `log(x)` is base 10.
- `ln(x)` is natural log.
- `log(base, value)` and `\log_{base}{value}` use a custom base.
- Trigonometric functions use radians.
- `%`, `rem(a,b)`, and `remainder(a,b)` return the signed remainder.
- `mod(a,b)` returns a non-negative modulo value.
- Numeric suffixes support `t`, `g`, `k`, `M`, `m`, `u`, `n`, `p`, and `f`.
- Suffix case is ignored except `M` is mega and `m` is milli.
- Lines that are not complete mathematical expressions remain ordinary text.
