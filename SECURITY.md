# Security & secret hygiene

This repo had a one-time leak of low-severity PII (WiFi SSID, an internal IP,
an invoice number) committed inside docs. History has been scrubbed. The guard
below exists to stop it happening again.

## What must never be committed

- `include/secrets.h` — real WiFi SSID/password. It is gitignored (all paths)
  and only the placeholder `include/secrets.h.example` is tracked.
- Personal documents: invoices/receipts (`*Invoice*.pdf`), the hardware photo,
  private keys (`*.pem`, `*.key`, `id_rsa`), and env files (`.env`).
- Private IPv4 addresses (192.168.x.x, 10.x.x.x, 172.16–31.x.x) and API
  keys/tokens in tracked source or docs.

Public URLs are fine and are NOT treated as secrets: the masjid site,
`wttr.in`, keyless `text.pollinations.ai`, and the `salaahclock.local` mDNS name.

## How the guard works

A version-controlled pre-commit hook lives at `.githooks/pre-commit` (POSIX
`sh`, no external tools — runs on stock macOS). It is activated per-clone with:

```sh
git config core.hooksPath .githooks
```

Because it is committed under `.githooks/` (not `.git/hooks/`), it survives
clones. Each collaborator runs the `git config` line once after cloning.

On every commit it scans **only the staged, added lines** and blocks the commit
(naming `file:line`, values redacted) when it finds:

- The **real** WiFi SSID or password — read live from `include/secrets.h` at
  commit time, so the actual secret is never hardcoded into the committed hook.
  Placeholder values from the example template are ignored.
- A staged `secrets.h` / `*.pem` / `*.key` / `id_rsa` / `.env` file.
- A private IPv4 address, or an obvious `api_key=`/`token=` assignment.

The hook never prints the secret it matched.

## Bypassing intentionally

If you are certain a flagged line is safe:

```sh
git commit --no-verify
```

The generic IP/token patterns are kept deliberately tight to avoid false
positives; the SSID/password check is the real defense.
