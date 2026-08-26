# Overnight P2 — 已知限制：评估 / 文档 / 测试

日期：2026-08-27

## 000基础材质没有「法线贴图开关」

**性质**：母球资产约定，不是绑定器漏写。

`ClassifyParentMapChannel` 把名称含「法线」的参数归到 Normal 通道。`000基础材质` 用法线**静态开关** `法线`，没有标量 `法线贴图开关`。`BindGeneratedTextures` 会对通道静态开关调用 `SetStaticSwitchParameterValueEditorOnly`。

Automation 用 `UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue` 的 Global GET **读不到**该静态开关。`ReuseUserParent` 里 `MapSwitchOn` 已回退 `GetAllStaticSwitchParameterInfo` + `GetStaticSwitchParameterValue`。不要把 Global GET 失败当成开关没关。

**5.6/5.7**：bundled `000基础材质` LoadObject 失败，该真实母球分支被 skip。不要为此改用户 `D:\UE5.8\Content\材质\MAT` 原件，也不要在过夜任务里重存 uasset。

**处理**：文档化。不改母球图。

## MIC 覆盖语义（左边勾、值为关）

Epic 材质实例编辑器对每个可覆盖参数有两层勾选：外层 = 是否覆盖父级；内层 = 开关值。把父级默认「开」改成「关」**必须**覆盖，因此左边会勾、值为关。这不是又打开了。

Jay Versluis 等公开说明与此一致：Static Switch 在实例上是「覆盖勾 + 值勾」。无法在不覆盖的情况下把父级 true 变成 false。

**处理**：文档化。不尝试 ClearParameterValues 再写，那会丢其它覆盖。

## Live Coding 挡住 UBT

改插件 C++ 后若 Editor 开着 Live Coding，UBT 会失败。过夜回归全部用退出后的 `Build.bat` + `UnrealEditor-Cmd`。

**处理**：文档化。不要在插件里探测 Live Coding。

## 5.8 GetSamplerTypeForTexture 弃用

5.8 标 C4996，要求 `MaterialExpressionUtils::GetSamplerTypeForTexture`。该头文件在 5.6/5.7 **不存在**。

**处理**：`PBRTextureLabEditorApi.h` 内联 `PBRTextureLab::GetSamplerTypeForTexture`，5.8 走新 API，5.6/5.7 走 `UMaterialExpressionTextureBase`。5.8 最终编译日志无该 C4996。

## 5.6 -NullRHI UndoTransaction

见 `reports/08-overnight-regression-ue56-57-58.md`。产品路径仍用 `FScopedTransaction`；无人值守 5.6 不调用 `GEditor->UndoTransaction()`。5.7/5.8 仍测真实 Undo/Redo。
