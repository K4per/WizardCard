# 仓库同步、图鉴与分支整理验收

日期：2026-10-07。用户授权同步远程、合并当前Alpha开发、编写卡池图鉴；随后要求核对两份内容侧材料，并把远程分支收敛为master与dev。

## 提交与分支

- `f658e48`：Alpha v1.0功能、内容、资源及30卡图鉴入库。
- `e503c43`：合并远程原main与Alpha开发分支，保留此前PR及完整历史。
- 用户将默认分支改为master后，检查原main与master/dev提交相同，再删除旧main。两个codex分支均确认是master祖先，清理名称后提交仍在历史中。
- 远程仅master与dev，默认master；本地也仅保留二者。CI触发分支与[分支约定](../branching.md)同步。

使用一次性SSH443连接配置完成推送，未更改用户全局Git配置，未强制推送、删除标签或改写提交。仓库地址：[K4per/WizardCard](https://github.com/K4per/WizardCard)。

## 图鉴与文档

[文字图鉴](../card-catalog.md)包含30张正式卡面、属性、费用、卡面效果、响应条件、构筑要点与九套预设牌表；[图文浏览版](../card-catalog.html)支持卡名/效果搜索及类型、稀有度、学派、基础资格组合筛选，点选详情保持显示。

生成器校验30卡配图存在、九套牌表各30张、同名限制和基础资格。浏览验收通过全部30张图片、组合筛选、持久详情、两种基础资格、空结果与390像素窄窗口无横向溢出；本机证据 `build/card-catalog-preview.png`、`build/check_catalog.cjs`。

[内容对接报告](content-handoff-review.md)逐项核对33个编号问题，区分9项文档笔误、23项历史/机制有意差异和1项历史哈希待核；当前指南、术语、历史基线标识和UI接入元数据已修正。未擅自恢复本次真人门槛、扩大手机平台范围、更改设计定稿权或素材许可。

## CI修复与结果

首次同步触发的[远程运行](https://github.com/K4per/WizardCard/actions/runs/37509650085)在Windows配置步骤失败，Linux依赖此任务而跳过。[诊断增强运行](https://github.com/K4per/WizardCard/actions/runs/37512387219)明确指出windows-latest没有Visual Studio 2022实例；现固定runner为windows-2022以匹配预设。另外核查发现json与Catch2原固定值不能从上游取得，本机FETCHCONTENT源码覆盖隐藏了这一独立问题。

固定值已改为实际通过本机测试的nlohmann/json v3.12.0 `55f93686c01528224f448c19128836e7df245f72`、Catch2 v3.8.1 `2b60af89e23d28eefc081bc930831ee9d45ea58b`，均已从上游确认存在；SFML固定提交不变。没有更改卡牌、引擎规则或复盘内容哈希。

最终[远程运行37512708820](https://github.com/K4per/WizardCard/actions/runs/37512708820)（提交`83167f7`）的三个任务均成功：Windows Release构建、完整测试与打包；Linux无窗口构建与完整测试；Linux重放Windows生成的普通、AI、教学三份记录。该提交的测试集合为179项；图鉴生成一致性检查同样通过。这是实际Linux与跨平台结果，不以本机Windows无窗口测试代替。

本机全新无窗口构建未使用源码覆盖：成功取得固定源码、Release构建及179项回归通过，83.04秒；证据为 `build/sync-clean-configure.log`、`build/sync-clean-build.log`、`build/sync-clean-tests.log`。后续合入的Godot路线和报告更新为文档变更，未改变上述已验证的核心与卡池。

## v1.5新增决定

用户明确采用Godot重构图形界面并保留内核。[新版路线](../plans/v1.5.md)已补充GDExtension桥接、现有用户数据兼容、Godot本地功能迁移及分层验收；LAN传输改为Godot TCP适配，联动与动画目标继续保留。当前尚未实现Godot迁移，手机平台仍未承诺。

源码同步不是建立版本标签或托管发行下载；现有Alpha ZIP仍是本地交付。此次未向任何协作者发送消息或代授仓库写入权限。
