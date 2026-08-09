# RiverOfInk build workflow

## UE C++ build rules

- 进行 Editor Target 构建前先关闭 `UnrealEditor.exe` 和 Live Coding。
- 只使用 UE bundled .NET 对应的 `Engine/Build/BatchFiles/Build.bat`；不要并行直接调用 `UnrealBuildTool.dll`。
- 同一时间只允许一个 RiverOfInk 构建任务；启动下一次构建前必须等待上一条 `Build.bat` 完整退出。
- 推荐命令：

  ```text
  E:\tool\epic\UE_5.8\Engine\Build\BatchFiles\Build.bat RiverOfInkEditor Win64 Development -Project=E:\project\UE\demo0803\RiverOfInk.uproject -WaitMutex -NoHotReload
  ```

- 构建结束后检查 `%LOCALAPPDATA%\UnrealBuildTool\Log.txt` 的 `Result:`、`Exception`、`Unhandled` 和 `Fatal`，不能只依据终端最后一行。
- 如果 `TargetMakefile.Load` 报 `BinaryArchiveReader`/`Makefile.bin` 读取错误，先让单个 UBT 进程重建该 makefile；不要并行启动第二个构建，也不要直接删除整个 `Intermediate`、`Binaries` 或 `DerivedDataCache`。
