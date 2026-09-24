# Security notes

- The application manifest requests administrator elevation because modifying Windows Firewall policy is a privileged operation.
- Only rules with the `NetControl Block - ` prefix are created/removed by the application.
- Existing third-party firewall rules are not deleted.
- Executable trust is not inferred from process names.
- User history and policy files are local-only.
- The app does not upload process names, paths, traffic data, or connection data.

## Reporting a vulnerability

Report **privately** — do not open a public issue. Use GitHub's private
[security advisory](https://github.com/jojin1709/NetControl/security/advisories/new)
or contact the maintainer through GitHub. Please include reproduction steps; we aim
to acknowledge within 7 days.
