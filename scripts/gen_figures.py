#!/usr/bin/env python3
"""Generate the multi-threading guide's SVG figures, tuned to the htmler theme.

Same house style as the other know-how guides: because the figures are inlined
as static images (no page CSS reaches them), every colour is chosen to work on
BOTH the dark (#0b0d12) and light (#ffffff) themes at once. A mid-slate around
luminance ~0.2 gives roughly 4.3:1 contrast three ways -- white text on the
fill, and the same colour as ink on either background.

  * slate blue  #6B7B94  (neutral boxes, connectors, axes, labels)
  * blue        #3E7CC0  (thread A / highlighted boxes)            + dark #2F5F98
  * teal        #1F918C  (thread B / positive "result" accent)
  * amber       #D9922B  (warning / contention; dark text on fill)
  * red         #D65A5F  (problem callouts / errors)
  * muted       #9AA0B4  (captions)
  * white       #FFFFFF  (text inside dark fills)
  * 1.5pt wide rules, hand-drawn Virgil font stack

Run:  python3 scripts/gen_figures.py
Output: figures/*.svg  (referenced from the chapter markdown)
"""
import base64
import io
import math
import os

# House-style constants (htmler blue theme, dual light/dark legible)
GREY = "#6B7B94"
GREY_D = "#55637A"
BLUE = "#3E7CC0"
BLUE_D = "#2F5F98"
TEAL = "#1F918C"
AMBER = "#D9922B"
RED = "#D65A5F"
WHITE = "#FFFFFF"
LIGHT = "#9AA0B4"
INK_DARK = "#1F2433"
FONT = ("'Virgil','Segoe Print','Bradley Hand','Comic Sans MS',"
        "'Segoe UI',system-ui,-apple-system,sans-serif")
MONO = ("'Virgil','SFMono-Regular',ui-monospace,'JetBrains Mono',Consolas,"
        "monospace")
RULE = 1.5

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "./JetBrains Mono Nerd Font Regular.woff2")

USED_CHARS = set()
FONT_STYLE = ""


def esc(s):
    USED_CHARS.update(str(s))
    return (str(s).replace("&", "&amp;").replace("<", "&lt;")
            .replace(">", "&gt;"))


def defs():
    marks = []
    for name, col in (("g", GREY), ("p", BLUE), ("t", TEAL),
                      ("r", RED), ("a", AMBER), ("l", LIGHT)):
        marks.append(
            f'<marker id="ah-{name}" viewBox="0 0 10 10" refX="8" refY="5" '
            f'markerWidth="4.5" markerHeight="4.5" '
            f'orient="auto-start-reverse">'
            f'<path d="M0 1L9 5L0 9z" fill="{col}"/></marker>')
    return "<defs>" + "".join(marks) + "</defs>"


def rrect(x, y, w, h, fill, rx=9, stroke=None, sw=RULE, dash=None, opacity=None):
    s = (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" ry="{rx}" '
         f'fill="{fill}"')
    if stroke:
        s += f' stroke="{stroke}" stroke-width="{sw}"'
    if dash:
        s += f' stroke-dasharray="{dash}"'
    if opacity is not None:
        s += f' opacity="{opacity}"'
    return s + "/>"


def tspan_lines(x, cy, lines, fill, size, weight, lh, mono=False):
    fam = MONO if mono else FONT
    n = len(lines)
    y0 = cy - (n - 1) * lh / 2.0
    out = [f'<text x="{x}" y="{y0}" fill="{fill}" font-family="{fam}" '
           f'font-size="{size}" font-weight="{weight}" text-anchor="middle" '
           f'dominant-baseline="central">']
    for i, ln in enumerate(lines):
        dy = 0 if i == 0 else lh
        out.append(f'<tspan x="{x}" dy="{dy}">{esc(ln)}</tspan>')
    out.append("</text>")
    return "".join(out)


def box(x, y, w, h, lines, fill=GREY, tcol=WHITE, size=13, weight=600,
        rx=9, lh=16, stroke=None, sw=RULE, dash=None, mono=False):
    if isinstance(lines, str):
        lines = lines.split("\n")
    r = rrect(x, y, w, h, fill, rx=rx, stroke=stroke, sw=sw, dash=dash)
    t = tspan_lines(x + w / 2.0, y + h / 2.0, lines, tcol, size, weight, lh, mono)
    return r + t


def obox(x, y, w, h, lines, stroke=GREY, tcol=GREY, size=13, weight=600,
         rx=9, lh=16, sw=RULE, dash=None, fill="none", mono=False):
    r = rrect(x, y, w, h, fill, rx=rx, stroke=stroke, sw=sw, dash=dash)
    t = tspan_lines(x + w / 2.0, y + h / 2.0, lines if isinstance(lines, list)
                    else [lines], tcol, size, weight, lh, mono)
    return r + t


def text(x, y, s, fill=GREY, size=13, weight=600, anchor="middle",
         italic=False, mono=False):
    fam = MONO if mono else FONT
    return (f'<text x="{x}" y="{y}" fill="{fill}" font-family="{fam}" '
            f'font-size="{size}" font-weight="{weight}" text-anchor="{anchor}"'
            f' dominant-baseline="central">{esc(s)}</text>')


def line(x1, y1, x2, y2, col=GREY, sw=RULE, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{col}" '
            f'stroke-width="{sw}"{d}/>')


def _mk(col):
    return {GREY: "g", BLUE: "p", TEAL: "t", RED: "r", AMBER: "a",
            LIGHT: "l"}.get(col, "g")


def arrow(x1, y1, x2, y2, col=GREY, sw=RULE, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{col}" '
            f'stroke-width="{sw}" marker-end="url(#ah-{_mk(col)})"{d}/>')


def path(d, col=GREY, sw=RULE, dash=None, arrow_end=False, fill="none"):
    dd = f' stroke-dasharray="{dash}"' if dash else ""
    m = f' marker-end="url(#ah-{_mk(col)})"' if arrow_end else ""
    return (f'<path d="{d}" fill="{fill}" stroke="{col}" stroke-width="{sw}"'
            f'{dd}{m}/>')


def circle(cx, cy, r, fill, stroke=None, sw=RULE):
    st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}"{st}/>'


