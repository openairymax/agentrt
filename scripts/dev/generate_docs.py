#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 文档生成工具
# 遵循 AgentOS 架构设计原则：反馈闭环、工程美学

"""
AgentOS 文档生成工具

自动从源代码生成 API 文档和参考手册，支持：
- 从 C 头文件提取 API 文档
- 从 Python docstring 生成文档
- 生成 Markdown 格式文档
- 生成 HTML 格式文档（可选）

Usage:
    python generate_docs.py --input path/to/source --output path/to/docs
    python generate_docs.py --format html --serve
"""

import argparse
import os
import re
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple


class DocGenerator:
    """文档生成器基类"""

    def __init__(self, source_dir: str, output_dir: str):
        self.source_dir = Path(source_dir)
        self.output_dir = Path(output_dir)
        self.docs: Dict[str, str] = {}

    def generate(self):
        raise NotImplementedError

    def save(self):
        self.output_dir.mkdir(parents=True, exist_ok=True)
        for name, content in self.docs.items():
            output_path = self.output_dir / name
            output_path.write_text(content, encoding="utf-8")
            print(f"Generated: {output_path}")


class CHeaderDocGenerator(DocGenerator):
    """C 头文件文档生成器"""

    def extract_functions(self, content: str) -> List[Dict]:
        functions = []
        func_pattern = r'/\*\*([\s\S]*?)\*/\s*\n\s*(\w+)\s+(\w+)\s*\(([^)]*)\)'

        for match in re.finditer(func_pattern, content):
            doc, return_type, name, params = match.groups()
            functions.append({
                "name": name,
                "return_type": return_type,
                "params": self._parse_params(params),
                "doc": self._clean_docstring(doc)
            })

        return functions

    def extract_structs(self, content: str) -> List[Dict]:
        structs = []
        struct_pattern = r'/\*\*([\s\S]*?)\*/\s*\n\s*typedef\s+struct\s+(\w+)\s*\{([\s\S]*?)\}\s*(\w+);'

        for match in re.finditer(struct_pattern, content):
            doc, _, fields, name = match.groups()
            structs.append({
                "name": name,
                "doc": self._clean_docstring(doc),
                "fields": self._parse_struct_fields(fields)
            })

        return structs

    def extract_enums(self, content: str) -> List[Dict]:
        enums = []
        enum_pattern = r'/\*\*([\s\S]*?)\*/\s*\n\s*typedef\s+enum\s*\{([\s\S]*?)\}\s*(\w+);'

        for match in re.finditer(enum_pattern, content):
            doc, values, name = match.groups()
            enums.append({
                "name": name,
                "doc": self._clean_docstring(doc),
                "values": [v.strip() for v in values.split(",") if v.strip()]
            })

        return enums

    def _parse_params(self, params: str) -> List[Tuple[str, str]]:
        result = []
        for param in params.split(","):
            param = param.strip()
            if param and param != "void":
                parts = param.split()
                if len(parts) >= 2:
                    result.append((parts[-1], " ".join(parts[:-1])))
                elif len(parts) == 1:
                    result.append((parts[0], "unknown"))
        return result

    def _parse_struct_fields(self, fields: str) -> List[Tuple[str, str, str]]:
        result = []
        for line in fields.split("\n"):
            line = line.strip().rstrip(";")
            if line and not line.startswith("//"):
                parts = line.split()
                if len(parts) >= 2:
                    field_type = " ".join(parts[:-1])
                    field_name = parts[-1]
                    result.append((field_name, field_type, ""))
        return result

    def _clean_docstring(self, doc: str) -> str:
        lines = []
        for line in doc.split("\n"):
            line = line.strip()
            line = re.sub(r'^\* ?', '', line)
            if line:
                lines.append(line)
        return "\n".join(lines)

    def generate(self):
        for header_path in self.source_dir.rglob("*.h"):
            rel_path = header_path.relative_to(self.source_dir)
            content = header_path.read_text(encoding="utf-8")

            functions = self.extract_functions(content)
            structs = self.extract_structs(content)
            enums = self.extract_enums(content)

            if functions or structs or enums:
                doc_name = str(rel_path.with_suffix("")).replace("/", "_") + ".md"
                self.docs[doc_name] = self._generate_markdown(
                    rel_path.stem, functions, structs, enums
                )

    def _generate_markdown(self, module: str, functions: List, structs: List, enums: List) -> str:
        lines = []
        lines.append(f"# {module}")
        lines.append("")
        lines.append(f"> Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append("")

        if enums:
            lines.append("## Enumerations")
            lines.append("")
            for enum in enums:
                lines.append(f"### `{enum['name']}`")
                lines.append("")
                if enum["doc"]:
                    lines.append(enum["doc"])
                    lines.append("")
                lines.append("```c")
                for value in enum["values"]:
                    lines.append(f"    {value}")
                lines.append("```")
                lines.append("")

        if structs:
            lines.append("## Structures")
            lines.append("")
            for struct in structs:
                lines.append(f"### `{struct['name']}`")
                lines.append("")
                if struct["doc"]:
                    lines.append(struct["doc"])
                    lines.append("")
                lines.append("```c")
                lines.append(f"typedef struct {struct['name']} {{")
                for field_name, field_type, _ in struct["fields"]:
                    lines.append(f"    {field_type} {field_name};")
                lines.append(f"}} {struct['name']};")
                lines.append("```")
                lines.append("")

        if functions:
            lines.append("## Functions")
            lines.append("")
            for func in functions:
                lines.append(f"### `{func['name']}`")
                lines.append("")
                if func["doc"]:
                    lines.append(func["doc"])
                    lines.append("")

                params = ", ".join([f"{pt} {pn}" for pn, pt in func["params"]])
                lines.append("```c")
                lines.append(f"{func['return_type']} {func['name']}({params});")
                lines.append("```")
                lines.append("")

        return "\n".join(lines)


class PythonDocGenerator(DocGenerator):
    """Python 文档生成器"""

    def extract_classes(self, content: str) -> List[Dict]:
        classes = []
        class_pattern = r'class (\w+)(?:\([^)]*\))?:\s*"""([\s\S]*?)"""'

        for match in re.finditer(class_pattern, content):
            name, doc = match.groups()
            classes.append({
                "name": name,
                "doc": doc.strip()
            })

        return classes

    def extract_functions(self, content: str) -> List[Dict]:
        functions = []
        func_pattern = r'def (\w+)\s*\(([^)]*)\)\s*(?:->\s*([^\s:]+))?:\s*"""([\s\S]*?)"""'

        for match in re.finditer(func_pattern, content):
            name, params, return_type, doc = match.groups()
            functions.append({
                "name": name,
                "params": [p.strip() for p in params.split(",") if p.strip()],
                "return_type": return_type or "None",
                "doc": doc.strip()
            })

        return functions

    def generate(self):
        for py_path in self.source_dir.rglob("*.py"):
            rel_path = py_path.relative_to(self.source_dir)
            content = py_path.read_text(encoding="utf-8")

            classes = self.extract_classes(content)
            functions = self.extract_functions(content)

            if classes or functions:
                doc_name = str(rel_path.with_suffix("")).replace("/", "_") + ".md"
                self.docs[doc_name] = self._generate_markdown(
                    rel_path.stem, classes, functions
                )

    def _generate_markdown(self, module: str, classes: List, functions: List) -> str:
        lines = []
        lines.append(f"# {module}")
        lines.append("")
        lines.append(f"> Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append("")

        if classes:
            lines.append("## Classes")
            lines.append("")
            for cls in classes:
                lines.append(f"### `{cls['name']}`")
                lines.append("")
                lines.append(cls["doc"])
                lines.append("")

        if functions:
            lines.append("## Functions")
            lines.append("")
            for func in functions:
                lines.append(f"### `{func['name']}`")
                lines.append("")
                lines.append(func["doc"])
                lines.append("")
                lines.append(f"**Parameters:** {', '.join(func['params']) or 'None'}")
                lines.append(f"**Returns:** {func['return_type']}")
                lines.append("")

        return "\n".join(lines)


class DocBuilder:
    """文档构建器"""

    def __init__(self, project_root: str):
        self.project_root = Path(project_root)
        self.generators: List[DocGenerator] = []

    def add_generator(self, generator: DocGenerator):
        self.generators.append(generator)

    def build(self):
        for generator in self.generators:
            print(f"Generating docs from {generator.source_dir}...")
            generator.generate()
            generator.save()


def main():
    parser = argparse.ArgumentParser(
        description="AgentOS Documentation Generator",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "--input", "-i",
        required=True,
        help="Source directory containing headers or Python files"
    )

    parser.add_argument(
        "--output", "-o",
        required=True,
        help="Output directory for generated documentation"
    )

    parser.add_argument(
        "--type", "-t",
        choices=["c", "python", "all"],
        default="all",
        help="Type of source files to process"
    )

    args = parser.parse_args()

    builder = DocBuilder(args.input)

    if args.type in ["c", "all"]:
        c_gen = CHeaderDocGenerator(args.input, args.output)
        builder.add_generator(c_gen)

    if args.type in ["python", "all"]:
        py_gen = PythonDocGenerator(args.input, args.output)
        builder.add_generator(py_gen)

    builder.build()

    print(f"\nDocumentation generated successfully!")
    return 0


if __name__ == "__main__":
    sys.exit(main())