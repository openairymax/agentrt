"""
AgentOS Python SDK 安装脚本
提供 setuptools 安装配置，支持 pip install
"""
from setuptools import setup, find_packages

setup(
    name="agentos",
    version="3.0.0",
    description="AgentOS Python SDK - 官方多语言 SDK 集合",
    long_description=open("README.md", "r", encoding="utf-8").read() if __name__ == "__main__" else "",
    long_description_content_type="text/markdown",
    author="SpharxWorks",
    author_email="team@spharx.com",
    url="https://github.com/spharx/agentos",
    packages=find_packages(exclude=["tests", "tests.*"]),
    python_requires=">=3.8",
    install_requires=[
        "requests>=2.28.0",
        "aiohttp>=3.8.0",
    ],
    extras_require={
        "test": [
            "pytest>=7.0.0",
            "pytest-asyncio>=0.18.0",
        ],
    },
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    license="MIT",
    keywords="agentos sdk client agent memory session skill",
    project_urls={
        "Documentation": "https://github.com/spharx/agentos/tree/main/agentos/toolkit/python",
        "Source": "https://github.com/spharx/agentos",
        "Bug Tracker": "https://github.com/spharx/agentos/issues",
    },
)
