# MCnotepad

MCnotepad 是一个原生 Win32 C++ 文本编辑器，目标是比传统编辑器启动更轻、工作区更清晰，并且不会丢失未保存的内容。

## 功能

**编辑与标签**
- 多标签页，显示修改状态（`*`），标签上带关闭按钮
- 单实例运行：再次打开文件（例如在资源管理器中双击）会交给已打开的窗口；同一个文件已打开时直接切换到该标签
- 查找、替换、全部替换：支持区分大小写、全字匹配、向上查找，到末尾自动从头继续；“全部替换”可一次撤销
- `F3` / `Shift+F3` 重复上一次查找；打开查找框时自动填入当前选中的文字
- 跳转到行（`Ctrl+G`）
- 粘贴时只取纯文本，不会带入网页或 Word 的字体和颜色
- 自动换行；行号栏与文字逐行对齐，自动换行时按真实行编号，并高亮当前行
- 四套主题（深色、浅色、护眼绿、暗夜蓝）；深色主题下标题栏和滚动条也是深色
- 支持高 DPI 和不同缩放比例的多显示器

**编码与换行（每个文件独立）**
- 打开时自动识别：UTF-8、UTF-8 BOM、UTF-16 LE、UTF-16 BE、ANSI（系统代码页，例如 GBK）
- 自动识别换行符：CRLF、LF、CR
- 保存时保持文件原来的编码和换行；可在“编码”菜单或工具栏中修改
- 按 ANSI 保存时，如有无法表示的字符，会先提示改用 UTF-8
- 状态栏显示当前文件的行列、选中字数、编码、换行和语言

**语言与工具**
- 根据扩展名识别语言（HTML、CSS、JavaScript、C/C++、Python、JSON、Markdown、SQL、PowerShell），也可以在“语言”菜单中手动指定
- 简单语法着色：HTML 的标签、注释和属性值；代码文件中的字符串。输入停顿后自动更新，不影响撤销
- 格式化 JSON（`Ctrl+Shift+F`），可以用 `Ctrl+Z` 撤销
- 运行当前文件（`F5`）：`.bat` `.cmd` `.ps1` `.py` `.js` 会在新的控制台中运行；`.html` 用默认浏览器打开

**会话与恢复**
- 正常关闭、系统重启或关机、意外结束后都会自动恢复工作区
- 恢复标签顺序、活动标签、内容、光标、滚动位置，以及窗口位置和大小
- 编辑内容约两秒内写入恢复快照，写入采用临时文件和原子替换
- 文件在其他程序中被修改或删除时，切回 MCnotepad 会提示重新加载

## 快捷键

| 快捷键 | 功能 |
|---|---|
| `Ctrl+N` / `Ctrl+O` / `Ctrl+S` / `Ctrl+Shift+S` | 新建 / 打开 / 保存 / 另存为 |
| `Ctrl+W` | 关闭标签 |
| `Ctrl+F` / `Ctrl+H` / `Ctrl+G` | 查找 / 替换 / 跳转到行 |
| `F3` / `Shift+F3` | 查找下一个 / 上一个 |
| `Ctrl+Shift+F` | 格式化 JSON |
| `F5` | 运行当前文件 |
| `Ctrl+,` | 偏好设置 |

## 构建

需要 Visual Studio 2026（MSVC）、Windows SDK 和 CMake 3.24 以上。

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-release --parallel 4
```

可执行文件位于 `build/windows-x64/Release/MCnotepad.exe`，运行时需要同目录下的 `resources/` 文件夹（工具栏图标）。

## 数据位置

```text
%LOCALAPPDATA%\MCnotepad\session.dat          会话与设置
%LOCALAPPDATA%\MCnotepad\recovery\*.snapshot  每个标签的恢复快照
%LOCALAPPDATA%\MCnotepad\running.lock         运行标记，用于检测异常退出
```

详见 [SESSION_RECOVERY.md](SESSION_RECOVERY.md)。
