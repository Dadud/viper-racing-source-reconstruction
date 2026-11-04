// Exercise the Frida agent's lifecycle and fail-closed classification with a
// minimal mock API. This does not validate native Frida watchpoint behavior.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const script = fs.readFileSync(
    path.join(__dirname, "..", "tools", "pcar_source_field_watch.js"), "utf8");

function runScenario(options = {}) {
    const imageBase = 0x400000n;
    const memory = new Map();
    const hooks = new Map();
    const emitted = [];
    let exceptionHandler = null;
    let timeoutCallback = null;
    let watchpointSetCount = 0;

    class MockPointer {
        constructor(value = 0n) { this.value = BigInt(value); }
        add(offset) { return new MockPointer(this.value + BigInt(offset)); }
        sub(pointer) { return new MockPointer(this.value - pointer.value); }
        toString() { return `0x${this.value.toString(16)}`; }
        equals(pointer) { return this.value === pointer.value; }
        isNull() { return this.value === 0n; }
        readU32() { return Number(memory.get(this.value) ?? 0) >>> 0; }
        readFloat() { return Number(memory.get(this.value) ?? 0); }
        readPointer() {
            const value = memory.get(this.value);
            return value instanceof MockPointer ? value : new MockPointer(value ?? 0n);
        }
    }

    const pointer = value => new MockPointer(value);
    const engine = { name: "race.bin", base: pointer(imageBase), size: 0x1c2000 };
    const watchedThread = {
        id: 7,
        watchpoints: new Map(),
        setHardwareWatchpoint(id, address, size, condition) {
            assert.equal(size, 4);
            assert.equal(condition, "w");
            watchpointSetCount++;
            if (options.rearmFailure && watchpointSetCount > 2)
                throw new Error("mock rearm failure");
            this.watchpoints.set(id, address);
        },
        unsetHardwareWatchpoint(id) { this.watchpoints.delete(id); },
    };
    const Process = {
        arch: "ia32",
        findModuleByName: name => name === "race.bin" ? engine : null,
        findModuleByAddress: address =>
            address.value >= imageBase && address.value < imageBase + BigInt(engine.size)
                ? engine : null,
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

    vm.runInNewContext(script, {
        Process,
        Interceptor,
        Thread: { backtrace: () => [pointer(0x431150n)] },
        Backtracer: { FUZZY: 1 },
        rpc: { exports: {} },
        send: event => emitted.push(event),
        setTimeout: callback => { timeoutCallback = callback; return 1; },
        Date,
        Map,
        Array,
        JSON,
        Number,
        String,
        Error,
    });

    const body = pointer(imageBase + 0x100000n);
    memory.set(body.value, 0x4c8188);
    memory.set(body.add(0x510).value, 7);
    const constructorInvocation = { context: { ecx: body } };
    if (!options.skipConstructor) {
        hooks.get(0x381e0).onEnter.call(constructorInvocation);
        assert.equal(watchedThread.watchpoints.size, 2);
    }

    function triggerWrite(offset, value, memoryOverride) {
        memory.set(body.add(offset).value, value);
        const event = {
            type: "single-step",
            context: {
                pc: pointer(0x43470cn),
                eip: pointer(0x43470cn),
            },
        };
        if (memoryOverride !== null) {
            event.memory = memoryOverride === undefined
                ? { address: body.add(offset), operation: "write" }
                : memoryOverride;
        }
        return exceptionHandler(event);
    }

    if (options.spuriousEvent) {
        assert.equal(exceptionHandler({
            type: "single-step",
            context: { pc: pointer(0x43470cn) },
        }), true);
    }
    if (!options.skipConstructor) {
        const firstMemory = options.missingMemoryDetails
            ? null
            : options.badMemory === "address"
            ? { address: body.add(0x600), operation: "write" }
            : options.badMemory === "operation"
                ? { address: body.add(0x5b0), operation: "read" }
                : undefined;
        triggerWrite(0x5b0, 12.5, firstMemory);
        hooks.get(0x381e0).onLeave.call(constructorInvocation);
        if (!options.rearmFailure) {
            assert.equal(watchedThread.watchpoints.size, 2,
                "constructor watchpoints remain active after constructor return");
            triggerWrite(0x8f8, 24.5);
        }
    }

    hooks.get(0x384d0).onEnter.call({ context: { ecx: body } });
    if (options.ambiguousCandidate) {
        const secondBody = pointer(imageBase + 0x110000n);
        memory.set(secondBody.value, 0x4c8188);
        memory.set(secondBody.add(0x510).value, 8);
        hooks.get(0x384d0).onEnter.call({ context: { ecx: secondBody } });
    }
    const tickInvocation = { context: { ecx: body } };
    hooks.get(0x31150).onEnter.call(tickInvocation);
    hooks.get(0x31150).onLeave.call(tickInvocation);
    timeoutCallback();

    const done = emitted.find(event => event.event === "pcar_source_field_watch_done");
    assert(done, "watch must emit a completion assessment");
    assert.equal(watchedThread.watchpoints.size, 0, "timeout clears watches");
    assert.equal(exceptionHandler, null, "timeout clears exception handler");
    assert.equal(done.callback_candidate_count, options.ambiguousCandidate ? 2 : 1);
    return { done, emitted };
}

const valid = runScenario();
assert.equal(valid.done.capture_valid, true);
assert.equal(valid.done.write_events, 2);
assert.equal(valid.done.selected_constructor_matched, true);
const constructorRow = valid.done.constructor_rows.find(row =>
    row.body === valid.done.selected_body);
assert.equal(constructorRow.watch_retained_after_return, true);
assert(valid.emitted.some(event => event.event === "pcar_source_field_write" &&
    event.target_offset === 0x5b0));
assert(valid.emitted.some(event => event.event === "pcar_source_field_write" &&
    event.target_offset === 0x8f8));
const validWithoutMemoryDetails = runScenario({ missingMemoryDetails: true });
assert.equal(validWithoutMemoryDetails.done.capture_valid, true);
assert.equal(validWithoutMemoryDetails.done.write_events, 2);

for (const scenario of [
    { spuriousEvent: true },
    { badMemory: "address" },
    { badMemory: "operation" },
    { ambiguousCandidate: true },
    { skipConstructor: true },
    { rearmFailure: true },
]) {
    const result = runScenario(scenario);
    assert.equal(result.done.capture_valid, false, JSON.stringify(scenario));
}

assert.equal(runScenario({ spuriousEvent: true }).done.unclassified_events, 1);
assert(runScenario({ ambiguousCandidate: true }).done.invalid_reasons.includes(
    "player_callback_identity_not_unique"));
assert(runScenario({ skipConstructor: true }).done.invalid_reasons.includes(
    "selected_constructor_watch_missing_or_not_retained"));
const failedRearm = runScenario({ rearmFailure: true });
assert.equal(failedRearm.done.constructor_rows[0].watch_retained_after_return, false);

console.log("PCAR field watch mock passed (valid and fail-closed scenarios).");
