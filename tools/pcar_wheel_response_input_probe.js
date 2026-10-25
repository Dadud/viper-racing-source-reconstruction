// Read-only, bounded follow-up to pcar_wheel_force_callsite_probe.js.
// Records selected wheel response fields and caller-stack bytes at the native
// force writers so future traces can identify the producer inputs. It never
// writes to the game process.
const engine = Process.findModuleByName("race.bin");
if (!engine) throw new Error("race.bin module not found");

const imageBase = 0x400000;
const imageEnd = imageBase + engine.size;
const at = va => {
  if (!Number.isInteger(va) || va < imageBase || va >= imageEnd) {
    throw new RangeError(`Address 0x${Number(va).toString(16)} is outside race.bin`);
  }
  return engine.base.add(va - imageBase);
};
const selectedLimit = 8;
const rowLimit = 128;
const stageLimit = 192;
// Keep the hook armed through menu navigation so it can capture the first
// eight selected-PCAR integrators after the player enters a race.
const captureMs = 120000;
let player = null;
let selectedIntegrators = 0;
let selectedWheelCalls = 0;
const active = new Map();
const rows = [];
const stageRows = [];
const listeners = [];
let completed = false;
let finishTimer = null;
const threadId = () => Process.getCurrentThreadId();
const safe = fn => { try { return fn(); } catch (_) { return null; } };
const bytesHex = (pointer, size) => safe(() => {
  const bytes = new Uint8Array(pointer.readByteArray(size));
  return Array.from(bytes, value => value.toString(16).padStart(2, "0")).join("");
});
const readFloat = (pointer, offset) => safe(() => pointer.add(offset).readFloat());
const readU32 = (pointer, offset) => safe(() => pointer.add(offset).readU32());
const readByte = (pointer, offset) => safe(() => pointer.add(offset).readU8());
const vector3 = (pointer, offset = 0) => safe(() => [
  pointer.add(offset).readFloat(),
  pointer.add(offset + 4).readFloat(),
  pointer.add(offset + 8).readFloat()
]);
const finish = () => {
  if (completed) return;
  completed = true;
  if (finishTimer !== null) clearTimeout(finishTimer);
  for (const listener of listeners) listener.detach();
  send({
    event: "pcar_wheel_response_input_probe_done",
    player: player ? player.toString() : null,
    selectedIntegrators,
    selectedWheelCalls,
    rows,
    stageRows
  });
};

listeners.push(Interceptor.attach(at(0x4384d0), {
  onEnter() {
    const object = this.context.ecx;
    if (object && !object.isNull()) player = object;
  }
}));

listeners.push(Interceptor.attach(at(0x431150), {
  onEnter() {
    const object = this.context.ecx;
    if (!player || !object.equals(player) || selectedIntegrators >= selectedLimit) return;
    active.set(threadId(), { serial: ++selectedIntegrators, object, nextWheel: 0, wheel: null });
  },
  onLeave() {
    const id = threadId();
    const state = active.get(id);
    if (!state) return;
    active.delete(id);
    if (state.serial === selectedLimit) finish();
  }
}));

listeners.push(Interceptor.attach(at(0x442530), {
  onEnter() {
    const state = active.get(threadId());
    if (!state) return;
    const index = state.nextWheel++;
    selectedWheelCalls++;
    const wheel = this.context.ecx;
    const body = state.object;
    state.wheel = { index, pointer: wheel };
    this.edge = {
      stage: "wheel-history-body-copy",
      integrator: state.serial,
      wheelIndex: index,
      wheel: wheel.toString(),
      body: body.toString(),
      nativeTick: readU32(at(0x508774)),
      bodyRamp_0x50c_before: readFloat(body, 0x50c),
      bodyRecordTime_0xe44_before: readFloat(body, 0xe44),
      vehicleOrdinal_0x510: readU32(body, 0x510),
      wheelContext_0x158_before: readFloat(wheel, 0x158),
      wheelHistory_0xe8_before: readFloat(wheel, 0xe8),
      stackAtWheelEntry: bytesHex(this.context.esp, 0x40)
    };
  },
  onLeave() {
    const state = active.get(threadId());
    if (state) {
      if (this.edge && stageRows.length < stageLimit) {
        const body = state.object;
        const wheel = state.wheel.pointer;
        this.edge.bodyRamp_0x50c_after = readFloat(body, 0x50c);
        this.edge.bodyRecordTime_0xe44_after = readFloat(body, 0xe44);
        this.edge.wheelContext_0x158_after = readFloat(wheel, 0x158);
        this.edge.wheelHistory_0xe8_after = readFloat(wheel, 0xe8);
        stageRows.push(this.edge);
      }
      state.wheel = null;
    }
  }
}));

