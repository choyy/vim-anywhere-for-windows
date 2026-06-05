# Vim Anywhere for Windows

在任何应用中选中文字，按下 `Ctrl+Alt+E`，用 Vim 编辑后保存退出，选中的文字自动替换。

## 使用

1. 安装 [Vim](https://www.vim.org/download.php)（gvim）
2. 运行 `vim-anywhere-for-windows.exe`，程序驻留系统托盘
3. 在任何应用中选中文字，按下 `Ctrl+Alt+E`
4. gvim 打开，编辑后 `:wq` 保存退出，原文自动替换

右键托盘图标可退出程序。

## 构建

### 依赖

- [xmake](https://xmake.io)
- Visual Studio
- Windows SDK

### 编译

```powershell
xmake
```

输出：`build\windows\x86\release\vim-anywhere-for-windows.exe`

## 原理

```
Ctrl+Alt+E (RegisterHotKey)
  → Ctrl+C (SendInput，先释放 Alt 避免冲突)
  → 读剪贴板 → 写临时文件
  → 启动 gvim (WaitForSingleObject 阻塞)
  → 读临时文件 → 写剪贴板
  → Ctrl+V (SendInput)
  → 恢复剪贴板
```
