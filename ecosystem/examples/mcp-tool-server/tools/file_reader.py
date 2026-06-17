# 文件读取工具 — MCP 工具示例
# 演示如何实现一个 MCP 工具：安全的文件读取器

from pathlib import Path


class FileReaderTool:
    """文件读取工具，支持读取文本文件内容。"""

    # 允许读取的文件扩展名白名单
    ALLOWED_EXTENSIONS = {
        ".txt", ".md", ".py", ".js", ".ts", ".json", ".yaml", ".yml",
        ".xml", ".html", ".css", ".csv", ".log", ".cfg", ".ini",
    }

    # 最大文件大小（1MB）
    MAX_FILE_SIZE = 1024 * 1024

    async def execute(self, path: str, encoding: str = "utf-8",
                      max_lines: int | None = None) -> str:
        """
        读取指定文件的内容。
        包含安全检查：仅允许读取白名单扩展名的文件，且文件大小有限制。
        """
        file_path = Path(path).resolve()

        # 安全检查1：文件扩展名
        if file_path.suffix.lower() not in self.ALLOWED_EXTENSIONS:
            return (
                f"错误：不允许读取文件类型 '{file_path.suffix}'，"
                f"允许的类型：{', '.join(sorted(self.ALLOWED_EXTENSIONS))}"
            )

        # 安全检查2：文件是否存在
        if not file_path.exists():
            return f"错误：文件不存在 — {path}"

        # 安全检查3：是否为文件（非目录）
        if not file_path.is_file():
            return f"错误：路径不是文件 — {path}"

        # 安全检查4：文件大小
        file_size = file_path.stat().st_size
        if file_size > self.MAX_FILE_SIZE:
            return (
                f"错误：文件过大（{file_size / 1024:.1f}KB），"
                f"最大允许 {self.MAX_FILE_SIZE / 1024:.1f}KB"
            )

        # 读取文件内容
        try:
            with open(file_path, "r", encoding=encoding) as f:
                if max_lines:
                    lines = []
                    for i, line in enumerate(f):
                        if i >= max_lines:
                            lines.append(f"... (已截断，仅显示前 {max_lines} 行)")
                            break
                        lines.append(line.rstrip())
                    content = "\n".join(lines)
                else:
                    content = f.read()

            # 添加文件信息头
            header = (
                f"📄 文件：{path}\n"
                f"大小：{file_size} 字节\n"
                f"{'─' * 40}"
            )
            return f"{header}\n{content}"

        except UnicodeDecodeError:
            return f"错误：文件编码不匹配，尝试使用 encoding 参数指定正确编码"
        except PermissionError:
            return f"错误：没有权限读取文件 — {path}"
