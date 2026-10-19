// A reference run in Node's real WASI engine. Kept separate from UWVM results.
const fs = require("node:fs");
const path = require("node:path");
const { spawnSync } = require("node:child_process");
const { createHash } = require("node:crypto");

if (process.argv[2] === "--run-wasm") {
  const { WASI } = require("node:wasi");
  const file = process.argv[3];
  const module = new WebAssembly.Module(fs.readFileSync(file));
  const wasi = new WASI({ version: "preview1", args: [file], env: {}, preopens: {} });
  wasi.start(new WebAssembly.Instance(module, wasi.getImportObject()));
} else {
  const [file, roundsText, compiler, jsSource] = process.argv.slice(2);
  const rounds = Number(roundsText);
  if (!Number.isSafeInteger(rounds) || rounds < 1) throw new Error("invalid rounds");
  const started = process.hrtime.bigint();
  const run = spawnSync(process.execPath, ["--no-warnings", __filename, "--run-wasm", file],
    { encoding: "utf8", timeout: 120000, maxBuffer: 1024 * 1024 });
  if (run.error || run.status !== 0) throw new Error(JSON.stringify(run));
  if (jsSource) {
    const source = spawnSync(process.execPath, [jsSource],
      { encoding: "utf8", timeout: 120000, maxBuffer: 1024 * 1024 });
    if (source.error || source.status !== 0 || source.stdout !== run.stdout)
      throw new Error("native JavaScript and Javy Wasm outputs differ: " + JSON.stringify(source));
  }
  const base = file.replace(/\.wasm$/, "");
  const receipt = { engine: "Node WASI reference", node_version: process.version,
    compiler, rounds, module: path.basename(file), exit: run.status,
    stdout: run.stdout, stderr: run.stderr,
    seconds: Number(process.hrtime.bigint() - started) / 1e9,
    wasm_sha256: createHash("sha256").update(fs.readFileSync(file)).digest("hex"),
    source_javascript_crosscheck: Boolean(jsSource) };
  fs.writeFileSync(base + ".expected.stdout", run.stdout);
  fs.writeFileSync(base + ".oracle.json", JSON.stringify(receipt, null, 2) + "\n");
  console.log(JSON.stringify(receipt));
}
