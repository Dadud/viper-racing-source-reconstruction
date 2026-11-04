// Verify passive thread sampling without attaching to the original game.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const source = fs.readFileSync(
    path.join(__dirname, "..", "tools", "original_startup_thread_sample.js"),
    "utf8").replace("__TARGET_TID__", "42");
const messages = [];
let timerCallback;
const base = 0x400000n;
class Pointer {
    constructor(value) { this.value = BigInt(value); }
    sub(other) { return new Pointer(this.value - other.value); }
    toString() { return `0x${this.value.toString(16)}`; }
}
const Process = {
    arch: "ia32", pointerSize: 4,
    enumerateThreads: () => [
        { id: 41, state: "running", context: { eip: new Pointer(0x401234), esp: new Pointer(0x500000) } },
        { id: 42, state: "waiting", context: { eip: new Pointer(0x405678), esp: new Pointer(0x501000) } },
    ],
    findModuleByAddress: () => ({ name: "race.bin", base: new Pointer(base) }),
};
vm.runInNewContext(source, {
    Process,
    send: message => messages.push(message),
    setInterval: callback => { timerCallback = callback; return 1; },
    clearInterval: () => {},
    Number, Error, String,
});
assert(messages.some(message => message.event === "startup_thread_sampler_ready" &&
    message.target_tid === 42));
timerCallback();
const sample = messages.find(message => message.event === "startup_thread_sample");
assert(sample, "the requested child thread should be sampled");
assert.equal(sample.target_tid, 42);
assert.equal(sample.pc, "0x405678");
assert.equal(sample.sp, "0x501000");
assert.equal(sample.module, "race.bin");
assert.equal(sample.module_offset, "0x5678");
console.log("Startup thread sampler mock passed (requested thread and module-relative PC/SP).");
