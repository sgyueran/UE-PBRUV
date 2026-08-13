# Task 6 — 选择、复制与命令入口

## 依赖

Task 1、Task 5 完成。

## 目标

将 UV 预设接入 Unreal Editor 的选择、菜单和快捷键系统。

## 实施

- 注册无默认快捷键、可重绑的命令：`PBRTextureLab.UVScale100`、`PBRTextureLab.UVScale500`。
- 接入 Level Editor 菜单/工具栏和 Content Browser 右键菜单。
- 支持 Static Mesh Actor、Static Mesh Component 和 Static Mesh 资产选择。
- 每次操作提供三项：复制网格并仅重绑当前组件、直接修改源资产、取消。
- 复制模式不得影响其他 Actor；直接修改要显示共享实例影响数。
- 空选择、混合选择和非 Static Mesh 给出明确提示。

## 验收

- 命令出现在 Keyboard Shortcuts 并可绑定。
- 复制/直接修改/取消语义正确。
- 不改动未选择对象。

