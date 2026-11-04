// Verify graphics-startup trace event wiring without running the original game.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const source = fs.readFileSync(
    path.join(__dirname, "..", "tools", "original_graphics_startup_trace.js"),
    "utf8");
const hooks = new Map();
const events = [];
let exceptionHandler = null;

class Pointer {
    constructor(value) { this.value = BigInt(value); }
    add(offset) { return new Pointer(this.value + BigInt(offset)); }
    toString() { return `0x${this.value.toString(16)}`; }
    toUInt32() { return Number(this.value & 0xffffffffn); }
    isNull() { return this.value === 0n; }
    readU32() { return this.toUInt32(); }
    readPointer() { return new Pointer(0xabcdefn); }
}
const pointer = value => new Pointer(value);
const Process = {
    arch: "ia32",
    pointerSize: 4,
    findModuleByName(name) {
        assert.equal(name, "race.bin");
        return { name, base: pointer(0x400000n) };
    },
    setExceptionHandler(callback) { exceptionHandler = callback; },
};
let nextExport = 0x8000n;
const Module = {
    getGlobalExportByName() { return pointer(nextExport++); },
};
const Interceptor = {
    attach(address, callbacks) { hooks.set(address.toString(), callbacks); return {}; },
};

vm.runInNewContext(source, {
    Process, Module, Interceptor, ptr: pointer,
    send: event => events.push(event), Error, String, Map,
});

const graphics = hooks.get("0x44e080");
assert(graphics, "graphics initialization entry hook should be installed");
const invocation = { returnAddress: pointer(0x44e085n) };
graphics.onEnter.call(invocation, [pointer(1n), pointer(2n), pointer(3n), pointer(4n)]);
graphics.onLeave.call(invocation, pointer(0x1234n));
assert(events.some(event => event.event === "graphics_startup_enter" &&
    event.address === "0x44e080" && event.args[0] === "0x1"));
assert(events.some(event => event.event === "graphics_startup_return" &&
    event.address === "0x44e080" && event.return_low_byte === 0x34));

const terminateAddress = events.find(event => event.event === "native_startup_hook_ready" &&
    event.api === "NtTerminateProcess").address;
const terminateHook = hooks.get(terminateAddress);
assert(terminateHook);
const terminateContext = { returnAddress: pointer(0x44ecb3n) };
terminateHook.onEnter.call(terminateContext, [pointer(0xffffffffn), pointer(5n)]);
terminateHook.onLeave.call(terminateContext, pointer(0n));
assert(events.some(event => event.event === "native_termination_call" &&
    event.exit_status === 5));
assert(events.some(event => event.event === "native_termination_return" &&
    event.status === 0));

assert.equal(typeof exceptionHandler, "function");
assert.equal(exceptionHandler({ type: "access-violation", address: pointer(0x401000n),
    context: { pc: pointer(0x401000n), sp: pointer(0x500000n) } }), false,
"exception observer must pass the exception to the normal process handler");
assert(events.some(event => event.event === "unhandled_native_exception" &&
    event.pc === "0x401000"));

console.log("Graphics startup trace mock passed (call events, termination status, and pass-through exceptions).");