def cylinder(x, y, w, h, fill=GREY, tcol=WHITE, lines=None, size=12,
             stroke=None, sw=RULE):
    ry = min(h * 0.16, 14)
    st = (f' stroke="{stroke}" stroke-width="{sw}"') if stroke else ""
    body = (f'<path d="M{x} {y+ry} A{w/2} {ry} 0 0 0 {x+w} {y+ry} '
            f'L{x+w} {y+h-ry} A{w/2} {ry} 0 0 1 {x} {y+h-ry} Z" '
            f'fill="{fill}"{st}/>')
    top = (f'<ellipse cx="{x+w/2}" cy="{y+ry}" rx="{w/2}" ry="{ry}" '
           f'fill="{fill}"{st}/>')
    t = ""
    if lines:
        t = tspan_lines(x + w / 2.0, y + h / 2.0 + ry / 2, lines, tcol, size,
                        600, 15)
    return body + top + t


def dash_boundary(x1, y, x2, label=None):
    """The user/kernel privilege boundary: a double dashed rule."""
    out = [line(x1, y, x2, y, AMBER, 1.4, dash="7 5"),
           line(x1, y + 4, x2, y + 4, AMBER, 1.4, dash="7 5")]
    if label:
        out.append(text((x1 + x2) / 2, y - 10, label, AMBER, 11, 700))
    return "".join(out)


def svg(w, h, body, title=""):
    t = f"<title>{esc(title)}</title>" if title else ""
    return (f'<?xml version="1.0" encoding="UTF-8"?>\n'
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" '
            f'width="{w}" height="{h}" font-family="{FONT}">{t}{FONT_STYLE}'
            f'{defs()}{body}</svg>\n')


