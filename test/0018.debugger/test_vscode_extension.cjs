"use strict";

const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const extensionPath = path.resolve(__dirname, "../../tools/debug");
let factory;
class DebugAdapterExecutable {
  constructor(command, args) { this.command = command; this.args = args; }
}
const vscode = {
  DebugAdapterExecutable,
  debug: {
    registerDebugAdapterDescriptorFactory(type, value) {
      assert.strictEqual(type, "uwvm-llvm-full");
      factory = value;
      return {dispose() {}};
    }
  }
};
const sandbox = {
  module: {exports: {}}, process,
  require(name) { return name === "vscode" ? vscode : require(name); }
};
vm.runInNewContext(fs.readFileSync(path.join(extensionPath, "extension.js"), "utf8"), sandbox);
const context = {extensionPath, subscriptions: []};
sandbox.module.exports.activate(context);
assert.strictEqual(context.subscriptions.length, 1);
const descriptor = factory.createDebugAdapterDescriptor({
  configuration: {pythonPath: "/trusted/python3", capability: "private-host-token"}
});
assert.strictEqual(descriptor.command, "/trusted/python3");
assert.deepStrictEqual(Array.from(descriptor.args), [path.join(extensionPath, "dap_adapter.py")]);
assert.ok(!JSON.stringify(descriptor).includes("private-host-token"));
for (const stepLevel of ["source", "wasm", "native"]) {
  const selected = factory.createDebugAdapterDescriptor({configuration: {stepLevel}});
  assert.deepStrictEqual(Array.from(selected.args), [path.join(extensionPath, "dap_adapter.py")]);
}
for (const stepLevel of [null, true, 1, [], {}, "unknown"]) {
  assert.throws(() => factory.createDebugAdapterDescriptor({configuration: {stepLevel}}), /stepLevel/);
}
const config = JSON.parse(fs.readFileSync(path.join(extensionPath, "package.json"), "utf8"));
const registered = config.contributes.debuggers[0];
assert.deepStrictEqual(registered.configurationAttributes.attach.properties.stepLevel.enum, ["source", "wasm", "native"]);
assert.strictEqual(registered.configurationAttributes.attach.properties.stepLevel.default, "source");
assert.ok(registered.configurationSnippets.every(item => item.body.stepLevel === "source"));
console.log("PASS VS Code descriptor validates source/wasm/native levels and keeps broker capability out of argv");
