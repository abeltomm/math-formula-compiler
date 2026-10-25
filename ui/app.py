#!/usr/bin/env python3
"""Mathematical Formula Compiler - desktop front end (Tkinter, dark theme).

This file contains NO compiler logic. Every token, tree, symbol and diagnostic
shown here is parsed from the JSON printed by the C executable
(see compiler_bridge.py). Only the Grammar tab shows static documentation.

Run:  python ui/app.py        (after building the C compiler)
"""
import os
import sys
import tkinter as tk
from tkinter import ttk, messagebox

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from compiler_bridge import CompilerError, parse_variables, run_compiler  # noqa: E402

C = dict(bg="#15171c", panel="#1d2027", panel2="#262a33", border="#363c48", text="#d9dee7",
         muted="#8b94a5", accent="#4cc2ff", ok="#3fb97f", err="#ef5b5b", warn="#e0a93b",
         editor="#101216")
UI_FONT = ("Segoe UI", 10)
MONO = ("Consolas", 11)

STAGES = ["Source Input", "Lexical Analysis", "Token Generation", "Syntax Analysis",
          "AST Generation", "Semantic Analysis", "Final Result"]

EXAMPLES = [
    ("valid", "2 + 3 * 4"), ("valid", "(a + b) * c"), ("valid", "x^2 + 2*x + 1"),
    ("valid", "sqrt(25)"), ("valid", "sqrt(x^2 + y^2)"), ("valid", "pow(2, 3) + 5"),
    ("valid", "-x + 4.5"), ("valid", "pow(a + b, 2)"), ("valid", "2^3^2"), ("valid", "-2^2"),
    ("invalid", "2 + * 3"), ("invalid", "(2 + 3"), ("invalid", "2 + 3)"), ("invalid", "sqrt()"),
    ("invalid", "pow(2)"), ("invalid", "pow(2, 3, 4)"), ("invalid", "unknown(5)"),
    ("invalid", "2 @ 3"), ("invalid", "5 +"), ("invalid", "3 / (2 - 2)"), ("invalid", "12abc"),
]

