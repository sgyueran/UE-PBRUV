# Task 4 Report — 母材质与材质实例

## 结论

Task 4 通过。已在插件内容中按引擎次版本生成参数化 Metallic/Roughness 母材质，并用 Task 1 核验过的 `UMaterialInstanceConstant` Editor-only setter 保存实例。三版本 Development Editor 编译退出码 0；`PBRTextureLab` Automation 各 15/15 Success；各版本第二次 `UnrealEditor-Cmd` 进程单独跑 `PBRTextureLab.Material.ReloadAfterRestart` 均为 1/1 Success。未实现 UV 命令或 Editor UI。这不是完整 OpenPBR。

## 前置依赖

Task 1、Task 3。

## 修改文件

- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabMaterial.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabMaterial.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabMaterialTests.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/PBRTextureLabEditor.Build.cs`（增加 `MaterialEditor`）
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabPixelCoreTests.cpp`（unity 编译：`MakeSolid` → `MakePixelCoreSolid`）
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabTextureImportTests.cpp`（unity 编译：`MakeSolid` → `MakeImportSolid`）
- `.gitignore`（忽略 `PBRTextureLabHost/Plugins/PBRTextureLab/Content/` 生成的母材质）
- `reports/checkpoint.json`

## 材质合同

- 必须在 Game Thread 调用。`bCancelled=true` 立即返回 `Cancelled`，不写盘。
- 母材质路径：`/PBRTextureLab/Materials/M_PBRTextureLabMR_<主版本><次版本>`。5.6 / 5.7 / 5.8 各写各的 uasset；较新引擎保存的包不能被较旧引擎完整加载。
- 参数名固定：`BaseColorTexture`、`NormalTexture`、`ORMTexture`、`HeightTexture`、`NormalStrength`、`HeightAmount`、`UVScale`。
- ORM：R=AO、G=Roughness、B=Metallic。
- Height 走 `BumpOffset`；默认 `HeightAmount=0`，关闭视差。不改网格。
- `NormalStrength` 在平坦法线 `(0,0,1)` 与采样法线之间 lerp，再 normalize。
- `UVScale` 乘 UV0；`TextureSample` 的库针脚名是 `UVs`（不是 `Coordinates`）。
- 实例用 Task 1 的 `SetTextureParameterValueEditorOnly` / `SetScalarParameterValueEditorOnly`，`FScopedTransaction` 包住创建。
- 冲突策略：`Cancel` / `Replace`（卸掉旧包后重建，避免加载仍引用已删跨版本母材质的残留实例）/ `UniqueName`。
- 若磁盘上已有不能完整加载的母材质包：`ResetLoaders` + `UPackageTools::UnloadPackages` + 删除伴随文件，再 `MarkAsFullyLoaded` 后新建。
- 5.8 `RecompileMaterial` 返回错误数组；5.6/5.7 该 API 为 `void`，编译错误不在此路径采集。
- 不声称实现完整 OpenPBR。

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
| UE 5.8 | `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 32.58s | `reports/build-ue58-task4.log` |
| UE 5.7 | `C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 5.39s（最终增量；最终代码已编译） | `reports/build-ue57-task4.log` |
| UE 5.6 | `C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 32.44s | `reports/build-ue56-task4.log` |

5.6 仍警告 MSVC 14.44 不是首选 14.38。编译已通过。

### Automation Tests（同进程）

```
"<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "...\PBRTextureLabHost.uproject" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput -AbsLog="<reports>/test-ueXX-task4.log" -ReportExportPath="<reports>/automation-ueXX-task4" -ExecCmds="Automation RunTests PBRTextureLab;Quit"
```

套件：Task 2 的 6 个 PixelCore 用例 + Task 3 的 5 个 Import 用例 + `PBRTextureLab.Material.{ParentCompiles,CreateInstance,CancelAndConflict,ReloadAfterRestart}`。

| 引擎 | 发现 | 通过 | 失败 | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---|
| UE 5.8 | 15 | 15 | 0 | 0 | `reports/automation-ue58-task4/index.json` |
| UE 5.7 | 15 | 15 | 0 | 0 | `reports/automation-ue57-task4/index.json` |
| UE 5.6 | 15 | 15 | 0 | 0 | `reports/automation-ue56-task4/index.json` |

覆盖：母材质 Surface/Opaque/DefaultLit；默认参数（含 `HeightAmount=0`）；实例绑定 Task 3 贴图与标量；显式取消；命名冲突不覆盖；磁盘存在。

### Editor 重启后加载（第二进程）

先跑完 `CreateInstance` 并退出，再启动新的 `UnrealEditor-Cmd`：

```
-ExecCmds="Automation RunTests PBRTextureLab.Material.ReloadAfterRestart;Quit"
```

`LoadObject<UMaterialInstanceConstant>` 读取 `/Game/PBRTextureLab/Automation/T4Inst<主版本><次版本>_Inst`，核验 Parent 与贴图/标量。日志中有 `FlushAsyncLoading`。

| 引擎 | 发现 | 通过 | 失败 | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---|
| UE 5.8 | 1 | 1 | 0 | 0 | `reports/automation-ue58-task4-restart/index.json` |
| UE 5.7 | 1 | 1 | 0 | 0 | `reports/automation-ue57-task4-restart/index.json` |
| UE 5.6 | 1 | 1 | 0 | 0 | `reports/automation-ue56-task4-restart/index.json` |

## 失败项

无。早期 5.7 曾因共用 `M_PBRTextureLabMR.uasset`（5.8 写入）出现“包只加载了一部分、无法保存”；已改为按引擎次版本分文件，并在重建前卸载半加载包。最终三版本 15/15 + 重启 1/1。

## 已知限制

- 母材质在首次 `GetOrCreateParentMaterial` 时写入插件 Content，不进 Git。
- 5.6/5.7 无法从 `RecompileMaterial` 取回错误字符串。
- 未做 Task 5 UV、Task 6 菜单/选择确认。
- 不是 OpenPBR。
