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
const snapshotBytes = 0x105c;
const sampleEveryCalls = 16;
const maxSamples = 160;
let allCalls = 0;
let pcarCalls = 0;
let samples = 0;
let readFailures = 0;

function readU32(address) {
  try { return address.readU32(); } catch (_) { return null; }
}

function emitSnapshot(object, count, phase) {
  try {
    const bytes = object.readByteArray(snapshotBytes);
    send({event: "pcar_integrator_object_snapshot", count,
      phase, timestampMs: Date.now(), object: object.toString(),
      vtable: readU32(object), byteLength: snapshotBytes}, bytes);
  } catch (error) {
    readFailures++;
    send({event: "pcar_integrator_object_snapshot_error", count,
      phase, object: object.toString(), error: String(error), readFailures});
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
    this.capture = pcarCalls % sampleEveryCalls === 0 && samples < maxSamples;
    if (!this.capture) return;
    samples++;
    emitSnapshot(this.object, this.selectedCount, "before");
  },
  onLeave() {
    if (this.capture)
      emitSnapshot(this.object, this.selectedCount, "after");
  }
});

send({event: "pcar_integrator_object_probe_ready", module: engine.name,
  base: base.toString(), arch: Process.arch,
  integrator: at(0x431150).toString(), playerVtable: playerVtable.toString(),
  snapshotBytes, sampleEveryCalls, maxSamples});

rpc.exports = {
  status() {
    return {allCalls, pcarCalls, samples, readFailures,
      sampleEveryCalls, maxSamples};
  }
};
