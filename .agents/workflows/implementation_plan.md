# Implementation Plan: Update tizenclaw-assets Workflows

1. **Copy Guidelines**:
   - `cp -r ~/samba/github/tizenclaw/.agents ~/samba/github/tizenclaw-assets/`
   - `cp ~/samba/github/tizenclaw/AGENTS.md ~/samba/github/tizenclaw-assets/`
   (Already accomplished via a terminal command)

2. **Refine Project Name**:
   - Write a python script `/tmp/rename_tizenclaw.py` to recursively read `.agents` and `AGENTS.md` and replace specific strings:
     - `tizenclaw-cli` -> remove or replace where appropriate (Wait, `tizenclaw-assets` has `tizenclaw-ocr`).
     - `TizenClaw` -> `tizenclaw-assets` or `TizenClaw-Assets` where applicable.
     - `tizenclaw` -> `tizenclaw-assets` for RPM package names and service names, if applicable.
   
3. **Refine `gbs_build.md` & `deploy_to_emulator.md`**:
   - Update `deploy.sh` mentions. `tizenclaw-assets` has its own `deploy.sh`.
   - Update RPM package names to `tizenclaw-assets-*.rpm`.

4. **Refine `cli_testing.md`**:
   - `tizenclaw-assets` doesn't have `tizenclaw-cli`. It has `tizenclaw-ocr`. Let's update `cli_testing.md` to `ocr_testing.md` or just rewrite it to reflect `tizenclaw-ocr`.

5. **Test & Review**:
   - Check the modified markdown files.
   - Verify syntax.

6. **Commit**:
   - Add `.agents` and `AGENTS.md`.
   - Create a conventional commit.
