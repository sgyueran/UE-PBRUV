# PBR Texture Lab

Unreal Engine **5.6–5.8** 的 Editor-only 插件：从一张图离线生成 Metallic/Roughness 工作流贴图，并对 Static Mesh 的 **UV0** 做绝对 `100×` / `500×` 缩放。

这不是完整 OpenPBR。Metallic 是启发式蒙版（全黑 / 常量 / 亮度阈值），不是物理金属度反演。

许可证：GPL-3.0。仓库：https://github.com/sgyueran/UE-PBRUV （默认分支 `develop`）。

## 安装

1. 把 `PBRTextureLabHost/Plugins/PBRTextureLab` 拷进目标工程的 `Plugins/`。
2. 用对应引擎的 `Build.bat` 编该工程的 Editor 目标。改插件后**退出 Editor** 再编（Live Coding 会挡住 UBT）。
3. 启用插件，窗口：**Tools → PBR Texture Lab**。另有 **PBR Asset Browser**。

本仓库的 `PBRTextureLabHost` 只用于编译和 Automation，不是游戏工程。

## 母球

插件内容 `/PBRTextureLab/Parents` 内置 13 个用户母球。界面下拉选择，默认优先 **`000基础材质`**。生成器文案 1:1 用母球自定义参数标题。

回退顺序：`/PBRTextureLab/Parents/000基础材质` → `/Game/材质/MAT`。不要改用户工程里的 MAT 原件。

在 UE 5.6/5.7 上，若 5.8 保存的母球 uasset 加载失败，请在下拉框或「其他母材质」里指定本机可用的母材质。

## 生成 / 组装

- **生成**：Texture2D 或本地 png/jpg/bmp/tga → Height / Normal / Roughness / Metallic / AO。可无缝（边缘融合、Wrap）。
- **组装**：把现成贴图组合成材质实例。
- 每材质独立文件夹，默认 `UniqueName`。
- **不再生成、导入或绑定 ORM。**
- 输出上限 4096。

### 子材质

插件**不覆盖** `基础色` / `粗糙度` / `高光度` / `金属度`。`法线强度` 默认 `0.1`。

取消勾选某通道：只关该通道 `*贴图开关` 和同通道静态开关。不写 `类型切换` / `反向`。未勾选不改法线/粗糙/金属强度；置换没有开关时把 `置换强度` 写成 0。

`000基础材质` 没有 `法线贴图开关`，法线靠静态开关 `法线`。

实例编辑器里未勾选通道会显示为**已覆盖且值为关**（左边勾、值关）。这是 MIC 覆盖语义，不是又打开了。

## UV 100 / 500

`NewUV = BaselineUV * (100,100)` 或 `(500,500)`。基准存在 `UPBRTextureLabUVPresetData`，100↔500 不累乘。默认只改 LOD0，可选同步其他 LOD。不改 Lightmap / 其它 UV 通道 / Skeletal Mesh。

| 命令 | 默认快捷键 |
|---|---|
| Set UV Tiling 100x100 | Ctrl+Alt+1 |
| Set UV Tiling 500x500 | Ctrl+Alt+5 |

可在 Editor Preferences 重绑。入口：Tools 菜单、工具栏、Content Browser 右键、Nomad Tab。网格被多处引用时会提示：复制并重绑 / 改源资产 / 取消。支持 Undo/Redo。

## 预览

左右分栏：左参数/UV，右 2D 通道 + 3D 预览。3D 可切球体 / 立方体 / 平面。无 GPU / `-NullRHI` 时 3D 预览关闭，避免 Editor viewport 崩溃。

生成后可跳转子材质目录，并把最近材质赋给选中的 Static Mesh Actor。

## 验证

过夜回归（2026-08-27）在 UE 5.6 / 5.7 / 5.8 上 `PBRTextureLab` Automation 失败项为 0。详见 `reports/08-overnight-regression-ue56-57-58.md`。
