import { WASI, WASIProcExit, File, OpenFile, PreopenDirectory, ConsoleStdout } from "/assets/shim/index.js";

/* ================= WASM runner ================= */

const wasmCache = {};
async function getModule(name) {
  if (!wasmCache[name]) wasmCache[name] = await WebAssembly.compileStreaming(fetch(`/wasm/${name}.wasm`));
  return wasmCache[name];
}

const sampleCache = {};
async function getSample(project, name) {
  const key = `${project}/${name}`;
  if (!sampleCache[key]) {
    const r = await fetch(`/samples/${project}/${name}`);
    if (!r.ok) throw new Error(`could not load sample ${name}`);
    sampleCache[key] = new Uint8Array(await r.arrayBuffer());
  }
  return sampleCache[key];
}

async function runWasm(name, argv, files) {
  const mod = await getModule(name);
  let out = "", err = "";
  const dataDir = new Map();
  for (const [fname, bytes] of Object.entries(files)) dataDir.set(fname, new File(bytes));
  const fds = [
    new OpenFile(new File([])),
    ConsoleStdout.lineBuffered(l => { out += l + "\n"; }),
    ConsoleStdout.lineBuffered(l => { err += l + "\n"; }),
    new PreopenDirectory("/data", dataDir),
  ];
  const wasi = new WASI([name, ...argv], [], fds);
  const inst = await WebAssembly.instantiate(mod, { wasi_snapshot_preview1: wasi.wasiImport });
  let code = 0;
  try { code = wasi.start(inst); }
  catch (e) { if (e instanceof WASIProcExit) code = e.code; else throw e; }
  return { out, err, code };
}

async function bytesFor(project, ref) {
  if (ref.bytes) return ref.bytes;
  if (sampleCache[`upload/${ref.name}`]) return sampleCache[`upload/${ref.name}`];
  return getSample(project, ref.name);
}

/* ================= sample descriptions ================= */
/* The input files are binary formats, so each gets a plain-English blurb
   (stats produced by actually running the tools on them). */

const SAMPLE_INFO = {
  "sample-A.list": "Tiny warm-up — exactly 1 IP address, nothing private.",
  "sample-B.list": "One single address, and it's a private 10.x.x.x one.",
  "sample-C.list": "10 addresses, 1 of them private. Small enough to read everything.",
  "sample-D.list": "100 addresses, 5 private. A nice mid-size list.",
  "sample-E.list": "10,000 addresses — 9,483 are private.",
  "sample-F.list": "The big one: 100,000 addresses (40,158 private).",
  "sample-G.list": "1,000 addresses, 473 private. Balanced mix.",
  "ex6.tbl":  "A one-route forwarding table: 1.0.0.0/8 → interface 10.",
  "ex10.tbl": "Just a default route: 0.0.0.0/8 → interface 1. Everything matches it.",
  "ex11.tbl": "7 routes with different prefixes — the fun one for longest-prefix matching.",
  "ex1.trace":  "A single packet (245.242.77.29 → 6.0.187.43).",
  "ex10.trace": "20 packets from one conversation — pair with ex10.tbl.",
  "ex11.trace": "11 packets across several destinations — pair with ex11.tbl.",
  "demo.trace": "Synthetic capture: 60 TCP/UDP packets between 5 hosts, 49 flows. Upload your own captures for real data.",
};

function describeFile(name) {
  return SAMPLE_INFO[name] || "Your uploaded file.";
}

/* ================= tiny DOM helpers ================= */

const $ = (s, el = document) => el.querySelector(s);
function h(tag, attrs = {}, ...kids) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === "class") el.className = v;
    else if (k.startsWith("on")) el.addEventListener(k.slice(2), v);
    else el.setAttribute(k, v);
  }
  for (const k of kids.flat()) el.append(k);
  return el;
}

function modeButtons(modes, state, onchange) {
  const wrap = h("div", { class: "modes" });
  for (const m of modes) {
    wrap.append(h("button", { onclick: () => { state.mode = m.flag; refresh(); onchange(); } }, `${m.label}  (${m.flag})`));
  }
  function refresh() {
    [...wrap.children].forEach((b, i) => b.classList.toggle("sel", modes[i].flag === state.mode));
  }
  state.mode = modes[0].flag;
  refresh();
  return wrap;
}

function setTerm(term, content, opts = {}) {
  term.textContent = "";
  if (opts.cmd !== undefined) opts.cmd.textContent = opts.cmdText ?? "";
  for (const part of content) {
    if (typeof part === "string") term.append(part);
    else term.append(h("span", { class: part.class }, part.text));
  }
  term.scrollTop = 0;
}

