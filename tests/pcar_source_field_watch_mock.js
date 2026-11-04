// Exercise the Frida agent's lifecycle with a minimal mock API. This checks
// script wiring and cleanup, not Frida's native hardware-watchpoint behavior.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const imageBase = 0x400000n;
const memory = new Map();
const hooks = new Map();
const emitted = [];
let exceptionHandler = null;
let timeoutCallback = null;

class MockPointer {
    constructor(value = 0n) {
        this.value = BigInt(value);
    }

    add(offset) {
        return new MockPointer(this.value + BigInt(offset));
    }

    sub(pointer) {
        return new MockPointer(this.value - pointer.value);
    }

    toString() {
        return `0x${this.value.toString(16)}`;
    }

    equals(pointer) {
        return this.value === pointer.value;
    }

    isNull() {
        return this.value === 0n;
    }

    readU32() {
        return Number(memory.get(this.value) ?? 0) >>> 0;
    }

    readFloat() {
        return Number(memory.get(this.value) ?? 0);
    }

    readPointer() {
        const value = memory.get(this.value);
        return value instanceof MockPointer ? value : new MockPointer(value ?? 0n);
    }
}

const pointer = value => new MockPointer(value);
const engine = {
    name: "race.bin",
    base: pointer(imageBase),
    size: 0x1c2000,
};
const watchedThread = {
    id: 7,
    watchpoints: new Map(),
    setHardwareWatchpoint(id, address, size, condition) {
        assert.equal(size, 4);
        assert.equal(condition, "w");
        this.watchpoints.set(id, address);
    },
    unsetHardwareWatchpoint(id) {
        this.watchpoints.delete(id);
    },
};

const Process = {
    arch: "ia32",
    findModuleByName: name => name === "race.bin" ? engine : null,
    findModuleByAddress: address =>
        address.value >= imageBase && address.value < imageBase + BigInt(engine.size)
            ? engine
            : null,
    enumerateThreads: () => [watchedThread],
    getCurrentThreadId: () => watchedThread.id,
    setExceptionHandler: handler => { exceptionHandler = handler; },
};
const Interceptor = {
    attach(address, callbacks) {
        hooks.set(Number(address.value - imageBase), callbacks);
        return { detach() {} };
    },
};
const Thread = {
    backtrace: () => [pointer(imageBase + 0x431150n)],
};

vm.runInNewContext(
    fs.readFileSync(path.join(__dirname, "..", "tools", "pcar_source_field_watch.js"), "utf8"),
    {
        Process,
        Interceptor,
        Thread,
        Backtracer: { FUZZY: 1 },
        ptr: pointer,
        send: event => emitted.push(event),
        setTimeout: callback => { timeoutCallback = callback; return 1; },
        Date,
        Map,
        Array,
        JSON,
        Number,
        String,
        Error,
    },
);

const body = pointer(imageBase + 0x100000n);
memory.set(body.value, 0x4c8188);
memory.set(body.add(0x510).value, 3);
const constructorInvocation = { context: { ecx: body } };

hooks.get(0x381e0).onEnter.call(constructorInvocation);
assert.equal(watchedThread.watchpoints.size, 2);

memory.set(body.add(0x5b0).value, 12.5);
assert.equal(exceptionHandler({
    type: "single-step",
    context: { pc: pointer(imageBase + 0x43470cn), eip: pointer(imageBase + 0x43470cn) },
    memory: { address: body.add(0x5b0), operation: "write" },
}), true);
assert.equal(watchedThread.watchpoints.size, 2, "watchpoints rearm after first hit");

memory.set(body.add(0x8f8).value, 24.5);
assert.equal(exceptionHandler({
    type: "breakpoint",
    context: { pc: pointer(imageBase + 0x434712n), eip: pointer(imageBase + 0x434712n) },
    memory: { address: body.add(0x8f8), operation: "write" },
}), true);
hooks.get(0x381e0).onLeave.call(constructorInvocation);
assert.equal(watchedThread.watchpoints.size, 0, "constructor removes its watches");

hooks.get(0x384d0).onEnter.call({ context: { ecx: body } });
const tickInvocation = { context: { ecx: body } };
hooks.get(0x31150).onEnter.call(tickInvocation);
hooks.get(0x31150).onLeave.call(tickInvocation);
assert.equal(watchedThread.watchpoints.size, 2, "integrator arms selected thread");

timeoutCallback();
assert.equal(watchedThread.watchpoints.size, 0, "timeout clears all watches");
assert.equal(exceptionHandler, null, "timeout clears the exception handler");
assert(emitted.some(event => event.event === "pcar_source_field_write" &&
    event.changed_offsets.includes(0x5b0)));
assert(emitted.some(event => event.event === "pcar_source_field_write" &&
    event.changed_offsets.includes(0x8f8)));
assert(emitted.some(event => event.event === "pcar_source_field_tick_sample"));

console.log(`PCAR field watch mock passed (${emitted.length} emitted events).`);