def write(rel_path, content):
    full = os.path.join(REPO_ROOT, rel_path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w") as f:
        f.write(content)
    print("wrote", rel_path, f"({len(content)} bytes)")


# FIGURES

ALL = []


def fig(fn):
    ALL.append(fn)
    return fn


# -- 00 foundations ----------------------------------------------------------
@fig
def fig_process_vs_thread():
    W, H = 800, 400
    b = [text(W / 2, 26, "Threads share the address space; processes do not",
              GREY, 15, 700)]
    b.append(box(60, 70, 320, 260, "", "none", rx=12, stroke=BLUE, sw=1.4))
    b.append(text(220, 88, "one process, two threads", BLUE, 11, 700))
    b.append(box(90, 108, 260, 40, ["shared: heap, globals, code, fds"], TEAL,
                 size=10, rx=8))
    for i in range(2):
        x = 90 + i * 130
        b.append(box(x, 168, 110, 130, "", "none", rx=8, stroke=GREY, sw=1))
        b.append(text(x + 55, 186, f"thread {i}", GREY, 10, 700))
        b.append(box(x + 12, 202, 86, 36, "own stack", GREY_D, size=9, rx=5))
        b.append(box(x + 12, 248, 86, 36, "own regs/PC", GREY_D, size=9, rx=5))
    b.append(box(440, 70, 300, 260, "", "none", rx=12, stroke=GREY, sw=1.4))
    b.append(text(590, 88, "two processes", GREY, 11, 700))
    for i in range(2):
        x = 470 + i * 130
        b.append(box(x, 118, 110, 185, "", "none", rx=8, stroke=GREY, sw=1))
        b.append(text(x + 55, 136, f"process {i}", GREY, 10, 700))
        for j, seg in enumerate(["stack", "heap", "data", "code"]):
            b.append(box(x + 12, 150 + j * 37, 86, 30, seg, GREY_D, size=9,
                         rx=5))
    b.append(text(W / 2, H - 30, "threads communicate through shared memory "
                  "(fast) \u2014 but a race in one corrupts all", LIGHT, 11,
                  500))
    b.append(text(W / 2, H - 12, "processes are isolated \u2014 safer, but IPC "
                  "and context switches cost more", LIGHT, 11, 500))
    write("figures/process-vs-thread.svg", svg(W, H, "".join(b),
          "Process vs thread"))


@fig
def fig_concurrency_vs_parallelism():
    W, H = 800, 360
    b = [text(W / 2, 26, "Concurrency vs parallelism", GREY, 16, 700)]
    b.append(box(60, 66, 300, 30, "CONCURRENCY (1 core, interleaved)", BLUE,
                 size=11, rx=8))
    seq = [("A", BLUE), ("B", TEAL), ("A", BLUE), ("B", TEAL), ("A", BLUE),
           ("B", TEAL)]
    for i, (lab, col) in enumerate(seq):
        b.append(box(60 + i * 50, 110, 46, 40, lab, col, size=12, rx=6))
    b.append(text(210, 170, "tasks take turns on one core (time-sliced)",
                  LIGHT, 10, 500))
    b.append(box(440, 66, 300, 30, "PARALLELISM (2 cores, simultaneous)", TEAL,
                 size=11, rx=8))
    for i in range(3):
        b.append(box(460 + i * 90, 110, 80, 30, "A", BLUE, size=11, rx=6))
        b.append(box(460 + i * 90, 146, 80, 30, "B", TEAL, size=11, rx=6))
    b.append(text(590, 196, "core 0 runs A while core 1 runs B", LIGHT, 10,
                  500))
    b.append(text(W / 2, 258, "concurrency is about STRUCTURE (dealing with "
                  "many things); parallelism is about EXECUTION (doing many at "
                  "once)", LIGHT, 11, 500))
    b.append(text(W / 2, H - 16, "you can have concurrency without parallelism "
                  "(1 core) \u2014 and Amdahl's law caps the parallel speedup",
                  LIGHT, 11, 500))
    write("figures/concurrency-vs-parallelism.svg", svg(W, H, "".join(b),
          "Concurrency vs parallelism"))


@fig
def fig_thread_os_mapping():
    W, H = 780, 360
    b = [text(W / 2, 26, "Threads are scheduled by the kernel", GREY, 16, 700)]
    b.append(box(60, 66, 660, 34, "user space: pthread_create / std::thread / "
                 "std::async", BLUE, size=11, rx=8))
    for i in range(4):
        b.append(box(90 + i * 160, 120, 130, 40, f"thread {i}", TEAL, size=11,
                     rx=8))
        b.append(arrow(155 + i * 160, 160, 155 + i * 160, 196, GREY))
    b.append(dash_boundary(60, 178, 720, "clone() creates a task; futex() "
             "blocks/wakes it"))
    b.append(box(60, 210, 660, 34, "kernel: each thread is a schedulable task "
                 "(struct task_struct)", GREY_D, size=11, rx=8))
    b.append(box(200, 268, 380, 40, ["scheduler multiplexes tasks onto cores"],
                 GREY, size=11, rx=8))
    b.append(text(W / 2, H - 16, "a context switch saves/restores registers + "
                  "may flush TLB \u2014 ~1\u20135 microseconds, so threads are "
                  "not free", LIGHT, 11, 500))
    write("figures/thread-os-mapping.svg", svg(W, H, "".join(b),
          "Thread OS mapping"))


@fig
def fig_race_condition():
    W, H = 800, 380
    b = [text(W / 2, 26, "A race condition: count++ is not atomic", GREY, 15,
              700)]
    b.append(text(W / 2, 56, "count++  \u2261  load \u2192 add 1 \u2192 store   "
                  "(3 steps)", GREY, 12, 600, mono=True))
    b.append(box(70, 84, 150, 30, "thread A", BLUE, size=11, rx=8))
    b.append(box(580, 84, 150, 30, "thread B", TEAL, size=11, rx=8))
    steps = [
        (128, "load count (0)", BLUE, 70),
        (168, "load count (0)", TEAL, 580),
        (208, "add 1 \u2192 1", BLUE, 70),
        (248, "add 1 \u2192 1", TEAL, 580),
        (288, "store 1", BLUE, 70),
        (328, "store 1  (lost!)", TEAL, 580),
    ]
    for y, lab, col, x in steps:
        b.append(box(x, y, 150, 30, lab, col, size=10, rx=6, mono=True))
    b.append(text(W / 2, 300, "both read 0, both write 1", RED, 11, 700))
    b.append(text(W / 2, H - 14, "two increments, but count ends at 1 \u2014 one "
                  "update vanished. fix: a mutex or an atomic RMW", LIGHT, 11,
                  500))
    write("figures/race-condition.svg", svg(W, H, "".join(b), "Race condition"))


# -- 01 pthreads / basic synchronization -------------------------------------
@fig
def fig_pthread_lifecycle():
    W, H = 820, 300
    b = [text(W / 2, 26, "Thread lifecycle", GREY, 16, 700)]
    b.append(box(50, 120, 130, 54, ["pthread_create", "std::thread"], BLUE,
                 size=10, lh=14, rx=10))
    b.append(arrow(180, 147, 250, 147, GREY))
    b.append(box(250, 120, 130, 54, ["RUNNING", "(runs start fn)"], TEAL,
                 size=10, lh=14, rx=10))
    b.append(arrow(380, 147, 450, 147, GREY))
    b.append(box(450, 120, 140, 54, ["TERMINATED", "(returns / exits)"], GREY,
                 size=10, lh=14, rx=10))
    b.append(arrow(590, 135, 690, 100, BLUE))
    b.append(box(690, 78, 110, 44, ["join()", "\u2192 reaps"], BLUE, size=10,
                 lh=14, rx=8))
    b.append(arrow(590, 160, 690, 195, AMBER))
    b.append(box(690, 174, 110, 44, ["detach()", "\u2192 auto-free"], AMBER,
                 tcol=INK_DARK, size=10, lh=14, rx=8))
    b.append(text(W / 2, 250, "you MUST join() or detach() every thread; a "
                  "std::thread destroyed while joinable calls std::terminate",
                  LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "join() blocks until the thread finishes and "
                  "collects it; detach() lets it run independently", LIGHT, 11,
                  500))
    write("figures/pthread-lifecycle.svg", svg(W, H, "".join(b),
          "Thread lifecycle"))


@fig
def fig_mutex():
    W, H = 780, 320
    b = [text(W / 2, 26, "A mutex serializes the critical section", GREY, 16,
              700)]
    b.append(box(60, 90, 150, 50, ["thread A", "lock()"], BLUE, size=11,
                 lh=15, rx=10))
    b.append(box(60, 200, 150, 50, ["thread B", "lock() ... blocks"], TEAL,
                 size=10, lh=15, rx=10))
    b.append(box(300, 130, 200, 80, ["CRITICAL SECTION", "(one owner at a "
                 "time)"], AMBER, tcol=INK_DARK, size=11, lh=17, rx=12))
    b.append(arrow(210, 115, 300, 150, BLUE))
    b.append(arrow(210, 225, 300, 195, TEAL, dash="5 4"))
    b.append(arrow(500, 170, 600, 170, GREY))
    b.append(box(600, 145, 130, 50, ["unlock() \u2192", "B proceeds"], TEAL,
                 size=10, lh=15, rx=10))
    b.append(text(W / 2, 270, "always release on every path \u2014 use "
                  "lock_guard/unique_lock (RAII) so an exception can't leak the "
                  "lock", LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "keep critical sections short; the lock is a "
                  "serialization point that caps your speedup", LIGHT, 11, 500))
    write("figures/mutex.svg", svg(W, H, "".join(b), "Mutex"))


@fig
def fig_condition_variable():
    W, H = 800, 360
    b = [text(W / 2, 26, "Condition variable: wait until a predicate holds",
              GREY, 15, 700)]
    b.append(box(60, 80, 300, 130, "", "none", rx=12, stroke=TEAL, sw=1.4))
    b.append(text(210, 98, "waiter", TEAL, 11, 700))
    b.append(box(85, 116, 250, 32, "lock(m)", TEAL, size=10, rx=6, mono=True))
    b.append(box(85, 154, 250, 44, ["while(!ready) cv.wait(m);", "// atomically "
                 "unlock + sleep"], TEAL, size=9, lh=13, rx=6, mono=True))
    b.append(box(440, 80, 300, 130, "", "none", rx=12, stroke=BLUE, sw=1.4))
    b.append(text(590, 98, "signaler", BLUE, 11, 700))
    b.append(box(465, 116, 250, 32, "lock(m); ready=true;", BLUE, size=9, rx=6,
                 mono=True))
    b.append(box(465, 154, 250, 32, "cv.notify_one();", BLUE, size=10, rx=6,
                 mono=True))
    b.append(arrow(465, 170, 335, 176, BLUE, dash="5 4"))
    b.append(text(W / 2, 250, "ALWAYS wait in a while-loop on the predicate: "
                  "guards against spurious wakeups AND lost/early wakeups",
                  RED, 11, 600))
    b.append(text(W / 2, H - 16, "wait() releases the mutex while sleeping and "
                  "re-acquires it on wake \u2014 the pair is atomic", LIGHT, 11,
                  500))
    write("figures/condition-variable.svg", svg(W, H, "".join(b),
          "Condition variable"))


@fig
def fig_producer_consumer():
    W, H = 800, 300
    b = [text(W / 2, 26, "Producer / consumer with a bounded buffer", GREY, 15,
              700)]
    b.append(box(60, 110, 140, 50, ["producer(s)", "push"], BLUE, size=11,
                 lh=15, rx=10))
    b.append(box(600, 110, 140, 50, ["consumer(s)", "pop"], TEAL, size=11,
                 lh=15, rx=10))
    for i in range(5):
        fill = TEAL if i < 3 else "none"
        if i < 3:
            b.append(box(280 + i * 50, 115, 44, 40, "", TEAL, rx=6))
        else:
            b.append(obox(280 + i * 50, 115, 44, 40, "", GREY, GREY, rx=6,
                          dash="3 3"))
    b.append(text(340, 90, "bounded buffer (capacity N)", GREY, 10, 600))
    b.append(arrow(200, 135, 280, 135, BLUE))
    b.append(arrow(520, 135, 600, 135, TEAL))
    b.append(text(W / 2, 205, "full \u2192 producer waits on 'not full'; empty "
                  "\u2192 consumer waits on 'not empty'", LIGHT, 11, 500))
    b.append(text(W / 2, 232, "two condition variables + one mutex give "
                  "automatic back-pressure", LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "the canonical use of condition variables",
                  LIGHT, 11, 500))
    write("figures/producer-consumer.svg", svg(W, H, "".join(b),
          "Producer consumer"))


@fig
def fig_rwlock():
    W, H = 780, 300
    b = [text(W / 2, 26, "Reader-writer lock", GREY, 16, 700)]
    for i in range(3):
        b.append(box(60, 70 + i * 55, 150, 40, f"reader {i}", TEAL, size=10,
                     rx=8))
        b.append(arrow(210, 90 + i * 55, 320, 130, TEAL))
    b.append(box(320, 100, 160, 70, ["shared data"], BLUE, size=12, rx=12))
    b.append(box(600, 110, 130, 50, ["writer", "(exclusive)"], AMBER,
                 tcol=INK_DARK, size=10, lh=14, rx=8))
    b.append(arrow(600, 135, 480, 135, AMBER))
    b.append(text(W / 2, 210, "many readers share the lock; a writer needs it "
                  "alone (and blocks all readers)", LIGHT, 11, 500))
    b.append(text(W / 2, 238, "win only when reads greatly outnumber writes and "
                  "sections aren't trivial", LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "C++17: std::shared_mutex + shared_lock "
                  "(read) / unique_lock (write)", LIGHT, 11, 500))
    write("figures/rwlock.svg", svg(W, H, "".join(b), "Reader-writer lock"))


@fig
def fig_barrier():
    W, H = 780, 320
    b = [text(W / 2, 26, "Barrier: all threads meet before any continues", GREY,
              15, 700)]
    xs = [120, 320, 520, 660]
    for i, x in enumerate(xs):
        b.append(box(x - 55, 70, 110, 34, f"thread {i}", BLUE, size=10, rx=8))
        b.append(arrow(x, 104, x, 150, GREY))
    b.append(box(70, 150, 640, 40, "BARRIER  (wait for all N to arrive)", AMBER,
                 tcol=INK_DARK, size=12, rx=10))
    for i, x in enumerate(xs):
        b.append(arrow(x, 190, x, 236, TEAL))
        b.append(box(x - 55, 236, 110, 34, "phase 2", TEAL, size=10, rx=8))
    b.append(text(W / 2, H - 16, "early arrivals block until the last thread "
                  "reaches the barrier \u2014 then all release together (phased "
                  "computation)", LIGHT, 11, 500))
    write("figures/barrier.svg", svg(W, H, "".join(b), "Barrier"))


@fig
def fig_semaphore():
    W, H = 780, 320
    b = [text(W / 2, 26, "Semaphore: a counter of available permits", GREY, 15,
              700)]
    b.append(box(300, 70, 180, 60, ["sem = 3", "(3 permits free)"], BLUE,
                 size=12, lh=16, rx=12))
    for i in range(3):
        b.append(box(60, 70 + i * 70, 150, 44, [f"thread {i}", "acquire() "
                     "\u2192 ok"], TEAL, size=9, lh=13, rx=8))
        b.append(arrow(210, 92 + i * 70, 300, 100, TEAL))
    b.append(box(560, 90, 160, 44, ["thread 3", "acquire() \u2192 blocks"],
                 AMBER, tcol=INK_DARK, size=9, lh=13, rx=8))
    b.append(arrow(560, 112, 480, 105, AMBER, dash="5 4"))
    b.append(text(W / 2, 220, "acquire (P): if count>0 decrement, else block. "
                  "release (V): increment, wake a waiter", LIGHT, 11, 500))
    b.append(text(W / 2, 250, "counting semaphore = N permits; binary semaphore "
                  "(N=1) \u2248 a mutex without ownership", LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "C++20: std::counting_semaphore / "
                  "binary_semaphore", LIGHT, 11, 500))
    write("figures/semaphore.svg", svg(W, H, "".join(b), "Semaphore"))


# -- 02 C++ threads ----------------------------------------------------------
@fig
def fig_lock_types():
    W, H = 820, 360
    b = [text(W / 2, 26, "C++ lock wrappers (RAII over a mutex)", GREY, 16,
              700)]
    rows = [
        ("lock_guard", "lock in ctor, unlock in dtor", "simplest scoped lock",
         TEAL),
        ("unique_lock", "movable, defer/try/timed, unlock early", "condition "
         "variables, flexibility", BLUE),
        ("scoped_lock", "locks MANY mutexes deadlock-free", "taking 2+ locks at "
         "once", BLUE_D),
        ("shared_lock", "shared (read) ownership", "readers on a shared_mutex",
         GREY_D),
    ]
    for i, (name, how, when, col) in enumerate(rows):
        y = 64 + i * 70
        b.append(box(50, y, 220, 52, name, col, size=12, rx=10, mono=False))
        b.append(text(290, y + 18, how, GREY, 11, 500, anchor="start"))
        b.append(text(290, y + 38, "use for: " + when, LIGHT, 10, 500,
                      anchor="start"))
    b.append(text(W / 2, H - 16, "prefer lock_guard/scoped_lock by default; "
                  "reach for unique_lock only when you need its extra powers",
                  LIGHT, 11, 500))
    write("figures/lock-types.svg", svg(W, H, "".join(b), "C++ lock types"))


@fig
def fig_future_async():
    W, H = 800, 340
    b = [text(W / 2, 26, "std::async / promise / future", GREY, 16, 700)]
    b.append(box(60, 90, 170, 54, ["std::async(f)", "or promise+thread"], BLUE,
                 size=10, lh=14, rx=10))
    b.append(arrow(230, 117, 320, 117, GREY))
    b.append(box(320, 80, 170, 74, ["shared state", "(result or exception,", "+ "
                 "ready flag)"], AMBER, tcol=INK_DARK, size=10, lh=15, rx=10))
    b.append(arrow(490, 117, 580, 117, GREY))
    b.append(box(580, 90, 170, 54, ["future.get()", "blocks until ready"], TEAL,
                 size=10, lh=14, rx=10))
    b.append(text(150, 190, "produces the value", LIGHT, 10, 500))
    b.append(text(665, 190, "consumes it once", LIGHT, 10, 500))
    b.append(text(W / 2, 240, "a future is a one-shot channel for a result "
                  "computed elsewhere; get() also re-throws a stored exception",
                  LIGHT, 11, 500))
    b.append(text(W / 2, 268, "promise = manual set_value; packaged_task wraps "
                  "a callable; shared_future = many getters", LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "std::async default policy may run deferred "
                  "(lazily on get) \u2014 pass launch::async to force a thread",
                  LIGHT, 11, 500))
    write("figures/future-async.svg", svg(W, H, "".join(b), "Futures"))


# -- 03 atomics & memory model -----------------------------------------------
@fig
def fig_atomic_cas():
    W, H = 780, 340
    b = [text(W / 2, 26, "Lock-free update with compare_exchange", GREY, 15,
              700)]
    b.append(box(300, 64, 200, 40, "old = a.load()", BLUE, size=11, rx=8))
    b.append(arrow(400, 104, 400, 132, GREY))
    b.append(box(280, 132, 240, 40, "new = f(old)", TEAL, size=11, rx=8))
    b.append(arrow(400, 172, 400, 200, GREY))
    b.append(box(240, 200, 320, 48, ["a.compare_exchange_weak(old, new)"],
                 AMBER, tcol=INK_DARK, size=12, rx=10))
    b.append(arrow(560, 224, 650, 224, TEAL))
    b.append(box(650, 204, 110, 40, "success", TEAL, size=11, rx=8))
    b.append(path("M240 224 C150 224 150 88 300 84", GREY, dash="5 4",
                  arrow_end=True))
    b.append(text(150, 150, "retry:", RED, 11, 700))
    b.append(text(180, 170, "value changed;", LIGHT, 9, 500))
    b.append(text(180, 184, "old auto-updated", LIGHT, 9, 500))
    b.append(text(W / 2, 288, "CAS is the atomic 'if the value is still old, set "
                  "it to new' \u2014 the heart of every lock-free algorithm",
                  LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "_weak may fail spuriously (cheap in a loop); "
                  "_strong does not (use when there is no loop)", LIGHT, 11,
                  500))
    write("figures/atomic-cas.svg", svg(W, H, "".join(b), "Atomic CAS"))


@fig
def fig_lockfree_stack():
    W, H = 780, 340
    b = [text(W / 2, 26, "Lock-free stack push (CAS on head)", GREY, 16, 700)]
    b.append(box(60, 100, 140, 44, ["new node", "next = head"], TEAL, size=10,
                 lh=14, rx=10))
    b.append(box(300, 80, 90, 40, "head", BLUE, size=11, rx=8))
    nodes = ["n2", "n1", "n0"]
    for i, n in enumerate(nodes):
        b.append(box(300 + i * 120, 150, 90, 40, n, GREY_D, size=11, rx=8))
        if i < 2:
            b.append(arrow(390 + i * 120, 170, 420 + i * 120, 170, GREY))
    b.append(arrow(345, 120, 345, 150, BLUE))
    b.append(arrow(200, 122, 300, 165, TEAL, dash="4 4"))
    b.append(text(W / 2, 235, "compare_exchange(head, new_node): if head is "
                  "unchanged, swing it to the new node", GREY, 11, 600))
    b.append(text(W / 2, 268, "if another thread pushed/popped first, retry with "
                  "the fresh head", LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "beware the ABA problem (Part 5.5): head may "
                  "read 'the same' after A\u2192B\u2192A \u2014 use tagged "
                  "pointers or hazard pointers", LIGHT, 11, 500))
    write("figures/lockfree-stack.svg", svg(W, H, "".join(b), "Lock-free stack"))


@fig
def fig_happens_before():
    W, H = 800, 340
    b = [text(W / 2, 26, "Release / acquire builds a happens-before edge", GREY,
              15, 700)]
    b.append(box(60, 70, 300, 200, "", "none", rx=12, stroke=BLUE, sw=1.4))
    b.append(text(210, 88, "thread A (producer)", BLUE, 11, 700))
    b.append(box(85, 108, 250, 34, "data = 42;", BLUE, size=10, rx=6,
                 mono=True))
    b.append(box(85, 150, 250, 40, "ready.store(true,", BLUE, size=10, rx=6,
                 mono=True))
    b.append(box(85, 192, 250, 34, "  release);", BLUE, size=10, rx=6,
                 mono=True))
    b.append(box(440, 70, 300, 200, "", "none", rx=12, stroke=TEAL, sw=1.4))
    b.append(text(590, 88, "thread B (consumer)", TEAL, 11, 700))
    b.append(box(465, 108, 250, 40, "while(!ready.load(", TEAL, size=10, rx=6,
                 mono=True))
    b.append(box(465, 150, 250, 34, "  acquire)) {}", TEAL, size=10, rx=6,
                 mono=True))
    b.append(box(465, 192, 250, 34, "use data; // sees 42", TEAL, size=10, rx=6,
                 mono=True))
    b.append(arrow(335, 172, 465, 128, AMBER))
    b.append(text(W / 2, 292, "the acquire-load that reads the release-store "
                  "SYNCHRONIZES-WITH it \u2192 everything before the store is "
                  "visible after the load", LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "without this edge, thread B could see ready=true "
                  "but data still stale (reordering)", LIGHT, 11, 500))
    write("figures/happens-before.svg", svg(W, H, "".join(b), "Happens-before"))


@fig
def fig_memory_orders():
    W, H = 800, 360
    b = [text(W / 2, 26, "The memory-order spectrum", GREY, 16, 700)]
    rows = [
        ("seq_cst", "single total order, all threads agree", "strongest / "
         "slowest (default)", RED),
        ("acq_rel", "release-store + acquire-load pair up", "RMW on a lock/flag",
         AMBER),
        ("acquire / release", "one-directional ordering barrier", "producer / "
         "consumer handoff", BLUE),
        ("relaxed", "atomicity only, NO ordering", "counters / stats",
         TEAL),
    ]
    for i, (name, how, when, col) in enumerate(rows):
        y = 66 + i * 66
        tc = INK_DARK if col == AMBER else WHITE
        b.append(box(50, y, 240, 50, name, col, tcol=tc, size=12, rx=10))
        b.append(text(310, y + 16, how, GREY, 11, 500, anchor="start"))
        b.append(text(310, y + 36, "use: " + when, LIGHT, 10, 500,
                      anchor="start"))
    b.append(text(W / 2, H - 16, "weaker = faster but harder to reason about; "
                  "default to seq_cst and only relax with a proof", LIGHT, 11,
                  500))
    write("figures/memory-orders.svg", svg(W, H, "".join(b), "Memory orders"))


@fig
def fig_store_load_reorder():
    W, H = 800, 340
    b = [text(W / 2, 26, "StoreLoad reordering: the surprising one", GREY, 15,
              700)]
    b.append(text(W / 2, 56, "initially  x = 0,  y = 0", GREY, 12, 600,
                  mono=True))
    b.append(box(80, 80, 300, 120, "", "none", rx=12, stroke=BLUE, sw=1.4))
    b.append(text(230, 98, "thread A", BLUE, 11, 700))
    b.append(box(105, 116, 250, 32, "x.store(1);", BLUE, size=10, rx=6,
                 mono=True))
    b.append(box(105, 154, 250, 32, "r1 = y.load();", BLUE, size=10, rx=6,
                 mono=True))
    b.append(box(420, 80, 300, 120, "", "none", rx=12, stroke=TEAL, sw=1.4))
    b.append(text(570, 98, "thread B", TEAL, 11, 700))
    b.append(box(445, 116, 250, 32, "y.store(1);", TEAL, size=10, rx=6,
                 mono=True))
    b.append(box(445, 154, 250, 32, "r2 = x.load();", TEAL, size=10, rx=6,
                 mono=True))
    b.append(text(W / 2, 232, "on real hardware r1 == 0 AND r2 == 0 is "
                  "possible \u2014 each core buffers its store past its load",
                  RED, 11, 600))
    b.append(text(W / 2, 262, "seq_cst (or a full fence) forbids this; relaxed "
                  "/ acq-rel do not", LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "the classic Dekker / Peterson lock breaks "
                  "without the right ordering", LIGHT, 11, 500))
    write("figures/store-load-reorder.svg", svg(W, H, "".join(b),
          "Store-load reorder"))


# -- 04 patterns -------------------------------------------------------------
@fig
def fig_thread_pool():
    W, H = 800, 340
    b = [text(W / 2, 26, "Thread pool: fixed workers drain a task queue", GREY,
              15, 700)]
    for i in range(3):
        b.append(box(60, 80 + i * 60, 130, 40, f"submitter {i}", GREY_D,
                     size=10, rx=8))
        b.append(arrow(190, 100 + i * 60, 300, 150, GREY))
    b.append(box(300, 120, 200, 60, ["task queue", "(mutex + condvar)"], AMBER,
                 tcol=INK_DARK, size=11, lh=16, rx=10))
    for i in range(3):
        b.append(box(610, 80 + i * 60, 130, 40, f"worker {i}", BLUE, size=10,
                     rx=8))
        b.append(arrow(500, 150, 610, 100 + i * 60, BLUE))
    b.append(text(W / 2, 240, "N workers (\u2248 hardware_concurrency) loop: "
                  "pop a task, run it, repeat \u2014 threads are reused, not "
                  "recreated", LIGHT, 11, 500))
    b.append(text(W / 2, H - 14, "amortizes thread-creation cost and bounds "
                  "concurrency; the backbone of most servers", LIGHT, 11, 500))
    write("figures/thread-pool.svg", svg(W, H, "".join(b), "Thread pool"))


@fig
def fig_pipeline():
    W, H = 820, 300
    b = [text(W / 2, 26, "Pipeline: each stage is a thread, connected by queues",
              GREY, 15, 700)]
    stages = [("stage 1", "parse", BLUE), ("stage 2", "transform", TEAL),
              ("stage 3", "write", BLUE_D)]
    for i, (name, note, col) in enumerate(stages):
        x = 70 + i * 250
        b.append(box(x, 110, 150, 56, [name, note], col, size=11, lh=16, rx=10))
        if i < 2:
            b.append(box(x + 150, 122, 100, 32, "queue", AMBER, tcol=INK_DARK,
                         size=9, rx=6))
            b.append(arrow(x + 250, 138, x + 320, 138, GREY))
    b.append(text(W / 2, 210, "stages run concurrently on different items \u2014 "
                  "like an assembly line; throughput = the slowest stage",
                  LIGHT, 11, 500))
    b.append(text(W / 2, H - 16, "bounded queues between stages give "
                  "back-pressure so a fast stage can't outrun a slow one", LIGHT,
                  11, 500))
    write("figures/pipeline.svg", svg(W, H, "".join(b), "Pipeline"))


@fig
def fig_work_stealing():
    W, H = 940, 360
    b = [text(W / 2, 26, "Work-stealing: idle threads steal tasks", GREY, 16,
              700)]
    for i, x in enumerate((40, 240, 440)):
        b.append(box(x, 66, 150, 30, f"worker {i}", BLUE, size=11, rx=8))
        for j in range(3):
            b.append(box(x + 20, 108 + j * 44, 110, 34, "task", TEAL, size=10,
                         rx=6))
    b.append(box(700, 66, 170, 30, "worker 3 (idle)", AMBER, tcol=INK_DARK,
                 size=10, rx=8))
    b.append(text(785, 128, "own deque empty", LIGHT, 9, 500))
    b.append(text(785, 148, "\u2192 steal!", AMBER, 11, 700))
    b.append(arrow(700, 138, 570, 122, AMBER))
    b.append(text(115, 258, "push/pop own end (LIFO)", LIGHT, 9, 500))
    b.append(text(W / 2, 300, "each worker owns a deque; thieves take from the "
                  "OTHER end to minimize contention", GREY, 12, 600))
    b.append(text(W / 2, H - 16, "the scheduling strategy behind TBB, Cilk, Go, "
                  "and Rust's rayon", LIGHT, 11, 500))
    write("figures/work-stealing.svg", svg(W, H, "".join(b), "Work stealing"))


@fig
def fig_dining_philosophers():
    W, H = 640, 460
    b = [text(W / 2, 26, "Dining philosophers: the deadlock classic", GREY, 15,
              700)]
    cx, cy, R = 320, 250, 150
    import math as _m
    forks = []
    for i in range(5):
        a = -_m.pi / 2 + i * 2 * _m.pi / 5
        px, py = cx + R * _m.cos(a), cy + R * _m.sin(a)
        b.append(circle(px, py, 34, BLUE))
        b.append(text(px, py, f"P{i}", WHITE, 13, 700))
        fa = a + _m.pi / 5
        fx, fy = cx + (R - 70) * _m.cos(fa), cy + (R - 70) * _m.sin(fa)
        forks.append((fx, fy))
        b.append(circle(fx, fy, 8, AMBER))
    b.append(text(cx, cy, "table", LIGHT, 12, 600))
    b.append(text(W / 2, H - 46, "each needs BOTH neighbouring forks; if all "
                  "grab left first \u2192 circular wait \u2192 deadlock", LIGHT,
                  11, 500))
    b.append(text(W / 2, H - 24, "fix: order the forks, or let one philosopher "
                  "pick right-first (break the cycle)", LIGHT, 11, 500))
    write("figures/dining-philosophers.svg", svg(W, H, "".join(b),
          "Dining philosophers"))


# -- 05 pitfalls -------------------------------------------------------------
@fig
def fig_deadlock():
    W, H = 760, 340
    b = [text(W / 2, 26, "Deadlock: circular wait on two locks", GREY, 16, 700)]
    b.append(box(80, 110, 150, 60, ["thread A", "holds L1", "wants L2"], BLUE,
                 size=10, lh=14, rx=10))
    b.append(box(530, 110, 150, 60, ["thread B", "holds L2", "wants L1"], TEAL,
                 size=10, lh=14, rx=10))
    b.append(box(340, 70, 90, 44, "lock L1", AMBER, tcol=INK_DARK, size=10,
                 rx=8))
    b.append(box(340, 170, 90, 44, "lock L2", AMBER, tcol=INK_DARK, size=10,
                 rx=8))
    b.append(arrow(230, 130, 340, 92, BLUE))
    b.append(arrow(385, 114, 385, 170, BLUE, dash="5 4"))
    b.append(arrow(530, 150, 430, 192, TEAL))
    b.append(arrow(385, 170, 385, 114, TEAL, dash="5 4"))
    b.append(text(W / 2, 258, "A waits for L2 (held by B); B waits for L1 (held "
                  "by A) \u2014 neither proceeds", RED, 11, 600))
    b.append(text(W / 2, H - 14, "fix: acquire locks in a GLOBAL order, or use "
                  "std::scoped_lock(L1, L2) which locks both atomically", LIGHT,
                  11, 500))
    write("figures/deadlock.svg", svg(W, H, "".join(b), "Deadlock"))


@fig
def fig_false_sharing():
    W, H = 800, 340
    b = [text(W / 2, 26, "False sharing: two cores, one cache line", GREY, 15,
              700)]
    b.append(box(60, 80, 150, 50, ["core 0", "writes a[0]"], BLUE, size=11,
                 lh=15, rx=10))
    b.append(box(590, 80, 150, 50, ["core 1", "writes a[1]"], TEAL, size=11,
                 lh=15, rx=10))
    b.append(box(300, 90, 200, 44, "cache line (64 B)", AMBER, tcol=INK_DARK,
                 size=11, rx=8))
    b.append(box(310, 100, 40, 24, "a[0]", GREY_D, size=9, rx=4))
    b.append(box(450, 100, 40, 24, "a[1]", GREY_D, size=9, rx=4))
    b.append(path("M210 105 C260 105 260 112 300 112", BLUE, arrow_end=True))
    b.append(path("M590 105 C540 105 540 112 500 112", TEAL, arrow_end=True))
    b.append(text(W / 2, 175, "a[0] and a[1] are independent, but share ONE "
                  "line \u2192 it ping-pongs between the two caches", RED, 11,
                  600))
    b.append(text(W / 2, 235, "fix: pad/align each hot datum to its own 64 B "
                  "line (alignas(64), per-thread accumulators)", LIGHT, 11,
                  500))
    b.append(text(W / 2, H - 16, "symptom: parallel code that scales WORSE than "
                  "serial", LIGHT, 11, 500))
    write("figures/false-sharing.svg", svg(W, H, "".join(b), "False sharing"))


@fig
def fig_aba_problem():
    W, H = 800, 320
    b = [text(W / 2, 26, "The ABA problem", GREY, 16, 700)]
    steps = [
        (70, "T1 reads head = A", BLUE),
        (250, "T2: pop A, pop B,", TEAL),
        (250, "    push A back", TEAL),
        (600, "T1 CAS(head, A\u2192X)", BLUE),
    ]
    b.append(box(60, 80, 200, 40, "T1 reads head = A", BLUE, size=10, rx=8))
    b.append(box(300, 70, 220, 40, "T2: pop A, pop B, free B", TEAL, size=10,
                 rx=8))
    b.append(box(300, 118, 220, 40, "T2: push A back (reused)", TEAL, size=10,
                 rx=8))
    b.append(box(560, 90, 200, 40, "T1 CAS succeeds!", RED, size=10, rx=8))
    b.append(arrow(260, 100, 300, 100, GREY))
    b.append(arrow(520, 120, 560, 110, GREY))
    b.append(text(W / 2, 200, "head is A again, so T1's compare_exchange thinks "
                  "nothing changed \u2014 but the list is now corrupt (B freed)",
                  RED, 11, 600))
    b.append(text(W / 2, 250, "the value matched, yet the world moved: "
                  "A \u2192 B \u2192 A", LIGHT, 11, 500))
    b.append(text(W / 2, H - 16, "fix: tagged pointers (value + counter), hazard "
                  "pointers, or RCU / epoch reclamation", LIGHT, 11, 500))
    write("figures/aba-problem.svg", svg(W, H, "".join(b), "ABA problem"))


@fig
def fig_lost_wakeup():
    W, H = 800, 340
    b = [text(W / 2, 26, "Lost wakeup: notify before wait", GREY, 16, 700)]
    b.append(box(60, 80, 300, 120, "", "none", rx=12, stroke=TEAL, sw=1.4))
    b.append(text(210, 98, "waiter (too late)", TEAL, 11, 700))
    b.append(box(85, 118, 250, 32, "if (!ready)", TEAL, size=10, rx=6,
                 mono=True))
    b.append(box(85, 156, 250, 32, "  cv.wait(m);  // sleeps forever", TEAL,
                 size=9, rx=6, mono=True))
    b.append(box(440, 80, 300, 120, "", "none", rx=12, stroke=BLUE, sw=1.4))
    b.append(text(590, 98, "signaler (fired first)", BLUE, 11, 700))
    b.append(box(465, 118, 250, 32, "ready = true;", BLUE, size=10, rx=6,
                 mono=True))
    b.append(box(465, 156, 250, 32, "cv.notify_one();  // no one waiting", BLUE,
                 size=8, rx=6, mono=True))
    b.append(text(W / 2, 240, "the notify happens between the check and the "
                  "wait \u2014 the signal is lost and the waiter sleeps forever",
                  RED, 11, 600))
    b.append(text(W / 2, H - 16, "fix: hold the mutex around the flag AND check "
                  "the predicate in a while-loop inside wait()", LIGHT, 11,
                  500))
    write("figures/lost-wakeup.svg", svg(W, H, "".join(b), "Lost wakeup"))


# -- 06 OpenMP ---------------------------------------------------------------
@fig
def fig_openmp_fork_join():
    W, H = 800, 340
    b = [text(W / 2, 26, "OpenMP fork-join model", GREY, 16, 700)]
    b.append(box(60, 150, 130, 40, "master thread", BLUE, size=10, rx=8))
    b.append(arrow(190, 170, 260, 170, GREY))
    b.append(text(400, 88, "#pragma omp parallel", TEAL, 11, 700, mono=True))
    for i in range(4):
        b.append(box(300, 110 + i * 38, 200, 30, f"thread {i}", TEAL, size=10,
                     rx=6))
    b.append(arrow(510, 170, 580, 170, GREY))
    b.append(box(580, 150, 160, 40, "join \u2192 master", BLUE, size=10, rx=8))
    b.append(text(W / 2, 280, "the runtime forks a team of threads at a "
                  "parallel region and joins them at the closing brace", LIGHT,
                  11, 500))
    b.append(text(W / 2, H - 16, "you annotate serial code with pragmas; the "
                  "compiler + runtime create and manage the threads", LIGHT, 11,
                  500))
    write("figures/openmp-fork-join.svg", svg(W, H, "".join(b),
          "OpenMP fork-join"))


@fig
def fig_openmp_reduction():
    W, H = 800, 340
    b = [text(W / 2, 26, "OpenMP reduction(+:sum)", GREY, 16, 700)]
    b.append(box(300, 66, 200, 34, "shared sum = 0", GREY_D, size=11, rx=8))
    for i in range(4):
        x = 60 + i * 185
        b.append(box(x, 120, 150, 44, [f"thread {i}", f"private sum{i}"], TEAL,
                     size=10, lh=14, rx=8))
        b.append(arrow(x + 75, 100, x + 75, 120, GREY))
        b.append(arrow(x + 75, 164, W / 2, 240, GREY, dash="4 4"))
    b.append(box(310, 240, 180, 40, "sum = \u03a3 private", BLUE, size=12,
                 rx=10))
    b.append(text(W / 2, H - 30, "each thread accumulates into a PRIVATE copy "
                  "(no contention); the runtime combines them at the end", LIGHT,
                  11, 500))
    b.append(text(W / 2, H - 12, "same idea as a parallel_reduce \u2014 avoids a "
                  "shared-write bottleneck and a race on sum", LIGHT, 11, 500))
    write("figures/openmp-reduction.svg", svg(W, H, "".join(b),
          "OpenMP reduction"))


def build_font_style(chars):
    if not os.path.exists(FONT_PATH):
        print("WARNING: Virgil.woff2 not found; figures fall back to a system "
              "handwriting font.")
        return ""
    from fontTools.subset import Options, Subsetter
    from fontTools.ttLib import TTFont
    text_ = "".join(sorted(chars))
    opts = Options()
    opts.flavor = "woff2"
    opts.desubroutinize = True
    opts.notdef_outline = True
    opts.recalc_bounds = True
    font = TTFont(FONT_PATH)
    ss = Subsetter(options=opts)
    ss.populate(text=text_)
    ss.subset(font)
    buf = io.BytesIO()
    font.save(buf)
    b64 = base64.b64encode(buf.getvalue()).decode("ascii")
    print(f"embedded font: {len(chars)} glyphs, {len(buf.getvalue())} bytes")
    return ('<style>@font-face{font-family:"Virgil";font-style:normal;'
            'font-weight:400 700;src:url("data:font/woff2;base64,'
            f'{b64}") format("woff2");}}</style>')


if __name__ == "__main__":
    for fn in ALL:
        fn()
    FONT_STYLE = build_font_style(USED_CHARS)
    for fn in ALL:
        fn()
    print(f"\nDone: {len(ALL)} figures generated.")
