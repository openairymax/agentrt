# Security Policy

## Supported Versions

We currently provide security updates for the following versions:

| Version | Supported          | Until       |
| -------- | ------------------ | ----------- |
| v1.0.x   | :white_check_mark: | 2026-12-31 |
| v0.9.x   :warning:           | Security only | 2026-06-30 |
| < 0.9    | :x:                | No          |

## Reporting a Vulnerability

We take the security of AgentOS seriously. If you discover a security vulnerability, **please do NOT open a public issue**. Instead, follow these steps:

### 1. Report via GitHub (Recommended)

Use [GitHub Security Advisories](https://github.com/SpharxTeam/AgentOS/security/advisories/new)

### 2. Report via Email

Send an email to: security@spharx.com
Subject format: [SECURITY] AgentOS Vulnerability Report - [Brief Description]

### 3. What to Include in Your Report

Please provide as much detail as possible:

- **Vulnerability Type**: e.g., XSS, SQL Injection, Buffer Overflow, Privilege Escalation
- **Affected Versions**: Confirm which versions are affected
- **Reproduction Steps**: Detailed steps and example code to reproduce
- **Potential Impact**: What harm this vulnerability could cause
- **Suggested Fix**: If you have one

### 4. Response Timeline

| Timeframe | Action |
|-----------|--------|
| Within 24 hours | Acknowledge receipt and initial assessment |
| Within 48 hours | Provide initial response and status update |
| Within 7 days | Complete fix or provide workaround |
| Within 14 days | Release security patch (if applicable) |

## Security Best Practices

### For Users

1. **Keep Updated**: Always use the latest version of AgentOS
2. **Principle of Least Privilege**: Grant only necessary permissions
3. **Network Security**: Ensure secure network environment, use HTTPS
4. **Regular Audits**: Regularly check logs and security configurations
5. **Backup**: Regularly backup important data

### For Developers

1. **Code Review**: All code changes must be reviewed
2. **Dependency Management**: Regularly update dependencies to fix known vulnerabilities
3. **Security Testing**: Perform security testing before release
4. **Input Validation**: Strictly validate all external inputs
5. **Error Handling**: Avoid leaking sensitive information

## Known Security Issues

Check current known security issues and their status:

- [GitHub Security Advisories](https://github.com/SpharxTeam/AgentOS/security/advisories)
- [Gitee Issues (tag: security)](https://gitee.com/spharx/agentos/issues?labels=security)

## Security Resources

- [OWASP Top 10](https://owasp.org/www-project-top-ten/)
- [CWE Top 25](https://cwe.mitre.org/top25/archive/2023/2023_cwe_top25.html)
- [AgentOS Architecture Documentation](../manuals/architecture/)
- [AgentOS Security Documentation](../manuals/security/)

## Acknowledgments

Thank you to all researchers and developers who contribute to AgentOS's security! Your efforts make our ecosystem more secure.

---

**Last updated**: 2026-04-02
