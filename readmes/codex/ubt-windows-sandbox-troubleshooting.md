# UBT 在 Codex Windows 沙盒中的构建故障记录

## 适用范围

适用于本项目以及在其他设备上使用 Codex、UE 5.8 和 `Build.bat` 构建 Editor Target 的任务。设备之间的用户名、UE 安装路径、项目路径和临时目录可能不同，处理时必须以当前设备的实际路径为准。

## 已确认的故障特征

当构建终端只输出以下两类信息后快速失败：

```text
Using bundled DotNet SDK version: ...
Running UnrealBuildTool: dotnet ... UnrealBuildTool.dll ...
```

同时出现以下现象时，优先按“UBT 启动阶段权限/文件占用问题”处理，而不是先修改 C++：

- `Build.bat` 返回非零码，或返回 .NET 异常码 `-532462766`。
- `C:\Users\<用户>\AppData\Local\UnrealBuildTool\Log.txt` 时间没有更新，仍是上一次构建的日志。
- 项目 `Binaries/Win64/UnrealEditor-<Project>.dll` 时间和大小没有更新。
- 临时目录的 `*Build.bat*.lock` 文件中出现：

```text
System.UnauthorizedAccessException: Access to the path is denied.
System.IO.FileSystem.MoveFile(...)
EpicGames.Core.Log.BackupLogFile(...)
UnrealBuildTool.UnrealBuildTool.Main(...)
```

## 根因

UBT 在真正解析项目和编译源文件之前，会备份默认日志：

```text
%LOCALAPPDATA%\UnrealBuildTool\Log.txt
```

在 Codex Windows 沙盒的未提升权限模式下，即使 Windows ACL 显示当前用户拥有修改权限，沙盒令牌仍可能不允许 UBT 对该日志执行移动/重命名。因此 UBT 会在日志初始化阶段退出，根本没有进入 C++ 编译阶段。

这不是以下问题：

- HUD 或其他 C++ 代码编译错误；
- `TargetMakefile` 或 DDC 必然损坏；
- `codex-windows-sandbox-setup.exe` 缺失；
- 需要删除整个 `Intermediate`、`Binaries` 或 `DerivedDataCache`。

项目级 `Saved/UnrealBuildTool/BuildConfiguration.xml` 不能可靠解决本问题：标准 Editor Target 构建会在项目配置完全解析前初始化 UBT 日志，仍可能使用用户级默认日志路径。

## 标准处理流程

### 1. 构建前清理运行进程

先关闭：

- `UnrealEditor.exe`；
- Live Coding；
- 同一项目残留的 `UnrealBuildTool`/Bundled DotNet 构建进程。

同一时间只运行一个 `Build.bat`。不要在已有构建等待锁时再启动第二条构建。

### 2. 只使用项目规定的 Build.bat

示例：

```text
<UE_ROOT>\Engine\Build\BatchFiles\Build.bat RiverOfInkEditor Win64 Development -Project=<PROJECT_ROOT>\RiverOfInk.uproject -WaitMutex -NoHotReload
```

不要直接调用 `UnrealBuildTool.dll` 代替 `Build.bat`，也不要并行调用多个 UBT 进程。

### 3. 早期启动失败时检查两处

先检查用户级 UBT 日志：

```powershell
$ubtLog = Join-Path $env:LOCALAPPDATA 'UnrealBuildTool\Log.txt'
Get-Content -LiteralPath $ubtLog -Tail 80
```

如果该日志没有更新，再检查构建临时目录中的锁文件：

```powershell
Get-ChildItem -LiteralPath $env:TEMP -Filter '*Build.bat*.lock' -Force |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 5 FullName,Length,LastWriteTime
```

`.lock` 文件由 `Build.bat` 捕获 UBT 的标准错误，早期启动异常经常只在这里留下完整堆栈。

### 4. 命中日志备份权限异常时

在关闭 UE/Live Coding 后，用当前设备允许的提升权限重新运行完全相同的 `Build.bat`。不要通过删除大目录来绕过该错误。

本项目已验证：提升权限后 UBT 能够正常备份日志、编译 HUD 相关 C++，并输出：

```text
Result: Succeeded
```

如果 Codex 的 Windows 沙盒支持选择运行级别，优先使用提升权限模式；这是对本地日志权限故障的环境修复，不是对项目代码的修改。

### 5. 成功判定

不能只看终端最后一行。构建结束后必须同时确认：

- UBT 日志包含 `Result: Succeeded`；
- 没有 `Exception`、`Unhandled` 或 `Fatal`；
- 项目目标 DLL/PDB 的时间或大小已更新；
- 重新启动 UE 后，运行时确实加载了新 DLL，而不是继续使用旧 Editor 进程。

如果 `TargetMakefile.Load` 报 `BinaryArchiveReader`/`Makefile.bin`，只允许让一个 UBT 进程重建该 makefile；不要并行启动第二个构建，也不要删除整个 `Intermediate`、`Binaries` 或 DDC。

## 跨设备注意事项

- 不要复制本设备的 `C:\Users\neo\...` 路径到另一台设备；使用另一台设备自己的 `%LOCALAPPDATA%` 和 `%TEMP%`。
- 不要把一次旧的 `Result: Succeeded` 当成当前构建成功；先检查 UBT 日志的 `Log started` 时间和目标 DLL 时间。
- 如果另一台设备只出现 `Log.txt` 移动/备份的 `UnauthorizedAccessException`，直接按“关闭进程 → 提升权限重跑同一 Build.bat → 检查日志和 DLL”处理。
- 如果错误变成编译器诊断、链接错误或 UHT 错误，再转入正常的代码/工具链排查，不要继续反复提升权限。

## 相关官方说明

[Windows 沙盒｜ChatGPT Learn](https://learn.chatgpt.com/zh-Hant/docs/windows/windows-sandbox)

