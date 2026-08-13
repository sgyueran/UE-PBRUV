# Task 3 Report — UE 纹理资产导入

## 结论

Task 3 通过。已把 Task 2 的像素结果经 ImageCore/ImageWrapper 编码为 PNG，写入 `Saved/PBRTextureLab/Staging`，再用 Task 1 核验过的 `UAssetImportTask` / `IAssetTools::ImportAssetTasks` 导入为可保存的 `UTexture2D`。三版本 Development Editor 编译退出码 0；`PBRTextureLab` Automation 各 11/11 Success；各版本第二次 `UnrealEditor-Cmd` 进程单独跑 `PBRTextureLab.Import.ReloadAfterRestart` 均为 1/1 Success。未创建材质实例。

## 前置依赖

Task 1、Task 2。

## 修改文件

- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabTextureImport.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabTextureImport.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabTextureImportTests.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/PBRTextureLabEditor.Build.cs`（增加 `ImageCore`、`ImageWrapper`）
- `.gitignore`（忽略 `PBRTextureLabHost/Content/PBRTextureLab/` 测试资产）
- `reports/checkpoint.json`

## 导入合同

- 必须在 Game Thread 调用。`bCancelled=true` 立即返回 `Cancelled`，不写盘。
- PNG 用 `IImageWrapperModule::CompressImage(EImageFormat::PNG)`；临时目录为 `Saved/PBRTextureLab/Staging/<guid>/`，RAII 清理。
- 工厂为 `UTextureFactory`，`bCreateMaterial=0`，`bAutomated=true`，`bAsync=false`，`bSave=false`；导入后再设压缩/sRGB 并按请求保存。
- 通道：BaseColor=`TC_Default`+sRGB；Normal=`TC_Normalmap`+非 sRGB；ORM=`TC_Masks`+非 sRGB；Height/AO/Roughness/Metallic=`TC_Grayscale`+非 sRGB。
- 冲突策略：`Cancel`（整批不导入）、`Replace`（调用方确认覆盖）、`UniqueName`（`CreateUniqueAssetName`）。
- `Replace` 时若已有包不能完全加载（跨引擎版本 uasset），删除磁盘文件后按新资产导入，避免“部分加载无法保存”。
- 导入失败只回滚本批新建资产，不删用户已有资产。整批包在 `FScopedTransaction` 中导入。

## 验证命令与结果

工作目录：`C:\Users\Administrator\Desktop\Plughins`  
工程：`PBRTextureLabHost\PBRTextureLabHost.uproject`  
工具链：VS 2022 Community，MSVC 14.44.35228，Windows SDK 10.0.22621.0

### 编译

```
"<Engine>\Engine\Build\BatchFiles\Build.bat" PBRTextureLabHostEditor Win64 Development -Project="C:\Users\Administrator\Desktop\Plughins\PBRTextureLabHost\PBRTextureLabHost.uproject" -WaitMutex
```

| 引擎 | Build.bat | 退出码 | 结果 | 耗时 | 日志 |
|---|---|---:|---|---|---|
| UE 5.8 | `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 32.77s | `reports/build-ue58-task3.log` |
| UE 5.7 | `C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 35.25s（首次全量）；磁盘上最终日志为增量 3.47s | `reports/build-ue57-task3.log` |
| UE 5.6 | `C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 36.19s | `reports/build-ue56-task3.log` |

5.6 仍警告 MSVC 14.44 不是首选 14.38。编译已通过。

### Automation Tests（同进程）

```
"<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "...\PBRTextureLabHost.uproject" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput -AbsLog="<reports>/test-ueXX-task3.log" -ReportExportPath="<reports>/automation-ueXX-task3" -ExecCmds="Automation RunTests PBRTextureLab;Quit"
```

套件：Task 2 的 6 个 PixelCore 用例 + `PBRTextureLab.Import.{HappyPath,CancelAndConflict,FailureCleanup,CreateAndSave,ReloadAfterRestart}`。

| 引擎 | 发现 | 通过 | 失败 | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---|
| UE 5.8 | 11 | 11 | 0 | 0 | `reports/automation-ue58-task3/index.json` |
| UE 5.7 | 11 | 11 | 0 | 0 | `reports/automation-ue57-task3/index.json` |
| UE 5.6 | 11 | 11 | 0 | 0 | `reports/automation-ue56-task3/index.json` |

覆盖：导入 7 张图、压缩/sRGB、staging 清理、显式取消、命名冲突不覆盖、非法输入/非法路径不留资产、磁盘存在、CreateAndSave 持久化。

### Editor 重启后加载（第二进程）

先跑完 `CreateAndSave` 并退出，再启动新的 `UnrealEditor-Cmd`：

```
-ExecCmds="Automation RunTests PBRTextureLab.Import.ReloadAfterRestart;Quit"
```

`LoadObject<UTexture2D>` 读取 `/Game/PBRTextureLab/Automation/T3Reload<主版本><次版本>_*`，并核验压缩与 sRGB。日志中有 `FlushAsyncLoading`。

| 引擎 | 发现 | 通过 | 失败 | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---|
| UE 5.8 | 1 | 1 | 0 | 0 | `reports/automation-ue58-task3-restart/index.json` |
| UE 5.7 | 1 | 1 | 0 | 0 | `reports/automation-ue57-task3-restart/index.json` |
| UE 5.6 | 1 | 1 | 0 | 0 | `reports/automation-ue56-task3-restart/index.json` |

## 失败项

无未解决的本插件编译失败或最终测试失败。

调试过程（已修复，不计入最终失败）：5.7 首次跑测时，5.8 写出的同名 uasset 自定义版本更新，5.7 只能部分加载，`SavePackages` 失败。已改为检测磁盘文件存在、`Replace` 时删除无法完整加载的包，测试资产按引擎版本分名（`T3Reload58` / `T3Reload57` / `T3Reload56`）。

## 已知限制

- 未创建材质实例（Task 4）。未做 UV / 菜单。
- `ImportAssetTasks` 会弹出 `FScopedSlowTask` 对话框；`-unattended` 下已跑通。
- 三版本顺序编译覆盖同一 `Intermediate/` / `Binaries/`。磁盘 DLL 属于最后一次成功的 UE 5.8 构建。
- Editor 无头跑测会改 `Saved/`，并可能写出 `DefaultInput.ini` / `AndroidFileServer` 配置；这些未纳入本 Task。
- 测试资产在 `Content/PBRTextureLab/`，已加入 `.gitignore`。

## 下一任务

Task 4 母材质与材质实例（依赖 1+3）。Task 5 仍可开始（只依赖 Task 1）。