function crumb(title) {
  return h("div", { class: "crumb" }, h("a", { onclick: () => show("home") }, "← Home"), ` / ${title}`);
}

/* ================= tool pages (WASM) ================= */

const PREVIEW_LINES = 18;

function wasmView(spec) {
  const state = {};
  const view = h("section", { class: "view", id: `view-${spec.id}` });

  // input preview panel
  const inCmd = h("div", { class: "cmdline" });
  const inTerm = h("div", { class: "term" });
  // output panel
  const outCmd = h("div", { class: "cmdline" });
  const outTerm = h("div", { class: "term" }, h("span", { class: "meta" }, "Hit Run to execute the command above on the selected input."));

  const updateOutCmd = () => { outCmd.textContent = `./${spec.binary} ${spec.buildArgs(state).display}`; };

  /* Live input preview: whenever a file is picked, decode it with the tool's
     own print mode and show the contents immediately. */
  async function previewFile() {
    const f = spec.previewOf(state);
    if (!f) return;
    const { ref, args, argsDisplay } = f;
    inCmd.textContent = argsDisplay;
    setTerm(inTerm, [{ class: "meta", text: `decoding ${ref.name}…` }]);
    try {
      const bytes = await bytesFor(spec.project, ref);
      const res = await runWasm(spec.binary, args, { [ref.name]: bytes });
      const lines = res.out.split("\n").filter((l, i, a) => !(l === "" && i === a.length - 1));
      const shown = lines.slice(0, PREVIEW_LINES).join("\n");
      const content = [
        { class: "meta", text: `${describeFile(ref.name)}\n\n` },
        shown || "(no printable records)",
      ];
      if (lines.length > PREVIEW_LINES)
        content.push({ class: "meta", text: `\n… ${lines.length - PREVIEW_LINES} more lines (run to see everything)` });
      setTerm(inTerm, content);
    } catch (e) {
      setTerm(inTerm, [{ class: "err", text: `could not decode: ${e.message}` }]);
    }
  }

  function fileSelect(id, label, options, key) {
    const desc = h("div", { class: "filedesc" }, describeFile(options[0]));
    const sel = h("select", { id },
      ...options.map(o => h("option", { value: o }, o)),
      h("option", { value: "__upload" }, "Upload my own file…"));
    const fileInput = h("input", { type: "file" });

    const sync = () => { desc.textContent = describeFile(state[key].name); updateOutCmd(); previewFile(); };

    sel.addEventListener("change", () => {
      if (sel.value === "__upload") { fileInput.click(); return; }
      state[key] = { name: sel.value, bytes: null };
      sync();
    });
    fileInput.addEventListener("change", async () => {
      const f = fileInput.files[0];
      if (!f) { sel.value = state[key].name; return; }
      const bytes = new Uint8Array(await f.arrayBuffer());
      let opt = [...sel.options].find(o => o.value === f.name);
      if (!opt) { opt = h("option", { value: f.name }, `${f.name} (uploaded)`); sel.insertBefore(opt, sel.lastElementChild); }
      sampleCache[`upload/${f.name}`] = bytes;
      sel.value = f.name;
      state[key] = { name: f.name, bytes };
      sync();
    });
    state[key] = { name: options[0], bytes: null };
    return h("div", { class: "field" }, h("label", { for: id }, label), sel, desc, fileInput);
  }

  const fileFields = spec.files.map(f => fileSelect(`${spec.id}-${f.key}`, f.label, f.samples, f.key));

  const runBtn = h("button", { class: "runbtn", onclick: run }, "Run ▸");
  async function run() {
    runBtn.disabled = true;
    setTerm(outTerm, [{ class: "meta", text: "running…" }]);
    try {
      const { argv, files } = await spec.resolve(state);
      const res = await runWasm(spec.binary, argv, files);
      const content = [];
      if (res.out) content.push(res.out);
      if (res.err) content.push({ class: "err", text: res.err });
      content.push({ class: "meta", text: `\n[exit code ${res.code}]` });
      setTerm(outTerm, content);
    } catch (e) {
      setTerm(outTerm, [{ class: "err", text: `error: ${e.message}` }]);
    }
    runBtn.disabled = false;
  }

  view.append(
    crumb(spec.navLabel),
    h("div", { class: "pagehead" },
      h("h2", {}, spec.title, h("span", { class: "badge wasm" }, "runs right in your browser")),
      h("p", { class: "tagline" }, spec.blurb)),
    h("div", { class: "card" },
      h("h3", {}, "Mode"),
      modeButtons(spec.modes, state, () => { updateOutCmd(); })),
    h("div", { class: "card" },
      h("h3", {}, "Input " + (spec.files.length > 1 ? "files" : "file")),
      h("div", { class: "row" }, fileFields),
      h("p", { class: "note" }, "These are binary formats — a text editor shows gibberish. The left panel below decodes whatever you select, live, using the tool's own print mode.")),
    h("div", { class: "duo" },
      h("div", { class: "panel" },
        h("h3", {}, "Inside the selected file"),
        inCmd, inTerm),
      h("div", { class: "panel" },
        h("h3", {}, "Output ", runBtn),
        outCmd, outTerm)),
  );
  // style tweak: run button inline with the output header
  runBtn.style.cssText = "float:right;margin-top:-8px;padding:6px 18px;font-size:13px";

  updateOutCmd();
  previewFile();
  return view;
}

