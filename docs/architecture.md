# 架构

`wizard_core` 为纯 C++17：GameState、Command、Rules、EffectQueue、EffectResolver、GameView，以及 GameEngine、PendingDecision、StateMaintenance、GameEvent。

`wizard_content` 负责 JSON 定义校验、卡组、命令和复盘文件；`wizard_runtime` 负责 SFML 场景、布局、资源、动画和音频；`wizard_client` 组合三者。CLI 和测试不依赖 SFML。

所有输入通过 submit(actor, command)，在副本上验证并原子提交。状态不可由界面直接修改。驱动运行到下一个决策或终局；选择携带决策 ID，过期选择被拒绝。支付、完整效果、关联清理、状态检查严格区分。

实例使用稳定 ID，区域为唯一权威位置；阵法宿主关系仅在解析区有效，符文跟随法术。绑定荷载跟随实例，临时荷载独立存在。费用记录区分每次尝试，取消不退款。触发捕获来源与效果快照。

查看者投影只包含自己的手牌、双方公开区域与牌库/手牌数量。私有日志只给拥有者。客户端换决策者前遮挡手牌；离线本地进程拥有完整状态，不构成针对恶意本机用户的安全边界。

随机采用指定整数算法与拒绝采样 Fisher-Yates。复盘保存初始种子、两套卡组、所有已接受命令、版本和内容哈希；在匹配版本重放并核对规范状态摘要。界面动画不控制结算时序。
