# Pull Request Template

Thank you for contributing to AgentOS! Before submitting your PR, please ensure you have completed the following checks:

## Checklist

### Code Quality
- [ ] Code follows project coding standards (C/C++/Python, etc.)
- [ ] Code has been formatted with clang-format/clang-tidy
- [ ] New public APIs have complete Doxygen documentation
- [ ] Code does not introduce new compiler warnings

### Testing
- [ ] New code has corresponding unit tests
- [ ] All existing tests still pass
- [ ] Test coverage has not decreased
- [ ] For performance optimizations, provide benchmark results

### Documentation
- [ ] Updated relevant documentation (README, API docs, etc.)
- [ ] New features have usage examples or tutorials
- [ ] Updated CHANGELOG.md (if applicable)

### Other
- [ ] PR description is clear, explaining the reason and content of changes
- [ ] Linked relevant Issues (format: Closes #123)
- [ ] Branch name follows conventions (feature/xxx, bugfix/xxx, etc.)

## PR Description

### Change Type
<!-- Please select one -->
- [ ] Bug Fix
- [ ] New Feature
- [ ] Performance Optimization
- [ ] Code Refactoring
- [ ] Documentation Update
- [ ] Other (please specify)

### Change Description
<!-- Clearly and concisely describe the changes in this PR -->

### Related Issues
<!-- Linked Issues, e.g.: Closes #123 -->

### Testing Instructions
<!-- Describe how to test these changes -->

### Screenshots/Logs
<!-- If applicable, provide screenshots or logs -->

## Notes

1. **Code Review**: Maintainers may suggest modifications, please respond promptly
2. **Merge Strategy**: Feature branches use squash and merge, bug fixes use merge commit
3. **License**: Confirm your contribution complies with the project's open source license

---

**Thank you again for your contribution!** 🎉