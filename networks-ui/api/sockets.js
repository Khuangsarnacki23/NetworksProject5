// Vercel serverless function: executes the real compiled C `sockets` client
// (static musl x86_64 binary in api/_bin/) which opens a genuine TCP
// connection to the user's socketsd server.
const { execFile } = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");

const MODES = new Set(["-t", "-b", "-g", "-r", "-l", "-j"]);
const FIELD_FLAGS = new Set(["n", "u", "d", "o", "C", "H", "A", "G", "P", "S", "R", "M"]);
const HOST_RE = /^[a-zA-Z0-9.-]{1,253}$/;

let binPath = null;
function ensureBinary() {
  if (binPath && fs.existsSync(binPath)) return binPath;
  const src = path.join(__dirname, "_bin", "sockets");
  binPath = "/tmp/sockets";
  fs.copyFileSync(src, binPath);
  fs.chmodSync(binPath, 0o755);
  return binPath;
}

module.exports = (req, res) => {
  if (req.method !== "POST") return res.status(405).json({ error: "POST only" });
  const { host, port, mode, fields = {} } = req.body || {};

  if (typeof host !== "string" || !HOST_RE.test(host)) return res.status(400).json({ error: "invalid host" });
  const portNum = Number(port);
  if (!Number.isInteger(portNum) || portNum < 1 || portNum > 65535) return res.status(400).json({ error: "invalid port" });
  if (!MODES.has(mode)) return res.status(400).json({ error: "invalid mode" });

  const argv = ["-h", host, "-p", String(portNum), mode];
  for (const [flag, value] of Object.entries(fields)) {
    if (!FIELD_FLAGS.has(flag)) return res.status(400).json({ error: `invalid flag -${flag}` });
    if (typeof value !== "string" || value.length > 200) return res.status(400).json({ error: `invalid value for -${flag}` });
    argv.push(`-${flag}`, value);
  }

  let bin;
  try { bin = ensureBinary(); }
  catch (e) { return res.status(500).json({ error: `binary unavailable: ${e.message}` }); }

  execFile(bin, argv, { timeout: 10000, cwd: "/tmp", maxBuffer: 1024 * 1024 }, (err, stdout, stderr) => {
    const display = argv.map(a => (/\s/.test(a) ? `'${a}'` : a)).join(" ");
    res.status(200).json({
      command: `./sockets ${display}`,
      stdout: stdout || "",
      stderr: stderr || "",
      exitCode: err ? (err.code === undefined || typeof err.code === "string" ? 1 : err.code) : 0,
      timedOut: !!(err && err.killed),
    });
  });
};
