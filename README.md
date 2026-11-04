# FuncGame

一个使用 Unreal Engine 5.8 和 C++ 实现的俯视函数攻击游戏原型。玩家选择难度后，输入以 `x` 为变量的表达式，发射沿函数轨迹飞行的子弹，防守逐波出现的敌人。

## 打开与游玩

1. 安装 Unreal Engine 5.8、Visual Studio C++ 工具链和 Git LFS。克隆仓库前执行 `git lfs install`，克隆后确认资源已完整下载。
2. 打开 `FuncGame.uproject`，按提示编译项目。在编辑器中点击“运行”。默认使用引擎的 Entry 关卡，场地在运行时创建；原有第三人称示例内容仍在项目中。
3. 选择简单或困难模式。输入框只填写 `y=` 后的内容；例如 `x`、`x^2`、`3sin(5x)`。游戏内的“函数帮助”介绍支持的函数、定义域和轨迹平移规则。

敌人触及左侧防线会导致失败。困难模式中，子弹三次撞到障碍物也会失败；简单模式的障碍物仍会消灭子弹，但不扣除失误次数。

## Unreal MCP

项目已为编辑器启用 UE 5.8 的 `ModelContextProtocol` 和 `AllToolsets` 插件。服务器可在编辑器控制台通过 `ModelContextProtocol.StartServer` 启动，也可在编辑器偏好设置中启用自动启动。默认本机地址是 `http://127.0.0.1:8000/mcp`。

在 Codex 使用的工作区根目录创建或合并 `.codex/config.toml`：

```toml
[mcp_servers.unreal_mcp]
url = "http://127.0.0.1:8000/mcp"
```

重新启动 Codex 会话后即可发现该服务器。服务器和工具集由运行中的 Unreal Editor 提供，编辑器关闭时连接不可用；编辑器的个人偏好设置不会提交到仓库。

## 配置

游戏参数位于 `Config/DefaultGame.ini` 的 `[/Script/FuncGame.FunctionGameMode]` 段。可调整敌人和子弹速度、子弹寿命与并发数、表达式和轨迹的计算预算，以及自动起点搜索范围。起点搜索使用一组固定候选值，`AutoStartSearchRange` 在 0 至 8 内限制候选范围；也可在游戏内手动输入起点。

`WaveComposition` 每项依次表示该波 1、2、3 级敌人的数量，默认五波合计 30 人。总敌人数随波次配置自动计算。每波至少需要两种等级，数量必须为非负整数；无效配置会回退到默认波次并在日志中提示。极端数值会受到保护性限制。

## 验证

Windows 上安装 Visual Studio C++ 工具链后，可在项目目录运行独立数学和规则测试：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Run-NativeTests.ps1
```

UE 自动化测试名称以 `FuncGame` 开头，可在编辑器的自动化测试界面运行，也可通过控制台命令 `Automation RunTests FuncGame` 运行。`FuncGame.World` 测试需要有已启动的游戏世界。

也可以在项目目录运行以下无图形测试（先把 `$ue` 设为本机 UE 5.8 安装目录）：

```powershell
& "$ue/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$PWD/FuncGame.uproject" /Engine/Maps/Entry -game -nullrhi -unattended -nosplash -nosound -DisablePlugins=AllToolsets,ModelContextProtocol "-ExecCmds=Automation RunTests FuncGame" "-TestExit=Automation Test Queue Empty"
```

此命令只在该次无图形测试中停用编辑器 MCP 插件；正常打开编辑器时 MCP 仍可使用。

当前实现覆盖游戏内帮助列出的基础数值函数和条件表达式；符号求导、积分、求和及更广泛的特殊函数尚未实现。轨迹采样无法安全判断连续性时会保守停止。
