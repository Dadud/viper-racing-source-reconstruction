// Bounded Frida capture for the original 32-bit race.bin client.
// Applies a synthetic DIK_UP return-value window through the game's own
// 0x413170 key-read wrapper, then correlates PCAR callback controls with
// selected 0x431150 body updates and their four 0x442530 wheel calls.
// This is a controlled input intervention, not physical keyboard delivery.
const engine = Process.findModuleByName("race.bin");
if (!engine) throw new Error("race.bin is not mapped");
if (engine.size < 0x1c2000) throw new Error("race.bin image is smaller than expected");

const at = va => engine.base.add(va - 0x400000);
const safe = fn => { try { return fn(); } catch (_) { return null; } };
const readFloat = (p, o) => safe(() => p.add(o).readFloat());
const readU8 = (p, o) => safe(() => p.add(o).readU8());
const vector = (p, o) => [readFloat(p, o), readFloat(p, o + 4), readFloat(p, o + 8)];
const snapshotBody = body => ({
  transform: safe(() => Array.from(new Uint8Array(body.add(0x38).readByteArray(0x30)))),
  position: vector(body, 0x5c),
  linearVelocity: vector(body, 0x234),
  angularVelocity: vector(body, 0x240),
  force: vector(body, 0x24c),
  moment: vector(body, 0x258),
});
const snapshotWheel = wheel => ({
  queryOrigin: vector(wheel, 0xc8),
  contactPoint: vector(wheel, 0x130),
  contactActive: readU8(wheel, 0x168),
  feature: safe(() => wheel.add(0x19c).readS32()),
  depthE8: readFloat(wheel, 0xe8),
  paired158: readFloat(wheel, 0x158),
  responseF4: vector(wheel, 0xf4),
  tireVector100: vector(wheel, 0x100),
});
const thread = () => Process.getCurrentThreadId();
const now = () => Date.now();
const t0 = now();
const inputStart = t0 + 2000;
const inputEnd = inputStart + 1500;
const stopAt = t0 + 7000;
const phases = { coast_before: 0, throttle: 0, coast_after: 0 };
const phaseSamples = { coast_before: 0, throttle: 0, coast_after: 0 };
const active = new Map();
let selectedPcar = null;
let callbackCount = 0;
let integratorCount = 0;
let wheelRows = 0;
const keyStats = { selector: 0xc8, reads: 0, patched: 0 };
const controls = {
  steering: at(0x509358),
  throttle: at(0x5093f4),
  brake: at(0x509320),
};
const phaseAt = t => t < inputStart ? "coast_before" :
  (t < inputEnd ? "throttle" : "coast_after");

const keyListener = Interceptor.attach(at(0x413170), {
  onEnter(args) { this.selector = args[0].toUInt32(); },
  onLeave(retval) {
    if (this.selector !== keyStats.selector) return;
    keyStats.reads++;
    const t = now();
    if (t >= inputStart && t < inputEnd && retval.toInt32() === 0) {
      retval.replace(1);
      keyStats.patched++;
    }
  }
});

const callbackListener = Interceptor.attach(at(0x4384d0), {
  onEnter() {
    const car = this.context.ecx;
    if (!car || car.isNull()) return;
    selectedPcar = car;
    callbackCount++;
  }
});

const integratorListener = Interceptor.attach(at(0x431150), {
  onEnter() {
    const body = this.context.ecx;
    if (!selectedPcar || !body.equals(selectedPcar)) return;
    const t = now();
    const phase = phaseAt(t);
    phases[phase]++;
    integratorCount++;
    // Sample every sixth PCAR integrator call, with bounded coverage of all
    // three control phases. The wheel hook records the four calls per sample.
    const sample = phases[phase] % 6 === 1 && phaseSamples[phase] < 16;
    if (!sample) return;
    phaseSamples[phase]++;
    const serial = integratorCount;
    this.sample = {
      event: "pcar_integrator_sample",
      phase,
      elapsed_ms: t - t0,
      integrator: serial,
      callbackCount,
      body: body.toString(),
      controls: {
        steering: readFloat(controls.steering, 0),
        throttle: readFloat(controls.throttle, 0),
        brake: readFloat(controls.brake, 0),
      },
      before: snapshotBody(body),
      wheels: [],
    };
    active.set(thread(), { body, sample: this.sample, nextWheel: 0 });
  },
  onLeave() {
    const state = active.get(thread());
    if (!state) return;
    state.sample.after = snapshotBody(state.body);
    state.sample.body_position_delta = state.sample.after.position.map(
      (value, index) => value - state.sample.before.position[index]);
    state.sample.linear_velocity_delta = state.sample.after.linearVelocity.map(
      (value, index) => value - state.sample.before.linearVelocity[index]);
    send(state.sample);
    active.delete(thread());
  }
});

const wheelListener = Interceptor.attach(at(0x442530), {
  onEnter() {
    const state = active.get(thread());
    if (!state || wheelRows >= 256) return;
    const wheel = this.context.ecx;
    this.state = state;
    this.wheel = wheel;
    this.row = {
      wheelIndex: state.nextWheel++,
      wheel: wheel.toString(),
      before: snapshotWheel(wheel),
    };
  },
  onLeave() {
    if (!this.row) return;
    this.row.after = snapshotWheel(this.wheel);
    this.state.sample.wheels.push(this.row);
    wheelRows++;
  }
});

send({
  event: "controlled_pcar_capture_ready",
  module: engine.name,
  base: engine.base.toString(),
  imageSize: engine.size,
  keySelector: keyStats.selector,
  throttleWindowMs: [inputStart - t0, inputEnd - t0],
  sampleEveryPlayerIntegrators: 6,
  maximumSamplesPerPhase: 16,
  stopAfterMs: stopAt - t0,
  syntheticInput: true,
  routeOrIdealLineInput: false,
});

setTimeout(() => {
  keyListener.detach();
  callbackListener.detach();
  integratorListener.detach();
  wheelListener.detach();
  send({
    event: "controlled_pcar_capture_done",
    elapsed_ms: now() - t0,
    pcarCallbacks: callbackCount,
    pcarIntegrators: integratorCount,
    sampledIntegratorsByPhase: phaseSamples,
    capturedWheelRows: wheelRows,
    keyStats,
    inputWindowMs: [inputStart - t0, inputEnd - t0],
  });
}, stopAt - t0);
