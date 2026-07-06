# GitHub 协作开发从零开始指南

适用项目：FMS

这份文档写给项目组长和第一次使用 Git / GitHub 的小组成员。目标是让大家能在 GitHub 上完成协作开发：拉代码、开分支、提交、推送、发 Pull Request、评审和合并。

## 一、组长需要做什么

### 1. 创建 GitHub 组织或仓库

如果只是课程小组项目，组长可以直接在自己的 GitHub 账号下创建仓库。

推荐仓库名：

```text
FMS
```

创建仓库时建议：

1. 登录 GitHub：<https://github.com>
2. 点击右上角 `+`，选择 `New repository`。
3. Repository name 填 `FMS`。
4. 可见性根据课程要求选择：
   - `Private`：推荐，小组内部协作。
   - `Public`：所有人都能看到代码。
5. 如果本地已有项目，不要勾选 `Add a README file`，避免和本地代码冲突。
6. 点击 `Create repository`。

### 2. 把本地项目提交到 GitHub

如果当前本地目录还不是 Git 仓库，进入项目根目录后执行：

```bash
git init
git add .
git commit -m "chore: initial project"
```

然后把本地仓库关联到 GitHub。把下面地址替换成你自己的仓库地址：

```bash
git remote add origin git@github.com:你的用户名或组织名/FMS.git
git branch -M main
git push -u origin main
```

如果已经是 Git 仓库，可以先查看远程地址：

```bash
git remote -v
```

如果之前关联的是极狐，可以改成 GitHub：

```bash
git remote set-url origin git@github.com:你的用户名或组织名/FMS.git
git push -u origin main
```

### 3. 邀请组员加入仓库

组长不需要和组员互相关注，也不需要先加好友。只需要知道组员的 GitHub 用户名或邮箱。

操作步骤：

1. 打开 GitHub 仓库页面。
2. 进入 `Settings`。
3. 进入 `Collaborators and teams`。
4. 点击 `Add people`。
5. 输入组员 GitHub 用户名或邮箱。
6. 权限选择：
   - `Write`：推荐给普通开发成员，可以推送分支、提交 Pull Request。
   - `Maintain`：给需要管理 Issue、PR、分支规则的成员。
   - `Admin`：只给组长或非常可信的负责人。
7. 发送邀请。

组员需要登录 GitHub 接受邀请，接受后才能推送代码。

### 4. 设置分支保护

建议保护 `main` 分支，避免大家直接把未检查的代码推到主分支。

操作步骤：

1. 打开仓库 `Settings`。
2. 进入 `Branches`。
3. 在 `Branch protection rules` 中添加规则。
4. Branch name pattern 填：

```text
main
```

建议开启：

- Require a pull request before merging
- Require approvals，至少 1 人审批
- Require status checks to pass before merging，如果项目后续配置了自动测试
- Do not allow bypassing the above settings，组长也遵守规则时可开启

如果课程项目节奏很快，至少要开启“合并前必须 Pull Request”。

### 5. 约定团队开发规则

组长需要在群里明确这些规则：

1. 不直接在 `main` 分支写代码。
2. 每个人开发前先从 `main` 拉最新代码。
3. 每个任务新建一个分支。
4. 完成后提交 Pull Request。
5. 至少一名组员或组长看过后再合并。
6. 合并前自己先确认能编译运行。
7. 不提交本地构建产物、临时文件、个人 IDE 缓存。

### 6. 准备 `.gitignore`

Qt / CMake 项目通常不要提交这些内容：

```gitignore
build/
*.user
*.user.*
.qtcreator/
.qtc_clangd/
.vs/
.vscode/.browse.VC.db*
.vscode/ipch/
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
compile_commands.json
*.obj
*.pdb
*.ilk
*.exe
*.dll
*.lib
*.exp
```

注意：如果团队统一使用 VS Code 配置，可以提交 `.vscode/settings.json`、`.vscode/tasks.json`、`.vscode/launch.json`。但不要提交个人缓存文件。

