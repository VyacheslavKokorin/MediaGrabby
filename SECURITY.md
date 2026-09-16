# Security Policy

## Reporting a vulnerability

Please do not publish security vulnerabilities in a public GitHub issue.

Use GitHub's private vulnerability reporting feature for this repository when it is available. If private reporting is unavailable, contact the repository owner through their GitHub profile and provide only enough information to establish a private communication channel.

Please include:

- the affected MediaGrabby version or commit;
- steps to reproduce the issue;
- the expected and actual behavior;
- the security impact;
- any suggested mitigation, if known.

Do not include real passwords, cookies, access tokens, proxy credentials, private URLs or other sensitive user data in reports, screenshots or logs.

## Sensitive data

MediaGrabby can display or log URLs and local filesystem paths. Proxy credentials are redacted by the application, and the proxy password is stored locally using Windows DPAPI. Before sharing logs or screenshots, review them for personal or confidential information.

Browser cookies are read by yt-dlp only when the user explicitly selects a browser. Do not attach browser cookie databases or exported cookies to GitHub issues.
