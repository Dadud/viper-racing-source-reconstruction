// Bounded Frida capture for the original 32-bit race.bin client.
// Applies short synthetic throttle/brake windows only when the game's
// resolved binding record identifies a keyboard input (type 1).
// This is a controlled input intervention, not physical keyboard delivery.
const engine = Process.findModuleByName("race.bin");
if (!engine) throw new Error("race.bin is not mapped");
if (engine.size < 0x1c2000) throw new Error("race.bin image is smaller than expected");

const at = va => engine.base.add(va - 0x400000);
const safe = fn => { try { return fn(); } catch (_) { return null; } };
const readFloat = (p, o) => safe(() => p.add(o).readFloat());
const readU8 = (p, o) => safe(() => p.add(o).readU8());
const readU32 = (p, o) => safe(() => p.add(o).readU32());
const readPointer = (p, o) => safe(() => p.add(o).readPointer().toString());
const vector = (p, o) => [readFloat(p, o), readFloat(p, o + 4), readFloat(p, o + 8)];
const snapshotPairBlock = (body, offset, wheelEndpoints) => {
  const block = body.add(offset);
  const endpoint1 = safe(() => block.add(4).readPointer());
  const endpoint2 = safe(() => block.add(8).readPointer());
  return {
    address: block.toString(),
    endpoint1: endpoint1 && !endpoint1.isNull() ? endpoint1.toString() : null,
    endpoint2: endpoint2 && !endpoint2.isNull() ? endpoint2.toString() : null,
    endpoint1_e4: wheelEndpoints && endpoint1 && !endpoint1.isNull()
      ? readFloat(endpoint1, 0xe4) : null,
    endpoint2_e4: wheelEndpoints && endpoint2 && !endpoint2.isNull()
      ? readFloat(endpoint2, 0xe4) : null,
    scale_18: readFloat(block, 0x18),
    interval_1c: readFloat(block, 0x1c),
  };
};
const snapshotBody = body => ({
  transform: safe(() => Array.from(new Uint8Array(body.add(0x38).readByteArray(0x30)))),
  position: vector(body, 0x5c),
  linearVelocity: vector(body, 0x234),
  angularVelocity: vector(body, 0x240),
  force: vector(body, 0x24c),
  moment: vector(body, 0x258),
  pcar: {
    source_5b0: readFloat(body, 0x5b0),
    source_8f8: readFloat(body, 0x8f8),
    selector_dc4: readFloat(body, 0xdc4),
    multiplier_dd0: readFloat(body, 0xdd0),
    integrator_product_ddc: readFloat(body, 0xddc),
    throttle_de0: readFloat(body, 0xde0),
    brake_de4: readFloat(body, 0xde4),
    clutch_de8: readFloat(body, 0xde8),
    selector_dec: readFloat(body, 0xdec),
    controller_c98: readPointer(body, 0xc98),
    controller_vtable: safe(() => body.add(0xc98).readPointer().toString()),
    selected_block: readPointer(safe(() => body.add(0xc98).readPointer()) || ptr(0), 0x4c),
    pair_d04: snapshotPairBlock(body, 0xd04, false),
    pair_d24: snapshotPairBlock(body, 0xd24, true),
    pair_d44: snapshotPairBlock(body, 0xd44, true),
  },
});
const snapshotWheel = wheel => ({
  spin_target_e4: readFloat(wheel, 0xe4),
  angular_rate_124: readFloat(wheel, 0x124),
  derived_rate_128: readFloat(wheel, 0x128),
  radius_38: readFloat(wheel, 0x38),
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
let t0 = null;
let phaseWindows = [];
let stopAt = null;
let phases = {};
let phaseSamples = {};
let controlSamplesByPhase = {};
const active = new Map();
let selectedPcar = null;
let callbackCount = 0;
let integratorCount = 0;
let wheelRows = 0;
const sampledControlRanges = {
  throttle: { minimum: Number.POSITIVE_INFINITY, maximum: Number.NEGATIVE_INFINITY },
  brake: { minimum: Number.POSITIVE_INFINITY, maximum: Number.NEGATIVE_INFINITY },
};
const keyStats = {
  throttle: { reads: 0, patched: 0, binding_type: null, selector: null },
  brake: { reads: 0, patched: 0, binding_type: null, selector: null },
};
const controls = {
  steering: at(0x509358),
  throttle: at(0x5093f4),
  brake: at(0x509320),
};
const bindingRecords = {
  throttle: at(0x509328),
  brake: at(0x509368),
};
const phaseAt = t => t0 === null ? null :
  (phaseWindows.find(window => t >= window.start && t < window.end) || null);
let finishTimer = null;
let attachTimeout = null;
let finished = false;
let captureStarted = false;
const beginCapture = () => {
  if (captureStarted || finished) return;
  captureStarted = true;
  if (attachTimeout !== null) clearTimeout(attachTimeout);
  t0 = now();
  phaseWindows = [
    { phase: "neutral_before", start: t0 + 1000, end: t0 + 2500, control: null },
    { phase: "throttle", start: t0 + 2500, end: t0 + 4000, control: "throttle" },
    { phase: "neutral_mid", start: t0 + 4000, end: t0 + 5500, control: null },
    { phase: "brake", start: t0 + 5500, end: t0 + 7000, control: "brake" },
    { phase: "neutral_after", start: t0 + 7000, end: t0 + 8500, control: null },
  ];
  stopAt = phaseWindows[phaseWindows.length - 1].end + 1000;
  phases = Object.fromEntries(phaseWindows.map(window => [window.phase, 0]));
  phaseSamples = Object.fromEntries(phaseWindows.map(window => [window.phase, 0]));
  controlSamplesByPhase = Object.fromEntries(phaseWindows.map(window => [
    window.phase,
    { throttle: { sum: 0, count: 0 }, brake: { sum: 0, count: 0 } },
  ]));
  send({
    event: "controlled_pcar_capture_armed",
    body: selectedPcar.toString(),
    phaseWindowsMs: phaseWindows.map(window => ({
      phase: window.phase,
      start: window.start - t0,
      end: window.end - t0,
      control: window.control,
    })),
    syntheticInput: true,
    routeOrIdealLineInput: false,
  });
  finishTimer = setTimeout(finishCapture, stopAt - now());
};

const keyListener = Interceptor.attach(at(0x413170), {
  onEnter(args) { this.selector = args[0].toUInt32(); },
  onLeave(retval) {
    const t = now();
    const phase = phaseAt(t);
    if (!phase || !phase.control) return;
    const stats = keyStats[phase.control];
    const binding = bindingRecords[phase.control];
    const bindingType = readU32(binding, 0);
    const selector = bindingType === 1 ? readU8(binding, 4) : null;
    stats.binding_type = bindingType;
    stats.selector = selector;
    if (selector !== this.selector) return;
    stats.reads++;
    if (bindingType === 1 && retval.toInt32() === 0) {
      retval.replace(1);
      stats.patched++;
    }
  }
});

const callbackListener = Interceptor.attach(at(0x4384d0), {
  onEnter() {
    const car = this.context.ecx;
    if (!car || car.isNull() || safe(() => car.readU32()) !== 0x4c8188) return;
    if (selectedPcar !== null) return;
    selectedPcar = car;
    callbackCount++;
    beginCapture();
    send({
      event: "pcar_selected_player",
      body: car.toString(),
      ordinal_0x510: readU32(car, 0x510),
    });
  }
});

const integratorListener = Interceptor.attach(at(0x431150), {
  onEnter() {
    const body = this.context.ecx;
    if (!selectedPcar || !body.equals(selectedPcar)) return;
    const t = now();
    const phaseWindow = phaseAt(t);
    if (!phaseWindow) return;
    const phase = phaseWindow.phase;
    phases[phase]++;
    integratorCount++;
    // Sample every sixth PCAR integrator call, with bounded coverage of all
    // five control phases. The wheel hook records four calls per sample.
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
      configuredBindings: Object.fromEntries(Object.entries(bindingRecords).map(
        ([name, record]) => [name, {
          record: record.toString(),
          type: readU32(record, 0),
          selector: readU8(record, 4),
        }])),
      before: snapshotBody(body),
      wheels: [],
    };
    for (const control of ["throttle", "brake"]) {
      const value = this.sample.controls[control];
      if (Number.isFinite(value)) {
        controlSamplesByPhase[phase][control].sum += value;
        controlSamplesByPhase[phase][control].count++;
        sampledControlRanges[control].minimum = Math.min(
          sampledControlRanges[control].minimum, value);
        sampledControlRanges[control].maximum = Math.max(
          sampledControlRanges[control].maximum, value);
      }
    }
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

function finishCapture(reason = null) {
  if (finished) return;
  finished = true;
  if (finishTimer !== null) clearTimeout(finishTimer);
  if (attachTimeout !== null) clearTimeout(attachTimeout);
  keyListener.detach();
  callbackListener.detach();
  integratorListener.detach();
  wheelListener.detach();
  const phaseCoverage = captureStarted &&
    Object.values(phaseSamples).every(count => count >= 4);
  const phaseMean = (phase, control) => {
    const sample = controlSamplesByPhase[phase][control];
    return sample.count > 0 ? sample.sum / sample.count : null;
  };
  const neutralMean = control => {
    if (!captureStarted) return null;
    const names = ["neutral_before", "neutral_mid", "neutral_after"];
    const means = names.map(name => phaseMean(name, control)).filter(value => value !== null);
    return means.length === names.length
      ? means.reduce((sum, value) => sum + value, 0) / means.length : null;
  };
  const neutralThrottle = neutralMean("throttle");
  const neutralBrake = neutralMean("brake");
  const throttleDuringThrottle = captureStarted ? phaseMean("throttle", "throttle") : null;
  const brakeDuringBrake = captureStarted ? phaseMean("brake", "brake") : null;
  const throttleObserved = keyStats.throttle.binding_type === 1 &&
    keyStats.throttle.reads > 0 && keyStats.throttle.patched > 0 &&
    throttleDuringThrottle !== null && neutralThrottle !== null &&
    throttleDuringThrottle > neutralThrottle + 1.0e-4;
  const brakeObserved = keyStats.brake.binding_type === 1 &&
    keyStats.brake.reads > 0 && keyStats.brake.patched > 0 &&
    brakeDuringBrake !== null && neutralBrake !== null &&
    brakeDuringBrake > neutralBrake + 1.0e-4;
  const captureValid = phaseCoverage && throttleObserved && brakeObserved && wheelRows >= 20;
  send({
    event: "controlled_pcar_capture_done",
    captureValid,
    invalidReasons: [
      ...(reason ? [reason] : []),
      ...(keyStats.throttle.binding_type !== 1 ? ["throttle_binding_not_keyboard_type_1"] : []),
      ...(keyStats.throttle.reads === 0 ? ["throttle_key_wrapper_not_reached"] : []),
      ...(keyStats.throttle.patched === 0 ? ["no_throttle_key_return_patched"] : []),
      ...(!throttleObserved && keyStats.throttle.patched > 0 ? ["throttle_response_not_observed"] : []),
      ...(keyStats.brake.binding_type !== 1 ? ["brake_binding_not_keyboard_type_1"] : []),
      ...(keyStats.brake.reads === 0 ? ["brake_key_wrapper_not_reached"] : []),
      ...(keyStats.brake.patched === 0 ? ["no_brake_key_return_patched"] : []),
      ...(!brakeObserved && keyStats.brake.patched > 0 ? ["brake_response_not_observed"] : []),
      ...(!phaseCoverage ? ["insufficient_phase_sample_coverage"] : []),
      ...(wheelRows < 20 ? ["insufficient_wheel_rows"] : []),
    ],
    elapsed_ms: t0 === null ? null : now() - t0,
    pcarCallbacks: callbackCount,
    pcarIntegrators: integratorCount,
    sampledIntegratorsByPhase: phaseSamples,
    capturedWheelRows: wheelRows,
    keyStats,
    configuredBindings: Object.fromEntries(Object.entries(bindingRecords).map(
      ([name, record]) => [name, {
        type: readU32(record, 0),
        selector: readU8(record, 4),
      }])),
    sampledControlRanges: Object.fromEntries(Object.entries(sampledControlRanges).map(
      ([name, range]) => [name, Number.isFinite(range.minimum)
        ? [range.minimum, range.maximum] : null])),
    meanControlByPhase: Object.fromEntries(phaseWindows.map(window => [window.phase, {
      throttle: phaseMean(window.phase, "throttle"),
      brake: phaseMean(window.phase, "brake"),
    }])),
    controlResponseComparedToNeutral: {
      throttle: { active: throttleDuringThrottle, neutral: neutralThrottle },
      brake: { active: brakeDuringBrake, neutral: neutralBrake },
    },
    phaseWindowsMs: phaseWindows.map(window => ({
      phase: window.phase,
      start: window.start - t0,
      end: window.end - t0,
      control: window.control,
    })),
  });
}

send({
  event: "controlled_pcar_capture_ready",
  module: engine.name,
  base: engine.base.toString(),
  imageSize: engine.size,
  startCondition: "first_selected_pcar_callback_0x4384d0",
  attachTimeoutMs: 60000,
  sampleEveryPlayerIntegrators: 6,
  maximumSamplesPerPhase: 16,
  syntheticInput: true,
  routeOrIdealLineInput: false,
});
attachTimeout = setTimeout(() => finishCapture("selected_pcar_not_seen_within_timeout"), 60000);
