# C++ 项目脚手架接入说明

## 接入目标

本项目没有直接照搬通用 C++ 脚手架，而是按 FMS 当前技术栈做了选择性接入。FMS 是 Qt 6 + CMake + MSVC/Ninja 桌面项目，已经具备基础目录结构和 VS Code 构建配置，因此本次重点接入能提升代码质量、协作一致性和展示可解释性的部分。

## 已接入内容

### 1. clang-format 代码格式规范

新增文件：

```text
.clang-format
```

作用：

- 统一 C++ 代码缩进、换行、大括号、指针声明等风格。
- 减少多人协作时因为代码格式不同产生的无效差异。
- 只建议格式化 `app/src` 下的业务代码，不格式化 `app/third_party` 第三方代码。

演示命令：

```powershell
powershell -ExecutionPolicy Bypass -File tools/format_cpp.ps1 -Check
powershell -ExecutionPolicy Bypass -File tools/format_cpp.ps1
```

### 2. cppcheck 静态代码检查

新增文件：

```text
tools/run_cppcheck.ps1
```

作用：

- 对 `app/src` 业务代码做静态检查。
- 检查潜在的警告、风格问题、性能问题和可移植性问题。
- 刻意排除 `app/third_party` 和 `app/build`，避免第三方源码和构建产物干扰结果。

演示命令：

```powershell
powershell -ExecutionPolicy Bypass -File tools/run_cppcheck.ps1
```

### 3. CMakePresets 构建预设

新增文件：

```text
app/CMakePresets.json
```

作用：

- 把当前 Qt/MSVC/Ninja 构建参数固化为 CMake 标准预设。
- 比只写在 VS Code settings/tasks 中更容易迁移到其他 IDE 或命令行。
- 保留当前项目使用的 Qt 6.8.3 MSVC Kit，不切换到文档示例里的 MinGW。

演示命令：

```powershell
cmake --preset qt-msvc-debug -S app
cmake --build --preset qt-msvc-debug
```

### 4. VS Code 任务接入

本机 `.vscode/tasks.json` 增加了格式化检查、格式化执行、静态检查三个任务，方便在 VS Code 中通过“运行任务”展示。

可演示任务：

- `Quality: Check Format`
- `Quality: Format app/src`
- `Quality: Cppcheck app/src`

## 暂不接入的内容及原因

### 1. 暂不接入 vcpkg

原因：

- 当前项目核心依赖是 Qt 6 和本地 `third_party/QtAntDesign`，没有新增外部 C++ 包管理需求。
- 文档示例中的 `fmt`、`gtest` 目前不是 FMS 必需依赖。
- 过早接入 vcpkg 会增加环境配置成本，尤其是 Qt/MSVC 与 vcpkg triplet 的路径协调。

后续接入时机：

- 需要引入 `gtest` 做单元测试。
- 需要引入 `fmt`、`spdlog`、压缩库、哈希库等外部库。
- 多人开发需要统一第三方库版本。

### 2. 不切换到 MinGW 脚手架

原因：

- 当前项目已经使用 Qt 6.8.3 MSVC 2022 64-bit Kit，并且可以生成运行。
- 文档中的 `x64-mingw-dynamic` 是示例环境，不适合直接替换当前 MSVC 工具链。
- 切换编译器会带来 Qt Kit、调试器、二进制兼容性和 DLL 部署问题。

### 3. 不格式化或检查 third_party

原因：

- `app/third_party/QtAntDesign` 是第三方 UI 代码，不属于本项目主要业务代码。
- 修改第三方代码格式会产生大量无意义 diff，影响后续升级或对比源码。
- 静态检查第三方目录会产生很多与 FMS 业务无关的告警。

### 4. 不重建项目目录

原因：

- 当前项目已经有 `src/core`、`src/services`、`src/ui`、`database`、`resources` 等分层。
- 现阶段更重要的是稳定业务功能，而不是为了脚手架形式重排目录。
- 重排目录会影响 CMake、Qt UI 文件、资源文件路径和已有代码引用。

## 课堂展示建议

可以按下面顺序展示：

1. 打开 `.clang-format`，说明这是代码风格统一配置。
2. 打开 `tools/format_cpp.ps1`，说明只处理 `app/src`，避免污染第三方代码。
3. 运行 `Quality: Check Format` 或命令行 `tools/format_cpp.ps1 -Check`。
4. 打开 `tools/run_cppcheck.ps1`，说明静态检查范围和排除规则。
5. 运行 `Quality: Cppcheck app/src` 或命令行脚本。
6. 打开 `app/CMakePresets.json`，说明构建参数从 IDE 私有配置提升为 CMake 标准配置。
7. 最后说明 vcpkg、MinGW、目录重构暂不接入的原因：不是不用脚手架，而是根据当前项目选择性落地。

## 总结话术

本次接入的重点不是“把模板完整复制进项目”，而是让脚手架服务于当前 FMS 项目。我们保留了现有 Qt/MSVC 构建体系，增加了格式规范、静态检查和标准化构建预设；同时避免引入当前不需要的 vcpkg、MinGW 切换和目录重构，从而降低环境风险并提升代码质量。