listeners.push(Interceptor.attach(at(0x43e6f0), {
  onEnter(args) {
    const state = active.get(threadId());
    if (!state || !state.wheel || rows.length >= rowLimit) return;
    const body = this.context.ecx;
    const wheel = state.wheel.pointer;
    const forcePtr = args[0];
    const pointPtr = args[1];
    const esp = this.context.esp;
    this.row = {
      integrator: state.serial,
      wheelIndex: state.wheel.index,
      wheel: wheel.toString(),
      body: body.toString(),
      returnAddress: this.returnAddress.toString(),
      force: vector3(forcePtr),
      point: vector3(pointPtr),
      center: vector3(body, 0x5c),
      accumulatorForceBefore: vector3(body, 0x24c),
      accumulatorMomentBefore: vector3(body, 0x258),
      wheelFields: {
        transformedApplicationPoint_0xc8: vector3(wheel, 0xc8),
        priorContactPoint_0x130: vector3(wheel, 0x130),
        tireVector_0xf4: vector3(wheel, 0xf4),
        tireVector_0x100: vector3(wheel, 0x100),
        animationBasis_0x110: vector3(wheel, 0x110),
        responseScalar_0x7c: readFloat(wheel, 0x7c),
        suspensionDepth_0xe8: readFloat(wheel, 0xe8),
        suspensionForce_0x178: readFloat(wheel, 0x178),
        wheelRate_0x124: readFloat(wheel, 0x124),
        contactFlag_0x168: readByte(wheel, 0x168),
        rawObject: bytesHex(wheel, 0x1a4)
      },
      bodyFields: {
        basis_0x50: vector3(body, 0x50),
        velocity_0x68: vector3(body, 0x68),
        angularVelocity_0x78: vector3(body, 0x78)
      },
      callerStackAtWriterEntry: bytesHex(esp, 0x80)
    };
  },
  onLeave() {
    if (!this.row) return;
    this.row.accumulatorForceAfter = vector3(ptr(this.row.body).add(0x24c));
    this.row.accumulatorMomentAfter = vector3(ptr(this.row.body).add(0x258));
    rows.push(this.row);
  }
}));

// At this exact point the finalized suspension magnitude is clamped and
// stored to the wheel before 0x443F60 runs.
listeners.push(Interceptor.attach(at(0x4430a0), {
  onEnter() {
    const state = active.get(threadId());
    if (!state || !state.wheel || stageRows.length >= stageLimit) return;
    const esp = this.context.esp;
    const wheel = this.context.edi;
    stageRows.push({
      stage: "suspension-force-inputs",
      integrator: state.serial,
      wheelIndex: state.wheel.index,
      wheel: wheel.toString(),
      body: this.context.esi.toString(),
      forceMagnitudeLocal_0x20: safe(() => esp.add(0x20).readFloat()),
      contactDepthLocal_0x1c: safe(() => esp.add(0x1c).readFloat()),
      wheelFields: {
        depth_0xe8: readFloat(wheel, 0xe8),
        computedForce_0x178: readFloat(wheel, 0x178),
        object: bytesHex(wheel, 0x1a4)
      },
      stack: bytesHex(esp, 0xa0)
    });
  }
}));

// Capture the exact x87 multiplication operands. The third component reads
// [esp+0x74] after the two wrapper arguments have been pushed, so its matching
// magnitude operand is [esp+0x28] at that same instruction.
for (const site of [
  { address: 0x4430b2, stage: "suspension-force-x", directionOffset: 0x64, magnitudeOffset: 0x20 },
  { address: 0x4430be, stage: "suspension-force-y", directionOffset: 0x68, magnitudeOffset: 0x20 },
  { address: 0x4430d8, stage: "suspension-force-z", directionOffset: 0x74, magnitudeOffset: 0x28 }
]) {
  listeners.push(Interceptor.attach(at(site.address), {
    onEnter() {
      const state = active.get(threadId());
      if (!state || !state.wheel || stageRows.length >= stageLimit) return;
      const esp = this.context.esp;
      const wheel = this.context.edi;
      stageRows.push({
        stage: site.stage,
        instruction: "0x" + site.address.toString(16),
        integrator: state.serial,
        wheelIndex: state.wheel.index,
        wheel: wheel.toString(),
        stackPointer: esp.toString(),
        directionOperand: safe(() => esp.add(site.directionOffset).readFloat()),
        magnitudeOperand: safe(() => esp.add(site.magnitudeOffset).readFloat()),
        wheelResponse_0x178: readFloat(wheel, 0x178),
        stack: bytesHex(esp, 0xa0)
      });
    }
  }));
}

// At the tire-force writer call, +0x100 is the vector after the final scalar
// projection and +0xC8 remains the transformed application point.
listeners.push(Interceptor.attach(at(0x443c04), {
  onEnter() {
    const state = active.get(threadId());
    if (!state || !state.wheel || stageRows.length >= stageLimit) return;
    const wheel = this.context.edi;
    const esp = this.context.esp;
    stageRows.push({
      stage: "tire-force-callsite",
      integrator: state.serial,
      wheelIndex: state.wheel.index,
      wheel: wheel.toString(),
      body: this.context.esi.toString(),
      tireVector_0x100: vector3(wheel, 0x100),
      responseVector_0xf4: vector3(wheel, 0xf4),
      applicationPoint_0xc8: vector3(wheel, 0xc8),
      responseScalar_0x7c: readFloat(wheel, 0x7c),
      contactFlag_0x168: readByte(wheel, 0x168),
      stack: bytesHex(esp, 0x80)
    });
  }
}));

send({
  event: "pcar_wheel_response_input_probe_ready",
  module: engine.path,
  base: engine.base.toString(),
  captureMs,
  selectedLimit,
  rowLimit
});

finishTimer = setTimeout(finish, captureMs);
