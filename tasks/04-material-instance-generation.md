# Task 4 — 母材质与材质实例

## 依赖

Task 1、Task 3 完成。

## 目标

为生成的贴图创建 UE Metallic/Roughness 材质实例。

## 实施

- 在插件内容中提供固定参数化母材质。
- 参数名固定：`BaseColorTexture`、`NormalTexture`、`ORMTexture`、`HeightTexture`、`NormalStrength`、`HeightAmount`、`UVScale`。
- ORM 连线：R=AO、G=Roughness、B=Metallic。
- Height 使用可关闭 Bump Offset；不得修改网格。
- 用 Task 1 已验证的 `UMaterialInstanceConstant` Editor API 保存实例。
- 不声称实现完整 OpenPBR。

## 验收

- 实例正确引用 Task 3 资产。
- Editor 重启后参数仍有效。
- 材质编译无错误。

