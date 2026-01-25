# Desktop-LLM-Helper

The application allows you to quickly process any selected text using pre-configured tasks.

Each task has its own customizable prompt for generating results. When you select a task, the program automatically copies the currently selected text, sends it along with the prompt to the specified LLM API endpoint, and returns the result—either inserting it into the current application or opening a separate window with the response.

---

**Key Features:**
- Global hotkey to open the task menu
- Support for inserting the response directly into the current application or displaying it in a separate window
- Customizable prompts, token limits, and generation temperature

---

## Build (Windows)

Requirements:
- Windows
- Qt 6.x (mingw_64) with Widgets and Network
- CMake 3.16+ (Qt Tools)
- Ninja (Qt Tools)
- MinGW (Qt Tools)

Example build using Qt Tools (CMake + Ninja + MinGW):
```
"C:\Qt\Tools\CMake_64\bin\cmake.exe" -S . -B cmake-build-debug -G Ninja ^
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" ^
  -DCMAKE_RC_COMPILER="C:/Qt/Tools/mingw1310_64/bin/windres.exe" ^
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe" ^
  -DQT_DIR="C:/Qt/6.8.1/mingw_64/lib/cmake/Qt6"
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build cmake-build-debug
```
