# Overnight P3/P4 — 预览、社区对照、文档

日期：2026-08-27

## 3D 预览（球体 / 立方体 / 平面）

- 默认仍是 **球体**。
- 网格解析抽成 `PBRTextureLabResolvePreviewMesh`：先 `UThumbnailManager` 的 `EditorSphere/Cube/Plane`，再 `/Engine/EngineMeshes/{Sphere,Cube}` 与 `/Engine/BasicShapes/Plane`，平面再回退 `/Engine/EngineMeshes/Plane`。
- **仅当 `FApp::CanEverRender()`** 才 `SNew(SPBRTextureLabPreviewViewport)`。`-NullRHI` 显示占位：「当前环境无法显示 3D 预览」。避免 5.7 TypedElement 断言。
- 新增 `PBRTextureLab.Preview.Primitives`：三形状均可解析且互不相同；无 RHI 时断言不能创建 viewport。

未验证：交互 Editor 里切形状后的像素/光照观感。

## 贴图开关与静态开关

无新绑定语义。生成：`ExportFlags` → `EnabledMaps`；组装：有贴图才开。静态开关继续用 MIC `SetStaticSwitchParameterValueEditorOnly`，禁止循环 `UMaterialEditingLibrary` setter。见 `reports/09-known-limitations.md`。

## 一键赋材质

`PBRTextureLab.Command.AssignMaterial` 已覆盖：空选、空材质、只改选中 Actor、未选中不变。三版本最终套件均 Success。未再加交互点击用例。

## 社区对照（只参考，未复制代码）

| 来源 | 可借鉴的安全模式 | 我们不采用 |
|---|---|---|
| Fab **PBR Forge** | 编辑器内单图出 Normal/Rough/Metal/AO；可调强度 | 在线/一键 ORM 作为默认管线（本产品已取消 ORM） |
| Epic 论坛 **SHADERSOURCE Texture Tools** | 2D+3D 预览、队列、无缝、声明 Editor 模块 | ComfyUI / 生成式 AI |
| AITEXTURED / PLAYTEX 等网页工具 | 单图 → 对齐的 PBR 通道；明确 Metallic 不可靠 | 联网、第三方像素核 |
| Fab **UV-Packer** | Editor-only、Static Mesh、事务 | 改 Lightmap / 自动打包 UV 岛 |
| **Auto Texture Tiling Pro** | 缩放网格后纹理密度 | 世界对齐/材质侧动态 UV 替代网格 UV0 绝对 100/500 |
| Epic **UV Editor**（文档） | 绝对/相对变换要分开；默认不碰未选通道 | UVEditor Beta 依赖 |

本插件保持：Editor-only、离线像素核、Metallic 免责声明、UV0 绝对缩放+基准指纹、冲突默认不覆盖、Undo 事务。

## 文档

- 仓库根 `README.md`：安装、母球、开关、UV 快捷键、限制。
- `reports/README.md`：报告索引。