### 7. 组长日常要做的事

组长主要负责维护协作秩序：

1. 分配任务和模块。
2. 确认每个任务对应一个分支。
3. 检查 Pull Request 内容是否清楚。
4. 合并前确认项目能构建。
5. 处理冲突时帮助组员判断保留哪部分代码。
6. 定期清理已经合并的分支。
7. 重要版本打 tag，例如：

```bash
git tag v0.1.0
git push origin v0.1.0
```

## 二、组员从零开始参与开发

## 1. 注册 GitHub 账号

打开 GitHub：<https://github.com>

注册账号后，把自己的 GitHub 用户名或注册邮箱发给组长，让组长邀请你进仓库。

## 2. 安装 Git

### Windows

下载并安装 Git：

<https://git-scm.com/download/win>

安装完成后，打开 Git Bash 或 PowerShell：

```bash
git --version
```

能看到版本号就说明安装成功。

### macOS

打开终端：

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

Fedora：

```bash
sudo dnf install git
```

## 3. 配置 Git 姓名和邮箱

每台电脑只需要配置一次：

```bash
git config --global user.name "你的名字"
git config --global user.email "你的邮箱"
```

查看配置：

```bash
git config --global --list
```

建议邮箱使用 GitHub 账号绑定的邮箱。

## 4. 配置 SSH 密钥

SSH 可以让你安全地推送代码，不需要每次输入账号密码。

### 4.1 检查是否已有 SSH 密钥

```bash
ls ~/.ssh
```

如果看到 `id_ed25519.pub` 或 `id_rsa.pub`，说明已有公钥。

### 4.2 生成 SSH 密钥

推荐：

```bash
ssh-keygen -t ed25519 -C "你的邮箱"
```

一路回车即可。

如果电脑不支持 `ed25519`：

```bash
ssh-keygen -t rsa -b 4096 -C "你的邮箱"
```

### 4.3 复制公钥

```bash
cat ~/.ssh/id_ed25519.pub
```

如果你生成的是 RSA：

```bash
cat ~/.ssh/id_rsa.pub
```

复制整行内容。

### 4.4 添加到 GitHub

1. 登录 GitHub。
2. 点击右上角头像。
3. 进入 `Settings`。
4. 进入 `SSH and GPG keys`。
5. 点击 `New SSH key`。
6. Title 写电脑名称，例如 `Laptop`。
7. Key 粘贴刚才复制的公钥。
8. 点击 `Add SSH key`。

### 4.5 测试 SSH

```bash
ssh -T git@github.com
```

第一次连接输入 `yes`。

如果看到类似成功认证的提示，就说明配置好了。

## 5. 克隆项目

等组长邀请你并且你接受邀请后，克隆仓库：

```bash
git clone git@github.com:组长用户名或组织名/FMS.git
cd FMS
```

如果暂时不用 SSH，也可以用 HTTPS：

```bash
git clone https://github.com/组长用户名或组织名/FMS.git
cd FMS
```

日常开发更推荐 SSH。

## 6. 开始开发前先更新主分支

进入项目目录后：

```bash
git switch main
git pull
```

如果提示没有 `main` 分支，先看当前分支：

```bash
git branch
```

有些项目主分支可能叫 `master`，但新项目建议统一用 `main`。

## 7. 新建自己的开发分支

不要直接在 `main` 分支写代码。

分支命名建议：

```text
feature/姓名或缩写-功能名
fix/姓名或缩写-问题名
docs/姓名或缩写-文档名
```

示例：

```bash
git switch -c feature/zhangsan-file-service
```

## 8. 修改代码并提交

查看改动：

```bash
git status
```

查看具体差异：

```bash
git diff
```

添加文件：

```bash
git add .
```

提交：

```bash
git commit -m "feat: add file service"
```

常用提交前缀：

- `feat:` 新功能
- `fix:` 修复问题
- `docs:` 文档
- `style:` 格式调整
- `refactor:` 重构
- `test:` 测试
- `chore:` 构建配置或杂项

