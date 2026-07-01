# DesktopIconCenter

DesktopIconCenter 是一个轻量级 Windows 桌面后台工具：当用户在桌面新建文件、文件夹或快捷方式时，程序会等待 Explorer 创建图标，然后把该图标移动到主显示器中央附近的最近空网格，避免新图标默认落在左上角并与桌面收纳区域重叠。

## 特性

- Win32 API + C++17 + Unicode。
- 无 .NET、MFC、Qt、第三方 DLL。
- 单 EXE 后台运行，隐藏主窗口，系统托盘交互。
- 使用 `ReadDirectoryChangesW` 监听当前用户桌面。
- 使用 `IShellWindows` / `IFolderView2` 枚举和移动 Explorer 桌面图标。
- 辅助实现 `LVM_FINDITEM`、`LVM_GETITEMCOUNT`、`LVM_GETITEMPOSITION`、`LVM_GETITEMSPACING`。
- 中央网格搜索会避开已有图标，优先寻找最近空位。
- 支持配置热重载、暂停/恢复监听、开机启动、日志查看。
- Explorer / 任务栏重启后会自动恢复托盘图标和桌面句柄。

## 编译方法

### Visual Studio 2022

1. 打开 `DesktopIconCenter.sln`。
2. 选择 `Release | x64`。
3. 生成解决方案。
4. 输出文件位于 `bin\x64\Release\DesktopIconCenter.exe`。

项目默认使用 `/MT` 静态运行库，便于单 EXE 分发。

### CMake

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## 运行方式

直接运行 `DesktopIconCenter.exe`。程序不会显示主窗口，会驻留系统托盘。

托盘右键菜单：

- 暂停监听 / 恢复监听
- 开机启动
- 打开配置
- 重新加载配置
- 查看日志
- 退出

## 配置文件

程序启动时会读取 EXE 同目录的 `config.ini`，不存在则自动生成。

```ini
[General]
Enable=true
Delay=200
AutoStart=false
MoveMode=Center
CenterX=960
CenterY=540
GridSearchRadius=20
IgnoreFolders=false
IgnoreShortcut=false
Allow360RealMouseDrag=false
Allow360Reload=false
```

说明：

- `Enable`：是否启用监听。
- `Delay`：收到桌面新增事件后等待 Explorer 创建图标的毫秒数。
- `AutoStart`：是否写入当前用户开机启动项。
- `Allow360RealMouseDrag`：360 桌面助手兼容失败时是否允许占用真实鼠标拖拽，默认 false。
- `Allow360Reload`：360 桌面助手兼容失败时是否允许重启 360 加载布局，默认 false。
- `MoveMode`：`Center` 使用主显示器工作区中心；`Custom` / `Fixed` / `Manual` 使用 `CenterX`、`CenterY`。
- `GridSearchRadius`：从中心向外搜索空网格的最大半径。
- `IgnoreFolders`：是否忽略文件夹。
- `IgnoreShortcut`：是否忽略 `.lnk` 快捷方式。

配置可通过托盘菜单“重新加载配置”生效；处理新增事件前也会自动重载一次。

## 项目结构

```text
DesktopIconCenter
├── DesktopIconCenter.sln
├── DesktopIconCenter.vcxproj
├── CMakeLists.txt
├── include
│   ├── App.h
│   ├── Config.h
│   ├── Desktop360Compat.h
│   ├── DesktopWatcher.h
│   ├── ExplorerDesktop.h
│   ├── DesktopListView.h
│   ├── IconManager.h
│   ├── GridManager.h
│   ├── TrayIcon.h
│   ├── Version.h
│   └── Logger.h
├── src
│   ├── main.cpp
│   ├── App.cpp
│   ├── Config.cpp
│   ├── Desktop360Compat.cpp
│   ├── DesktopWatcher.cpp
│   ├── ExplorerDesktop.cpp
│   ├── DesktopListView.cpp
│   ├── IconManager.cpp
│   ├── GridManager.cpp
│   ├── TrayIcon.cpp
│   └── Logger.cpp
├── resource
│   ├── app.ico
│   ├── resource.h
│   └── resource.rc
├── config.ini
└── README.md
```

## Windows API 使用说明

- `ReadDirectoryChangesW`：监听桌面目录新增和重命名完成事件。
- `SHGetKnownFolderPath(FOLDERID_Desktop)`：获取当前用户桌面路径。
- `FindWindowW` / `EnumWindows` / `FindWindowExW`：定位 `Progman`、`WorkerW`、`SHELLDLL_DefView`、`SysListView32`。
- `SendMessageTimeoutW`：给桌面 ListView 发送超时消息，避免 Explorer 卡死拖住本程序。
- `LVM_FINDITEMW`、`LVM_GETITEMCOUNT`、`LVM_GETITEMPOSITION`、`LVM_GETITEMSPACING`：辅助定位、读取图标信息和网格间距。
- `IShellWindows`、`IShellBrowser`、`IShellView`、`IFolderView2`：通过官方 Shell COM 接口枚举桌面项、读取坐标并移动目标图标。
- `MonitorFromPoint` / `GetMonitorInfoW`：获取主显示器工作区作为中心计算范围。
- `Shell_NotifyIconW`：创建和维护系统托盘图标。
- `RegSetValueExW` / `RegDeleteValueW`：管理当前用户开机启动项。

## 兼容性

目标系统：Windows 10 / Windows 11，x64，Explorer 桌面由系统默认 Shell 管理。

实现优先使用 Shell COM 的 `IFolderView2::SelectAndPositionItems` 移动图标，避免把未验证的跨进程写入作为主路径。ListView 消息仅作为文档要求的辅助观测和兼容路径，并统一设置超时。

## 日志

日志目录：`logs`。

日志文件示例：`2026-06-29.log`。

记录内容包括：监听开始、检测新文件、定位图标、移动成功、失败原因、Explorer/任务栏恢复等。

## 后续开发计划

- 增加托盘气泡提示开关。
- 支持指定显示器而不只是主显示器。
- 增加更细粒度的图标尺寸/网格偏移配置。
- 增加单元测试覆盖 GridManager 的搜索算法。

## 360 桌面助手兼容说明

从 v1.0.6 开始，360 桌面助手兼容逻辑默认进入“严格静默”模式：只尝试窗口消息拖拽，不移动真实鼠标，也不会杀掉/重启 360 桌面助手。

如果 360 当前版本拒绝窗口消息拖拽，程序会记录 `360 silent move was not confirmed; reload skipped because Allow360Reload=false`，并停止 360 兼容兜底，避免出现桌面布局闪烁或重载。

如需恢复旧版本兜底，可在 `config.ini` 中手动设置：`Allow360RealMouseDrag=true` 启用真实鼠标拖拽，`Allow360Reload=true` 启用修改 `DTFenceData.dtf` 后重启 360 加载布局。默认两个选项均为 `false`。
