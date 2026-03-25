# GitHub Skill

GitHub integration skill for repository management, issue tracking, and CI/CD automation.

## Features

- Repository creation, deletion, and management
- Issue creation, editing, and comment management
- Pull request creation, review, and merge
- Workflow dispatch and status monitoring
- Release creation and management
- Team and collaborator management
- Code search and content search
- Commit and branch operations

## Installation

```bash
pip install PyGithub requests
```

## Usage

```python
from github_skill import GitHubSkill

# Create and initialize GitHub connection
github = GitHubSkill({
    "github_token": "your-github-token",
    "organization": "my-org"
})
result = github.initialize()

# Get user info
user = github.get_user()
print(user.data)

# Create repository
repo = github.create_repository(
    name="my-new-repo",
    description="A new repository",
    private=False
)

# Create issue
issue = github.create_issue(
    owner="my-org",
    repo="my-new-repo",
    title="Bug found",
    body="Description of the bug",
    labels=["bug"]
)

# Create pull request
pr = github.create_pull_request(
    owner="my-org",
    repo="my-new-repo",
    title="Feature implementation",
    head="feature-branch",
    base="main"
)

# Search code
results = github.search_code("language:python function:process")

# Close connection
github.close()
```

## Configuration

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| github_token | string | "" | GitHub personal access token |
| organization | string | None | Default organization name |
| default_branch | string | "main" | Default branch name |
| api_base_url | string | "https://api.github.com" | GitHub API base URL |
| timeout | integer | 30 | API timeout in seconds |

## API Reference

### Repository Operations

- `get_user()` - Get current user info
- `get_repository(owner, repo)` - Get repository info
- `create_repository(name, ...)` - Create new repository
- `delete_repository(owner, repo)` - Delete repository
- `list_repositories()` - List user repositories
- `get_file_content(owner, repo, path)` - Get file content
- `create_or_update_file(owner, repo, path, content, message)` - Create/update file

### Issue Operations

- `create_issue(owner, repo, title, ...)` - Create issue
- `get_issue(owner, repo, number)` - Get issue info
- `list_issues(owner, repo, ...)` - List issues
- `update_issue(owner, repo, number, ...)` - Update issue

### Pull Request Operations

- `create_pull_request(owner, repo, title, ...)` - Create PR
- `get_pull_request(owner, repo, number)` - Get PR info
- `merge_pull_request(owner, repo, number)` - Merge PR

### Branch Operations

- `create_branch(owner, repo, name, from_branch)` - Create branch
- `delete_branch(owner, repo, name)` - Delete branch

### Release Operations

- `create_release(owner, repo, tag_name, ...)` - Create release

### Workflow Operations

- `dispatch_workflow(owner, repo, workflow_id, ...)` - Dispatch workflow
- `get_workflow_runs(owner, repo, workflow_id)` - Get workflow runs

### Search Operations

- `search_code(query)` - Search code
- `search_repositories(query)` - Search repositories

### Team Management

- `add_collaborator(owner, repo, username, permission)` - Add collaborator