GRAMMAR_TEXT = """CONTEXT-FREE GRAMMAR (EBNF)  -  parsed by recursive descent
=======================================================
expression      ->  additive
additive        ->  multiplicative { ("+" | "-") multiplicative }
multiplicative  ->  unary { ("*" | "/") unary }
unary           ->  ("+" | "-") unary  |  power
power           ->  primary [ "^" unary ]
primary         ->  NUMBER
                 |  IDENTIFIER
                 |  call
                 |  "(" expression ")"
call            ->  (FUNCTION | IDENTIFIER) "(" [ expression { "," expression } ] ")"

Terminals:  NUMBER  IDENTIFIER  FUNCTION(sqrt, pow)  + - * / ^ ( ) ,

OPERATOR PRECEDENCE (highest first)
-----------------------------------
  Level  Operator            Associativity   Grammar rule
  1      ( )  call()         -               primary
  2      ^                   right           power
  3      unary + -           right           unary
  4      *  /                left            multiplicative
  5      +  -                left            additive

Consequences
  2^3^2   =  2^(3^2)  = 512     (right-associative, because the exponent is a `unary`)
  -2^2    = -(2^2)    = -4      (unary sits ABOVE power in the grammar)
  2^-3    =  2^(-3)             (the exponent may itself be a signed operand)
  8/4/2   = (8/4)/2   = 1       (left-associative)

LEFT-RECURSION ELIMINATION (grammar transformation)
---------------------------------------------------
  Original (ambiguous / left-recursive):   E -> E + T | E - T | T
  Transformed (usable by top-down parser): E -> T E'      E' -> + T E' | - T E' | epsilon
  The EBNF repetition { ... } above is the compact form of E'; the parser realises it
  as a while-loop that builds LEFT-leaning tree nodes, preserving left associativity.

Semantic rules (checked AFTER parsing, on the AST)
  sqrt takes exactly 1 argument, pow exactly 2; any other function name is unsupported.
  Division by a constant that folds to 0 is rejected; sqrt of a constant < 0 is rejected.
"""


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Mathematical Formula Compiler")
        self.geometry("1280x860")
        self.minsize(1000, 720)
        self.configure(bg=C["bg"])
        self._style()
        self._build_header()
        self._build_editor()
        self._build_pipeline()
        self._build_results()
        self.set_status("Ready")
        self.reset_pipeline()
        self.editor.insert("1.0", "x^2 + 2*x + 1")
        self.editor.focus_set()

    # ------------------------------------------------------------ styling
    def _style(self):
        s = ttk.Style(self)
        s.theme_use("clam")
        s.configure(".", background=C["bg"], foreground=C["text"], font=UI_FONT)
        s.configure("Treeview", background=C["panel"], fieldbackground=C["panel"], foreground=C["text"],
                    rowheight=26, borderwidth=0, font=("Consolas", 10))
        s.configure("Treeview.Heading", background=C["panel2"], foreground=C["accent"], relief="flat",
                    font=("Segoe UI", 10, "bold"), padding=6)
        s.map("Treeview", background=[("selected", "#2b4a63")], foreground=[("selected", "#ffffff")])
        s.map("Treeview.Heading", background=[("active", C["panel2"])])
        s.configure("TNotebook", background=C["bg"], borderwidth=0, tabmargins=(0, 4, 0, 0))
        s.configure("TNotebook.Tab", background=C["panel"], foreground=C["muted"], padding=(14, 7), borderwidth=0)
        s.map("TNotebook.Tab", background=[("selected", C["panel2"])], foreground=[("selected", C["accent"])])
        s.configure("TCombobox", fieldbackground=C["panel2"], background=C["panel2"], foreground=C["text"],
                    arrowcolor=C["accent"], bordercolor=C["border"])
        s.map("TCombobox", fieldbackground=[("readonly", C["panel2"])], foreground=[("readonly", C["text"])])
        s.configure("Vertical.TScrollbar", background=C["panel2"], troughcolor=C["panel"], bordercolor=C["panel"],
                    arrowcolor=C["muted"])
        s.configure("TCheckbutton", background=C["bg"], foreground=C["text"])
        s.map("TCheckbutton", background=[("active", C["bg"])])
        self.option_add("*TCombobox*Listbox.background", C["panel2"])
        self.option_add("*TCombobox*Listbox.foreground", C["text"])

    def _button(self, parent, text, command, primary=False):
        bg = C["accent"] if primary else C["panel2"]
        fg = "#06222f" if primary else C["text"]
        b = tk.Button(parent, text=text, command=command, bg=bg, fg=fg, activebackground=C["accent"],
                      activeforeground="#06222f", relief="flat", bd=0, padx=16, pady=7, cursor="hand2",
                      font=("Segoe UI", 10, "bold" if primary else "normal"),
                      highlightthickness=1, highlightbackground=C["border"])
        return b

    # ------------------------------------------------------------ layout
    def _build_header(self):
        h = tk.Frame(self, bg=C["panel"], highlightthickness=1, highlightbackground=C["border"])
        h.pack(fill="x", padx=14, pady=(14, 8))
        left = tk.Frame(h, bg=C["panel"])
        left.pack(side="left", padx=16, pady=10)
        tk.Label(left, text="Mathematical Formula Compiler", bg=C["panel"], fg=C["text"],
                 font=("Segoe UI", 18, "bold")).pack(anchor="w")
        tk.Label(left, text="Compiler Design \u2014 Front-End Analysis", bg=C["panel"], fg=C["accent"],
                 font=("Segoe UI", 10)).pack(anchor="w")
        right = tk.Frame(h, bg=C["panel"])
        right.pack(side="right", padx=18)
        self.dot = tk.Canvas(right, width=14, height=14, bg=C["panel"], highlightthickness=0)
        self.dot.pack(side="left", padx=(0, 8))
        self.dot_item = self.dot.create_oval(2, 2, 12, 12, fill=C["muted"], outline="")
        self.status_lbl = tk.Label(right, text="Ready", bg=C["panel"], fg=C["text"], font=("Segoe UI", 11, "bold"))
        self.status_lbl.pack(side="left")

    def _build_editor(self):
        box = tk.Frame(self, bg=C["panel"], highlightthickness=1, highlightbackground=C["border"])
        box.pack(fill="x", padx=14, pady=4)
        tk.Label(box, text="FORMULA EDITOR", bg=C["panel"], fg=C["muted"],
                 font=("Segoe UI", 9, "bold")).pack(anchor="w", padx=14, pady=(10, 4))
        self.editor = tk.Text(box, height=4, bg=C["editor"], fg=C["text"], insertbackground=C["accent"],
                              font=("Consolas", 14), relief="flat", padx=10, pady=8, undo=True, wrap="word",
                              highlightthickness=1, highlightbackground=C["border"], highlightcolor=C["accent"])
        self.editor.pack(fill="x", padx=14)
        self.editor.tag_configure("err", background="#6b2327", foreground="#ffffff")
        self.editor.bind("<Control-Return>", lambda e: (self.analyze(), "break")[1])
        self.editor.bind("<Key>", self._on_edit)

        row = tk.Frame(box, bg=C["panel"])
        row.pack(fill="x", padx=14, pady=10)
        self.examples = {("\u2713 " if k == "valid" else "\u2717 ") + f: f for k, f in EXAMPLES}
        self.combo = ttk.Combobox(row, values=list(self.examples), state="readonly", width=26)
        self.combo.set("Load an example formula\u2026")
        self.combo.bind("<<ComboboxSelected>>", self._load_example)
        self.combo.pack(side="left")

        self.eval_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(row, text="Evaluate (optional) with variables:", variable=self.eval_var).pack(side="left", padx=(18, 6))
        self.vars_entry = tk.Entry(row, width=22, bg=C["editor"], fg=C["text"], insertbackground=C["accent"],
                                   relief="flat", font=("Consolas", 11), highlightthickness=1,
                                   highlightbackground=C["border"], highlightcolor=C["accent"])
        self.vars_entry.insert(0, "x=3, y=4")
        self.vars_entry.pack(side="left", ipady=4)

        self._button(row, "Analyze / Compile  (Ctrl+Enter)", self.analyze, primary=True).pack(side="right")
        self._button(row, "Clear Formula", self.clear).pack(side="right", padx=8)

    def _on_edit(self, event):
        """Typing clears the old error highlight (modifier keys alone do not)."""
        if event.keysym not in ("Control_L", "Control_R", "Shift_L", "Shift_R", "Alt_L", "Alt_R"):
            self.editor.tag_remove("err", "1.0", "end")

    def _build_pipeline(self):
        box = tk.Frame(self, bg=C["bg"])
        box.pack(fill="x", padx=14, pady=(8, 2))
        self.stage_labels = []
        for i, name in enumerate(STAGES):
            lbl = tk.Label(box, text="%d. %s" % (i + 1, name), font=("Segoe UI", 9, "bold"), width=17, height=3,
                           justify="center", highlightthickness=1)
            lbl.pack(side="left", expand=True, fill="x")
            self.stage_labels.append(lbl)
            if i < len(STAGES) - 1:
                tk.Label(box, text="\u203A", bg=C["bg"], fg=C["muted"], font=("Segoe UI", 16)).pack(side="left", padx=2)
        self.result_lbl = tk.Label(self, text="", bg=C["bg"], fg=C["text"], font=("Segoe UI", 12, "bold"),
                                   anchor="w", justify="left", wraplength=1200)
        self.result_lbl.pack(fill="x", padx=16, pady=(8, 0))
        self.eval_lbl = tk.Label(self, text="", bg=C["bg"], fg=C["muted"], font=("Consolas", 11), anchor="w")
        self.eval_lbl.pack(fill="x", padx=16)

    def _tree(self, parent, cols):
        frame = tk.Frame(parent, bg=C["panel"])
        tree = ttk.Treeview(frame, columns=[c[0] for c in cols], show="headings", selectmode="browse")
        for key, title, width, anchor in cols:
            tree.heading(key, text=title)
            tree.column(key, width=width, anchor=anchor, stretch=(key == cols[-1][0]))
        sb = ttk.Scrollbar(frame, orient="vertical", command=tree.yview)
        tree.configure(yscrollcommand=sb.set)
        tree.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        tree.tag_configure("bad", foreground=C["err"])
        tree.tag_configure("good", foreground=C["ok"])
        tree.tag_configure("dim", foreground=C["muted"])
        return frame, tree

    def _text(self, parent):
        frame = tk.Frame(parent, bg=C["panel"])
        t = tk.Text(frame, bg=C["editor"], fg=C["text"], font=MONO, relief="flat", padx=14, pady=10, wrap="none",
                    state="disabled", highlightthickness=0)
        sb = ttk.Scrollbar(frame, orient="vertical", command=t.yview)
        t.configure(yscrollcommand=sb.set)
        t.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        t.tag_configure("h", foreground=C["accent"], font=("Consolas", 11, "bold"))
        t.tag_configure("ok", foreground=C["ok"])
        t.tag_configure("bad", foreground=C["err"])
        t.tag_configure("dim", foreground=C["muted"])
        return frame, t

    def _build_results(self):
        self.nb = ttk.Notebook(self)
        self.nb.pack(fill="both", expand=True, padx=14, pady=(8, 14))

        f, self.tok_tree = self._tree(self.nb, [("n", "#", 50, "center"), ("type", "Token Type", 170, "w"),
                                                ("lex", "Lexeme", 200, "w"), ("pos", "Position (line:col)", 160, "w")])
        self.nb.add(f, text="Tokens")

        f, t = self._text(self.nb)
        self.nb.add(f, text="Grammar")
        self._write(t, [(GRAMMAR_TEXT, None)])

        f, self.ast_txt = self._text(self.nb)
        self.nb.add(f, text="Parse Tree / AST")

        sym = tk.Frame(self.nb, bg=C["panel"])
        f, self.sym_tree = self._tree(sym, [("name", "Identifier", 180, "w"), ("kind", "Kind", 110, "w"),
                                            ("type", "Type", 140, "w"), ("occ", "Occurrences", 110, "center"),
                                            ("pos", "First seen (line:col)", 200, "w")])
        f.pack(fill="both", expand=True)
        tk.Label(sym, text="Policy: variables are free and implicitly declared as real numbers; they need no value "
                           "to pass compilation. Values are only needed by the optional evaluator.",
                 bg=C["panel"], fg=C["muted"], anchor="w", wraplength=1100, justify="left").pack(fill="x", padx=10, pady=6)
        self.nb.add(sym, text="Symbol Table")

        f, self.sem_tree = self._tree(self.nb, [("name", "Semantic Check", 210, "w"), ("exam", "Examined", 90, "center"),
                                                ("viol", "Violations", 90, "center"), ("res", "Result", 90, "center"),
                                                ("desc", "What is checked", 500, "w")])
        self.nb.add(f, text="Semantic Analysis")

        f, self.err_tree = self._tree(self.nb, [("cat", "Category", 100, "w"), ("stage", "Stage", 160, "w"),
                                                ("pos", "Position", 90, "center"), ("msg", "Message", 800, "w")])
        self.nb.add(f, text="Errors")

        f, self.sum_txt = self._text(self.nb)
        self.nb.add(f, text="Summary")

    # ------------------------------------------------------------ helpers
    def _write(self, widget, parts):
        widget.configure(state="normal")
        widget.delete("1.0", "end")
        for text, tag in parts:
            widget.insert("end", text, tag) if tag else widget.insert("end", text)
        widget.configure(state="disabled")

    def set_status(self, name):
        color = {"Ready": C["muted"], "Processing": C["accent"], "Success": C["ok"], "Error": C["err"]}[name]
        self.status_lbl.config(text=name)
        self.dot.itemconfig(self.dot_item, fill=color)

    def set_stage(self, i, state):
        look = {"pending": (C["panel2"], C["muted"], C["border"], "\u00B7"),
                "processing": (C["accent"], "#06222f", C["accent"], "\u2026"),
                "passed": ("#1d4a36", "#b8f0d4", C["ok"], "\u2713 passed"),
                "failed": ("#5a2226", "#ffd0d0", C["err"], "\u2717 FAILED"),
                "skipped": (C["panel"], C["muted"], C["border"], "\u2013 skipped")}[state]
        lbl = self.stage_labels[i]
        lbl.config(text="%d. %s\n%s" % (i + 1, STAGES[i], look[3]), bg=look[0], fg=look[1],
                   highlightbackground=look[2])

    def reset_pipeline(self, state="pending"):
        for i in range(len(STAGES)):
            self.set_stage(i, state)

    def clear(self):
        self.editor.delete("1.0", "end")
        self.editor.focus_set()
        self.reset_pipeline()
        self.set_status("Ready")
        self.result_lbl.config(text="")
        self.eval_lbl.config(text="")
        for tree in (self.tok_tree, self.sym_tree, self.sem_tree, self.err_tree):
            tree.delete(*tree.get_children())
        for t in (self.ast_txt, self.sum_txt):
            self._write(t, [])
        self.nb.tab(5, text="Errors")

    def _load_example(self, _event):
        formula = self.examples.get(self.combo.get())
        if formula:
            self.editor.delete("1.0", "end")
            self.editor.insert("1.0", formula)
            self.analyze()

    # ------------------------------------------------------------ main action
    def analyze(self):
        formula = self.editor.get("1.0", "end-1c")
        evaluate = self.eval_var.get()
        if evaluate:
            try:
                parse_variables(self.vars_entry.get())
            except ValueError as exc:
                messagebox.showerror("Invalid variable list", str(exc))
                return
        self.set_status("Processing")
        self.reset_pipeline("processing")
        self.result_lbl.config(text="Running the compiler\u2026", fg=C["muted"])
        self.update_idletasks()
        try:
            rep = run_compiler(formula, evaluate=evaluate, variables=self.vars_entry.get())
        except CompilerError as exc:
            self.reset_pipeline()
            self.set_status("Error")
            self.result_lbl.config(text="Could not run the compiler.", fg=C["err"])
            messagebox.showerror("Compiler unavailable", str(exc))
            return
        self.render(rep)

    # ------------------------------------------------------------ rendering
    def render(self, rep):
        ok = rep["status"] == "success"
        for i, st in enumerate(rep["stages"]):
            self.set_stage(i, st["status"])

        # Tokens
        self.tok_tree.delete(*self.tok_tree.get_children())
        for i, t in enumerate(rep["tokens"], 1):
            tag = "bad" if t["type"] == "INVALID_TOKEN" else ("dim" if t["type"] == "END_OF_INPUT" else "")
            self.tok_tree.insert("", "end", values=(i, t["type"], t["lexeme"], "%d:%d" % (t["line"], t["col"])),
                                 tags=(tag,))

        # AST
        if rep["ast_tree"]:
            self._write(self.ast_txt, [("Abstract Syntax Tree\n", "h"), ("\n" + rep["ast_tree"], None),
                                       ("\nPrefix (S-expression) form\n", "h"), ("\n" + rep["ast_sexpr"] + "\n", "ok")])
        else:
            stage = rep["failed_stage"] or "an earlier stage"
            self._write(self.ast_txt, [("No AST was generated.\n", "bad"),
                                       ("\nCompilation stopped at %s, so the parser never produced a complete tree." % stage, "dim")])

        # Symbols
        self.sym_tree.delete(*self.sym_tree.get_children())
        for s in rep["symbols"]:
            self.sym_tree.insert("", "end", values=(s["name"], s["kind"], s["type"], s["occurrences"],
                                                    "%d:%d" % (s["line"], s["col"])))

        # Semantic checks
        self.sem_tree.delete(*self.sem_tree.get_children())
        if rep["semantic_checks"]:
            for c in rep["semantic_checks"]:
                self.sem_tree.insert("", "end", values=(c["name"], c["examined"], c["violations"],
                                                        c["status"].upper(), c["description"]),
                                     tags=("bad" if c["status"] == "failed" else "good",))
        else:
            self.sem_tree.insert("", "end", values=("(not run)", "", "", "SKIPPED",
                                                    "Semantic analysis needs a valid AST; an earlier stage failed."),
                                 tags=("dim",))

        # Errors
        self.err_tree.delete(*self.err_tree.get_children())
        self.editor.tag_remove("err", "1.0", "end")
        for e in rep["errors"]:
            pos = "%d:%d" % (e["line"], e["col"]) if e["line"] else "-"
            self.err_tree.insert("", "end", values=(e["category"], e["stage"], pos, e["message"]), tags=("bad",))
            if e["line"]:
                idx = "%d.%d" % (e["line"], max(e["col"] - 1, 0))
                self.editor.tag_add("err", idx, idx + "+1c")
        ev = rep["evaluation"]
        if ev["requested"] and ev["performed"] and not ev["ok"]:
            pos = "%d:%d" % (ev["line"], ev["col"]) if ev["line"] else "-"
            self.err_tree.insert("", "end", values=("Runtime", "Evaluation (optional)", pos, ev["message"]),
                                 tags=("bad",))
        n_err = len(self.err_tree.get_children())
        self.nb.tab(5, text="Errors (%d)" % n_err if n_err else "Errors")

        # Result banner
        if ok:
            self.set_status("Success")
            self.result_lbl.config(text="Compilation Successful \u2014 No Lexical, Syntax, or Semantic Errors.", fg=C["ok"])
            self.nb.select(2)
        else:
            self.set_status("Error")
            first = rep["errors"][0]
            where = ", line %d col %d" % (first["line"], first["col"]) if first["line"] else ""
            n = len(rep["errors"])
            self.result_lbl.config(text="Compilation Failed \u2014 stage: %s%s\n%s%s" % (
                rep["failed_stage"], where, first["message"], "  (+%d more)" % (n - 1) if n > 1 else ""), fg=C["err"])
            self.nb.select(5)

        # Optional evaluation line
        if not ev["requested"]:
            self.eval_lbl.config(text="")
        elif not ev["performed"]:
            self.eval_lbl.config(text="Evaluation skipped: the formula did not compile.", fg=C["muted"])
        elif ev["ok"]:
            self.eval_lbl.config(text="Evaluation result = %s" % ("%.10g" % ev["value"]), fg=C["accent"])
        else:
            self.eval_lbl.config(text="Evaluation (runtime) error: %s" % ev["message"], fg=C["warn"])

        self._render_summary(rep)

    def _render_summary(self, rep):
        ok = rep["status"] == "success"
        parts = [("Compilation summary\n", "h"), ("=" * 60 + "\n", "dim"),
                 ("Formula : %s\n" % rep["source"].replace("\n", " \u23CE "), None),
                 ("Status  : ", None),
                 ("SUCCESS\n" if ok else "FAILED at %s\n" % rep["failed_stage"], "ok" if ok else "bad"),
                 ("\nPipeline stages\n", "h")]
        for i, st in enumerate(rep["stages"], 1):
            mark = {"passed": "\u2713", "failed": "\u2717", "skipped": "\u2013"}[st["status"]]
            parts.append(("  %s %d. %-18s %s\n" % (mark, i, st["name"], st["status"]),
                          {"passed": "ok", "failed": "bad", "skipped": "dim"}[st["status"]]))
        real = [t for t in rep["tokens"] if t["type"] != "END_OF_INPUT"]
        parts += [("\nStatistics\n", "h"),
                  ("  Tokens (excluding end marker) : %d\n" % len(real), None),
                  ("  Distinct variables            : %d  %s\n" % (
                      len(rep["symbols"]), "(" + ", ".join(s["name"] for s in rep["symbols"]) + ")" if rep["symbols"] else ""), None),
                  ("  Diagnostics                   : %d\n" % len(rep["errors"]), None)]
        if rep["ast_sexpr"]:
            parts.append(("  AST (prefix form)             : %s\n" % rep["ast_sexpr"], None))
        ev = rep["evaluation"]
        if ev["requested"]:
            parts.append(("\nEvaluation (optional back-end, separate from the front end)\n", "h"))
            if not ev["performed"]:
                parts.append(("  Skipped - the formula did not compile.\n", "dim"))
            elif ev["ok"]:
                parts.append(("  Result: %.10g\n" % ev["value"], "ok"))
            else:
                parts.append(("  Runtime error: %s\n" % ev["message"], "bad"))
        if ok:
            parts.append(("\nCompilation Successful \u2014 No Lexical, Syntax, or Semantic Errors.\n", "ok"))
        else:
            parts.append(("\nDiagnostics\n", "h"))
            for e in rep["errors"]:
                pos = " at %d:%d" % (e["line"], e["col"]) if e["line"] else ""
                parts.append(("  [%s]%s %s\n" % (e["category"], pos, e["message"]), "bad"))
        self._write(self.sum_txt, parts)


if __name__ == "__main__":
    App().mainloop()
