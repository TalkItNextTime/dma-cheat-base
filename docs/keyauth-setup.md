# KeyAuth 接入准备

本仓库已经补齐 KeyAuth 接入起步环境，包含三部分：

1. 仓库级 MCP 配置文件：`/.mcp.json`
2. Codex 全局 MCP 安装脚本：`/scripts/install_keyauth_mcp.ps1`
3. 项目运行时授权配置骨架：`/project-d/Source/Security/KeyAuthConfig.hpp`

## 1. Codex MCP 配置

这台机器上的 Codex 全局配置文件实际位置是：

`C:\Users\Admin\.codex\config.toml`

由于当前工作沙箱不能直接改用户目录，仓库内提供了一键安装脚本：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install_keyauth_mcp.ps1
```

脚本会把以下配置写入 `config.toml`：

```toml
[mcp_servers.KeyAuth]
type = "stdio"
command = "cmd"
args = ["/c", "npx", "-y", "apidog-mcp-server@latest", "--site-id=447751"]
```

## 2. 项目运行时环境变量

仓库内模板文件：

`project-d/.env.keyauth.example`

当前约定的环境变量如下：

- `PROJECTD_KEYAUTH_ENABLED`
- `PROJECTD_KEYAUTH_API_BASE`
- `PROJECTD_KEYAUTH_APP_NAME`
- `PROJECTD_KEYAUTH_OWNER_ID`
- `PROJECTD_KEYAUTH_APP_SECRET`
- `PROJECTD_KEYAUTH_LICENSE_KEY`
- `PROJECTD_KEYAUTH_HARDWARE_ID`

默认 API 地址：

`https://keyauth.win/api/1.3/`

## 3. 当前代码骨架

`project-d/Source/Security/KeyAuthConfig.hpp` 已提供：

- 环境变量读取
- 布尔启用开关解析
- KeyAuth API 基础地址默认值
- 必填字段完整性判断

这份骨架是后续实装以下功能的固定入口：

- 启动前密钥校验
- 机器码绑定
- 到期时间查询
- 菜单内授权状态展示
- 无效授权阻断 DMA / SDK 初始化

## 4. 下一步落点

后续真正实装时，直接沿这条链路接入：

1. `main.cpp` 启动早期读取 `Security::KeyAuthConfig::FromEnvironment()`
2. 新增 `KeyAuthClient` 发起 `license` / `init` / `check` 请求
3. 新增 `LicenseGuard` 在 `config.Init()` 后、`dma.Init()` 前阻断未授权启动
4. Overlay 底栏显示授权状态、到期时间、当前版本
