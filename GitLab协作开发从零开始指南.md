# 极狐 GitLab 协作开发从零开始指南

适用项目：<https://jihulab.com/lfq43-group/FMS>

这份文档写给第一次使用 Git / 极狐 GitLab 的小组成员。按顺序做完后，大家就可以在同一个项目里协作开发，而不需要先互相加好友。

## 1. 项目负责人：先邀请成员

项目负责人登录极狐 GitLab 后：

1. 打开项目：<https://jihulab.com/lfq43-group/FMS>
2. 进入左侧菜单的 **管理 / 项目信息 / 成员** 页面。不同版本界面文字可能略有差异，核心入口是“成员”。
3. 点击 **邀请成员**。
4. 输入成员的极狐 GitLab 用户名或邮箱。
5. 选择角色：
   - **Developer**：推荐给普通开发成员，可以拉取、推送分支、创建合并请求。
   - **Maintainer**：只给项目负责人或需要管理仓库设置的人。
   - **Guest / Reporter**：通常不适合开发协作，权限较少。
6. 点击邀请。

如果成员暂时没有账号，让成员先注册极狐 GitLab 账号，再把用户名或注册邮箱发给项目负责人。

## 2. 成员：安装 Git

### Windows

1. 下载并安装 Git：<https://git-scm.com/download/win>
2. 安装时大部分选项保持默认即可。
3. 安装完成后，打开 **Git Bash** 或 PowerShell，输入：

```bash
git --version
```

能看到版本号就说明安装成功。

### macOS

打开终端，输入：

```bash
git --version
```

如果系统提示安装 Command Line Tools，按提示安装即可。

### Linux

Ubuntu / Debian：

```bash
sudo apt update
sudo apt install git
```

CentOS / Fedora：

```bash
sudo dnf install git
```

## 3. 第一次使用 Git：配置姓名和邮箱

每位成员只需要配置一次：

```bash
git config --global user.name "你的名字"
git config --global user.email "你的邮箱"
```

查看配置是否成功：

```bash
git config --global --list
```

建议邮箱使用注册极狐 GitLab 的邮箱，这样提交记录更容易和账号对应。

## 4. 配置 SSH 密钥

SSH 密钥可以让你不用每次输入账号密码，也更适合日常开发。

### 4.1 检查是否已有 SSH 密钥

在终端输入：

```bash
ls ~/.ssh
```

如果能看到类似 `id_ed25519.pub` 或 `id_rsa.pub` 的文件，可以继续使用已有密钥。

### 4.2 生成新的 SSH 密钥

如果没有密钥，输入：

```bash
ssh-keygen -t ed25519 -C "你的邮箱"
```

一路回车即可。如果系统不支持 `ed25519`，可以改用：

```bash
ssh-keygen -t rsa -b 4096 -C "你的邮箱"
```

### 4.3 复制公钥

Windows Git Bash / macOS / Linux：

```bash
cat ~/.ssh/id_ed25519.pub
```

如果你生成的是 RSA 密钥：

```bash
cat ~/.ssh/id_rsa.pub
```

复制输出的整行内容，从 `ssh-ed25519` 或 `ssh-rsa` 开始，到邮箱结束。

### 4.4 添加到极狐 GitLab

1. 登录极狐 GitLab。
2. 点击右上角头像。
3. 进入 **编辑配置文件**。
4. 进入 **访问权限 > SSH 密钥**。
5. 点击 **添加新密钥**。
6. 粘贴刚才复制的公钥。
7. 标题可以写自己的电脑名称，例如 `Lenovo Laptop`。
8. 保存。

### 4.5 测试 SSH 是否可用

```bash
ssh -T git@jihulab.com
```

第一次连接会询问是否信任主机，输入 `yes`。如果看到欢迎或认证成功相关提示，就说明配置好了。

## 5. 克隆项目到本地

选择一个你想放代码的目录，例如桌面或工作目录，然后执行：

```bash
git clone git@jihulab.com:lfq43-group/FMS.git
cd FMS
```

如果 SSH 暂时配置失败，也可以用 HTTPS：

```bash
git clone https://jihulab.com/lfq43-group/FMS.git
cd FMS
```

但日常开发更推荐 SSH。

## 6. 推荐协作流程

不要直接在 `main` 或 `master` 分支上开发。每个功能、修复或实验都新建一个自己的分支。

### 6.1 每次开始写代码前，先更新主分支

先确认当前分支：

```bash
git branch
```

切换到主分支：

```bash
git switch main
```

如果项目主分支叫 `master`，则使用：

```bash
git switch master
```

拉取最新代码：

```bash
git pull
```

### 6.2 新建自己的开发分支

分支命名建议：

```text
feature/姓名或缩写-功能名
fix/姓名或缩写-问题名
docs/姓名或缩写-文档名
```

示例：

```bash
git switch -c feature/zhangsan-login
```

### 6.3 写代码后查看改动

```bash
git status
```

