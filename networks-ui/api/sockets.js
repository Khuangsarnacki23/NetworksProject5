// Serverless function: executes the real compiled C `sockets` client
// (static musl x86_64 binary in api/_bin/) which opens a genuine TCP
// connection to the socketsd server. The server location is fixed
// here on the backend — the frontend only chooses the command.
const { execFile } = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");

const SERVER_HOST = process.env.SOCKETSD_HOST || "network-5-kaizilla.fly.dev";
const SERVER_PORT = process.env.SOCKETSD_PORT || "4642";

const MODES = new Set(["-t", "-b", "-g", "-r", "-l", "-j", "-a", "-e", "-i"]);
const FIELD_FLAGS = new Set(["n", "u", "d", "o", "C", "H", "A", "G", "P", "S", "R", "M"]);

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
  const { mode, fields = {} } = req.body || {};

  if (!MODES.has(mode)) return res.status(400).json({ error: "invalid mode" });

  const argv = ["-h", SERVER_HOST, "-p", SERVER_PORT, mode];
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
