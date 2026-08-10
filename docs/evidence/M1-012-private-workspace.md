# M1-012 private workspace guidance

Date: 2026-08-10

An actual ignored `private/README.local.md` now documents:

- the current game, manifest, extractor, and fixture layout;
- the prohibition on tracking or distributing private contents;
- authoritative source/payload verification commands;
- the strict no-overwrite regeneration workflow;
- source ISO preservation and interrupted-staging cleanup guidance;
- current ISO, XEX, and extractor hashes; and
- its own update triggers.

`git check-ignore -v private/README.local.md` resolves to the root `/private/` rule. A prohibited-path staging audit confirms that neither the guide nor any other private payload appears in the index.