查看具体改动：

```bash
git diff
```

### 6.4 提交代码

添加要提交的文件：

```bash
git add 文件名
```

如果确认所有改动都要提交：

```bash
git add .
```

提交：

```bash
git commit -m "feat: 完成登录页面"
```

常用提交信息前缀：

- `feat:` 新功能
- `fix:` 修复问题
- `docs:` 文档修改
- `style:` 格式调整，不影响功能
- `refactor:` 重构
- `test:` 测试相关
- `chore:` 工程配置或杂项

### 6.5 推送分支到极狐 GitLab

第一次推送新分支：

```bash
git push -u origin feature/zhangsan-login
```

之后同一分支继续推送：

```bash
git push
```

## 7. 创建合并请求 Merge Request

代码推送后，打开项目网页：<https://jihulab.com/lfq43-group/FMS>

通常页面会提示你为刚推送的分支创建 Merge Request。如果没有提示：

1. 进入左侧 **合并请求 / Merge Requests**。
2. 点击 **新建合并请求**。
3. 源分支选择你自己的开发分支，例如 `feature/zhangsan-login`。
4. 目标分支选择 `main` 或项目实际主分支。
5. 标题写清楚这次改了什么。
6. 描述里建议写：
   - 做了什么
   - 怎么测试
   - 有没有需要别人重点看的地方
7. 提交合并请求。

推荐描述模板：

```markdown
## 做了什么
- 

## 怎么测试
- 

## 需要注意
- 
```

## 8. 代码评审和合并

推荐规则：

1. 普通成员开发完成后创建 Merge Request。
2. 至少让一位组员或负责人看一遍代码。
3. 有问题就在 Merge Request 页面评论或继续提交修复。
4. 通过后由项目负责人或有权限的成员合并。
5. 合并后删除已经完成的功能分支，保持仓库整洁。

## 9. 每天开发的标准动作

每天开始：

```bash
git switch main
git pull
git switch -c feature/你的名字-今天任务
```

开发过程中经常看状态：

```bash
git status
```

完成一个小阶段就提交：

```bash
git add .
git commit -m "feat: 简短说明"
git push -u origin 当前分支名
```

提交 Merge Request 后等待评审。

## 10. 遇到冲突怎么办

如果拉取或合并时出现 conflict，说明你和别人改了同一个地方。

常见处理流程：

```bash
git status
```

打开冲突文件，你会看到类似内容：

```text
<<<<<<< HEAD
当前分支的内容
=======
别人分支的内容
>>>>>>> 分支名
```

手动修改成最终想保留的内容，并删除 `<<<<<<<`、`=======`、`>>>>>>>` 这些标记。

然后：

```bash
git add 冲突文件名
git commit
git push
```

如果不确定怎么处理，不要乱删代码，先把冲突文件截图或发给负责人一起看。

## 11. 常见问题

### 11.1 `Permission denied`

通常是 SSH 密钥没有配置好，检查：

```bash
ssh -T git@jihulab.com
```

如果失败，重新检查第 4 节。

### 11.2 `Repository not found`

可能原因：

- 项目地址写错。
- 没有被邀请进项目。
- 登录的极狐 GitLab 账号不是被邀请的账号。

### 11.3 `git pull` 前提示有本地修改

先看状态：

```bash
git status
```

如果这些修改要保留，先提交：

```bash
git add .
git commit -m "wip: 保存当前进度"
```

再拉取：

```bash
git pull
```

### 11.4 不小心在主分支写了代码

不要慌，先新建分支保住当前改动：

```bash
git switch -c feature/你的名字-补救分支
```

然后正常提交和推送：

```bash
git add .
git commit -m "feat: 保存误写在主分支上的改动"
git push -u origin feature/你的名字-补救分支
```

之后再创建 Merge Request。

## 12. 团队约定建议

建议小组统一以下规则：

1. 主分支只放稳定代码，不直接推送。
2. 每个任务一个分支。
3. 分支名写清楚是谁做什么。
4. 提交信息要能看懂。
5. 合并前至少自己运行一遍项目或测试。
6. 大改动先在群里说一声，避免多人同时改同一批文件。
7. 不提交无关文件，例如本地缓存、临时文件、IDE 私人配置。

## 13. 最小命令速查

```bash
# 克隆项目
git clone git@jihulab.com:lfq43-group/FMS.git

# 查看当前状态
git status

# 切换主分支并更新
git switch main
git pull

# 新建分支
git switch -c feature/your-name-task

# 提交
git add .
git commit -m "feat: describe your change"

# 推送
git push -u origin feature/your-name-task
```

## 参考资料

- 极狐 GitLab SSH 密钥文档：<https://gitlab.cn/docs/jh/user/ssh/>
- GitLab Merge Request 文档：<https://docs.gitlab.com/user/project/merge_requests/>
- GitLab 创建 Merge Request 文档：<https://docs.gitlab.com/user/project/merge_requests/creating_merge_requests/>