/* ================= tool specs ================= */

const addressAuditor = wasmView({
  id: "addresses", project: "proj1", binary: "proj1",
  navLabel: "Address Auditor", cardName: "IPv4 Address Auditor",
  title: "IPv4 Address Auditor",
  blurb: "Reads binary files packed with IPv4 addresses and makes them human-readable: print every address, or summarize the list and count how many live in private 10.x.x.x space.",
  modes: [{ flag: "-s", label: "Summarize" }, { flag: "-p", label: "Print every address" }],
  files: [{
    key: "list", label: "Address list (-r)",
    samples: ["sample-C.list","sample-A.list","sample-B.list","sample-D.list","sample-G.list","sample-E.list","sample-F.list"],
  }],
  previewOf: s => s.list && {
    ref: s.list,
    args: ["-p", "-r", `/data/${s.list.name}`],
    argsDisplay: `./proj1 -p -r ${s.list.name}`,
  },
  buildArgs: s => ({ display: `${s.mode} -r ${s.list?.name ?? ""}` }),
  resolve: async s => ({
    argv: [s.mode, "-r", `/data/${s.list.name}`],
    files: { [s.list.name]: await bytesFor("proj1", s.list) },
  }),
});

const forwardingEngine = wasmView({
  id: "forwarding", project: "proj2", binary: "proj2",
  navLabel: "Forwarding Engine", cardName: "Packet Forwarding Engine",
  title: "Packet Forwarding Engine",
  blurb: "A router's brain in software. Feed it a forwarding table and a packet trace, and it decides — packet by packet, using longest-prefix match — which interface each one leaves on.",
  modes: [
    { flag: "-s", label: "Simulate forwarding" },
    { flag: "-p", label: "Print the packets" },
    { flag: "-r", label: "Print the table" },
  ],
  files: [
    { key: "tbl", label: "Forwarding table (-f)", samples: ["ex11.tbl","ex10.tbl","ex6.tbl"] },
    { key: "trace", label: "Packet trace (-t)", samples: ["ex11.trace","ex10.trace","ex1.trace"] },
  ],
  previewOf: s => {
    // preview follows whatever the current mode consumes: table for -r, trace otherwise
    if (s.mode === "-r" && s.tbl) return { ref: s.tbl, args: ["-r", "-f", `/data/${s.tbl.name}`], argsDisplay: `./proj2 -r -f ${s.tbl.name}` };
    if (s.trace) return { ref: s.trace, args: ["-p", "-t", `/data/${s.trace.name}`], argsDisplay: `./proj2 -p -t ${s.trace.name}` };
    return null;
  },
  buildArgs: s => {
    if (s.mode === "-p") return { display: `-p -t ${s.trace?.name ?? ""}` };
    if (s.mode === "-r") return { display: `-r -f ${s.tbl?.name ?? ""}` };
    return { display: `-s -f ${s.tbl?.name ?? ""} -t ${s.trace?.name ?? ""}` };
  },
  resolve: async s => {
    const files = {}; const argv = [s.mode];
    if (s.mode !== "-p") { argv.push("-f", `/data/${s.tbl.name}`); files[s.tbl.name] = await bytesFor("proj2", s.tbl); }
    if (s.mode !== "-r") { argv.push("-t", `/data/${s.trace.name}`); files[s.trace.name] = await bytesFor("proj2", s.trace); }
    return { argv, files };
  },
});

