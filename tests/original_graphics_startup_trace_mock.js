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
const memory = new Map();
let exceptionHandler = null;

class Pointer {
    constructor(value) { this.value = BigInt(value); }
    add(offset) { return new Pointer(this.value + BigInt(offset)); }
    toString() { return `0x${this.value.toString(16)}`; }
    toUInt32() { return Number(this.value & 0xffffffffn); }
    isNull() { return this.value === 0n; }
    readU32() { return this.value === 0x509c52n ? 0 : this.toUInt32(); }
    readPointer() { return new Pointer(memory.get(this.value) ?? 0xabcdefn); }
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
const invocation = { returnAddress: pointer(0x44e085n), context: {
    ecx: pointer(0x10n), esi: pointer(0x20n), edi: pointer(0x30n),
} };
graphics.onEnter.call(invocation, [pointer(1n), pointer(2n), pointer(3n), pointer(4n)]);
graphics.onLeave.call(invocation, pointer(0x1234n));
assert(events.some(event => event.event === "graphics_startup_enter" &&
    event.address === "0x44e080" && event.args[0] === "0x1"));
assert(events.some(event => event.event === "graphics_startup_return" &&
    event.address === "0x44e080" && event.return_low_byte === 0x34));

const directDrawCreate = hooks.get("0x8000");
assert(directDrawCreate, "DirectDrawCreate interface discovery hook should be installed");
memory.set(0x1000n, 0x2000n); // DirectDrawCreate output -> IDirectDraw object
memory.set(0x2000n, 0x3000n); // object -> vtable
memory.set(0x3000n, 0x4000n); // IUnknown::QueryInterface
memory.set(0x3020n, 0x5000n); // IDirectDraw::EnumDisplayModes (slot 8)
const createContext = {};
directDrawCreate.onEnter.call(createContext, [pointer(0n), pointer(0x1000n)]);
directDrawCreate.onLeave.call(createContext, pointer(0n));
assert(events.some(event => event.event === "directdraw_interface_observed" &&
    event.object === "0x2000" && event.enum_display_modes === "0x5000"));
assert(hooks.has("0x4000"), "QueryInterface should be hooked on the discovered interface");
const realEnum = hooks.get("0x5000");
assert(realEnum, "EnumDisplayModes should be hooked on the discovered interface");
const realEnumContext = { context: { ecx: pointer(0x2000n) } };
realEnum.onEnter.call(realEnumContext,
    [pointer(0n), pointer(0x6000n), pointer(0x7000n), pointer(0x44ee00n)]);
assert(events.some(event => event.event === "directdraw_enum_display_modes_enter" &&
    event.object === "0x2000" && event.descriptor === "0x6000" &&
    event.callback === "0x44ee00"));

const modeEnumeration = hooks.get("0x44eeb0");
modeEnumeration.onEnter.call(invocation,
    [pointer(1n), pointer(2n), pointer(3n), pointer(4n)]);
assert(events.some(event => event.event === "graphics_startup_enter" &&
    event.address === "0x44eeb0" && event.mode_640x480_flag_0x509c52 === 0));
const enumWrapper = hooks.get("0x458d60");
enumWrapper.onEnter.call(invocation,
    { 0: pointer(0x101n), 1: pointer(0x200n),
      2: pointer(0x300n), 3: pointer(0x400n) });
assert(events.some(event => event.event === "graphics_startup_enter" &&
    event.address === "0x458d60" && event.registers.ecx === "0x10" &&
    event.argument_words.length === 4 && event.wrapper_words.length === 16 &&
    event.enum_mode_descriptor_words.length === 27));

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
