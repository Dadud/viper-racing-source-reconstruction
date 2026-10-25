// Bounded read-only Stalker probe for the runtime writer of the actual PCAR
// wheel object's +0x158. This distinguishes that field from the separate
// 0x4329C0 frame-context row at the same offset. It follows one selected
// 0x431150 call and samples the four wheel fields before each native
// instruction, reporting the immediately preceding instruction as a candidate.
// No target memory is written. Stalker changes timing; use only as diagnostic
// evidence, never as a gameplay-parity capture.
const engine = Process.findModuleByName("race.bin");
if (!engine) throw new Error("race.bin module not found");

const imageBase = 0x400000;
const imageEnd = imageBase + engine.size;
const at = va => {
  if (!Number.isInteger(va) || va < imageBase || va >= imageEnd)
    throw new RangeError(`VA 0x${Number(va).toString(16)} is outside race.bin`);
  return engine.base.add(va - imageBase);
};
const instructionLimit = 500000;
const changeLimit = 64;
const timeoutMs = 120000;
let selectedPlayer = null;
let active = null;
let completed = false;
let finishTimer = null;
let selectedPlayerThread = null;
let selectedPlayerOrdinal = null;
const listeners = [];
const readField = pointer => {
  try {
    const bits = pointer.add(0x158).readU32();
    return { bits, value: pointer.add(0x158).readFloat() };
  } catch (_) {
    return null;
  }
};

function finish(reason) {
  if (completed) return;
  completed = true;
  if (finishTimer !== null) clearTimeout(finishTimer);
  if (active !== null) {
    try { Stalker.unfollow(active.threadId); } catch (_) {}
    try { Stalker.flush(); } catch (_) {}
  }
  for (const listener of listeners) listener.detach();
  send({
    event: "pcar_wheel_158_writer_probe_done",
    reason,
    player: selectedPlayer ? selectedPlayer.toString() : null,
    playerVtable: selectedPlayer ? "0x4c8188" : null,
    playerOrdinal_0x510: selectedPlayerOrdinal,
    playerSelectionThreadId: selectedPlayerThread,
    tracedThreadId: active ? active.threadId : null,
    integrator: active ? active.integrator : null,
    instructionCount: active ? active.instructionCount : 0,
    instructionBudgetExceeded: active ? active.instructionBudgetExceeded : false,
    wheelFieldsBefore: active ? active.before : [],
    wheelFieldsAfter: active ? active.after : [],
    changes: active ? active.changes : []
  });
}

function sampleFields(state, instructionAddress, instructionText) {
  for (let index = 0; index < state.wheels.length; index++) {
    const current = readField(state.wheels[index]);
    if (!current) continue;
    const prior = state.lastBits[index];
    if (prior !== null && prior !== current.bits && state.changes.length < changeLimit) {
      state.changes.push({
        wheelIndex: index,
        wheel: state.wheels[index].toString(),
        previousBits: "0x" + prior.toString(16).padStart(8, "0"),
        currentBits: "0x" + current.bits.toString(16).padStart(8, "0"),
        previousValue: state.lastValues[index],
        currentValue: current.value,
        candidatePrecedingAddress: state.previousInstructionAddress,
        candidatePrecedingInstruction: state.previousInstructionText,
        observedBeforeAddress: instructionAddress,
        observedBeforeInstruction: instructionText
      });
    }
    state.lastBits[index] = current.bits;
    state.lastValues[index] = current.value;
  }
}

listeners.push(Interceptor.attach(at(0x4384d0), {
  onEnter() {
    const object = this.context.ecx;
    if (object && !object.isNull() && selectedPlayer === null &&
        object.readU32() === 0x4c8188) {
      selectedPlayer = object;
      selectedPlayerThread = Process.getCurrentThreadId();
      try { selectedPlayerOrdinal = object.add(0x510).readU32(); } catch (_) {}
    }
  }
}));

listeners.push(Interceptor.attach(at(0x431150), {
  onEnter() {
    const object = this.context.ecx;
    if (completed || active !== null || selectedPlayer === null ||
        !object || !object.equals(selectedPlayer)) return;
    this.traceThisCall = true;
    const threadId = Process.getCurrentThreadId();
    const wheels = Array.from({ length: 4 }, (_, index) =>
      object.add(0x554 + index * 0x1a4));
    const before = wheels.map((wheel, index) => {
      const field = readField(wheel);
      return {
        wheelIndex: index,
        wheel: wheel.toString(),
        bits: field ? "0x" + field.bits.toString(16).padStart(8, "0") : null,
        value: field ? field.value : null
      };
    });
    const state = {
      threadId,
      integrator: 1,
      instructionCount: 0,
      instructionBudgetExceeded: false,
      wheels,
      before,
      after: [],
      changes: [],
      lastBits: before.map(row => row.bits === null ? null :
        Number.parseInt(row.bits.slice(2), 16) >>> 0),
      lastValues: before.map(row => row.value),
      previousInstructionAddress: null,
      previousInstructionText: null
    };
    active = state;
    Stalker.follow(threadId, {
      transform(iterator) {
        let instruction;
        while ((instruction = iterator.next()) !== null) {
          const address = instruction.address.toString();
          const instructionText = instruction.toString();
          iterator.putCallout(() => {
            if (active !== state || completed) return;
            state.instructionCount++;
            sampleFields(state, address, instructionText);
            state.previousInstructionAddress = address;
            state.previousInstructionText = instructionText;
            if (state.instructionCount >= instructionLimit &&
                !state.instructionBudgetExceeded) {
              state.instructionBudgetExceeded = true;
              try { Stalker.unfollow(threadId); } catch (_) {}
            }
          });
          iterator.keep();
        }
      }
    });
  },
  onLeave() {
    if (!this.traceThisCall || active === null) return;
    const state = active;
    state.after = state.wheels.map((wheel, index) => {
      const field = readField(wheel);
      return {
        wheelIndex: index,
        wheel: wheel.toString(),
        bits: field ? "0x" + field.bits.toString(16).padStart(8, "0") : null,
        value: field ? field.value : null
      };
    });
    sampleFields(state, "integrator-return", "0x431150 return");
    finish(state.instructionBudgetExceeded ? "instruction-budget-exceeded" :
      state.changes.length === 0 ? "one-integrator-no-field-change" : "field-change-captured");
  }
}));

finishTimer = setTimeout(() => finish(active !== null ? "timeout-active-integrator" :
  selectedPlayer !== null ? "timeout-no-integrator" : "timeout-no-pcar-callback"), timeoutMs);
