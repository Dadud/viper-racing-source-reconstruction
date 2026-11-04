// Validate launcher diagnostics and suspended-child matching without Windows.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const source = fs.readFileSync(
    path.join(__dirname, "..", "tools", "original_launcher_child_bridge.js"),
    "utf8").replace("__EXPECTED_CWD_JSON__", JSON.stringify(
        "C:\\Stage\\GameRoot\\Data"));
assert(!source.includes("0x4017B0"),
    "do not install an entry hook on the launcher's one-byte RET stub");
assert(source.includes("launcher_code_hook_unavailable"),
    "a failed diagnostic marker must not abort the remaining bridge hooks");
const memory = new Map();
const hooks = new Map();
const emitted = [];
let delayedResume = null;

class MockPointer {
    constructor(value = 0n) { this.value = BigInt(value); }
    add(offset) { return new MockPointer(this.value + BigInt(offset)); }
    toString() { return `0x${this.value.toString(16)}`; }
    toUInt32() { return Number(this.value & 0xffffffffn); }
    toInt32() { return this.toUInt32() | 0; }
    isNull() { return this.value === 0n; }
    readCString() { return String(memory.get(this.value) ?? ""); }
    readUtf16String() { return this.readCString(); }
    readPointer() {
        const value = memory.get(this.value);
        return value instanceof MockPointer ? value : new MockPointer(value ?? 0n);
    }
    readU32() {
        const value = memory.get(this.value);
        return value instanceof MockPointer ? value.toUInt32() : Number(value ?? 0);
    }
}

const pointer = value => new MockPointer(value);
const exportsByName = new Map();
let nextExport = 0x8000n;
const Module = {
    getGlobalExportByName(name) {
        if (!exportsByName.has(name)) exportsByName.set(name, pointer(nextExport++));
        return exportsByName.get(name);
    },
};
const launcher = { name: "Viper Racing.exe", base: pointer(0x400000n) };
const Process = {
    arch: "ia32",
    pointerSize: 4,
    enumerateModules: () => [launcher],
};
const Interceptor = {
    attach(address, callbacks) { hooks.set(address.toString(), callbacks); return {}; },
};
function NativeFunction(_address, _returnType, _argumentTypes, abi) {
    assert.equal(abi, undefined);
    return () => 0;
}

vm.runInNewContext(source, {
    Process, Module, Interceptor, NativeFunction,
    ptr: pointer,
    send: event => emitted.push(event),
    setTimeout: callback => { delayedResume = callback; return 1; },
    Map, Set, String, Error, JSON,
});

const canaryInvocation = {};
hooks.get("0x401600").onEnter.call(canaryInvocation, []);
hooks.get("0x401600").onLeave.call(canaryInvocation,
    { toUInt32: () => 0x100 });
assert(emitted.some(event => event.event === "launcher_code_enter" &&
    event.address === "0x401600"));
assert(emitted.some(event => event.event === "launcher_code_return" &&
    event.address === "0x401600" && event.return_low_byte === 0));

const dataPathBuffer = pointer(0x9000n);
memory.set(dataPathBuffer.value, "C:\\Stage\\GameRoot\\Data");
const dataPathInvocation = {};
hooks.get("0x401880").onEnter.call(dataPathInvocation, [dataPathBuffer]);
hooks.get("0x401880").onLeave.call(dataPathInvocation,
    { toUInt32: () => 0x100 });
const pathResult = emitted.find(event => event.event === "launcher_data_directory_result");
assert.equal(pathResult.succeeded, false,
    "the launcher checks the low-byte Boolean return, not upper EAX bits");
assert.equal(pathResult.path, "C:\\Stage\\GameRoot\\Data");

const drivePath = pointer(0x9100n);
memory.set(drivePath.value, "F:\\");
const getDriveType = hooks.get(Module.getGlobalExportByName("GetDriveTypeA").toString());
const driveInvocation = {};
getDriveType.onEnter.call(driveInvocation, [drivePath]);
getDriveType.onLeave.call(driveInvocation, { toUInt32: () => 5 });
assert(emitted.some(event => event.event === "launcher_path_probe" &&
    event.api === "GetDriveTypeA" && event.path === "F:\\" && event.result === 5));

const appName = pointer(0x9200n);
const command = pointer(0x9300n);
const cwd = pointer(0x9400n);
const processInfo = pointer(0x9500n);
memory.set(appName.value, "");
memory.set(command.value, "Data\\race.bin -1");
memory.set(cwd.value, "C:\\Stage\\GameRoot\\Data");
memory.set(processInfo.add(4).value, pointer(0xA000n));
memory.set(processInfo.add(8).value, 1234);
memory.set(processInfo.add(12).value, 5678);
const createProcess = hooks.get(Module.getGlobalExportByName("CreateProcessA").toString());
const args = [appName, command, pointer(0), pointer(0), pointer(0),
    pointer(0), pointer(0), cwd, pointer(0), processInfo];
const processInvocation = { returnAddress: pointer(0x401472n) };
createProcess.onEnter.call(processInvocation, args);
assert.equal(args[5].toUInt32(), 4,
    "only the matching race.bin child gets CREATE_SUSPENDED");
createProcess.onLeave.call(processInvocation, { toInt32: () => 1 });
assert(emitted.some(event => event.event === "launcher_process_create_call" &&
    event.command === "Data\\race.bin -1"));
assert(emitted.some(event => event.event === "race_child_ready_for_attach" &&
    event.pid === 1234 && event.tid === 5678));
assert.equal(typeof delayedResume, "function");
delayedResume();
assert(emitted.some(event => event.event === "race_child_resumed" &&
    event.previousSuspendCount === 0));

console.log("Launcher child bridge mock passed (path diagnostics and suspended-child match).");
