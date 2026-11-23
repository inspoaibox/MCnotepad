# MCnotepad

MCnotepad 是一个原生 Win32 C++ 文本编辑器，目标是提供比传统编辑器更轻的启动和更清晰的工作区。当前版本已经包含可直接运行的多标签工作区和会话恢复。

## 已完成

- 顶部中文菜单和工具栏
- 多标签页、活动状态、修改状态和标签关闭按钮
- `Ctrl+N`、`Ctrl+O`、`Ctrl+S`、`Ctrl+Shift+S`、`Ctrl+W`、`Ctrl+F`、`Ctrl+H`、撤销、重做、复制、粘贴、全选
- 查找、替换、替换全部和循环查找
- UTF-8、UTF-8 BOM、UTF-16 LE、UTF-16 BE、ANSI 读取
- 深色编辑区、行号栏、基础 HTML 标签和字符串着色
- 语言、编码、换行和主题侧边栏
- 状态栏显示行列、保存状态、编码、换行和语言
- 正常关闭、系统重启和意外结束后的会话恢复
- 恢复标签顺序、活动标签、内容、光标、滚动位置和窗口尺寸
- 每两秒自动写入恢复快照，使用临时文件和原子替换
- 根目录 `logo.png` 已接入窗口图标和安装资源

## 构建

当前环境使用 Visual Studio 18 2026、Windows SDK 和 CMake。

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-release --parallel 4
```

可执行文件位于 `build/windows-x64/Release/MCnotepad.exe`。

也可以显式构建到 `build-vs18`。

```powershell
cmake -S . -B build-vs18 -G "Visual Studio 18 2026" -A x64
cmake --build build-vs18 --config Release --parallel 4
```

## 会话恢复位置

```text
%LOCALAPPDATA%\MCnotepad\session.dat
%LOCALAPPDATA%\MCnotepad\recovery\tab-1.snapshot
```
