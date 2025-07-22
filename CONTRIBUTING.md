# Contributing to PICO Radar

我们非常欢迎社区的贡献！为了保持代码库的整洁、可读和高质量，我们建立了一套自动化的代码规范和静态分析流程。请在开始编码前，花几分钟时间配置您的开发环境。

## 1. 编码风格

本项目遵循 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)。

我们使用 `clang-format` 来自动格式化代码，确保风格一致。配置文件是根目录下的 `.clang-format`。

## 2. 代码质量

我们使用 `clang-tidy` 进行静态分析，以捕捉潜在的bug、性能问题和不符合现代C++实践的代码。配置文件是根目录下的 `.clang-tidy`。

## 3. 自动化环境设置 (必需)

我们使用 `pre-commit` 框架来自动化上述工具的执行。在您每次提交代码时，它会自动运行 `clang-format` 和 `clang-tidy`，并自动修复大部分问题。

请按照以下步骤在您的本地仓库中设置它：

### 步骤 1: 安装 pre-commit

如果您还没有安装 `pre-commit`，请通过 pip 安装：

```bash
pip install pre-commit
```

### 步骤 2: 安装 Git 钩子

在项目根目录下运行以下命令，它会在您的本地 `.git/hooks` 目录中安装钩子脚本：

```bash
pre-commit install
```

### 步骤 3: 生成编译数据库

`clang-tidy` 需要一个名为 `compile_commands.json` 的文件来了解如何编译项目。我们的CMake配置会自动生成这个文件。您只需要像往常一样配置项目即可：

```bash
# 在项目根目录下
mkdir -p build
cd build
cmake ..
```

完成以上步骤后，您的环境就配置好了！现在，当您运行 `git commit` 时，代码会自动被格式化和检查。

## 4. 手动运行检查

如果您想在提交前对所有文件手动运行一次检查，可以使用以下命令：

```bash
pre-commit run --all-files
```

## 5. 内存安全性检查

为了确保代码的内存安全，我们集成了 `Valgrind` 进行动态内存分析。

### 运行内存检查

我们提供了一个方便的脚本来自动化内存检查的流程。在项目根目录下运行：

```bash
./scripts/run_memcheck.sh
```

这个脚本会：
1. 创建一个单独的构建目录 `build_memcheck`。
2. 使用 `Debug` 模式和 Valgrind 支持来配置 CMake。
3. 构建项目。
4. 使用 CTest 和 Valgrind 运行所有测试，并报告任何内存泄漏或错误。

**注意:** 运行内存检查会比普通测试慢很多，这是正常现象。

感谢您的贡献！
