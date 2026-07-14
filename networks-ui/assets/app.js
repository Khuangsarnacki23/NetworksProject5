import { WASI, WASIProcExit, File, OpenFile, PreopenDirectory, ConsoleStdout } from "/assets/shim/index.js";

/* ---------------- WASM runner ---------------- */

const wasmCache = {};
async function getModule(name) {
  if (!wasmCache[name]) {
    wasmCache[name] = await WebAssembly.compileStreaming(fetch(`/wasm/${name}.wasm`));
  }
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

// Run a WASI program with the given argv and virtual /data files. Returns {out, err, code}.
async function runWasm(name, argv, files) {
  const mod = await getModule(name);
  let out = "", err = "";
  const dataDir = new Map();
  for (const [fname, bytes] of Object.entries(files)) dataDir.set(fname, new File(bytes));
  const fds = [
    new OpenFile(new File([])),                                  // stdin
    ConsoleStdout.lineBuffered(l => { out += l + "\n"; }),       // stdout
    ConsoleStdout.lineBuffered(l => { err += l + "\n"; }),       // stderr
    new PreopenDirectory("/data", dataDir),
  ];
  const wasi = new WASI([name, ...argv], [], fds);
  const inst = await WebAssembly.instantiate(mod, { wasi_snapshot_preview1: wasi.wasiImport });
  let code = 0;
  try { code = wasi.start(inst); }
  catch (e) { if (e instanceof WASIProcExit) code = e.code; else throw e; }
  return { out, err, code };
}

/* ---------------- UI helpers ---------------- */

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

function fileSelect(id, label, options, state, key, onchange) {
  const sel = h("select", { id }, ...options.map(o => h("option", { value: o }, o)), h("option", { value: "__upload" }, "Upload a file…"));
  const fileInput = h("input", { type: "file" });
  sel.addEventListener("change", async () => {
    if (sel.value === "__upload") { fileInput.click(); return; }
    state[key] = { name: sel.value, bytes: null };
    onchange();
  });
  fileInput.addEventListener("change", async () => {
    const f = fileInput.files[0];
    if (!f) { sel.value = options[0]; state[key] = { name: options[0], bytes: null }; onchange(); return; }
    const bytes = new Uint8Array(await f.arrayBuffer());
    let opt = [...sel.options].find(o => o.value === f.name);
    if (!opt) { opt = h("option", { value: f.name }, `${f.name} (uploaded)`); sel.insertBefore(opt, sel.lastElementChild); }
    sampleCache[`upload/${f.name}`] = bytes;
    sel.value = f.name;
    state[key] = { name: f.name, bytes };
    onchange();
  });
  state[key] = { name: options[0], bytes: null };
  return h("div", { class: "field" }, h("label", { for: id }, label), sel, fileInput);
}

function modeButtons(modes, state, onchange) {
  const wrap = h("div", { class: "modes" });
  for (const m of modes) {
    const b = h("button", { onclick: () => { state.mode = m.flag; refresh(); onchange(); } }, `${m.flag}  ·  ${m.label}`);
    wrap.append(b);
  }
  function refresh() {
    [...wrap.children].forEach((b, i) => b.classList.toggle("sel", modes[i].flag === state.mode));
  }
  state.mode = modes[0].flag;
  refresh();
  return wrap;
}

function termWrite(term, res, cmd) {
  term.textContent = "";
  term.append(h("span", { class: "meta" }, `$ ${cmd}\n`));
  if (res.out) term.append(res.out);
  if (res.err) term.append(h("span", { class: "err" }, res.err));
  term.append(h("span", { class: "meta" }, `\n[exit code ${res.code}]`));
  term.scrollTop = term.scrollHeight;
}

/* ---------------- View builder for WASM projects ---------------- */

// spec: {id,title,binary,blurb,modes:[{flag,label}], files:[{key,label,flag,samples,project}], buildArgs(state)}
function wasmView(spec) {
  const state = {};
  const view = h("section", { class: "view", id: `view-${spec.id}` });
  const term = h("div", { class: "term" }, h("span", { class: "meta" }, "output will appear here"));
  const cmdEl = h("div", { class: "cmd" });

  const updateCmd = () => {
    cmdEl.textContent = `./${spec.binary} ${spec.buildArgs(state).display}`;
  };

  const fileFields = spec.files.map(f =>
    fileSelect(`${spec.id}-${f.key}`, f.label, f.samples, state, f.key, updateCmd));

  const runBtn = h("button", { class: "runbtn", onclick: run }, "Run");
  async function run() {
    runBtn.disabled = true;
    term.textContent = "running…";
    try {
      const { argv, files } = await spec.resolve(state);
      const res = await runWasm(spec.binary, argv, files);
      termWrite(term, res, `./${spec.binary} ${spec.buildArgs(state).display}`);
    } catch (e) {
      term.textContent = ""; term.append(h("span", { class: "err" }, `error: ${e.message}`));
    }
    runBtn.disabled = false;
  }

  view.append(
    h("h2", {}, spec.title, h("span", { class: "badge wasm" }, "WASM · runs in browser")),
    h("p", { class: "desc" }, spec.blurb),
    h("div", { class: "card" },
      h("h3", {}, "Mode"),
      modeButtons(spec.modes, state, updateCmd),
      h("div", { style: "height:14px" }),
      h("h3", {}, "Input files"),
      h("div", { class: "row" }, fileFields)),
    h("div", { class: "card" },
      h("h3", {}, "Command"),
      h("div", { class: "cmdbar" }, cmdEl, runBtn)),
    h("div", { class: "card" }, h("h3", {}, "Output"), term),
  );
  updateCmd();
  return view;
}

async function bytesFor(project, ref) {
  if (ref.bytes) return ref.bytes;
  if (sampleCache[`upload/${ref.name}`]) return sampleCache[`upload/${ref.name}`];
  return getSample(project, ref.name);
}

/* ---------------- Project specs ---------------- */

const proj1 = wasmView({
  id: "proj1", title: "Project 1 — IPv4 Address Lists", binary: "proj1",
  blurb: "Parses a file of IPv4 addresses. Print mode (-p) echoes every address; summary mode (-s) counts total and private (10.x.x.x) addresses.",
  modes: [{ flag: "-p", label: "print mode" }, { flag: "-s", label: "summary mode" }],
  files: [{ key: "list", label: "Address list (-r)", samples: ["sample-A.list","sample-B.list","sample-C.list","sample-D.list","sample-E.list","sample-F.list","sample-G.list"] }],
  buildArgs: s => ({ display: `${s.mode} -r ${s.list?.name ?? ""}` }),
  resolve: async s => ({
    argv: [s.mode, "-r", `/data/${s.list.name}`],
    files: { [s.list.name]: await bytesFor("proj1", s.list) },
  }),
});

const proj2 = wasmView({
  id: "proj2", title: "Project 2 — Packet Forwarding Simulator", binary: "proj2",
  blurb: "Router forwarding logic. Packet print mode (-p) dumps a trace; table mode (-r) prints a forwarding table; simulation mode (-s) forwards each traced packet using longest-prefix match against the table.",
  modes: [{ flag: "-p", label: "packet print (needs trace)" }, { flag: "-r", label: "print table (needs table)" }, { flag: "-s", label: "simulate (needs both)" }],
  files: [
    { key: "tbl", label: "Forwarding table (-f)", samples: ["ex10.tbl","ex11.tbl","ex6.tbl"] },
    { key: "trace", label: "Packet trace (-t)", samples: ["ex1.trace","ex10.trace","ex11.trace"] },
  ],
  buildArgs: s => {
    if (s.mode === "-p") return { display: `-p -t ${s.trace?.name ?? ""}` };
    if (s.mode === "-r") return { display: `-r -f ${s.tbl?.name ?? ""}` };
    return { display: `-s -f ${s.tbl?.name ?? ""} -t ${s.trace?.name ?? ""}` };
  },
  resolve: async s => {
    const files = {};
    const argv = [s.mode];
    if (s.mode !== "-p") { argv.push("-f", `/data/${s.tbl.name}`); files[s.tbl.name] = await bytesFor("proj2", s.tbl); }
    if (s.mode !== "-r") { argv.push("-t", `/data/${s.trace.name}`); files[s.trace.name] = await bytesFor("proj2", s.trace); }
    return { argv, files };
  },
});

const proj3 = wasmView({
  id: "proj3", title: "Project 3 — Trace Analysis (NetFlow / RTT)", binary: "proj3",
  blurb: "Analyzes binary packet traces (8-byte timestamp + Ethernet + IPv4 + TCP/UDP headers per record). Packet mode (-p) prints one line per packet; NetFlow mode (-n) aggregates flows; RTT mode (-r) estimates round-trip times from SEQ/ACK pairs. demo.trace is a synthetic sample generated in this repo — upload your own course traces for real data.",
  modes: [{ flag: "-p", label: "packet print" }, { flag: "-n", label: "NetFlow" }, { flag: "-r", label: "RTT" }],
  files: [{ key: "trace", label: "Binary trace (-f)", samples: ["demo.trace"] }],
  buildArgs: s => ({ display: `-f ${s.trace?.name ?? ""} ${s.mode}` }),
  resolve: async s => ({
    argv: ["-f", `/data/${s.trace.name}`, s.mode],
    files: { [s.trace.name]: await bytesFor("proj3", s.trace) },
  }),
});

/* ---------------- Project 5 — real TCP sockets ---------------- */

function proj5View() {
  const view = h("section", { class: "view", id: "view-proj5" });
  const term = h("div", { class: "term" }, h("span", { class: "meta" }, "output will appear here"));
  const cmdEl = h("div", { class: "cmd" });
  const state = { mode: "-t" };

  const host = h("input", { type: "text", placeholder: "e.g. my-socketsd.fly.dev", value: localStorage.getItem("p5host") || "" });
  const port = h("input", { type: "number", placeholder: "4642", value: localStorage.getItem("p5port") || "4642" });

  const FIELDS = {
    "-t": [["n", "Team name (-n)"]],
    "-b": [["n", "Team name (-n)"], ["u", "Player name (-u)"]],
    "-g": [["d", "Date (-d)"], ["o", "Time (-o)"], ["C", "Location (-C)"], ["H", "Home team (-H)"], ["A", "Away team (-A)"]],
    "-r": [["G", "Game ID (-G)"], ["n", "Team (-n)"], ["u", "Player (-u)"], ["P", "Points (-P)"], ["S", "Assists (-S)"], ["R", "Rebounds (-R)"], ["M", "Minutes (-M)"]],
    "-l": [["u", "Player (-u), or leave blank"], ["n", "Team (-n), or leave blank"], ["G", "Game ID (-G), or leave blank"]],
    "-j": [],
  };
  const MODES = [
    { flag: "-t", label: "add team" }, { flag: "-b", label: "add player" }, { flag: "-g", label: "create game" },
    { flag: "-r", label: "record stats" }, { flag: "-l", label: "list stats" }, { flag: "-j", label: "dump JSON" },
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
    if (!FIELDS[state.mode].length) fieldWrap.append(h("p", { class: "note" }, "No extra arguments — dumps all league data to league_dump.json on the server."));
  }

  function currentArgs() {
    const args = { host: host.value.trim(), port: port.value.trim(), mode: state.mode, fields: {} };
    for (const [flag, inp] of Object.entries(inputs)) {
      const v = inp.value.trim();
      if (v) args.fields[flag] = v;
    }
    return args;
  }

  function updateCmd() {
    const a = currentArgs();
    let c = `./sockets -h ${a.host || "<host>"} -p ${a.port || "<port>"} ${a.mode}`;
    for (const [f, v] of Object.entries(a.fields)) c += ` -${f} ${/\s/.test(v) ? `'${v}'` : v}`;
    cmdEl.textContent = c;
  }

  const modes = modeButtons(MODES, state, () => { renderFields(); updateCmd(); });
  const runBtn = h("button", { class: "runbtn", onclick: run }, "Run");

  async function run() {
    const a = currentArgs();
    if (!a.host || !a.port) { term.textContent = ""; term.append(h("span", { class: "err" }, "Set the socketsd server host and port first.")); return; }
    localStorage.setItem("p5host", a.host); localStorage.setItem("p5port", a.port);
    runBtn.disabled = true;
    term.textContent = "connecting over TCP…";
    try {
      const r = await fetch("/api/sockets", {
        method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(a),
      });
      const j = await r.json();
      if (j.error) { term.textContent = ""; term.append(h("span", { class: "err" }, `error: ${j.error}`)); }
      else termWrite(term, { out: j.stdout, err: j.stderr, code: j.exitCode }, j.command);
    } catch (e) {
      term.textContent = ""; term.append(h("span", { class: "err" }, `request failed: ${e.message}`));
    }
    runBtn.disabled = false;
  }

  host.addEventListener("input", updateCmd); port.addEventListener("input", updateCmd);

  view.append(
    h("h2", {}, "Project 5 — League Stats over TCP Sockets", h("span", { class: "badge tcp" }, "real TCP · via API")),
    h("p", { class: "desc" }, "The actual compiled C client binary runs in a serverless function and opens a real TCP connection to your socketsd server. Deploy socketsd with the Dockerfile in server/ (Fly.io, AWS, any box with a public port), then point this page at it."),
    h("div", { class: "card" },
      h("h3", {}, "socketsd server"),
      h("div", { class: "row" },
        h("div", { class: "field" }, h("label", {}, "Host (-h)"), host),
        h("div", { class: "field" }, h("label", {}, "Port (-p)"), port))),
    h("div", { class: "card" }, h("h3", {}, "Command"), modes, h("div", { style: "height:14px" }), fieldWrap,
      h("div", { class: "cmdbar" }, cmdEl, runBtn)),
    h("div", { class: "card" }, h("h3", {}, "Output"), term),
    h("p", { class: "note" }, "Tip: register teams before players, create a game before recording stats. list stats accepts exactly one of player / team / game ID."),
  );
  renderFields(); updateCmd();
  return view;
}

/* ---------------- Shell ---------------- */

const views = [
  { id: "proj1", label: "Project 1 · IPv4", el: proj1 },
  { id: "proj2", label: "Project 2 · Forwarding", el: proj2 },
  { id: "proj3", label: "Project 3 · NetFlow/RTT", el: proj3 },
  { id: "proj5", label: "Project 5 · Sockets", el: proj5View() },
];

const nav = $("#nav"), main = $("#main");
for (const v of views) {
  const b = h("button", { onclick: () => show(v.id) }, v.label);
  b.dataset.view = v.id;
  nav.append(b);
  main.append(v.el);
}
function show(id) {
  for (const v of views) v.el.classList.toggle("active", v.id === id);
  [...nav.children].forEach(b => b.classList.toggle("active", b.dataset.view === id));
  location.hash = id;
}
show(location.hash.slice(1) || "proj1");
