# Segfault - learn C & C++ by writing it

Segfault is a desktop app (C/C++17 + [raylib 5.5](https://www.raylib.com/)) for preparing the
**coding exams**: Lab1 (numbers, loops, arrays, 2D arrays, parsing C code) and Exam 2 (pointers,
pointer arithmetic, memory management, linked lists).

The exams are about **writing code**, so the app is too. It has three main parts and a quiz:

| | What you do |
|---|---|
| **Study** | 24 lessons in 9 sections. Each one explains an idea and has **step-through code walkthroughs**, tables, tips, common mistakes, quick checks, links to interactive demos (memory visualizer, linked list lab, grid) and the practice problems for that topic. |
| **Practice** | 50 problems, LeetCode style. Write the function in the built-in code editor, **Run** it against the example cases, then **Submit** to run every test, hidden edge cases included. Hints and the reference solution are available, and your code is saved automatically. |
| **Mock Exam** | A timed exam (Paper 1: C fundamentals, Paper 2: pointers & lists, or a quick 25-minute mock). You can run the example tests while you work. When you submit, hidden tests mark every problem and you get a score and a review against the reference solutions. |
| **Quick Quiz** | 68 multiple-choice questions for quick revision, with explanations and a list of mistakes to review. |

![Home](docs/screenshots/01-home.png)

| | |
|---|---|
| ![Practice problem](docs/screenshots/04-practice-problem.png) | ![Study](docs/screenshots/02-study.png) |
| ![Practice list](docs/screenshots/03-practice-list.png) | ![Mock exam](docs/screenshots/05-mock-exam.png) |
| ![Memory visualizer](docs/screenshots/07-memory-visualizer.png) | ![Linked list lab](docs/screenshots/08-linked-list-lab.png) |

## How your code is compiled

When you press Run, your code goes into a small test harness, gets compiled, and runs with a time
limit. Each test case prints one result line, and the app reads those lines back. Compiler errors
point at the line in **your** code. Infinite loops get *Time Limit Exceeded*, and crashes get
*Runtime Error*. The exam rules are checked too: "no array indexing" in the pointer problems, and
"no if" in the diamond pattern.

The app looks for a compiler at start-up, in this order:
1. `g++` or `clang++` in your PATH (MinGW / MSYS2 on Windows, Xcode tools on macOS, gcc on Linux)
2. **Visual Studio** (`cl.exe`, found with `vswhere`)
3. **TinyCC**, bundled with the Windows build. It is C only: 39 of the 50 problems work with it. The other 11 need C++ (references, classes, `new`) and ask for one of the compilers above.

If you have several compilers, click the compiler button to switch between them.

## Running it

- **Windows (ready-made):** extract `Segfault-Windows.zip` and double-click `Segfault.exe`.
- **Windows (from source):** double-click `run.bat`, or open the folder in Visual Studio 2022 (*File > Open > Folder*) and press F5.
- **macOS / Linux:** `./run.sh`

`run.bat` and `run.sh` run CMake for you. To do it by hand:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
raylib is downloaded automatically the first time.

## Tests

```
./build/pq_tests                  # 141 unit tests of the reference solutions
./build/Segfault --selftest   # compiles every problem's reference solution with every compiler
                                  # found and checks that all tests pass and every starter fails;
                                  # also checks lesson links and the visualizer programs
```

## Project structure

```
src/solutions/   reference solutions (Lab1, the pointer functions, both linked lists): compiled
                 into a library used by the unit tests; Study and Practice show this same code
src/app/         main loop and scaling, UI toolkit (ui.cpp), code editor (editor.cpp),
                 compiler detection + test runner (runner.cpp), process launching with
                 time limits (process.cpp), progress and saved code (app.cpp)
src/content/     lessons.cpp, problems.cpp (statements, starter code, tests),
                 questions.cpp, mem_programs.cpp
src/scenes/      home, study, practice, mock, quiz, workspace (problem screen),
                 visualizer, listlab, grid (interactive demos)
third_party/     TinyCC for Windows (LGPL, see COPYING.txt)
resources/fonts  Inter + JetBrains Mono (SIL OFL)
```

Text is rendered at exactly the font's pixel size for the current window scale, and the
Windows/macOS builds are DPI-aware, so text stays sharp on any screen.

## Credits
raylib (zlib), TinyCC (LGPL 2.1), Inter and JetBrains Mono (SIL OFL 1.1).
Content is based on the SETU module material: Lab1.cpp, the Coding Exam 2 brief, and the
"Pointers in C,C++" and "Memory Management" lecture notes.
