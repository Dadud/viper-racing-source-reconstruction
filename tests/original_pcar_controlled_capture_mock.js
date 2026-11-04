// Exercise controlled PCAR capture field sampling with a mock Frida API.
// This validates script wiring only; it does not emulate native Frida or race.bin.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const imageBase = 0x400000n;
const memory = new Map();
const hooks = new Map();
const emitted = [];
let timeoutCallback = null;
let clock = 1000;

class MockPointer {
    constructor(value = 0n) { this.value = BigInt(value); }
    add(offset) { return new MockPointer(this.value + BigInt(offset)); }
    toString() { return `0x${this.value.toString(16)}`; }
    equals(pointer) { return this.value === pointer.value; }
    isNull() { return this.value === 0n; }
    readFloat() { return Number(memory.get(this.value) ?? 0); }
    readU8() { return Number(memory.get(this.value) ?? 0) & 0xff; }
    readU32() { return Number(memory.get(this.value) ?? 0) >>> 0; }
    readS32() { return Number(memory.get(this.value) ?? 0) | 0; }
    readPointer() {
        const value = memory.get(this.value);
        return value instanceof MockPointer ? value : new MockPointer(value ?? 0n);
    }
    readByteArray(length) { return new Uint8Array(length).buffer; }
}

const ptr = value => new MockPointer(value);
const engine = { name: "race.bin", base: ptr(imageBase), size: 0x1c2000 };
const threadId = () => 7;
const Process = {
    findModuleByName: name => name === "race.bin" ? engine : null,
    getCurrentThreadId: threadId,
};
const Interceptor = {
    attach(address, callbacks) {
        hooks.set(Number(address.value - imageBase), callbacks);
        return { detach() {} };
    },
};
const keyReturn = { value: 0, toInt32() { return this.value; }, replace(v) { this.value = v; } };
const script = fs.readFileSync(
    path.join(__dirname, "..", "tools", "original_pcar_controlled_wheel_capture.js"),
    "utf8",
);

vm.runInNewContext(script, {
    Process,
    Interceptor,
    ptr,
    send: event => emitted.push(event),
    setTimeout: callback => { timeoutCallback = callback; return 1; },
    clearTimeout: () => {},
    Date: { now: () => clock },
    Map,
    Array,
    Object,
    Number,
    Uint8Array,
    JSON,
    String,
    Error,
});

const body = ptr(imageBase + 0x100000n);
const wheel = ptr(imageBase + 0x110000n);
const controller = ptr(imageBase + 0x120000n);
const d24 = ptr(imageBase + 0x130000n);
const throttleBinding = ptr(0x509328n);
const brakeBinding = ptr(0x509368n);
memory.set(body.value, 0x4c8188);
memory.set(body.add(0xc98).value, controller);
memory.set(controller.add(0x4c).value, d24);
memory.set(throttleBinding.value, 1);
memory.set(throttleBinding.add(4).value, 0xc8);
memory.set(brakeBinding.value, 1);
memory.set(brakeBinding.add(4).value, 0x39);
memory.set(ptr(0x5093f4n).value, 0.25);
memory.set(ptr(0x509320n).value, 0);
memory.set(ptr(0x509358n).value, 0);

const keyHook = hooks.get(0x13170);
clock = 4000;
const keyInvocation = { selector: 0xc8 };
keyHook.onEnter.call(keyInvocation, [{ toUInt32: () => keyInvocation.selector }]);
keyReturn.value = 0;
keyHook.onLeave.call(keyInvocation, keyReturn);
assert.equal(keyReturn.value, 0, "no synthetic input before selected PCAR is seen");

hooks.get(0x384d0).onEnter.call({ context: { ecx: body } });
clock = 7000;
keyHook.onEnter.call(keyInvocation, [{ toUInt32: () => keyInvocation.selector }]);
keyReturn.value = 0;
keyHook.onLeave.call(keyInvocation, keyReturn);
assert.equal(keyReturn.value, 1, "synthetic throttle return is patched");

clock = 10000;
memory.set(brakeBinding.value, 2);
const unsupportedBrakeInvocation = { selector: 0x39 };
keyHook.onEnter.call(unsupportedBrakeInvocation,
    [{ toUInt32: () => unsupportedBrakeInvocation.selector }]);
const unsupportedBrakeReturn = {
    value: 0,
    toInt32() { return this.value; },
    replace(value) { this.value = value; },
};
keyHook.onLeave.call(unsupportedBrakeInvocation, unsupportedBrakeReturn);
assert.equal(unsupportedBrakeReturn.value, 0,
    "non-keyboard binding types are not synthesized as key presses");
memory.set(brakeBinding.value, 1);
const brakeInvocation = { selector: 0x39 };
keyHook.onEnter.call(brakeInvocation, [{ toUInt32: () => brakeInvocation.selector }]);
const brakeReturn = {
    value: 0,
    toInt32() { return this.value; },
    replace(value) { this.value = value; },
};
keyHook.onLeave.call(brakeInvocation, brakeReturn);
assert.equal(brakeReturn.value, 1, "configured type-1 brake key is patched");

const integrator = hooks.get(0x31150);
const invocation = { context: { ecx: body } };
integrator.onEnter.call(invocation);
const wheelInvocation = { context: { ecx: wheel } };
hooks.get(0x42530).onEnter.call(wheelInvocation);
hooks.get(0x42530).onLeave.call(wheelInvocation);
integrator.onLeave.call(invocation);

assert.equal(typeof timeoutCallback, "function");
timeoutCallback();
const sample = emitted.find(event => event.event === "pcar_integrator_sample");
assert(sample, "integrator sample emitted");
assert.equal(sample.configuredBindings.throttle.type, 1);
assert.equal(sample.configuredBindings.throttle.selector, 0xc8);
assert.equal(sample.configuredBindings.brake.type, 1);
assert.equal(sample.configuredBindings.brake.selector, 0x39);
assert.equal(emitted[0].event, "controlled_pcar_capture_ready");
assert(emitted.some(event => event.event === "controlled_pcar_capture_armed"));
assert(emitted.some(event => event.event === "pcar_selected_player" &&
    event.body === body.toString()));
const done = emitted.find(event => event.event === "controlled_pcar_capture_done");
assert(done && done.captureValid === false,
    "one sample must fail closed on inadequate phase and wheel coverage");
assert(done.invalidReasons.includes("insufficient_phase_sample_coverage"));
assert(done.invalidReasons.includes("insufficient_wheel_rows"));

console.log(`Controlled PCAR capture mock passed (${emitted.length} emitted events).`);
