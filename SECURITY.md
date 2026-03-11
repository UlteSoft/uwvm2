# Security Policy

## Supported Versions

The following table lists which versions of **uwvm2** are currently supported with security updates.

| Version | Supported |
| ------- | --------- |
| master / latest | ✅ |

### Notes

- Only the **latest stable branch** and the **current development branch (`master`)** receive full security updates.
- Older versions may receive limited fixes if the vulnerability is critical and easy to backport.
- Versions marked as unsupported will **not receive security patches**.

Users are strongly encouraged to upgrade to the latest version.

---

# Reporting a Vulnerability

If you discover a security vulnerability in **uwvm2**, please report it responsibly.

## Preferred Method

Please report security issues through **GitHub Security Advisories**:

https://github.com/UlteSoft/uwvm2/security/advisories/new

This allows us to coordinate responsible disclosure before public release.

## Alternative Contact

If GitHub Security Advisories cannot be used, you may report vulnerabilities by opening a **private security report** or contacting the project maintainer.

Please include as much information as possible:

- Detailed description of the vulnerability
- Steps to reproduce
- Proof-of-concept code (if available)
- Affected versions
- Operating system and compiler version

Example:

```
OS: Linux x86_64
Compiler: GCC 14
uwvm2 version: master
```

---

# Response Timeline

We aim to respond to security reports according to the following timeline:

| Stage | Expected Time |
|------|---------------|
| Initial response | within 72 hours |
| Vulnerability confirmation | within 7 days |
| Patch or mitigation | depends on severity |

Security fixes may be released as:

- a patch release
- a GitHub security advisory
- a direct source code fix in the repository

---

# Responsible Disclosure

Please **do not publicly disclose vulnerabilities** before a fix is available.

This allows maintainers to:

- investigate the issue
- prepare patches
- coordinate responsible disclosure

Researchers who responsibly report vulnerabilities may be credited in the security advisory unless anonymity is requested.

---

# Security Scope

The following components are considered in scope:

- WebAssembly module loading
- Memory management
- Virtual machine execution
- Host interaction interfaces
- Platform memory mapping implementations

Examples of relevant vulnerability classes include:

- memory corruption
- out-of-bounds access
- sandbox escape
- undefined behavior leading to code execution

---

# Out of Scope

The following issues are generally **not considered security vulnerabilities**:

- Denial of service caused by malformed WebAssembly input without memory corruption
- Issues originating from third-party dependencies
- Bugs in external **compiler toolchains used to build uwvm2** (e.g., GCC, Clang, MSVC)
- Behavior caused by undefined behavior in user-provided WebAssembly modules