## 9. 推送分支

第一次推送当前分支：

```bash
git push -u origin feature/zhangsan-file-service
```

之后继续推送：

```bash
git push
```

## 10. 创建 Pull Request

推送后打开 GitHub 仓库页面。

通常页面会提示你 `Compare & pull request`。点击它。

如果没有提示：

1. 进入仓库的 `Pull requests`。
2. 点击 `New pull request`。
3. base 选择 `main`。
4. compare 选择你的开发分支。
5. 填写标题和说明。
6. 点击 `Create pull request`。

PR 描述建议模板：

```markdown
## 做了什么
- 

## 怎么测试
- 

## 需要注意
- 
```

## 11. 代码评审和合并

PR 发出后：

1. 等组长或组员 review。
2. 如果别人提出修改意见，在本地继续改。
3. 改完后继续提交并推送到同一个分支。
4. GitHub 会自动更新这个 PR。
5. 审核通过后由组长或有权限的成员合并。

合并后可以删除远程分支，保持仓库整洁。

## 12. 遇到冲突怎么办

如果 PR 页面提示冲突，或本地拉取时出现 conflict，说明你和别人改到了同一部分代码。

先更新主分支：

```bash
git switch main
git pull
```

切回自己的分支：

```bash
git switch feature/你的分支名
```

把主分支合进来：

```bash
git merge main
```

如果出现冲突，打开冲突文件，会看到：

```text
<<<<<<< HEAD
你当前分支的内容
=======
main 分支的内容
>>>>>>> main
```

手动改成最终需要的内容，并删除这些冲突标记。

然后：

```bash
git add .
git commit
git push
```

如果不确定怎么处理，先不要乱删，发给组长一起看。

## 13. 每天开发的推荐流程

每天开始：

```bash
git switch main
git pull
git switch -c feature/你的名字-今天任务
```

开发中：

```bash
git status
git add .
git commit -m "feat: 简短说明"
```

推送：

```bash
git push -u origin 当前分支名
```

然后去 GitHub 创建 Pull Request。

## 14. 常见问题

### Permission denied

通常是 SSH 没配置好。

测试：

```bash
ssh -T git@github.com
```

失败就重新检查 SSH key 是否添加到 GitHub。

### Repository not found

可能原因：

- 仓库地址写错。
- 组长还没邀请你。
- 你还没接受邀请。
- 你登录的 GitHub 账号不是被邀请的账号。

### 不能 push 到 main

这是正常的。如果组长设置了分支保护，就不能直接推送到 `main`。请新建分支并提交 Pull Request。

### VS Code 里新增了 C++ 文件但编译不到

如果项目使用 CMake，新增 `.cpp/.h/.ui` 后要检查 `CMakeLists.txt` 是否已经加入目标。

加完后重新运行：

```text
CMake: Configure
```

再构建项目。

## 15. 最小命令速查

```bash
# 克隆
git clone git@github.com:组长用户名或组织名/FMS.git

# 更新主分支
git switch main
git pull

# 新建分支
git switch -c feature/your-name-task

# 提交
git status
git add .
git commit -m "feat: describe your change"

# 推送
git push -u origin feature/your-name-task
```

## 16. 推荐仓库设置总结

组长最终建议完成这些设置：

- 仓库创建完成并推送初始代码。
- 邀请所有组员为 Collaborator。
- 普通组员权限使用 `Write`。
- 保护 `main` 分支。
- 要求通过 Pull Request 合并。
- 准备 `.gitignore`。
- 明确分支命名和提交信息规则。
- 每次合并前至少确认项目能构建。

## 参考资料

- GitHub SSH 文档：<https://docs.github.com/authentication/connecting-to-github-with-ssh>
- GitHub 邀请协作者文档：<https://docs.github.com/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/inviting-collaborators-to-a-personal-repository>
- GitHub Pull Request 文档：<https://docs.github.com/pull-requests>
- GitHub 分支保护文档：<https://docs.github.com/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches>