const trafficAnalyzer = wasmView({
  id: "traffic", project: "proj3", binary: "proj3",
  navLabel: "Traffic Analyzer", cardName: "Traffic Analyzer",
  title: "Traffic Analyzer — NetFlow & RTT",
  blurb: "Turns raw binary packet captures (timestamp + Ethernet + IP + TCP/UDP headers) into insight: per-packet breakdowns, NetFlow-style flow aggregation, and round-trip-time estimates matched from TCP SEQ/ACK pairs.",
  modes: [
    { flag: "-p", label: "Per-packet breakdown" },
    { flag: "-n", label: "NetFlow flows" },
    { flag: "-r", label: "Round-trip times" },
  ],
  files: [{ key: "trace", label: "Binary capture (-f)", samples: ["demo.trace"] }],
  previewOf: s => s.trace && {
    ref: s.trace,
    args: ["-f", `/data/${s.trace.name}`, "-p"],
    argsDisplay: `./proj3 -f ${s.trace.name} -p`,
  },
  buildArgs: s => ({ display: `-f ${s.trace?.name ?? ""} ${s.mode}` }),
  resolve: async s => ({
    argv: ["-f", `/data/${s.trace.name}`, s.mode],
    files: { [s.trace.name]: await bytesFor("proj3", s.trace) },
  }),
});

/* ================= League Manager (real TCP) ================= */

function leagueView() {
  const view = h("section", { class: "view", id: "view-league" });
  const state = { mode: "-t" };

  const cmdEl = h("div", { class: "cmdline" });
  const term = h("div", { class: "term" }, h("span", { class: "meta" }, "Build a command and send it — the reply comes back over a live TCP connection."));

  const FIELDS = {
    "-t": [["n", "Team name (-n)"]],
    "-b": [["n", "Team name (-n)"], ["u", "Player name (-u)"]],
    "-g": [["d", "Date (-d)"], ["o", "Time (-o)"], ["C", "Location (-C)"], ["H", "Home team (-H)"], ["A", "Away team (-A)"]],
    "-r": [["G", "Game ID (-G)"], ["n", "Team (-n)"], ["u", "Player (-u)"], ["P", "Points (-P)"], ["S", "Assists (-S)"], ["R", "Rebounds (-R)"], ["M", "Minutes (-M)"]],
    "-l": [["u", "Player (-u), or leave blank"], ["n", "Team (-n), or leave blank"], ["G", "Game ID (-G), or leave blank"]],
    "-j": [],
  };
  const MODES = [
    { flag: "-t", label: "Add a team" }, { flag: "-b", label: "Add a player" }, { flag: "-g", label: "Create a game" },
    { flag: "-r", label: "Record stats" }, { flag: "-l", label: "Look up stats" }, { flag: "-j", label: "Dump JSON" },
  ];

  const fieldWrap = h("div", { class: "row" });
  const inputs = {};

  function renderFields() {
    fieldWrap.textContent = "";
    for (const k of Object.keys(inputs)) delete inputs[k];
    for (const [flag, label] of FIELDS[state.mode]) {
      const inp = h("input", { type: "text", oninput: updateCmd });
      inputs[flag] = inp;
      fieldWrap.append(h("div", { class: "field" }, h("label", {}, label), inp));
    }
    if (!FIELDS[state.mode].length) fieldWrap.append(h("p", { class: "note" }, "No extra arguments needed — the server writes everything it knows to league_dump.json."));
  }

  function currentArgs() {
    const args = { mode: state.mode, fields: {} };
    for (const [flag, inp] of Object.entries(inputs)) {
      const v = inp.value.trim();
      if (v) args.fields[flag] = v;
    }
    return args;
  }

  function updateCmd() {
    const a = currentArgs();
    let c = `./sockets -h <league-server> -p 4642 ${a.mode}`;
    for (const [f, v] of Object.entries(a.fields)) c += ` -${f} ${/\s/.test(v) ? `'${v}'` : v}`;
    cmdEl.textContent = c;
  }

  const modes = modeButtons(MODES, state, () => { renderFields(); updateCmd(); });
  const runBtn = h("button", { class: "runbtn", onclick: run }, "Send ▸");

  async function run() {
    const a = currentArgs();
    runBtn.disabled = true;
    setTerm(term, [{ class: "meta", text: "opening TCP connection…" }]);
    try {
      const r = await fetch("/api/sockets", {
        method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(a),
      });
      const j = await r.json();
      if (j.error) setTerm(term, [{ class: "err", text: `error: ${j.error}` }]);
      else {
        cmdEl.textContent = j.command; // the exact command that ran, server hostname included
        const content = [];
        if (j.stdout) content.push(j.stdout);
        if (j.stderr) content.push({ class: "err", text: j.stderr });
        content.push({ class: "meta", text: `\n[exit code ${j.exitCode}]` });
        setTerm(term, content);
      }
    } catch (e) {
      setTerm(term, [{ class: "err", text: `request failed: ${e.message}` }]);
    }
    runBtn.disabled = false;
  }

  view.append(
    crumb("League Manager"),
    h("div", { class: "pagehead" },
      h("h2", {}, "League Stats Manager", h("span", { class: "badge tcp" }, "live TCP server")),
      h("p", { class: "tagline" }, "A client/server pair speaking a custom text protocol over raw TCP. The compiled C client runs on the backend and talks to a live league server — register teams and players, schedule games, record box scores, query stats. Every button press below is a real socket connection.")),
    h("div", { class: "card" },
      h("h3", {}, "Build a command"),
      modes, h("div", { style: "height:14px" }), fieldWrap),
    h("div", { class: "panel" },
      h("h3", {}, "What actually runs ", runBtn),
      cmdEl, term),
    h("p", { class: "note" }, "Order matters, like in real life: register a team before its players, create a game before recording stats. Look-up accepts exactly one of player / team / game ID. The server sleeps when idle and wakes on connection — the first command after a quiet spell takes a couple of extra seconds."),
  );
  runBtn.style.cssText = "float:right;margin-top:-8px;padding:6px 18px;font-size:13px";
  renderFields(); updateCmd();
  return view;
}

