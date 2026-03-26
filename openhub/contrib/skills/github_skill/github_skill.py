# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
GitHub Integration Skill
=====================

This module provides GitHub integration capabilities for the OpenHub platform,
including repository management, issue tracking, pull request automation, and CI/CD.

Architecture:
- PyGithub for GitHub API interaction
- Requests library for direct API calls
- Support for GitHub REST API and GraphQL API
- Rate limit handling and retry logic

Features:
- Repository creation, deletion, and management
- Issue creation, editing, and comment management
- Pull request creation, review, and merge
- Workflow dispatch and status monitoring
- Release creation and management
- Team and collaborator management
- Code search and content search
- Commit and branch operations
"""

import base64
import logging
import time
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Any, Dict, List, Optional

try:
    from github import Github, GithubException
    from github.Repository import Repository
    from github.Issue import Issue
    from github.PullRequest import PullRequest
    from github.Workflow import Workflow
    PYTHON_GITHUB_AVAILABLE = True
except ImportError:
    PYTHON_GITHUB_AVAILABLE = False

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class IssueState(Enum):
    """GitHub issue states."""
    OPEN = "open"
    CLOSED = "closed"
    ALL = "all"


class PullRequestState(Enum):
    """Pull request states."""
    OPEN = "open"
    CLOSED = "closed"
    MERGED = "merged"
    ALL = "all"


class NotificationReason(Enum):
    """GitHub notification reasons."""
    ASSIGN = "assign"
    AUTHOR = "author"
    COMMENT = "comment"
    CI_ACTIVITY = "ci_activity"
    INVITATION = "invitation"
    MENTION = "mention"
    REVIEW_REQUESTED = "review_requested"
    STATE_CHANGE = "state_change"
    SUBSCRIBED = "subscribed"
    TEAM_MENTION = "team_mention"


@dataclass
class GitHubConfig:
    """GitHub configuration."""
    github_token: str = ""
    organization: Optional[str] = None
    default_branch: str = "main"
    api_base_url: str = "https://api.github.com"
    timeout: int = 30
    per_page: int = 100


@dataclass
class RepositoryInfo:
    """Repository information."""
    name: str
    full_name: str
    description: Optional[str]
    private: bool
    default_branch: str
    url: str
    html_url: str
    created_at: str
    updated_at: str
    pushed_at: str
    language: Optional[str]
    stars: int
    forks: int
    open_issues: int
    watchers: int


@dataclass
class IssueInfo:
    """Issue information."""
    id: int
    number: int
    title: str
    body: Optional[str]
    state: str
    author: str
    assignees: List[str]
    labels: List[str]
    milestone: Optional[str]
    created_at: str
    updated_at: str
    closed_at: Optional[str]
    comments: int
    url: str


@dataclass
class PullRequestInfo:
    """Pull request information."""
    id: int
    number: int
    title: str
    body: Optional[str]
    state: str
    author: str
    base: str
    head: str
    merged: bool
    mergeable: Optional[bool]
    review_comments: int
    commits: int
    additions: int
    deletions: int
    created_at: str
    updated_at: str
    merged_at: Optional[str]
    url: str


@dataclass
class WorkflowRunInfo:
    """Workflow run information."""
    id: int
    name: str
    status: str
    conclusion: Optional[str]
    workflow_id: int
    head_branch: str
    head_sha: str
    run_number: int
    event: str
    created_at: str
    updated_at: str
    html_url: str


@dataclass
class ReleaseInfo:
    """Release information."""
    id: int
    tag_name: str
    name: Optional[str]
    body: Optional[str]
    draft: bool
    prerelease: bool
    created_at: str
    published_at: str
    html_url: str
    tarball_url: str
    zipball_url: str


@dataclass
class GitHubResult:
    """Result of a GitHub operation."""
    success: bool
    data: Any = None
    error: Optional[str] = None
    status_code: Optional[int] = None


class GitHubSkill:
    """Main GitHub integration skill class."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        """Initialize the GitHub skill with configuration."""
        self.config = self._parse_config(config or {})
        self.github = None
        self.session = None
        self._rate_limit_remaining = None
        self._rate_limit_reset = None

    def _parse_config(self, config: Dict[str, Any]) -> GitHubConfig:
        """Parse configuration dictionary."""
        return GitHubConfig(
            github_token=config.get("github_token", ""),
            organization=config.get("organization"),
            default_branch=config.get("default_branch", "main"),
            api_base_url=config.get("api_base_url", "https://api.github.com"),
            timeout=config.get("timeout", 30),
            per_page=config.get("per_page", 100)
        )

    def initialize(self) -> GitHubResult:
        """Initialize GitHub API connection."""
        if not self.config.github_token:
            return GitHubResult(success=False, error="GitHub token is required")

        if PYTHON_GITHUB_AVAILABLE:
            try:
                self.github = Github(
                    self.config.github_token,
                    timeout=self.config.timeout
                )
                user = self.github.get_user()
                user.login
                logger.info(f"GitHub initialized: {user.login}")
                return GitHubResult(success=True, data={"username": user.login})
            except Exception as e:
                return GitHubResult(success=False, error=str(e))
        elif REQUESTS_AVAILABLE:
            self.session = requests.Session()
            self.session.headers.update({
                "Authorization": f"token {self.config.github_token}",
                "Accept": "application/vnd.github.v3+json",
                "X-GitHub-Api-Version": "2022-11-28"
            })
            return GitHubResult(success=True, data={"message": "Using requests session"})
        else:
            return GitHubResult(success=False, error="No GitHub library available. Install PyGithub or requests.")

    def _check_rate_limit(self) -> bool:
        """Check if we're within rate limits."""
        if self._rate_limit_remaining is not None:
            if self._rate_limit_remaining <= 0:
                if self._rate_limit_reset and time.time() < self._rate_limit_reset:
                    logger.warning(f"Rate limited. Resets at {self._rate_limit_reset}")
                    return False
        return True

    def _update_rate_limit(self, headers: Dict[str, Any]):
        """Update rate limit information from response headers."""
        if "X-RateLimit-Remaining" in headers:
            self._rate_limit_remaining = int(headers["X-RateLimit-Remaining"])
        if "X-RateLimit-Reset" in headers:
            self._rate_limit_reset = int(headers["X-RateLimit-Reset"])

    def get_user(self) -> GitHubResult:
        """Get current user information."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                user = self.github.get_user()
                return GitHubResult(success=True, data={
                    "login": user.login,
                    "name": user.name,
                    "email": user.email,
                    "avatar_url": user.avatar_url,
                    "html_url": user.html_url,
                    "public_repos": user.public_repos,
                    "public_gists": user.public_gists,
                    "followers": user.followers,
                    "following": user.following
                })
            else:
                response = self.session.get(f"{self.config.api_base_url}/user")
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json(), status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def get_repository(self, owner: str, repo: str) -> GitHubResult:
        """Get repository information."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                return GitHubResult(success=True, data=self._repo_to_dict(repository))
            else:
                response = self.session.get(f"{self.config.api_base_url}/repos/{owner}/{repo}")
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json(), status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def _repo_to_dict(self, repo) -> Dict[str, Any]:
        """Convert repository object to dictionary."""
        return {
            "name": repo.name,
            "full_name": repo.full_name,
            "description": repo.description,
            "private": repo.private,
            "default_branch": repo.default_branch,
            "url": repo.url,
            "html_url": repo.html_url,
            "created_at": repo.created_at.isoformat() if repo.created_at else None,
            "updated_at": repo.updated_at.isoformat() if repo.updated_at else None,
            "pushed_at": repo.pushed_at.isoformat() if repo.pushed_at else None,
            "language": repo.language,
            "stars": repo.stargazers_count,
            "forks": repo.forks_count,
            "open_issues": repo.open_issues_count,
            "watchers": repo.watchers_count
        }

    def create_repository(
        self,
        name: str,
        description: str = "",
        private: bool = False,
        has_issues: bool = True,
        has_wiki: bool = False,
        has_downloads: bool = False,
        auto_init: bool = False,
        license_template: Optional[str] = None
    ) -> GitHubResult:
        """Create a new repository."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                user = self.github.get_user()
                repo = user.create_repo(
                    name=name,
                    description=description,
                    private=private,
                    has_issues=has_issues,
                    has_wiki=has_wiki,
                    has_downloads=has_downloads,
                    auto_init=auto_init,
                    license_template=license_template
                )
                return GitHubResult(success=True, data=self._repo_to_dict(repo))
            else:
                data = {
                    "name": name,
                    "description": description,
                    "private": private,
                    "has_issues": has_issues,
                    "has_wiki": has_wiki,
                    "has_downloads": has_downloads,
                    "auto_init": auto_init
                }
                if license_template:
                    data["license_template"] = license_template

                response = self.session.post(
                    f"{self.config.api_base_url}/user/repos",
                    json=data
                )
                self._update_rate_limit(response.headers)
                if response.status_code in [200, 201]:
                    return GitHubResult(success=True, data=response.json(), status_code=response.status_code)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def delete_repository(self, owner: str, repo: str) -> GitHubResult:
        """Delete a repository."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                repository.delete()
                return GitHubResult(success=True, data={"message": f"Repository {owner}/{repo} deleted"})
            else:
                response = self.session.delete(f"{self.config.api_base_url}/repos/{owner}/{repo}")
                self._update_rate_limit(response.headers)
                if response.status_code == 204:
                    return GitHubResult(success=True, data={"message": f"Repository {owner}/{repo} deleted"}, status_code=204)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def list_repositories(self, per_page: Optional[int] = None, sort: str = "updated") -> GitHubResult:
        """List repositories for the authenticated user."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        per_page = per_page or self.config.per_page

        try:
            if self.github:
                repos = self.github.get_user().get_repos(per_page=per_page, sort=sort)
                return GitHubResult(success=True, data=[self._repo_to_dict(r) for r in repos])
            else:
                response = self.session.get(
                    f"{self.config.api_base_url}/user/repos",
                    params={"per_page": per_page, "sort": sort}
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json(), status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def create_issue(
        self,
        owner: str,
        repo: str,
        title: str,
        body: Optional[str] = None,
        assignees: Optional[List[str]] = None,
        labels: Optional[List[str]] = None,
        milestone: Optional[int] = None
    ) -> GitHubResult:
        """Create a new issue."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                issue = repository.create_issue(
                    title=title,
                    body=body,
                    assignees=assignees or [],
                    labels=labels or []
                )
                return GitHubResult(success=True, data=self._issue_to_dict(issue))
            else:
                data = {"title": title}
                if body:
                    data["body"] = body
                if assignees:
                    data["assignees"] = assignees
                if labels:
                    data["labels"] = labels
                if milestone:
                    data["milestone"] = milestone

                response = self.session.post(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/issues",
                    json=data
                )
                self._update_rate_limit(response.headers)
                if response.status_code in [200, 201]:
                    return GitHubResult(success=True, data=response.json(), status_code=response.status_code)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def _issue_to_dict(self, issue) -> Dict[str, Any]:
        """Convert issue object to dictionary."""
        return {
            "id": issue.id,
            "number": issue.number,
            "title": issue.title,
            "body": issue.body,
            "state": issue.state,
            "author": issue.user.login if hasattr(issue, 'user') else issue.user,
            "assignees": [a.login if hasattr(a, 'login') else a for a in issue.assignees],
            "labels": [l.name if hasattr(l, 'name') else l for l in issue.labels],
            "milestone": issue.milestone.title if issue.milestone else None,
            "created_at": issue.created_at.isoformat() if issue.created_at else None,
            "updated_at": issue.updated_at.isoformat() if issue.updated_at else None,
            "closed_at": issue.closed_at.isoformat() if issue.closed_at else None,
            "comments": issue.comments,
            "url": issue.url if hasattr(issue, 'url') else None
        }

    def get_issue(self, owner: str, repo: str, issue_number: int) -> GitHubResult:
        """Get issue information."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                issue = repository.get_issue(issue_number)
                return GitHubResult(success=True, data=self._issue_to_dict(issue))
            else:
                response = self.session.get(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/issues/{issue_number}"
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json(), status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def list_issues(
        self,
        owner: str,
        repo: str,
        state: IssueState = IssueState.OPEN,
        labels: Optional[str] = None,
        sort: str = "created",
        direction: str = "desc"
    ) -> GitHubResult:
        """List issues in a repository."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                issues = repository.get_issues(
                    state=state.value,
                    sort=sort,
                    direction=direction
                )
                return GitHubResult(success=True, data=[self._issue_to_dict(i) for i in issues])
            else:
                params = {"state": state.value, "sort": sort, "direction": direction}
                if labels:
                    params["labels"] = labels

                response = self.session.get(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/issues",
                    params=params
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json(), status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def update_issue(
        self,
        owner: str,
        repo: str,
        issue_number: int,
        title: Optional[str] = None,
        body: Optional[str] = None,
        state: Optional[str] = None,
        assignees: Optional[List[str]] = None,
        labels: Optional[List[str]] = None
    ) -> GitHubResult:
        """Update an issue."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            data = {}
            if title is not None:
                data["title"] = title
            if body is not None:
                data["body"] = body
            if state is not None:
                data["state"] = state
            if assignees is not None:
                data["assignees"] = assignees
            if labels is not None:
                data["labels"] = labels

            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                issue = repository.get_issue(issue_number)
                issue.edit(**data)
                return GitHubResult(success=True, data=self._issue_to_dict(issue))
            else:
                response = self.session.patch(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/issues/{issue_number}",
                    json=data
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json(), status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def create_pull_request(
        self,
        owner: str,
        repo: str,
        title: str,
        body: Optional[str] = None,
        head: str = "",
        base: Optional[str] = None,
        maintainer_can_modify: bool = True,
        draft: bool = False
    ) -> GitHubResult:
        """Create a pull request."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        base = base or self.config.default_branch

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                pr = repository.create_pull(
                    title=title,
                    body=body or "",
                    head=head,
                    base=base
                )
                return GitHubResult(success=True, data=self._pr_to_dict(pr))
            else:
                data = {
                    "title": title,
                    "body": body or "",
                    "head": head,
                    "base": base,
                    "maintainer_can_modify": maintainer_can_modify,
                    "draft": draft
                }
                response = self.session.post(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/pulls",
                    json=data
                )
                self._update_rate_limit(response.headers)
                if response.status_code in [200, 201]:
                    return GitHubResult(success=True, data=response.json(), status_code=response.status_code)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def _pr_to_dict(self, pr) -> Dict[str, Any]:
        """Convert pull request object to dictionary."""
        return {
            "id": pr.id,
            "number": pr.number,
            "title": pr.title,
            "body": pr.body,
            "state": pr.state,
            "author": pr.user.login if hasattr(pr, 'user') else pr.user,
            "base": pr.base.ref if hasattr(pr.base, 'ref') else pr.base,
            "head": pr.head.ref if hasattr(pr.head, 'ref') else pr.head,
            "merged": pr.merged,
            "mergeable": pr.mergeable,
            "review_comments": pr.review_comments,
            "commits": pr.commits,
            "additions": pr.additions,
            "deletions": pr.deletions,
            "created_at": pr.created_at.isoformat() if pr.created_at else None,
            "updated_at": pr.updated_at.isoformat() if pr.updated_at else None,
            "merged_at": pr.merged_at.isoformat() if pr.merged_at else None,
            "url": pr.url if hasattr(pr, 'url') else None
        }

    def get_pull_request(self, owner: str, repo: str, pr_number: int) -> GitHubResult:
        """Get pull request information."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                pr = repository.get_pull(pr_number)
                return GitHubResult(success=True, data=self._pr_to_dict(pr))
            else:
                response = self.session.get(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/pulls/{pr_number}"
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json(), status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def merge_pull_request(self, owner: str, repo: str, pr_number: int, commit_title: Optional[str] = None, commit_message: Optional[str] = None, merge_method: str = "merge") -> GitHubResult:
        """Merge a pull request."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                pr = repository.get_pull(pr_number)
                merged = pr.merge(commit_title=commit_title, commit_message=commit_message)
                return GitHubResult(success=True, data={"merged": merged, "message": "Pull request merged" if merged else "Merge failed"})
            else:
                data = {"merge_method": merge_method}
                if commit_title:
                    data["commit_title"] = commit_title
                if commit_message:
                    data["commit_message"] = commit_message

                response = self.session.put(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/pulls/{pr_number}/merge",
                    json=data
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json(), status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def create_branch(self, owner: str, repo: str, branch_name: str, from_branch: Optional[str] = None) -> GitHubResult:
        """Create a new branch."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        from_branch = from_branch or self.config.default_branch

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                source_ref = repository.get_git_ref(f"heads/{from_branch}")
                repository.create_git_ref(ref=f"refs/heads/{branch_name}", sha=source_ref.object.sha)
                return GitHubResult(success=True, data={"name": branch_name, "from": from_branch})
            else:
                ref_response = self.session.get(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/git/refs/heads/{from_branch}"
                )
                self._update_rate_limit(ref_response.headers)
                if ref_response.status_code != 200:
                    return GitHubResult(success=False, error=ref_response.text, status_code=ref_response.status_code)

                sha = ref_response.json()["object"]["sha"]

                data = {
                    "ref": f"refs/heads/{branch_name}",
                    "sha": sha
                }
                response = self.session.post(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/git/refs",
                    json=data
                )
                self._update_rate_limit(response.headers)
                if response.status_code in [200, 201]:
                    return GitHubResult(success=True, data=response.json(), status_code=response.status_code)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def delete_branch(self, owner: str, repo: str, branch_name: str) -> GitHubResult:
        """Delete a branch."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                repository.get_git_ref(f"heads/{branch_name}").delete()
                return GitHubResult(success=True, data={"message": f"Branch {branch_name} deleted"})
            else:
                response = self.session.delete(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/git/refs/heads/{branch_name}"
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 204:
                    return GitHubResult(success=True, data={"message": f"Branch {branch_name} deleted"}, status_code=204)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def create_release(
        self,
        owner: str,
        repo: str,
        tag_name: str,
        target_commitish: Optional[str] = None,
        name: Optional[str] = None,
        body: Optional[str] = None,
        draft: bool = False,
        prerelease: bool = False
    ) -> GitHubResult:
        """Create a release."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                release = repository.create_git_release(
                    tag=tag_name,
                    target=target_commitish,
                    name=name or tag_name,
                    message=body or "",
                    draft=draft,
                    prerelease=prerelease
                )
                return GitHubResult(success=True, data={
                    "id": release.id,
                    "tag_name": release.tag_name,
                    "name": release.title,
                    "draft": release.draft,
                    "prerelease": release.prerelease
                })
            else:
                data = {
                    "tag_name": tag_name,
                    "draft": draft,
                    "prerelease": prerelease
                }
                if target_commitish:
                    data["target_commitish"] = target_commitish
                if name:
                    data["name"] = name
                if body:
                    data["body"] = body

                response = self.session.post(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/releases",
                    json=data
                )
                self._update_rate_limit(response.headers)
                if response.status_code in [200, 201]:
                    return GitHubResult(success=True, data=response.json(), status_code=response.status_code)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def dispatch_workflow(
        self,
        owner: str,
        repo: str,
        workflow_id: str,
        ref: str = "main",
        inputs: Optional[Dict[str, str]] = None
    ) -> GitHubResult:
        """Dispatch a workflow run."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                workflow = repository.get_workflow(workflow_id)
                # workflow.dispatch() 返回 None（成功时）
                success = workflow.dispatch(ref=ref, inputs=inputs or {})
                if success:
                    return GitHubResult(success=True, data={"message": "Workflow dispatched successfully"})
                else:
                    return GitHubResult(success=False, error="Failed to dispatch workflow")
            else:
                data = {"ref": ref}
                if inputs:
                    data["inputs"] = inputs

                response = self.session.post(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/actions/workflows/{workflow_id}/dispatches",
                    json=data
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 204:
                    return GitHubResult(success=True, data={"message": "Workflow dispatched"}, status_code=204)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def get_workflow_runs(
        self,
        owner: str,
        repo: str,
        workflow_id: str,
        branch: Optional[str] = None,
        status: Optional[str] = None
    ) -> GitHubResult:
        """Get workflow runs."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                workflow = repository.get_workflow(workflow_id)
                runs = workflow.get_runs(branch=branch, status=status)
                return GitHubResult(success=True, data=[{
                    "id": r.id,
                    "name": r.name,
                    "status": r.status,
                    "conclusion": r.conclusion,
                    "head_branch": r.head_branch,
                    "head_sha": r.head_sha,
                    "run_number": r.run_number,
                    "event": r.event,
                    "created_at": r.created_at.isoformat() if r.created_at else None
                } for r in runs])
            else:
                params = {}
                if branch:
                    params["branch"] = branch
                if status:
                    params["status"] = status

                response = self.session.get(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/actions/workflows/{workflow_id}/runs",
                    params=params
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json()["workflow_runs"], status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def search_code(self, query: str, per_page: Optional[int] = None) -> GitHubResult:
        """Search for code."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        per_page = per_page or self.config.per_page

        try:
            if self.github:
                results = self.github.search_code(query, per_page=per_page)
                return GitHubResult(success=True, data=[{
                    "name": r.name,
                    "path": r.path,
                    "sha": r.sha,
                    "url": r.url,
                    "repository": r.repository.full_name
                } for r in results])
            else:
                response = self.session.get(
                    f"{self.config.api_base_url}/search/code",
                    params={"q": query, "per_page": per_page}
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json()["items"], status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def search_repositories(self, query: str, sort: Optional[str] = None, per_page: Optional[int] = None) -> GitHubResult:
        """Search for repositories."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        per_page = per_page or self.config.per_page

        try:
            if self.github:
                results = self.github.search_repositories(query, sort=sort, per_page=per_page)
                return GitHubResult(success=True, data=[self._repo_to_dict(r) for r in results])
            else:
                params = {"q": query, "per_page": per_page}
                if sort:
                    params["sort"] = sort

                response = self.session.get(
                    f"{self.config.api_base_url}/search/repositories",
                    params=params
                )
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json()["items"], status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def get_file_content(self, owner: str, repo: str, path: str, ref: Optional[str] = None) -> GitHubResult:
        """Get file content from a repository."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                contents = repository.get_contents(path, ref=ref)
                if isinstance(contents, list):
                    return GitHubResult(success=False, error="Path is a directory")
                return GitHubResult(success=True, data={
                    "name": contents.name,
                    "path": contents.path,
                    "sha": contents.sha,
                    "content": contents.content,
                    "encoding": contents.encoding,
                    "size": contents.size
                })
            else:
                url = f"{self.config.api_base_url}/repos/{owner}/{repo}/contents/{path}"
                if ref:
                    url += f"?ref={ref}"
                response = self.session.get(url)
                self._update_rate_limit(response.headers)
                if response.status_code == 200:
                    data = response.json()
                    if "content" in data:
                        data["content"] = base64.b64decode(data["content"]).decode("utf-8")
                    return GitHubResult(success=True, data=data, status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def create_or_update_file(
        self,
        owner: str,
        repo: str,
        path: str,
        content: str,
        message: str,
        branch: Optional[str] = None,
        sha: Optional[str] = None
    ) -> GitHubResult:
        """Create or update a file in a repository."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            encoded_content = base64.b64encode(content.encode("utf-8")).decode("utf-8")

            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                result = repository.create_file(
                    path=path,
                    message=message,
                    content=encoded_content,
                    branch=branch,
                    sha=sha
                )
                return GitHubResult(success=True, data={
                    "commit": {
                        "sha": result.commit.sha,
                        "message": result.commit.message
                    }
                })
            else:
                data = {
                    "message": message,
                    "content": encoded_content
                }
                if branch:
                    data["branch"] = branch
                if sha:
                    data["sha"] = sha

                url = f"{self.config.api_base_url}/repos/{owner}/{repo}/contents/{path}"
                method = self.session.put if sha else self.session.post
                response = method(url, json=data)
                self._update_rate_limit(response.headers)
                if response.status_code in [200, 201]:
                    return GitHubResult(success=True, data=response.json(), status_code=response.status_code)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def add_collaborator(self, owner: str, repo: str, username: str, permission: str = "push") -> GitHubResult:
        """Add a collaborator to a repository."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                repository = self.github.get_repo(f"{owner}/{repo}")
                repository.add_to_collaborators(username, permission=permission)
                return GitHubResult(success=True, data={"username": username, "permission": permission})
            else:
                data = {"permission": permission}
                response = self.session.put(
                    f"{self.config.api_base_url}/repos/{owner}/{repo}/collaborators/{username}",
                    json=data
                )
                self._update_rate_limit(response.headers)
                if response.status_code in [200, 201, 204]:
                    return GitHubResult(success=True, data={"username": username, "permission": permission}, status_code=response.status_code)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def get_rate_limit(self) -> GitHubResult:
        """Get API rate limit information."""
        if not self.github and not self.session:
            return GitHubResult(success=False, error="GitHub not initialized")

        try:
            if self.github:
                rate_limit = self.github.get_rate_limit()
                return GitHubResult(success=True, data={
                    "limit": rate_limit.core.limit,
                    "remaining": rate_limit.core.remaining,
                    "reset": rate_limit.core.reset.isoformat() if rate_limit.core.reset else None
                })
            else:
                response = self.session.get(f"{self.config.api_base_url}/rate_limit")
                if response.status_code == 200:
                    return GitHubResult(success=True, data=response.json()["rate"], status_code=200)
                return GitHubResult(success=False, error=response.text, status_code=response.status_code)
        except Exception as e:
            return GitHubResult(success=False, error=str(e))

    def close(self) -> Dict[str, Any]:
        """Cleanup resources."""
        if self.session:
            self.session.close()
        return {"success": True}

    def __enter__(self):
        """Context manager entry."""
        self.initialize()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()


def create_skill(config: Optional[Dict[str, Any]] = None) -> GitHubSkill:
    """Factory function to create a GitHubSkill instance."""
    return GitHubSkill(config)
