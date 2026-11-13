# Universal Smart Search Engine — Release 1
# Installation & Setup Guide

---

## 1. System Requirements

### Hardware Requirements
- **CPU:** 64-bit x86-64 (Intel Core i3 / AMD Ryzen 3 or higher) or Apple Silicon (M1/M2/M3/M4).
- **RAM:** 4 GB minimum (8 GB recommended for indexing $>100,000$ documents).
- **Disk Space:** 300 MB free space for application, libraries, and index storage.

---

## 2. Installation on macOS (Apple Silicon / Intel)

### Option A: Install Pre-Built Native `.app` Bundle (Recommended)
1. Build the application bundle using CMake:
   ```bash
   /Users/a123/Qt/Tools/CMake/CMake.app/Contents/bin/cmake --build build --target SearchEngineApp
   ```
2. Copy the bundled application into your `/Applications` directory:
   ```bash
   cp -R "build/Universal Smart Search Engine.app" /Applications/
   ```
3. Launch the application:
   - Press `Cmd + Space`, type `Universal Smart Search Engine`, and press `Enter`.
   - Alternatively, open it from Launchpad or Finder.

### Option B: Terminal Command-Line Setup (CLI Command)
To launch the application from any terminal session by typing `searchengine`:
```bash
mkdir -p ~/.local/bin
ln -sf "/Applications/Universal Smart Search Engine.app/Contents/MacOS/Universal Smart Search Engine" ~/.local/bin/searchengine
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
searchengine
```

---

## 3. Installation on Windows 10/11 x64

### Prerequisites
1. **Visual Studio 2022 Community / Professional:** Ensure the **"Desktop development with C++"** workload is installed (MSVC v143 toolset).
2. **Qt 6 Framework:** Install Qt 6.5 LTS, 6.6+, or 6.8+ (MSVC 2022 64-bit).
3. **CMake:** Version 3.20 or newer.

### Build and Deployment Steps
1. Open the **x64 Native Tools Command Prompt for VS 2022**.
2. Clone and navigate to the repository:
   ```cmd
   git clone https://github.com/sibtaintahir512-code/Universal-Smart-Search-Engine.git
   cd Universal-Smart-Search-Engine
   ```
3. Configure the build:
   ```cmd
   cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.5.3/msvc2022_64"
   ```
4. Compile the project in Release configuration:
   ```cmd
   cmake --build build --config Release --parallel
   ```
5. Deploy required Qt runtime dependencies using `windeployqt`:
   ```cmd
   windeployqt build\Release\SearchEngineApp.exe
   ```
6. Run the application:
   ```cmd
   .\build\Release\SearchEngineApp.exe
   ```

---

## 4. Verifying Test Suite Execution

To execute all 15,595 automated unit, integration, failure, and benchmark tests:

### On macOS:
```bash
./build/tests/UnitTestsRunner
```

### On Windows:
```cmd
.\build\tests\Release\UnitTestsRunner.exe
```

Expected result:
```
 >>> ALL 15595 ASSERTIONS PASSED SUCCESSFULLY! (0 Failures) <<<
```
