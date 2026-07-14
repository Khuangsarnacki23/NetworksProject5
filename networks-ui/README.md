# Networks Projects — Router Lab UI

One UI for all four CSDS 325 C projects. No code was changed — the originals are compiled as-is.

| View | Program | How it runs |
|---|---|---|
| Project 1 | `proj1` (IPv4 lists, `-p`/`-s`) | WebAssembly, in your browser |
| Project 2 | `proj2` (forwarding sim, `-p`/`-r`/`-s`) | WebAssembly, in your browser |
| Project 3 | `proj3` (NetFlow/RTT, `-p`/`-n`/`-r`) | WebAssembly, in your browser |
| Project 5 | `sockets` → `socketsd` | Real static C binary in a Vercel function, real TCP to your server |

Each view lets you pick bundled sample files (or upload your own), shows the exact CLI command, and runs it. Project 3's `demo.trace` is synthetic (the course samples lived on the class server) — upload real traces for real data.

## Deploy the UI to Vercel (~2 min)

```sh
cd networks-ui
npx vercel          # first time: link/create project
npx vercel --prod
```

Or push this folder to a GitHub repo and import it at vercel.com/new. No build step, no framework — leave all settings default.

Local preview with the API working: `npx vercel dev`

## Deploy socketsd (needed only for the Project 5 view)

The server needs a public raw-TCP port, which Vercel can't host. Two easy options:

**Fly.io (free tier, ~3 min):**
```sh
cd server
fly launch --no-deploy --copy-config --name my-socketsd
fly deploy
```
Then in the UI use host `my-socketsd.fly.dev`, port `4642`.

**AWS EC2 (or any Linux box):**
```sh
gcc -O2 -o socketsd socketsd.c
./socketsd -p 4642
```
Open inbound TCP 4642 in the security group; use the instance's public DNS in the UI.

## How Project 5 stays "real sockets"

`api/_bin/sockets` is your unmodified `sockets.c` compiled as a fully static
x86_64 musl binary (`zig cc -target x86_64-linux-musl -static`). The
`/api/sockets` function validates the form input, then `execFile`s that binary,
which does the actual `gethostbyname` → `socket` → `connect` → protocol
exchange with `socketsd` over TCP. Output is streamed back to the page.

## Layout

```
index.html, assets/      frontend (vanilla JS + vendored browser_wasi_shim)
wasm/                    proj1/2/3 compiled with zig cc -target wasm32-wasi
samples/                 bundled input files per project
api/sockets.js           serverless wrapper around the client binary
api/_bin/sockets         static x86_64 client binary (runs on Vercel)
server/                  Dockerfile + fly.toml to host socketsd
```
