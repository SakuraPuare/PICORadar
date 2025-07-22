---
title: "DevLog-12-覆盖率工具迁移记：从 lcov 到 gcovr"
date: 2024-06-09
---

# 覆盖率工具迁移记：从 lcov 到 gcovr

作者：书樱

## 背景

在 PICO Radar 项目的持续集成和质量保障体系中，代码覆盖率一直是我们衡量测试充分性的重要指标。最初我们采用了 lcov + genhtml 方案，但随着项目复杂度提升和 CI 场景的多样化，lcov 的一些局限性逐渐显现。为此，我们决定将覆盖率工具全面迁移到 gcovr。

## 为什么要迁移？

- **兼容性更好**：gcovr 原生支持多平台，且对 Python 环境友好，易于在 CI/CD 环境下部署。
- **输出格式丰富**：gcovr 支持 HTML、XML、JSON、文本等多种报告格式，方便与各种工具链集成。
- **配置简单**：无需手动收集和过滤 info 文件，命令行参数灵活，易于维护。
- **社区活跃**：gcovr 文档完善，社区活跃，遇到问题更容易获得支持。

## 迁移步骤

### 1. 脚本替换

原 `scripts/generate_coverage_report.sh` 主要逻辑如下：

```bash
# lcov 方案片段
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '*/test/*' ... --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory html/
lcov --list coverage_filtered.info > summary.txt
```

迁移后，全部替换为 gcovr：

```bash
gcovr . \
  --root .. \
  --exclude '../test/.*' \
  --html-details html/index.html \
  --xml coverage.xml \
  --json coverage.json \
  --print-summary > summary.txt
```

### 2. CMake 配置调整

原有 CMakeLists.txt 会查找 lcov/genhtml 并提示安装，现在只保留 `--coverage` 编译选项，并推荐用 gcovr：

```cmake
if(ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
    message(STATUS "建议使用 gcovr 生成覆盖率报告，已不再查找 lcov/genhtml。")
endif()
```

### 3. 文档与 README 更新

- 在 README.md、docs/COVERAGE.md 等文档中，统一说明覆盖率工具为 gcovr，给出安装和使用方法。
- ROADMAP.md 记录本次迁移为已完成事项。

### 4. CI/CD 兼容性验证

由于 gcovr 支持多种格式输出，方便后续与 GitHub Actions 等持续集成平台集成，极大提升了自动化能力。

## 迁移过程中的问题与经验

- **路径过滤**：gcovr 的 `--exclude` 支持正则，迁移时要注意路径写法与 lcov 有所不同。
- **依赖安装**：部分环境下需用 `pip install gcovr`，有时系统包管理器版本较旧，建议优先用 pip。
- **报告格式**：gcovr 的 HTML 报告美观且交互性强，XML/JSON 便于后续自动化分析。
- **CI 适配**：gcovr 在 GitHub Actions 上表现稳定，参数灵活，易于与 artifact 上传等步骤集成。

## 迁移后效果

- 覆盖率报告生成更快，格式更多样。
- 脚本和配置更简洁，维护成本降低。
- 新同学上手更容易，文档一致性提升。

## 结语

本次覆盖率工具的迁移，是 PICO Radar 项目工程化道路上的又一次进步。选择合适的工具，既是对团队效率的提升，也是对代码质量的保障。希望这份经验能为后来者提供参考。

——书樱 