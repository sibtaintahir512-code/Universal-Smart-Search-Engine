# Build & Toolchain Setup Instructions (BUILD_INSTRUCTIONS)
## Project: Universal Smart Search Engine
### Release 1 — Local Universal File Search Engine
**Document Version:** 1.0.0  
**Target Platform:** Windows 10/11 x64 (MSVC 2022 / C++17 / CMake / Qt 6 Widgets)  
**Status:** Phase 1 Toolchain Specification  

---

## 1. System Requirements & Approved Technology Stack

The Universal Smart Search Engine is architected natively for the Windows 64-bit desktop environment:

| Component | Minimum Version | Recommended Specification |
| :--- | :--- | :--- |
| **Operating System** | Windows 10 (Build 19041+) / Windows 11 | Windows 11 64-bit |
| **Compiler** | MSVC 2022 (v143) 19.30+ | Visual Studio 2022 Community / Professional / Build Tools |
| **C++ Standard** | ISO C++17 (`/std:c++17`) | C++17 conformance mode (`/permissive- /utf-8`) |
| **Build System** | CMake 3.20+ | CMake 3.28+ |
| **GUI Framework** | Qt 6.2+ Widgets (MSVC 2022 x64) | Qt 6.5 LTS or Qt 6.7 MSVC 2022 64-bit |
| **Windows SDK** | 10.0.19041.0+ | Windows 11 SDK (10.0.22621.0+) |
| **IDE / Editor** | Visual Studio Code | VS Code with C/C++ Extension Pack & CMake Tools |
| **Version Control**| Git 2.30+ | Git for Windows (x64) |

---

## 2. Environment Setup Guide (Windows 10/11 x64)

### Step 1: Install Visual Studio 2022 (MSVC Toolchain)
1. Download the installer from [Visual Studio Downloads](https://visualstudio.microsoft.com/downloads/) (Community Edition is free).
2. Run the Visual Studio Installer and select the workload:
   - **Desktop development with C++**
3. Ensure the following optional components are checked in the right panel:
   - **MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)**
   - **Windows 11 SDK (10.0.22621.0)** or **Windows 10 SDK**
   - **C++ CMake tools for Windows**
4. Complete installation and verify in PowerShell / Command Prompt:
   ```cmd
   cl /?
   ```

### Step 2: Install Git & CMake
1. **Git:** Download and install from [git-scm.com](https://git-scm.com/download/win).
2. **CMake:** Download the x64 Windows MSI installer from [cmake.org](https://cmake.org/download/). Select **"Add CMake to system PATH"** during installation.
3. Verify in terminal:
   ```cmd
   cmake --version
   git --version
   ```

### Step 3: Install Qt 6 Widgets (MSVC 2022 64-bit)
1. Download the official **Qt Online Installer** from [qt.io/download-open-source](https://www.qt.io/download-open-source).
2. In the installer component selection tree, navigate to:
   - `Qt` $\to$ `Qt 6.x` (e.g., Qt 6.5 LTS or Qt 6.7) $\to$ check **`MSVC 2022 64-bit`**.
3. Complete installation. The default installation path is typically:
   ```cmd
   C:\Qt\6.5.3\msvc2022_64
   ```
4. Add the Qt `bin` directory to your user `PATH` environment variable:
   ```cmd
   C:\Qt\6.5.3\msvc2022_64\bin
   ```

### Step 4: Configure Visual Studio Code
1. Open VS Code.
2. Install the recommended extensions from the Extensions marketplace:
   - **C/C++ Extension Pack** (`ms-vscode.cpptools-extension-pack`)
   - **CMake Tools** (`ms-vscode.cmake-tools`)
3. Set the CMake kit to **Visual Studio Community 2022 Release - amd64** (or `x64 Native Tools`).

---

## 3. Command-Line Build Instructions (MSVC x64)

Always execute command-line builds from the **x64 Native Tools Command Prompt for VS 2022** (accessible from the Windows Start Menu).

### 3.1 Configure Build with CMake
Pass the Qt installation path via `CMAKE_PREFIX_PATH`:

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.5.3/msvc2022_64"
```

*Alternative for Ninja Generator:*
```cmd
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="C:/Qt/6.5.3/msvc2022_64"
```

### 3.2 Build Debug Configuration
```cmd
cmake --build build --config Debug
```

### 3.3 Build Release Configuration
```cmd
cmake --build build --config Release
```

### 3.4 Execute Unit Tests
```cmd
ctest --test-dir build -C Debug --output-on-failure
```
Or execute the standalone binary directly:
```cmd
.\build\tests\Debug\UnitTestsRunner.exe
```

### 3.5 Run the Desktop Application Shell
Before launching, copy the required Qt runtime DLLs using the `windeployqt` utility:
```cmd
windeployqt build\Debug\SearchEngineApp.exe
.\build\Debug\SearchEngineApp.exe
```

---

## 4. Current Host Environment Inspection Findings

An exhaustive inspection of the current host machine was conducted during Phase 1:

| Component | Status on Host | Host Finding / Details |
| :--- | :--- | :--- |
| **Host OS** | Non-Target Host | **macOS 15.0 Darwin (Apple Silicon ARM64)** |
| **Visual Studio 2022 / MSVC** | **Missing** | MSVC is proprietary to Windows x64; not natively available on macOS. |
| **Windows SDK** | **Missing** | Windows SDK headers (`Windows.h`, etc.) require a Windows host or cross-SDK. |
| **CMake** | **Missing on Host** | `cmake` is not installed in the macOS host PATH. |
| **Qt 6 (MSVC 2022)** | **Missing on Host** | Windows prebuilt Qt 6 MSVC binaries are absent on macOS host. |
| **Git** | **Installed** | Apple Git 2.50.1 available at `/usr/bin/git`. |
| **VS Code & Extensions** | **Installed** | VS Code present with `ms-vscode.cmake-tools`, `ms-vscode.cpptools-extension-pack`. |

### Root Cause Analysis & Resolution Protocol
1. **Cause:** The active terminal environment is running on a macOS Apple Silicon host, whereas the university project specifications formally require Windows 10/11 x64 with MSVC 2022.
2. **Strict Adherence:** Per the strict rules of Phase 1, the technology stack was **not silently altered**. The build scaffolding, CMake modules, compiler flags, and target configurations are authored strictly for MSVC 2022 x64 conformance mode.
3. **Action Required for User:**
   - Clone or copy this repository to a Windows 10/11 x64 machine (or launch a Windows VM via Parallels / VMware / Dual Boot).
   - Follow the step-by-step setup in Section 2 above to run the native MSVC 2022 build.
