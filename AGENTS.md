# Working preferences

- Never use type casts, type assertions, or explicit or implicit `any`. Validate external data at boundaries and model valid states with types.
- Prefer the simplest maintainable design that meets the requested scope. Preserve unrelated changes and avoid unrelated cleanup.
- Read relevant project documentation. Keep durable conventions here and detailed procedures, plans and progress in linked documents.
- Use agent teams or councils only when requested. Preserve the selected model unless the user asks to change it.
- Ask only when missing information materially changes the outcome or required authority is absent. Reuse decisions and approvals already given; continue independent work while awaiting an answer.
- Run relevant checks and reuse valid final-state evidence. Report results and material gaps concisely; distinguish observed results from assumptions.

# Finish the user-facing task

- Porthole is the native Qt/Linux app running on the physical frame. The older `web/` frontend is a separate experimental preview; confirm the actual surface before editing.
- A request to change the running frame includes the normal delivery workflow: implement, verify, commit and push the scoped change, merge its pull request, then build/sign/publish a release through the existing Wi-Fi updater. Do not stop at a local edit, screenshot, passing test, pushed branch or merged PR.
- Carry that workflow through without another discretionary permission question. Honor an explicit local-only, no-push, draft-only or deferred-deployment instruction. Required tool/sandbox approvals still apply, as do authority checks for actions outside the requested scope.
- Build and test the matching ARMv7 binaries before publishing. Inspect the final diff and package only the intended application binaries. Follow [signed release and verification procedures](linux/update/README.md); source pushes alone do not update the frame.
- Use the existing signed Wi-Fi update channel. Do not require a USB cable for a routine app release or make USB verification a prerequisite for publication. The frame checks automatically; its Software updates control can request an immediate check.
- Verify that GitHub marks the intended release Latest, that its public assets download without authentication, and that the downloaded manifest, signature, archive and binary hashes match the tested release.
- When existing device access is available, verify the installed version/hashes, service health, rollback state and preservation of settings, credentials, calendar selections and briefing history. Never claim installation from publication or download counts alone. If no device confirmation is available, complete publication and clearly report that installation is unconfirmed.
- Do not expose profiles, credentials, signing keys or private device evidence. Preserve the existing update channel, saved state and rollback protections. Kernel, firmware and privileged service changes require their own documented workflow.
- Documentation-only requests do not require an application release. For all tasks, finish every authorized step that is possible; report a concrete blocker only when one actually prevents the remaining work.

# Project entry points

- [Project overview and build](README.md)
- [Native app, tests and preview modes](linux/native/README.md)
- [Signed Wi-Fi releases](linux/update/README.md)
- [Deferred Google activation](linux/google/AGENT-SETUP.md)
