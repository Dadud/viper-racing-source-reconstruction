// Read-only, bounded probe for the original x86 Viper Racing engine.
// Attach after the player PCAR callback is active. This records force-writer
// arguments, body accumulators, and the currently executing 0x442530 wheel.
// Verify the on-disk race.bin SHA-256 before attaching; no target memory is
// written by this script.
const engine = Process.findModuleByName("race.bin");
if (!engine) throw new Error("race.bin module not found");

const at = va => engine.base.add(va - 0x400000);
const maxSelectedIntegrators = 4;
const maxRows = 96;
let player = null;
let selectedIntegrators = 0;
let selectedWheelCalls = 0;
let selectedForceWriterCalls = 0;
const active = new Map();
const rows = [];
const listeners = [];
const threadId = () => Process.getCurrentThreadId();
const safe = read => { try { return read(); } catch (_) { return null; } };
const vector3 = pointer => safe(() => [
  pointer.readFloat(), pointer.add(4).readFloat(), pointer.add(8).readFloat()
]);

listeners.push(Interceptor.attach(at(0x4384d0), {
  onEnter() {
    const object = this.context.ecx;
    if (object && !object.isNull()) player = object;
  }
}));

listeners.push(Interceptor.attach(at(0x431150), {
  onEnter() {
    const object = this.context.ecx;
    if (!player || !object.equals(player) ||
        selectedIntegrators >= maxSelectedIntegrators) return;
    selectedIntegrators++;
    active.set(threadId(), {
      serial: selectedIntegrators,
      object,
      nextWheel: 0,
      currentWheel: null,
      lastWheelContext: null
    });
  },
  onLeave() { active.delete(threadId()); }
}));

listeners.push(Interceptor.attach(at(0x442530), {
  onEnter() {
    const state = active.get(threadId());
    if (!state) return;
    const index = state.nextWheel++;
    selectedWheelCalls++;
    state.currentWheel = index;
    state.lastWheelContext = {
      index,
      pointer: this.context.ecx.toString(),
      esi: this.context.esi.toString(),
      edi: this.context.edi.toString()
    };
  },
  onLeave() {
    const state = active.get(threadId());
    if (state) state.currentWheel = null;
  }
}));

listeners.push(Interceptor.attach(at(0x43e6f0), {
  onEnter(args) {
    const state = active.get(threadId());
    if (!state || rows.length >= maxRows) return;
    selectedForceWriterCalls++;
    const body = this.context.ecx;
    const call = {
      integrator: state.serial,
      wheel: state.currentWheel,
      wheelContext: state.currentWheel === null ? null : state.lastWheelContext,
      returnAddress: this.returnAddress.toString(),
      body: body.toString(),
      force: vector3(args[0]),
      point: vector3(args[1]),
      center: vector3(body.add(0x5c)),
      beforeForce: vector3(body.add(0x24c)),
      beforeMoment: vector3(body.add(0x258)),
      backtrace: []
    };
    if (state.currentWheel !== null && call.returnAddress === "0x43113f") {
      call.backtrace = safe(() => Thread.backtrace(
        this.context, Backtracer.ACCURATE).slice(0, 10)
          .map(address => address.toString())) || [];
    }
    this.call = call;
  },
  onLeave() {
    if (!this.call) return;
    this.call.afterForce = vector3(this.call.body.add(0x24c));
    this.call.afterMoment = vector3(this.call.body.add(0x258));
    this.call.body = this.call.body.toString();
    rows.push(this.call);
  }
}));

send({
  event: "pcar_wheel_force_callsite_probe_ready",
  module: engine.path,
  base: engine.base.toString(),
  durationMs: 3000,
  maxSelectedIntegrators,
  maxRows
});

setTimeout(() => {
  for (const listener of listeners) listener.detach();
  send({
    event: "pcar_wheel_force_callsite_probe_done",
    player: player ? player.toString() : null,
    selectedIntegrators,
    selectedWheelCalls,
    selectedForceWriterCalls,
    rows
  });
}, 3000);
