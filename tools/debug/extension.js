"use strict";

const path = require("path");
const vscode = require("vscode");

function activate(context) {
  context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory(
    "uwvm-llvm-full", {
      createDebugAdapterDescriptor(session) {
        const level = session.configuration.stepLevel === undefined ? "source" : session.configuration.stepLevel;
        if (!["source", "wasm", "native"].includes(level)) {
          throw new Error("UWVM stepLevel must be source, wasm or native");
        }
        const python = session.configuration.pythonPath ||
          (process.platform === "win32" ? "python" : "python3");
        if (typeof python !== "string" || python.length === 0) {
          throw new Error("UWVM DAP requires a host Python 3 executable");
        }
        return new vscode.DebugAdapterExecutable(
          python, [path.join(context.extensionPath, "dap_adapter.py")]);
      }
    }));
}

function deactivate() {}

module.exports = {activate, deactivate};
