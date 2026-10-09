---
name: commit
description: Commit changes to the repository
---

# Grouping
Check `git diff` and `git diff --staged` before writing a message.
One commit per logical unit of work.

# Message format
`topic: subject` — conventional commits

- Subject ≤50 chars
- Body lines ≤72 chars, blank line between subject and body; keep the body concise and to the point
- At the bottom of the commit message, add 'Co-authored-by: <model_name>' (no harness name like Copilot or Codex - just your model name and version if you know it)
