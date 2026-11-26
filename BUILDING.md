# 构建 MCnotepad

本机已经配置 Visual Studio 18 2026、Windows SDK 和 CMake。

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-release --parallel 4
```

输出文件：

```text
build/windows-x64/Release/MCnotepad.exe
```

便携版目录和压缩包位于：

```text
dist/MCnotepad-portable/
dist/MCnotepad-portable.zip
```

程序会在 `%LOCALAPPDATA%\MCnotepad` 中保存会话和恢复快照。

## 源码结构

程序是一个编译单元：`src/main.cpp` 按顺序 `#include` 了 `src/parts/` 下的 21 个部分（`00_app_state.cpp` 到 `20_main_window.cpp`），每个部分的用途写在 `main.cpp` 的注释里。这些部分不会单独编译（CMake 中标记为 `HEADER_FILE_ONLY`），后面的部分会用到前面部分的声明，所以新增代码请放进对应的部分，不要调整顺序。

自检：

```powershell
build/windows-x64/Release/MCnotepad.exe --self-test
```

退出码为 0 表示全部通过；失败时会打印出错检查所在的文件行号。
