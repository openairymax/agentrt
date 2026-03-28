#!/bin/bash
# =============================================================================
# AgentOS cupolas Module Version Management Script
# Version: 1.0.0
# Description: Handles version extraction, validation, and bumping
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_DIR="$(dirname "$SCRIPT_DIR")"
VERSION_FILE="${MODULE_DIR}/VERSION"
CHANGELOG_FILE="${MODULE_DIR}/CHANGELOG.md"

# =============================================================================
# Version Functions
# =============================================================================

get_version() {
    if [[ -f "$VERSION_FILE" ]]; then
        cat "$VERSION_FILE"
    else
        echo "0.0.0"
    fi
}

get_git_version() {
    local tag=""
    if git describe --tags --exact-match 2>/dev/null | grep -q "^cupolas-v"; then
        tag=$(git describe --tags --exact-match)
        echo "${tag#cupolas-v}"
    else
        local commit_count=$(git rev-list --count HEAD 2>/dev/null || echo "0")
        local short_sha=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
        echo "0.0.0-dev.${commit_count}.${short_sha}"
    fi
}

parse_version() {
    local version="$1"
    local major minor patch prerelease
    
    if [[ "$version" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)(-(.+))?$ ]]; then
        major="${BASH_REMATCH[1]}"
        minor="${BASH_REMATCH[2]}"
        patch="${BASH_REMATCH[3]}"
        prerelease="${BASH_REMATCH[5]:-}"
        
        echo "major=$major minor=$minor patch=$patch prerelease=$prerelease"
    else
        echo "ERROR: Invalid version format: $version" >&2
        return 1
    fi
}

bump_version() {
    local current_version="$1"
    local bump_type="${2:-patch}"
    
    local major minor patch prerelease
    eval $(parse_version "$current_version")
    
    case "$bump_type" in
        major)
            major=$((major + 1))
            minor=0
            patch=0
            ;;
        minor)
            minor=$((minor + 1))
            patch=0
            ;;
        patch)
            patch=$((patch + 1))
            ;;
        prerelease)
            if [[ -n "$prerelease" ]]; then
                if [[ "$prerelease" =~ ^([a-z]+)\.([0-9]+)$ ]]; then
                    local pre_type="${BASH_REMATCH[1]}"
                    local pre_num="${BASH_REMATCH[2]}"
                    prerelease="${pre_type}.$((pre_num + 1))"
                else
                    prerelease="${prerelease}.1"
                fi
            else
                prerelease="rc.1"
            fi
            ;;
        *)
            echo "ERROR: Unknown bump type: $bump_type" >&2
            return 1
            ;;
    esac
    
    if [[ -n "$prerelease" ]]; then
        echo "${major}.${minor}.${patch}-${prerelease}"
    else
        echo "${major}.${minor}.${patch}"
    fi
}

set_version() {
    local new_version="$1"
    echo "$new_version" > "$VERSION_FILE"
    echo "Version updated to: $new_version"
}

# =============================================================================
# Validation Functions
# =============================================================================

validate_version() {
    local version="$1"
    
    if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?$ ]]; then
        echo "ERROR: Invalid semantic version: $version" >&2
        return 1
    fi
    
    return 0
}

check_version_consistency() {
    local file_version=$(get_version)
    local cmake_version=""
    local header_version=""
    
    if [[ -f "${MODULE_DIR}/CMakeLists.txt" ]]; then
        cmake_version=$(grep -oP 'VERSION\s+\K[0-9]+\.[0-9]+\.[0-9]+' "${MODULE_DIR}/CMakeLists.txt" | head -1)
    fi
    
    if [[ -f "${MODULE_DIR}/include/cupolas.h" ]]; then
        header_version=$(grep -oP 'DOMES_VERSION\s+"?\K[0-9]+\.[0-9]+\.[0-9]+' "${MODULE_DIR}/include/cupolas.h" | head -1)
    fi
    
    echo "VERSION file: $file_version"
    echo "CMakeLists.txt: $cmake_version"
    echo "cupolas.h: $header_version"
    
    if [[ -n "$cmake_version" ]] && [[ "$file_version" != "$cmake_version" ]]; then
        echo "WARNING: Version mismatch between VERSION file and CMakeLists.txt" >&2
    fi
    
    if [[ -n "$header_version" ]] && [[ "$file_version" != "$header_version" ]]; then
        echo "WARNING: Version mismatch between VERSION file and cupolas.h" >&2
    fi
}

