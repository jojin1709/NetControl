# Contributing to NetControl

Thanks for helping improve NetControl. The workflow is deliberately simple:

> **Fork → find a bug → fix it in your fork → open a PR here.**

We review and merge PRs that make the project better. What we do *not* accept is
copying this code into other repositories or products — the license forbids it
(see [LICENSE.txt](LICENSE.txt)). All fixes come back as pull requests to this repo.

## Ground rules

- **Authorized use only.** This tool manages firewall rules on machines you own or
  are authorized to administer. PRs that add capabilities for attacking third-party
  systems will be rejected.
- **Honest scope.** Never simulate a capability Windows user-mode APIs cannot provide.
  Ask mode is monitoring-only, bandwidth limits are pending WFP — if you add a feature
  that needs a driver, document it in `docs/WFP-CALLOUT.md` instead of faking it.
- **No new dependencies without discussion.** The project links only documented
  Windows libraries (advapi32, iphlpapi, ole32, shell32, ws2_32, …). Open an issue
  first if you need more.

## Before you open a PR

1. Configure and build clean:

   ```powershell
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```

2. Run the test suite:

   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```

3. If you change core logic, add or extend a test in `tests/test_core.cpp`.

4. Update the docs you touched (`README.md`, `docs/ARCHITECTURE.md`,
   `docs/TEST_PLAN.md`, `docs/ROADMAP.md`).

## PR checklist

- [ ] Build has zero errors
- [ ] `ctest` passes
- [ ] New behavior is covered by tests or a manual step in `docs/TEST_PLAN.md`
- [ ] Honest-scope rules respected (nothing faked)
- [ ] PR title says what changed, e.g. `Fix IPv6 EStats delta overflow`

## Reporting bugs

Open a [bug report](.github/ISSUE_TEMPLATE/bug_report.md) with:

- Windows version and edition
- NetControl version / commit
- Steps to reproduce
- Expected vs actual behavior
- Relevant logs or a screenshot

Security issues: see [docs/SECURITY.md](docs/SECURITY.md) — report privately first.

## Style

- C++20, MSVC `/W4 /permissive-`, no compiler warnings
- Keep the single-file Win32 UI style in `src/Ui.cpp` (raw Win32 + GDI, no frameworks)
- All user-facing strings go through `TR(...)` / L10n (en/de/es)
- No comments unless the logic is genuinely non-obvious
