# Development Journal - template

The brief asks for a Development Journal (a separate document in OneDrive) and a screencast for
each Tuesday and Friday deliverable. Copy this structure into your Word document and write one
entry per deliverable, in your own words.

---

## Entry: <date>  (Tuesday / Friday deliverable)

**Team:** <name 1>, <name 2>
**Hours this period:** <name 1>: x h, <name 2>: y h

### What we did
- ...

### Decisions and why
- e.g. "We used raylib with a 1280x800 virtual canvas so the layout is the same on every laptop."
- e.g. "The app calls the real exam solutions instead of hard-coded answers, so what students see is real behaviour."

### Problems and how we solved them
- ...

### Use of AI (the brief allows any free AI)
- Tool used:
- What we asked it to do:
- What we changed or checked ourselves:

### Testing
- `pq_tests` result:
- `Segfault --selftest` result:
- Manual checks:

### Screencast
- File name in the OneDrive folder:
- What it shows:

### Next steps
- ...

---

## Suggested screencast checklist (final demo, 17 November)
1. Home: the three paths (Study, Practice, Mock Exam), the quiz and progress by section.
2. Study: a lesson with a step-through walkthrough, a quick check, then "Open demo" (memory visualizer).
3. Practice: open a problem, write the code, Run (one case fails), fix it, Submit -> Accepted.
4. Practice: show a compile error pointing at your line, and Time Limit Exceeded with a slow prime check.
5. Mock Exam: start the quick mock, run the examples, submit, show the results and the reference solution.
6. Quick Quiz: a few questions and the results screen.
7. The code: `src/solutions` + `pq_tests`, how the runner builds the test harness, `--selftest`.
