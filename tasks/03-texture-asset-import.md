# Task 3 — UE 纹理资产导入

## 依赖

Task 1、Task 2 完成。

## 目标

将像素结果创建为可保存、可重载的 UE 纹理资产。

## 实施

- 用 ImageCore/ImageWrapper 编码结果；临时文件只写 `Saved/PBRTextureLab/Staging`。
- 使用 Task 1 已验证的 `UAssetImportTask` / `IAssetTools::ImportAssetTasks` 导入资产。
- 创建 BaseColor、Normal、Roughness、Metallic、AO、Height、ORM。
- 正确设置 sRGB 与压缩：BaseColor=Default+sRGB；Normal=Normalmap+非 sRGB；ORM=Masks+非 sRGB；灰度图=Grayscale/Masks+非 sRGB。
- 处理取消、命名冲突、导入失败和临时文件清理。
- 不创建材质实例。

## 验收

- Editor 重启后所有纹理可加载。
- 导入配置与通道语义正确。
- 失败路径不留下错误资产或无关文件。

