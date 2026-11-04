// Mock the process-local DirectSound hooks without launching Windows audio.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const script = fs.readFileSync(
    path.join(__dirname, "..", "tools", "original_directsound_process_silencer.js"),
    "utf8");
const memory = new Map();
const attachments = new Map();
const replacements = new Map();
const emitted = [];
const nativeCalls = [];
const nativeResults = new Map();

class MockPointer {
    constructor(value = 0n) { this.value = BigInt(value); }
    add(offset) { return new MockPointer(this.value + BigInt(offset)); }
    toString() { return `0x${this.value.toString(16)}`; }
    isNull() { return this.value === 0n; }
    readPointer() {
        const value = memory.get(this.value);
        return value instanceof MockPointer ? value : new MockPointer(value ?? 0n);
    }
}

const pointer = value => new MockPointer(value);
const dsound = {
    name: "dsound.dll",
    findExportByName(name) {
        return name === "DirectSoundCreate" ? pointer(0x1000n)
            : name === "DirectSoundCreate8" ? pointer(0x1004n) : null;
    },
};
const Interceptor = {
    attach(address, callbacks) {
        attachments.set(address.toString(), callbacks);
        return { detach() {} };
    },
    replace(address, callback) { replacements.set(address.toString(), callback); },
};
const Process = {
    arch: "ia32",
    pointerSize: 4,
    findModuleByName: name => name.toLowerCase() === "dsound.dll" ? dsound : null,
    attachModuleObserver(observer) {
        observer.onAdded(dsound);
        return { detach() {} };
    },
};
function NativeFunction(address, _returnType, _argumentTypes, abi) {
    assert.equal(abi, "stdcall");
    return (...args) => {
        nativeCalls.push({ address: address.toString(), args });
        return nativeResults.get(address.toString()) ?? 0;
    };
}
function NativeCallback(callback, _returnType, _argumentTypes, abi) {
    assert.equal(abi, "stdcall");
    return callback;
}

vm.runInNewContext(script, {
    Process, Interceptor, NativeFunction, NativeCallback,
    Module: { load: name => { assert.equal(name, "dsound.dll"); return dsound; } },
    ptr: pointer,
    rpc: { exports: {} },
    send: event => emitted.push(event),
    Map, Set, String, Error,
});

assert(emitted.some(event => event.event === "directsound_process_silencer_ready"));
assert(attachments.has("0x1000"));

const device = pointer(0x10000n);
const deviceVtable = pointer(0x11000n);
memory.set(device.value, deviceVtable);
memory.set(deviceVtable.add(3 * 4).value, pointer(0x2000n));
memory.set(deviceVtable.add(5 * 4).value, pointer(0x3000n));
const deviceOut = pointer(0x12000n);
memory.set(deviceOut.value, device);
const createInvocation = { output: null };
attachments.get("0x1000").onEnter.call(createInvocation,
    [pointer(0), deviceOut, pointer(0)]);
attachments.get("0x1000").onLeave.call(createInvocation,
    { toInt32: () => 0 });
assert(attachments.has("0x2000"));
assert(attachments.has("0x3000"));

function makeBuffer(bufferAddress, tableAddress) {
    const buffer = pointer(bufferAddress);
    const table = pointer(tableAddress);
    memory.set(buffer.value, table);
    memory.set(table.add(12 * 4).value, pointer(0x4000n));
    memory.set(table.add(15 * 4).value, pointer(0x5000n));
    return buffer;
}
const buffer1 = makeBuffer(0x20000n, 0x21000n);
const bufferOut1 = pointer(0x22000n);
memory.set(bufferOut1.value, buffer1);
const createBufferInvocation = { output: null };
attachments.get("0x2000").onEnter.call(createBufferInvocation,
    [device, pointer(0), bufferOut1, pointer(0)]);
attachments.get("0x2000").onLeave.call(createBufferInvocation,
    { toInt32: () => 0 });
assert(replacements.has("0x5000"));
assert(replacements.has("0x4000"));

const setVolume = replacements.get("0x5000");
const play = replacements.get("0x4000");
assert.equal(setVolume(buffer1, 0), 0);
assert.equal(nativeCalls.at(-1).args[1], -10000,
    "requested volume is clamped to DirectSound minimum");
assert.equal(play(buffer1, 0, 2, 1), 0);
assert.equal(nativeCalls.at(-2).args[1], -10000,
    "each Play reapplies the silent volume before forwarding");
assert.deepEqual(nativeCalls.at(-1).args.slice(1), [0, 2, 1],
    "Play arguments and native transition are forwarded");

const buffer2 = makeBuffer(0x23000n, 0x24000n);
const bufferOut2 = pointer(0x25000n);
memory.set(bufferOut2.value, buffer2);
nativeResults.set("0x5000", -1);
const duplicateInvocation = { output: null };
attachments.get("0x3000").onEnter.call(duplicateInvocation,
    [device, buffer1, bufferOut2, pointer(0)]);
attachments.get("0x3000").onLeave.call(duplicateInvocation,
    { toInt32: () => 0 });
assert(emitted.some(event => event.event === "directsound_buffer_silenced" &&
    event.source === "DuplicateSoundBuffer"));
assert(emitted.filter(event =>
    event.event === "directsound_play_forwarded_silent").length === 1);
const priorPlayCalls = nativeCalls.filter(call => call.address === "0x4000").length;
assert.equal(play(buffer2, 0, 0, 0), 0,
    "an unmutable buffer is treated as stopped without audible playback");
assert.equal(nativeCalls.filter(call => call.address === "0x4000").length,
    priorPlayCalls, "unmutable buffer is not forwarded to native Play");
assert(emitted.some(event =>
    event.event === "directsound_play_suppressed_unmutable_buffer"));

console.log("DirectSound process silencer mock passed (creation, duplication, clamp, and forwarded Play).");
