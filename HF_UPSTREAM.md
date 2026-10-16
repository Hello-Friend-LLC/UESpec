# HF upstream record

This repository is a Hello-Friend-LLC fork of a third-party plugin, imported under
the HF_Plugins convention (DOC-0073). HF changes live on branch `hf-5-8` only.

| Field | Value |
|---|---|
| Upstream URL | https://github.com/Mithril0rd/uespec-public |
| Upstream branch | `main` |
| Pinned upstream SHA | `8244ccde87166de2792cc2fbf0e282f4903fe2bb` |
| Upstream commit date | 2026-04-29 |
| Fork import date | 2026-10-16 |
| License (SPDX) | Apache-2.0 (see `LICENSE`) |
| HF branch | `hf-5-8` (UE 5.8.3 port and HF changes) |
| Companion backend | https://github.com/Hello-Friend-LLC/uespec-mcp (branch `hf-main`) |

Update procedure: `git remote add upstream https://github.com/Mithril0rd/uespec-public.git`,
fetch, merge `upstream/main` into `hf-5-8`, push, then bump the gitlink in HF_Plugins
(`Docs/UESPEC_UPDATE_ROLLBACK.md`). Keep local patches as minimal, upstream-PR-shaped commits.