/* ================= Homepage ================= */

const TOOLS = [
  { id: "addresses", tag: "Tool 01", name: "IPv4 Address Auditor", desc: "Decode binary lists of IP addresses — print them all, or count totals and private 10.x space.", el: addressAuditor },
  { id: "forwarding", tag: "Tool 02", name: "Packet Forwarding Engine", desc: "A router in software: longest-prefix matching packets from a trace against a forwarding table.", el: forwardingEngine },
  { id: "traffic", tag: "Tool 03", name: "Traffic Analyzer", desc: "Raw packet captures decoded into per-packet views, aggregated NetFlow flows, and RTT estimates.", el: trafficAnalyzer },
  { id: "league", tag: "Tool 04", name: "League Stats Manager", desc: "A custom protocol over real TCP sockets — manage a sports league from the wire up.", el: leagueView() },
];

function homeView() {
  const view = h("section", { class: "view", id: "view-home" });
  view.append(
    h("div", { class: "hero" },
      h("h1", {}, "Welcome to the ", h("em", {}, "Router Lab")),
      h("p", {}, "Four networking tools I wrote in C, running live on this page. Pick one below, choose an input (every sample is decoded and previewed for you), and watch the exact terminal command run behind the scenes.")),
    h("div", { class: "projgrid" },
      ...TOOLS.map(t =>
        h("button", { class: "projcard", onclick: () => show(t.id) },
          h("div", { class: "num" }, t.tag),
          h("h3", {}, t.name),
          h("p", {}, t.desc),
          h("div", { class: "go" }, "Open tool →")))),
    h("div", { class: "featrow" },
      h("div", { class: "feat" }, h("h4", {}, "The real code, untouched"),
        h("p", {}, "Nothing was rewritten for the web. Three tools are the original C compiled to WebAssembly running in your browser; the fourth runs the original client binary against a live TCP server.")),
      h("div", { class: "feat" }, h("h4", {}, "See inside every file"),
        h("p", {}, "Inputs are binary formats, so each page decodes your selected file live and shows its contents next to the output — no mystery blobs.")),
      h("div", { class: "feat" }, h("h4", {}, "Commands, not magic"),
        h("p", {}, "Every panel shows the exact command being executed behind the scenes, flags and all — the UI doubles as a cheat sheet for the CLI.")),
      h("div", { class: "feat" }, h("h4", {}, "Bring your own data"),
        h("p", {}, "Every file picker accepts uploads — run your own address lists, tables, and captures through the same code."))),
  );
  return view;
}

/* ================= shell / routing ================= */

const nav = $("#nav"), main = $("#main");
const views = [
  { id: "home", label: "Home", el: homeView() },
  ...TOOLS.map(t => ({ id: t.id, label: t.name.replace("IPv4 Address Auditor", "Address Auditor").replace("Packet Forwarding Engine", "Forwarding Engine").replace("League Stats Manager", "League Manager"), el: t.el })),
];

for (const v of views) {
  const a = h("a", { onclick: () => show(v.id) }, v.label);
  a.dataset.view = v.id;
  nav.append(a);
  main.append(v.el);
}
document.querySelector(".logo").addEventListener("click", () => show("home"));

function show(id) {
  for (const v of views) v.el.classList.toggle("active", v.id === id);
  [...nav.children].forEach(a => a.classList.toggle("active", a.dataset.view === id));
  location.hash = id === "home" ? "" : id;
  window.scrollTo(0, 0);
}
show(location.hash.slice(1) || "home");
