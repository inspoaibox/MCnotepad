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
