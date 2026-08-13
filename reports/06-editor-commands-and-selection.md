# Task 6 Report — 选择、复制与命令入口

## 结论

Task 6 通过。已注册无默认快捷键、可重绑的 `PBRTextureLab.UVScale100` / `PBRTextureLab.UVScale500`，接入 Level Editor 的 Tools 菜单、User 工具栏和 Content Browser 资产右键菜单。选择支持 Static Mesh Actor、Static Mesh Component 和 Static Mesh 资产。每次操作可复制网格并只重绑当前组件、直接改源资产，或取消。三版本 Development Editor 编译退出码 0；`PBRTextureLab` Automation 各 24/24 Success。未做 Nomad Tab 或贴图生成 UI。

## 前置依赖

Task 1、Task 5。

## 修改文件

- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabCommands.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabCommands.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabCommand.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabCommand.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabCommandTests.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabEditorModule.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabEditorModule.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabUV.h`（`bTransact`）
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabUV.cpp`
- `reports/checkpoint.json`

## 命令合同

- 命令上下文 `PBRTextureLab`，命令名 `UVScale100` / `UVScale500`，默认 `FInputChord()`，出现在 Editor Keyboard Shortcuts。
- 选择优先级：关卡里的 Static Mesh Actor/Component；若没有世界选择，再读 Content Browser 的 `UStaticMesh`。
- 空选择 → `EmptySelection`；只有非 Static Mesh → `Unsupported`；Static Mesh 与非 Static Mesh 混选 → `MixedSelection`。三者都打 Warning，不改资产。
- 交互对话框：`Yes` = 复制网格并只重绑选中组件；`No` = 改源资产（文案列出共享组件数）；`Cancel` = 取消。无人值守默认 Cancel。
- 复制用 Task 1 核验过的 `IAssetTools::DuplicateAsset` / `CreateUniqueAssetName`，只 `SetStaticMesh` 选中组件。未选中的 Actor 仍引用原资产。
- 直接修改调用 Task 5 `ApplyUVPreset`。整次命令包在一个 `FScopedTransaction` 里。
- 不改 Skeletal Mesh、Landscape、Lightmap UV。

## 验证命令与结果

工作目录：`C:\Users\Administrator\Desktop\Plughins`  
工程：`PBRTextureLabHost\PBRTextureLabHost.uproject`  
工具链：VS 2022 Community，MSVC 14.44.35228，Windows SDK 10.0.22621.0

跨版本共享 `Intermediate/` 时，5.8 会吃到 5.6/5.7 的 UHT 头。换引擎前已删除 Host/插件 `Intermediate/` 再编。

### 编译

```
"<Engine>\Engine\Build\BatchFiles\Build.bat" PBRTextureLabHostEditor Win64 Development -Project="C:\Users\Administrator\Desktop\Plughins\PBRTextureLabHost\PBRTextureLabHost.uproject" -WaitMutex
```

| 引擎 | Build.bat | 退出码 | 结果 | 耗时 | 日志 |
|---|---|---:|---|---|---|
| UE 5.8 | `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 36.04s（清 Intermediate 后全量）；磁盘日志为最终增量 14.20s | `reports/build-ue58-task6.log` |
| UE 5.7 | `C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 34.88s | `reports/build-ue57-task6.log` |
| UE 5.6 | `C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 33.86s | `reports/build-ue56-task6.log` |

5.6 仍警告 MSVC 14.44 不是首选 14.38。编译已通过。

### Automation Tests

```
"<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "...\PBRTextureLabHost.uproject" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput -AbsLog="<reports>/test-ueXX-task6.log" -ReportExportPath="<reports>/automation-ueXX-task6" -ExecCmds="Automation RunTests PBRTextureLab;Quit"
```

套件：既有 20 个用例 + `PBRTextureLab.Command.{Shortcuts,SelectionGuards,DuplicateAndModify,AssetSelection}`。

| 引擎 | 发现 | 通过 | 失败 | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---|
| UE 5.8 | 24 | 24 | 0 | 0 | `reports/automation-ue58-task6/index.json` |
| UE 5.7 | 24 | 24 | 0 | 0 | `reports/automation-ue57-task6/index.json` |
| UE 5.6 | 24 | 24 | 0 | 0 | `reports/automation-ue56-task6/index.json` |

覆盖：`FInputBindingManager` 能查到两条命令且默认和弦为空；Tools/User 工具栏/资产右键菜单可 Extend；空选/非 SM/混选有提示且不改 UV；复制只重绑选中 Actor；未选中 Actor 仍用原网格；直接改源资产影响共享实例；取消不改。

未单独开第二进程做命令重启：命令注册在模块启动时发生，不依赖磁盘资产。Task 5 的 `UV.ReloadAfterRestart` 仍在同套件里通过。

## 失败项

无。早期 `GEditor->SelectActor` 对模板关卡里新 spawn 的 Actor 打 “invalid flags” Warning；测试已改为 Task 1 的 `USelection::Select`。

## 已知限制

- 无人值守下 Prompt 默认 Cancel，避免卡住对话框。
- 复制要求源资产在 `/Game` 下。
- Content Browser 右键条目挂在通用 `AssetContextMenu` 上，非 Static Mesh 执行时会提示不支持。
- 未做 Task 7 Nomad Tab / 贴图生成 UI。
