# Task 5 Report — UV 100×/500× 绝对预设

## 结论

Task 5 通过。已用 Task 1 核验过的 `GetMeshDescription` / `CommitMeshDescription` / `GetVertexInstanceUVs` / `FScopedTransaction` 实现 Static Mesh **仅 UV0** 的绝对 `100×`、`500×` 缩放。比例与 UV/拓扑指纹存在 `UAssetUserData` 上；`100→500→100` 从基准 UV 重写，不累计。三版本 Development Editor 编译退出码 0；`PBRTextureLab` Automation 各 20/20 Success；各版本第二次 Editor 进程 `PBRTextureLab.UV.ReloadAfterRestart` 均为 1/1 Success。未注册菜单或快捷键。

## 前置依赖

Task 1。未依赖 Task 2–4 的功能，但同模块一并编译/跑测。

## 修改文件

- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabUV.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabUVPresetData.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabUV.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabUVTests.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabEditorApi.h`（`ModifyStaticMeshDescription` / `IsStaticMeshDescriptionValid`）
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabMaterial.cpp`（unity：`SetError` 等改为 `Material*` 前缀）
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabTextureImport.cpp`（unity：`Import*` 前缀）
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabMaterialTests.cpp`（`PersistMaterialBaseName`）
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabTextureImportTests.cpp`（`PersistImportBaseName`）
- `reports/checkpoint.json`

## UV 合同

- 必须在 Game Thread 调用。`bCancelled=true` 立即返回 `Cancelled`，不改网格。
- 只改 Static Mesh 全部可编辑源 LOD 的 UV0。Lightmap 通道等于 UV0 时拒绝。不改 UV1+、Skeletal Mesh、Landscape。
- 首次成功应用把当前 UV0 存为基准（scale 1），再写成 `基准 × 预设`。之后 `100`/`500` 都从同一基准重写。
- `UPBRTextureLabUVPresetData`（`UAssetUserData`）记录 `AppliedScale`、每 LOD 的基准 UV、拓扑指纹、基准 UV 指纹。
- 当前 UV0 或拓扑与“基准 × 已应用比例”不一致时返回 `BaselineMismatch`，要求 `EstablishUVBaseline` 或 `bRebaseline=true`。
- `FScopedTransaction` + `Mesh->Modify()` + `ModifyMeshDescription`；Undo/Redo 恢复 UV 与元数据。
- 不使用 UVEditor Beta 或自动展开 API。

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
| UE 5.8 | `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 5.49s（最终增量；首次成功 5.86s） | `reports/build-ue58-task5.log` |
| UE 5.7 | `C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 34.61s | `reports/build-ue57-task5.log` |
| UE 5.6 | `C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 33.20s | `reports/build-ue56-task5.log` |

5.6 仍警告 MSVC 14.44 不是首选 14.38。编译已通过。

### Automation Tests（同进程）

```
"<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "...\PBRTextureLabHost.uproject" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput -AbsLog="<reports>/test-ueXX-task5.log" -ReportExportPath="<reports>/automation-ueXX-task5" -ExecCmds="Automation RunTests PBRTextureLab;Quit"
```

套件：既有 15 个用例 + `PBRTextureLab.UV.{AbsolutePresets,UndoRedo,CancelAndUnsupported,BaselineMismatch,ReloadAfterRestart}`。

| 引擎 | 发现 | 通过 | 失败 | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---|
| UE 5.8 | 20 | 20 | 0 | 0 | `reports/automation-ue58-task5/index.json` |
| UE 5.7 | 20 | 20 | 0 | 0 | `reports/automation-ue57-task5/index.json` |
| UE 5.6 | 20 | 20 | 0 | 0 | `reports/automation-ue56-task5/index.json` |

覆盖：双 LOD UV0 数值、`100→500→100` 无累计、UV1/Lightmap 不动、Undo/Redo、取消、空网格/空指针/Lightmap=UV0 安全失败、外部改 UV 后要求重设基准。

### Editor 重启后加载（第二进程）

```
-ExecCmds="Automation RunTests PBRTextureLab.UV.ReloadAfterRestart;Quit"
```

`LoadObject<UStaticMesh>` 读取 `/Game/PBRTextureLab/Automation/T5UV<主版本><次版本>`，核验 `AppliedScale==100` 且 UV0 仍为基准×100。日志中有 `FlushAsyncLoading`。

| 引擎 | 发现 | 通过 | 失败 | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---|
| UE 5.8 | 1 | 1 | 0 | 0 | `reports/automation-ue58-task5-restart/index.json` |
| UE 5.7 | 1 | 1 | 0 | 0 | `reports/automation-ue57-task5-restart/index.json` |
| UE 5.6 | 1 | 1 | 0 | 0 | `reports/automation-ue56-task5-restart/index.json` |

## 失败项

无。早期 5.8 曾把 `BaselineMismatch` / 不支持对象打成 `Error` 日志，Automation 把未声明的 Error 判失败；已改为 `Warning` 并在测试里 `AddExpectedMessagePlain`。

## 已知限制

- 未做 Task 6 菜单、快捷键、复制网格/改源资产确认。
- 不改 Lightmap 通道；若 Lightmap 就是 UV0 则拒绝。
- 基准 UV 存在资产的 `UAssetUserData` 里，大网格会增加资产体积。
- 不是 OpenPBR。
