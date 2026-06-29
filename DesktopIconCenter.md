# DesktopIconCenter

## 项目简介

开发一个 Windows 桌面后台工具，实现：

**当用户在桌面新建任何文件、文件夹、快捷方式时，自动将对应桌面图标移动到屏幕中央（或中央附近最近空位），避免与左上角桌面收纳盒重叠。**

整个程序无需安装，仅一个 EXE，后台运行，占用资源极低。

---

# 技术要求

## 开发环境

* Visual Studio 2022
* C++17
* x64
* Release
* Win32 API
* Unicode
* 单 EXE
* 不依赖 .NET
* 不依赖 MFC
* 不依赖 Qt
* 不依赖第三方 DLL
* 支持 Windows 10
* 支持 Windows 11

---

# 项目目录

```
DesktopIconCenter
│
├── DesktopIconCenter.sln
├── DesktopIconCenter.vcxproj
├── CMakeLists.txt
│
├── include
│    App.h
│    DesktopWatcher.h
│    ExplorerDesktop.h
│    DesktopListView.h
│    IconManager.h
│    GridManager.h
│    TrayIcon.h
│    Config.h
│
├── src
│    main.cpp
│    App.cpp
│    DesktopWatcher.cpp
│    ExplorerDesktop.cpp
│    DesktopListView.cpp
│    IconManager.cpp
│    GridManager.cpp
│    TrayIcon.cpp
│    Config.cpp
│
├── resource
│    app.ico
│    resource.rc
│
├── config.ini
│
└── README.md
```

---

# 功能需求

## 1、后台运行

程序启动后：

* 不显示主窗口
* 驻留系统托盘
* 单 EXE
* 内存 < 5MB
* CPU 占用接近 0

托盘菜单：

```
DesktopIconCenter

✔ 开机启动

打开配置

暂停监听

恢复监听

退出
```

---

## 2、监听桌面

监听：

```
C:\Users\<User>\Desktop
```

使用：

```
ReadDirectoryChangesW
```

监听事件：

```
FILE_ACTION_ADDED

FILE_ACTION_RENAMED_NEW_NAME
```

监听：

* 文件
* 文件夹
* 快捷方式

忽略：

```
desktop.ini

Thumbs.db
```

---

## 3、等待 Explorer 创建图标

收到新增事件：

```
Sleep(150~300ms)
```

等待 Explorer 创建图标。

可配置：

```
Delay=200
```

---

## 4、获取桌面 ListView

获取：

```
Progman

WorkerW

SHELLDLL_DefView

SysListView32
```

兼容：

Windows10

Windows11

多显示器

Explorer 重启

必须自动重新获取。

---

## 5、定位新增图标

根据：

文件名

使用：

```
LVM_FINDITEM
```

定位对应图标。

如果失败：

自动重试：

```
200ms

500ms

1000ms
```

最多：

```
5 次
```

---

## 6、获取所有图标

获取：

```
LVM_GETITEMCOUNT

LVM_GETITEMPOSITION
```

获得：

```
名称

坐标

矩形

```

建立缓存。

---

## 7、中央空位算法（重点）

不能固定：

```
960,540
```

否则容易覆盖已有图标。

要求：

建立桌面网格。

例如：

```
□□□□□□□□□□□□

□□□□□□□□□□□□

□□□□□★□□□□□□

□□□□□□□□□□□□
```

★ 为目标中心。

如果已占用：

向四周扩散寻找最近空位。

算法：

```
BFS

或者

螺旋搜索
```

例如：

```
        □

    □       □

        ★

    □       □
```

找到最近可用网格。

---

## 8、移动图标

要求：

移动图标到：

```
中央最近空位
```

注意：

不得覆盖已有图标。

不得闪烁。

不得影响其它图标。

必须兼容：

自动刷新。

Explorer 重启。

---

## 9、配置文件

config.ini

```
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
```

支持：

Reload。

无需重启程序。

---

## 10、托盘

右键：

```
DesktopIconCenter

暂停监听

恢复监听

开机启动

打开配置

查看日志

退出
```

---

## 11、日志

logs

```
2026-06-29.log
```

记录：

```
监听开始

检测新文件

定位图标

移动成功

失败原因

Explorer 重启
```

---

## 12、Explorer 自动恢复

Explorer 重启：

自动重新：

获取：

```
WorkerW

ListView
```

恢复监听。

无需重启程序。

---

## 13、多显示器

自动识别：

主显示器。

中央：

```
MonitorFromPoint()

GetMonitorInfo()
```

支持：

以后扩展：

指定显示器。

---

## 14、异常处理

避免：

Explorer 卡死。

发送消息超时：

```
SendMessageTimeout
```

禁止：

无限等待。

---

# 性能要求

启动：

<100ms

CPU：

0%

内存：

<5MB

线程：

监听线程

UI线程

日志线程（可选）

---

# 编码规范

全部中文注释。

每个类：

不少于 20 行说明。

禁止：

全局变量。

禁止：

裸 new/delete。

使用：

```
std::unique_ptr

std::wstring

std::filesystem
```

统一：

RAII。

---

# README

包括：

项目介绍。

编译方法。

运行方式。

项目结构。

Windows API 使用说明。

兼容性。

后续开发计划。

---

# 重要实现说明

Windows 桌面图标由 Explorer 管理，图标位置属于 Explorer 进程中的 ListView。实现图标定位、读取和移动时，应采用兼容 Windows 10/11 的方式，不要依赖未验证的跨进程消息调用。

请优先使用稳定、可维护的 Win32/Shell API 方案，并做好 Explorer 重启、桌面刷新、多显示器等兼容处理。

---

# 最终目标

生成一个可以直接编译运行的完整项目：

* Visual Studio 2022
* C++17
* x64
* Release
* 单 EXE
* 无第三方 DLL
* 无需安装
* 中文注释完整
* 工程完整
* 可直接上传 GitHub 开源

如果发现由于 Windows Explorer 的架构限制，无法稳定实现桌面图标移动，请优先采用官方支持的 Shell/Explorer 扩展机制，或明确说明限制并给出最稳定的替代实现，不要为了满足需求而使用不可维护或依赖未文档化行为的 Hack。