# =============================================================================
# Changelog Functions
# =============================================================================

generate_changelog_entry() {
    local version="$1"
    local previous_version="${2:-}"
    
    local date_str=$(date -u +%Y-%m-%d)
    local commit_range=""
    
    if [[ -n "$previous_version" ]]; then
        commit_range="cupolas-v${previous_version}..HEAD"
    else
        commit_range="HEAD~10..HEAD"
    fi
    
    echo "## [$version] - $date_str"
    echo ""
    
    local changes=$(git log $commit_range --oneline --no-decorate -- cupolas/ 2>/dev/null | head -20)
    
    if [[ -n "$changes" ]]; then
        echo "### Changes"
        echo ""
        echo "$changes" | while read -r line; do
            echo "- $line"
        done
    else
        echo "### Changes"
        echo ""
        echo "- No significant changes"
    fi
    echo ""
}

# =============================================================================
# Artifact Functions
# =============================================================================

generate_artifact_name() {
    local version="$1"
    local os="${2:-linux}"
    local arch="${3:-x86_64}"
    
    echo "agentos-cupolas-${version}-${os}-${arch}"
}

generate_manifest() {
    local version="$1"
    local output_file="${2:-MANIFEST.json}"
    
    cat > "$output_file" << EOF
{
    "name": "agentos-cupolas",
    "version": "${version}",
    "build_date": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
    "git_commit": "$(git rev-parse HEAD 2>/dev/null || echo 'unknown')",
    "git_ref": "$(git symbolic-ref --short HEAD 2>/dev/null || git describe --tags --exact-match 2>/dev/null || echo 'unknown')",
    "git_sha_short": "$(git rev-parse --short HEAD 2>/dev/null || echo 'unknown')",
    "build_host": "$(hostname 2>/dev/null || echo 'unknown')",
    "build_user": "$(whoami 2>/dev/null || echo 'unknown')"
}
EOF
    
    echo "Manifest generated: $output_file"
}

# =============================================================================
# CLI Interface
# =============================================================================

print_usage() {
    cat << EOF
AgentOS cupolas Module Version Management

Usage: $0 <command> [options]

Commands:
    get              Get current version
    get-git          Get version from git tags
    parse <version>  Parse version components
    bump <type>      Bump version (major|minor|patch|prerelease)
    set <version>    Set version
    validate <ver>   Validate version format
    check            Check version consistency
    changelog <ver>  Generate changelog entry
    artifact <ver>   Generate artifact name
    manifest <ver>   Generate manifest file

Options:
    -h, --help       Show this help message

Examples:
    $0 get
    $0 bump minor
    $0 set 1.2.0
    $0 changelog 1.2.0
    $0 artifact 1.2.0 linux x86_64
EOF
}

main() {
    local command="${1:-help}"
    shift || true
    
    case "$command" in
        get)
            get_version
            ;;
        get-git)
            get_git_version
            ;;
        parse)
            parse_version "${1:-$(get_version)}"
            ;;
        bump)
            bump_version "$(get_version)" "${1:-patch}"
            ;;
        set)
            set_version "${1:?Version required}"
            ;;
        validate)
            validate_version "${1:?Version required}"
            ;;
        check)
            check_version_consistency
            ;;
        changelog)
            generate_changelog_entry "${1:?Version required}" "${2:-}"
            ;;
        artifact)
            generate_artifact_name "${1:?Version required}" "${2:-linux}" "${3:-x86_64}"
            ;;
        manifest)
            generate_manifest "${1:?Version required}" "${2:-MANIFEST.json}"
            ;;
        help|-h|--help)
            print_usage
            ;;
        *)
            echo "ERROR: Unknown command: $command" >&2
            print_usage
            exit 1
            ;;
    esac
}

main "$@"
