// Bounded, read-only trace of the two PCAR timer branch boundaries.
// Load into an already-running race.bin process; this script never writes to
// target memory and does not launch or resume the game.
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
const safe = fn => { try { return fn(); } catch (_) { return null; } };
const readFloat = (pointer, offset = 0) => safe(() => pointer.add(offset).readFloat());
const readU32 = (pointer, offset = 0) => safe(() => pointer.add(offset).readU32());
const maxRows = 16;
const captureMs = 120000;
// If the first callback is not the player PCAR, set this to the observed body
// pointer before loading the script. Null selects the first readable PCAR.
const targetOverride = null;
let rows = [];
let nextSerial = 0;
let finished = false;
let finishTimer = null;
const active = new Map();
const listeners = [];
const threadId = () => Process.getCurrentThreadId();

function checkedPointer(value) {
  const candidate = ptr(value);
  if (candidate.isNull() || !Process.findRangeByAddress(candidate)) {
    throw new RangeError("target override is null or unreadable");
  }
  return candidate;
}
let target = targetOverride === null ? null : checkedPointer(targetOverride);

function finish(reason) {
  if (finished) return;
  finished = true;
  if (finishTimer !== null) clearTimeout(finishTimer);
  for (const listener of listeners) listener.detach();
  send({
    event: "pcar_timer_branch_probe_done",
    reason,
    target: target ? target.toString() : null,
    rows
  });
}

listeners.push(Interceptor.attach(at(0x4384d0), {
  onEnter() {
    if (target) return;
    const object = this.context.ecx;
    if (object && !object.isNull() && Process.findRangeByAddress(object)) target = object;
  }
}));

// At 0x43358D, FNSTSW at 0x43358B has stored the x87 compare status in AX,
// and TEST at 0x43358D has not executed yet. FSTP at 0x433587 stored the
// computed elapsed value at [ESP+4]. ESI is the body from 0x433574.
listeners.push(Interceptor.attach(at(0x43358d), {
  onEnter() {
    const body = this.context.esi;
    if (!target || !body.equals(target) || rows.length >= maxRows) return;
    const row = {
      call: ++nextSerial,
      body: body.toString(),
      nativeTick: safe(() => readU32(at(0x508774))),
      rampTime_0xe44: readFloat(body, 0xe44),
      rampValue_0x50c: readFloat(body, 0x50c),
      ordinal_0x510: readU32(body, 0x510),
      elapsedAtEspPlus4: safe(() => this.context.esp.add(4).readFloat()),
      x87StatusAx: this.context.eax.toUInt32() & 0xffff,
      pairLookupReached: false,
      pairRecord: null,
      pairComponent_0x4: null,
      pairComponent_0x8: null
    };
    active.set(threadId(), row);
    rows.push(row);
  }
}));

// 0x4335E1 has loaded the ordinal-indexed pair-row pointer into EAX;
// 0x4335E5 tests it. This site is reached only after the elapsed-time path
// passes the 6.2-second gate. Capture the pointer and its two consumed fields
// without dereferencing invalid/unmapped memory.
listeners.push(Interceptor.attach(at(0x4335e5), {
  onEnter() {
    const row = active.get(threadId());
    if (!row || !this.context.esi.equals(target)) return;
    row.pairLookupReached = true;
    const pair = this.context.eax;
    row.pairRecord = pair.isNull() ? "0x0" : pair.toString();
    if (pair.isNull() || !Process.findRangeByAddress(pair)) return;
    row.pairComponent_0x4 = readFloat(pair, 4);
    row.pairComponent_0x8 = readFloat(pair, 8);
  }
}));

// Return hook only bounds/completes each captured invocation; it does not
// infer that a skipped 0x4335E5 means any particular upstream condition.
listeners.push(Interceptor.attach(at(0x433570), {
  onLeave() {
    const row = active.get(threadId());
    if (!row) return;
    row.rampTimeAfter_0xe44 = readFloat(ptr(row.body), 0xe44);
    row.rampValueAfter_0x50c = readFloat(ptr(row.body), 0x50c);
    active.delete(threadId());
    if (rows.length >= maxRows) finish("row_limit");
  }
}));

send({
  event: "pcar_timer_branch_probe_ready",
  module: engine.path,
  base: engine.base.toString(),
  captureMs,
  maxRows,
  targetOverride: targetOverride === null ? null : ptr(targetOverride).toString(),
  staticTarget: "race.bin SHA-256 f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56"
});
finishTimer = setTimeout(() => finish("timeout"), captureMs);
