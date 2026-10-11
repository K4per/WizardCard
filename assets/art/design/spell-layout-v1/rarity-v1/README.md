# 古金法术卡：稀有度宝石差分

使用内置 image_gen 编辑用户提供的基底，保持整体古金酒红风格与信息布局，以宝石颜色和主宝石切形区分五档。各版为独立 RGBA PNG，文字数值留空，卡框外部及插画窗中心透明。

| 稀有度 | 宝石设计 | 文件 |
|---|---|---|
| 普通 | 白色晶石，简洁切面 | [PNG](spell-common.png) |
| 罕见 | 绿色椭圆宝石 | [PNG](spell-uncommon.png) |
| 稀有 | 青色六边形晶石 | [PNG](spell-rare.png) |
| 史诗 | 紫色菱形晶石 | [PNG](spell-epic.png) |
| 传说 | 金黄色放射多面晶石 | [PNG](spell-legendary.png) |

装饰小宝石随稀有度配色。传说主宝石为金黄色，框体继续使用古金。AI 编辑保留整体布局，但各版边缘和纹理存在细微重绘，未作逐像素锁定；当前为独立完整卡框设计稿，不是可直接互换的宝石独立图层。没有替换运行资源。

生成记录见 [prompts.md](prompts.md)，尺寸及透明抽样见 [manifest.json](manifest.json)。
