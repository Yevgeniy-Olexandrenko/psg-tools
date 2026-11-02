---
name: prepare-commit
description: Prepare the work in the psg-tools working tree for a commit. Review the new and changed code and algorithms, refactor where it helps, and cut the comments down to the essentials in Simplified Technical English. Use this skill whenever the user gets ready to commit or asks to prepare, clean up, polish or review the changes before a commit ("готовим к коммиту", "подготовь к коммиту", "давай закоммитим", "почисти перед коммитом", "ревью перед коммитом"), even when the skill is not named. The skill never commits; the user commits the work.
---

# Prepare a commit

The user reviews every change and commits it. This skill makes the working
tree ready for that: the changed code is reviewed and refactored where that
helps, and its comments are short and clear. Do not commit, stage, push or
amend, and do not offer to commit (CLAUDE.md forbids it).

## 1. Find the scope

- Run `git status --short` and `git diff --stat`. The modified and the new
  (untracked) source files are the scope.
- Use `git diff` to see which code is new. Review the new and changed code
  and the code it calls; leave older code alone unless the change touches it.
- Read each changed region as a whole, not only the hunks: a refactor needs
  the context.
- The committed exes (`psg-tools-x64.exe`, `psg-tools-x86.exe`) are build
  output, not code to review. Untracked folders that are not part of the
  change (tools, reference emulators, playlists) stay out of scope; ask when
  it is not clear.

## 2. Review and refactor

Look for these problems and fix them:

- Bugs: off-by-one, overflow (check the maximum of each fixed-point value),
  signed and unsigned mixes, rounding, division by zero, uninitialized
  members, data races between the player thread and the GUI.
- Algorithms: wrong results, a simpler equivalent, work per frame that a
  table or a cache removes. `psg_audio` is meant to run on embedded targets
  too, so keep its per-frame code in integers and without allocations.
- Leftovers of experiments: unused functions, parameters, members,
  constants and includes, temporary keys, switches and defaults.
- Duplicated code that a small helper removes, names that do not tell what
  a thing is, magic numbers that need a named constant.
- Differences from the surrounding code: naming (`m_` members, `k_`
  constants), formatting, tabs or spaces as the file has them.
- The rules of CLAUDE.md: ASCII-only sources (use escapes such as
  `wchar_t(0x258C)`), Windows 7 APIs only, new files listed in the
  `.vcxproj` and `.vcxproj.filters`, CRLF in the project files.

Refactor only where the result is clearly better: simpler, safer, or faster
where speed matters. Keep the behavior; if a fix changes it, say so in the
report.

## 3. Comments

Rewrite the comments of the changed code in Simplified Technical English
(ASD-STE100):

- Short sentences: not more than 20 words for an instruction, 25 for a
  description. One idea in each sentence.
- Simple words with one meaning. Use the same word for the same thing.
- Active voice and present tense.
- Write only what the code does not show: why, units and fixed-point
  formats, the source of a formula or a constant, limits, thread rules,
  chip quirks. Remove comments that only repeat the code.
- Keep the comments few: a header for a non-obvious class or function, one
  line before a tricky block. Match the comment style of the file.

Example:

    before: // the player thread keeps writing while the levels are copied, so the
            // oldest quarter of the history, which it overwrites next, is not taken
    after:  // The player thread writes while the GUI copies, so do not take
            // the oldest quarter: the player writes there next.

## 4. Verify

- Build Release x64 and x86 with MSBuild (see CLAUDE.md). Both must build
  without new warnings. The Release exes are committed, so keep the new ones.
- If the user runs `psg-tools-x64.exe` (`Get-Process psg-tools*`), the exe
  in the repo root is locked. Do not stop the user's process: build a copy
  in the scratchpad with `cl`, and build the root exes when the user closes it.
- Run what exercises the change, for example play `sample.pt3` in a terminal
  tab with `-w <scratch>.wav`, or a scratch harness for an algorithm. A
  refactor must keep the results: compare numbers or output before and after.
- Delete the scratch output files.

## 5. Report

Reply in the user's language. Tell:

- what the review found and fixed, bugs first; what was refactored and removed;
- how the comments changed, in short;
- what was built and checked, and what was not;
- the open questions that the user must decide.
