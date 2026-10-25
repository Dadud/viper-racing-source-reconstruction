// Read-only probe for the shared native car integrator at 0x431150.
// Attach this to race.bin after the launcher-owned bridge has resumed the
// child. It snapshots only the selected PCAR object, before and after every
// 16th selected-PCAR integrator call, with a hard sample bound.

const modules = Process.enumerateModules();
const engine = modules.find(m => m.name.toLowerCase() === "race.bin");
if (!engine) throw new Error("race.bin module not found");
if (Process.arch !== "ia32" || Process.pointerSize !== 4)
  throw new Error("This probe expects the original 32-bit x86 client.");

const base = engine.base;
const at = va => base.add(va - 0x400000);
const playerVtable = at(0x4c8188);
// 0x422580 allocates PCAR objects at 0xEF0 bytes. This bound covers the
// four 0x1A4-byte wheel objects and body fields through +0xECC.
const snapshotBytes = 0xef0;
const sampleEveryCalls = 16;
const maxSamples = 64;
let allCalls = 0;
let pcarCalls = 0;
let attemptedSamples = 0;
let successfulPairs = 0;
let readFailures = 0;

function readU32(address) {
  try { return address.readU32(); } catch (_) { return null; }
}

function snapshotHex(object) {
  const bytes = object.readByteArray(snapshotBytes);
  if (bytes === null) throw new Error("bounded object read returned null");
  const view = new Uint8Array(bytes);
  let hex = "";
  for (let i = 0; i < view.length; i++)
    hex += view[i].toString(16).padStart(2, "0");
  return hex;
}

function emitSnapshot(object, count, phase) {
  try {
    const rawHex = snapshotHex(object);
    send({event: "pcar_integrator_object_snapshot", count,
      phase, timestampMs: Date.now(), object: object.toString(),
      vtable: readU32(object), byteLength: snapshotBytes, rawHex});
    return true;
  } catch (error) {
    readFailures++;
    send({event: "pcar_integrator_object_snapshot_error", count,
      phase, object: object.toString(), error: String(error), readFailures});
    return false;
  }
}

Interceptor.attach(at(0x431150), {
  onEnter() {
    allCalls++;
    this.object = this.context.ecx;
    this.vtable = this.object.isNull() ? null : readU32(this.object);
    if (this.vtable !== playerVtable.toUInt32()) {
      if (allCalls <= 8)
        send({event: "integrator_non_pcar_sample", count: allCalls,
          object: this.object.toString(), vtable: this.vtable});
      return;
    }

    pcarCalls++;
    this.selectedCount = pcarCalls;
    this.capture = pcarCalls % sampleEveryCalls === 0 &&
      attemptedSamples < maxSamples;
    if (!this.capture) return;
    attemptedSamples++;
    this.beforeCaptured = emitSnapshot(this.object, this.selectedCount, "before");
  },
  onLeave() {
    if (this.capture && emitSnapshot(this.object, this.selectedCount, "after") &&
        this.beforeCaptured)
      successfulPairs++;
  }
});

send({event: "pcar_integrator_object_probe_ready", module: engine.name,
  modulePath: engine.path, moduleSize: engine.size,
  base: base.toString(), arch: Process.arch,
  integrator: at(0x431150).toString(), playerVtable: playerVtable.toString(),
  snapshotBytes, sampleEveryCalls, maxSamples,
  expectedExecutableSha256:
    "F9D6BD3EE1B9E8A7E101BF4CC3C4712969176255111BCF76A85D82568096DA56"});

rpc.exports = {
  status() {
    return {allCalls, pcarCalls, attemptedSamples, successfulPairs, readFailures,
      sampleEveryCalls, maxSamples};
  }
};
