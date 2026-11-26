# 构建与发布 MCnotepad

## 该用哪个版本

| 版本 | 位置 | 用途 | 数据保存在 |
|---|---|---|---|
| **安装版**（推荐日常使用） | `dist\<版本>\MCnotepad-<版本>-setup.exe` 安装后，开始菜单中的 MCnotepad | 平时使用 | `%LOCALAPPDATA%\MCnotepad` |
| **便携版** | `dist\<版本>\MCnotepad-<版本>-portable.zip`，解压到任意位置 | U 盘、其他电脑、不想安装时 | 解压目录中的 `data` 文件夹 |
| 开发版 | `build\windows-x64\Release\MCnotepad.exe` | 只用于开发和测试，不要日常使用 | `%LOCALAPPDATA%\MCnotepad-dev` |

- 开发版的标题栏显示“MCnotepad [开发版]”，数据与正式版分开，测试时不会影响日常使用的标签。
- “帮助 → 关于”会显示版本号、对应的 Git 提交、版本类型（安装版 / 便携版 / 开发版）、程序位置和数据位置。在资源管理器中查看 exe 的“属性 → 详细信息”也能看到版本。
- 安装版和便携版的数据互相独立，可以同时运行。

## 发布（生成安装版和便携版）

需要 Visual Studio 18 2026、Windows SDK、CMake 和 [Inno Setup 6](https://jrsoftware.org/isdl.php)。

1. 在 `CMakeLists.txt` 的 `project(MCnotepad VERSION x.y.z ...)` 中修改版本号。
2. 提交所有修改。
3. 运行：

```powershell
powershell -ExecutionPolicy Bypass -File package.ps1
```

脚本会在 `build\package` 中编译发布版本，运行自检，然后在 `dist\<版本>\` 中生成：

```text
MCnotepad-<版本>-setup.exe      安装程序
MCnotepad-<版本>-portable.zip   便携版
SHA256SUMS.txt                  校验值
```

有未提交的修改时脚本会停止；加上 `-AllowDirty` 可以生成测试用的安装包（版本信息中会标记 `-dirty`）。

安装程序默认只为当前用户安装，不需要管理员权限（第一页可以选择为所有用户安装）；可选创建桌面快捷方式和右键菜单“用 MCnotepad 打开”。安装或升级时，正在运行的 MCnotepad（包括隐藏在托盘中的）会先保存所有标签再退出。卸载时保留 `%LOCALAPPDATA%\MCnotepad` 中的会话和恢复快照。

## 开发构建

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-release --parallel 4
```

输出：`build\windows-x64\Release\MCnotepad.exe`（开发版）。

自检：

```powershell
build\windows-x64\Release\MCnotepad.exe --self-test
```

退出码为 0 表示全部通过；失败时会打印出错检查所在的文件行号。

## 源码结构

程序是一个编译单元：`src/main.cpp` 按顺序 `#include` 了 `src/parts/` 下的 21 个部分（`00_app_state.cpp` 到 `20_main_window.cpp`），每个部分的用途写在 `main.cpp` 的注释里。这些部分不会单独编译（CMake 中标记为 `HEADER_FILE_ONLY`），后面的部分会用到前面部分的声明，所以新增代码请放进对应的部分，不要调整顺序。

版本号来自 `CMakeLists.txt`，通过 `src/version.h.in` 生成；安装程序的定义在 `installer\MCnotepad.iss`，简体中文界面文字来自 Inno Setup 官方仓库的翻译文件 `installer\ChineseSimplified.isl`。
