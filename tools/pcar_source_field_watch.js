// Exact field watch for the PCAR constructor inputs used by 0x434700.
// Attaches to an already-running 32-bit race.bin process; it does not launch
// the game or modify input. The 0x4381E0 constructor supplies the PCAR body
// directly, so both fields are watched through the constructor chain and then
// through the selected player's first race updates.
const engine = Process.findModuleByName("race.bin");
if (!engine) throw new Error("race.bin is not mapped");
if (Process.arch !== "ia32") throw new Error("expected 32-bit x86 race.bin");
const at = va => engine.base.add(va - 0x400000);
const fields = [0x5b0, 0x8f8];
const state = {
  selected: null,
  selectedOrdinal: null,
  callbackCandidates: new Map(),
  watchedThreads: new Map(),
  constructorRows: [],
  tickSamples: 0,
  writeEvents: 0,
  unclassifiedEvents: 0,
  errors: [],
};
const safe = fn => { try { return fn(); } catch (e) { return { error: String(e) }; } };
const readFloat = (body, offset) => safe(() => body.add(offset).readFloat());
const readValues = body => fields.map(offset => safe(() => body.add(offset).readFloat()));
const snapshotState = body => ({
  body: body.toString(),
  source_5b0: readFloat(body, 0x5b0),
  source_8f8: readFloat(body, 0x8f8),
  selector_dc4: readFloat(body, 0xdc4),
  multiplier_dd0: readFloat(body, 0xdd0),
  product_ddc: readFloat(body, 0xddc),
  throttle_de0: readFloat(body, 0xde0),
  brake_de4: readFloat(body, 0xde4),
  clutch_de8: readFloat(body, 0xde8),
  selector_dec: readFloat(body, 0xdec),
  controller_c98: safe(() => body.add(0xc98).readPointer().toString()),
  selected_block: safe(() => body.add(0xc98).readPointer().add(0x4c)
    .readPointer().toString()),
  controls: {
    steering: safe(() => at(0x509358).readFloat()),
    throttle: safe(() => at(0x5093f4).readFloat()),
    brake: safe(() => at(0x509320).readFloat()),
    clutch: safe(() => at(0x5093b8).readFloat()),
  },
  wheels: Array.from({ length: 4 }, (_, index) => {
    const wheel = body.add(0x554 + index * 0x1a4);
    return {
      address: wheel.toString(),
      spin_target_e4: readFloat(wheel, 0xe4),
      angular_rate_124: readFloat(wheel, 0x124),
      derived_rate_128: readFloat(wheel, 0x128),
    };
  }),
});
const formatAddress = address => {
  const module = Process.findModuleByAddress(address);
  return module ? `${module.name}+0x${address.sub(module.base).toString()}` : address.toString();
};
const armThread = (thread, body, purpose) => {
  if (state.watchedThreads.has(thread.id)) {
    const prior = state.watchedThreads.get(thread.id);
    if (prior.body.equals(body)) return true;
    fields.forEach((_, index) => safe(() => thread.unsetHardwareWatchpoint(index)));
    state.watchedThreads.delete(thread.id);
  }
  const last = readValues(body);
  const results = fields.map((offset, index) => safe(() => {
    thread.setHardwareWatchpoint(index, body.add(offset), 4, "w");
    return true;
  }));
  if (results.some(result => result !== true)) {
    state.errors.push(`watchpoint_arm_failed_${purpose}_thread_${thread.id}: ${JSON.stringify(results)}`);
    fields.forEach((_, index) => safe(() => thread.unsetHardwareWatchpoint(index)));
    return false;
  }
  state.watchedThreads.set(thread.id, { thread, body, last, purpose });
  return true;
};
const writeRow = (details, watched) => {
  const current = readValues(watched.body);
  const changedOffsets = fields.filter((_, index) =>
    JSON.stringify(current[index]) !== JSON.stringify(watched.last[index]));
  const instruction = safe(() => details.context.pc);
  const writerModule = instruction && instruction.toString
    ? safe(() => Process.findModuleByAddress(instruction)) : null;
  const memoryAddress = safe(() => details.memory.address);
  const memoryOperation = safe(() => details.memory.operation);
  const changedOffset = changedOffsets.length === 1 ? changedOffsets[0] : null;
  const expectedAddress = changedOffset === null
    ? null : watched.body.add(changedOffset);
  const memoryMatches = !details.memory ||
    (memoryOperation === "write" && changedOffset !== null && memoryAddress &&
      memoryAddress.equals(expectedAddress));
  const instructionMatches = writerModule && writerModule.name === engine.name;
  if (changedOffset === null || !memoryMatches || !instructionMatches) {
    state.unclassifiedEvents++;
    state.errors.push(`unclassified_watch_event_${state.unclassifiedEvents}`);
    send({
      event: "pcar_source_field_watch_event_unclassified",
      elapsed_ms: Date.now() - started,
      purpose: watched.purpose,
      exception_type: details.type,
      instruction: safe(() => details.context.pc.toString()),
      watched_address: safe(() => details.memory.address.toString()),
      memory_operation: safe(() => details.memory.operation),
      changed_offsets: changedOffsets,
      body: watched.body.toString(),
    });
    watched.last = current;
    return false;
  }
  state.writeEvents++;
  send({
    event: "pcar_source_field_write",
    elapsed_ms: Date.now() - started,
    purpose: watched.purpose,
    thread_id: Process.getCurrentThreadId(),
    exception_type: details.type,
    instruction: safe(() => details.context.pc.toString()),
    instruction_x86: safe(() => details.context.eip.toString()),
    watched_address: safe(() => details.memory.address.toString()),
    memory_operation: safe(() => details.memory.operation),
    body: watched.body.toString(),
    offsets: fields,
    target_offset: changedOffset,
    changed_offsets: changedOffsets,
    values: current,
    backtrace: safe(() => Thread.backtrace(details.context, Backtracer.FUZZY)
      .map(formatAddress)),
  });
  watched.last = current;
};
const started = Date.now();

Process.setExceptionHandler(details => {
  const threadId = Process.getCurrentThreadId();
  const watched = state.watchedThreads.get(threadId);
  if (!watched || !["breakpoint", "single-step"].includes(details.type)) return false;
  writeRow(details, watched);
  fields.forEach((_, index) => safe(() => watched.thread.unsetHardwareWatchpoint(index)));
  state.watchedThreads.delete(threadId);
  // Hardware watchpoints are one-shot in the exception handler. Re-arm after
  // clearing the debug event so subsequent field writes remain observable.
  armThread(watched.thread, watched.body, watched.purpose);
  return true;
});

const constructor = Interceptor.attach(at(0x4381e0), {
  onEnter() {
    this.body = this.context.ecx;
    this.threadId = Process.getCurrentThreadId();
    this.thread = Process.enumerateThreads().find(thread => thread.id === this.threadId);
    this.startValues = readValues(this.body);
    this.startState = snapshotState(this.body);
    if (!this.thread) {
      state.errors.push("constructor_thread_not_found");
      return;
    }
    this.watchInstalled = armThread(this.thread, this.body, "constructor");
    send({
      event: "pcar_constructor_watch_armed",
      body: this.body.toString(),
      thread_id: this.threadId,
      starting_values: this.startValues,
      starting_state: this.startState,
      watch_installed: this.watchInstalled,
    });
  },
  onLeave() {
    if (!this.body) return;
    const row = {
      body: this.body.toString(),
      thread_id: this.threadId,
      before: this.startValues,
      after: readValues(this.body),
      final_state: snapshotState(this.body),
      watch_installed: this.watchInstalled,
    };
    state.constructorRows.push(row);
    if (this.watchInstalled && state.watchedThreads.has(this.threadId)) {
      fields.forEach((_, index) => safe(() =>
        this.thread.unsetHardwareWatchpoint(index)));
      state.watchedThreads.delete(this.threadId);
    }
    send({ event: "pcar_constructor_watch_done", ...row });
  },
});

const callback = Interceptor.attach(at(0x4384d0), {
  onEnter() {
    const body = this.context.ecx;
    if (body.isNull() || safe(() => body.readU32()) !== 0x4c8188) return;
    state.callbackCandidates.set(body.toString(), {
      body,
      ordinal: safe(() => body.add(0x510).readU32()),
    });
    if (state.selected !== null) return;
    state.selected = body;
    state.selectedOrdinal = safe(() => body.add(0x510).readU32());
    send({
      event: "pcar_callback_candidate",
      body: body.toString(),
      ordinal_510: state.selectedOrdinal,
      selection_is_provisional: true,
      state: snapshotState(body),
    });
  },
});

const integrator = Interceptor.attach(at(0x431150), {
  onEnter() {
    if (state.selected === null || !this.context.ecx.equals(state.selected)) return;
    const threadId = Process.getCurrentThreadId();
    const thread = state.watchedThreads.get(threadId)?.thread || Process.enumerateThreads()
      .find(item => item.id === threadId);
    if (!thread) return;
    const watchInstalled = armThread(thread, state.selected, "selected_integrator");
    this.body = state.selected;
    this.before = readValues(this.body);
    this.beforeState = snapshotState(this.body);
    this.watchInstalled = watchInstalled;
    state.tickSamples++;
  },
  onLeave() {
    if (!this.body) return;
    send({
      event: "pcar_source_field_tick_sample",
      elapsed_ms: Date.now() - started,
      tick_sample: state.tickSamples,
      before: this.before,
      after: readValues(this.body),
      before_state: this.beforeState,
      after_state: snapshotState(this.body),
      body: this.body.toString(),
      watch_installed: this.watchInstalled,
    });
  },
});

send({
  event: "pcar_source_field_watch_ready",
  module: engine.name,
  image_size: engine.size,
  constructor: "0x4381E0",
  selected_callback: "0x4384D0",
  integrator: "0x431150",
  watched_fields: fields,
  duration_ms: 15000,
  route_or_ideal_line_input: false,
});

setTimeout(() => {
  for (const watched of state.watchedThreads.values()) {
    fields.forEach((_, index) => safe(() => watched.thread.unsetHardwareWatchpoint(index)));
  }
  state.watchedThreads.clear();
  constructor.detach();
  callback.detach();
  integrator.detach();
  Process.setExceptionHandler(null);
  const selectedBody = state.selected ? state.selected.toString() : null;
  const selectedConstructor = state.constructorRows.find(row =>
    row.body === selectedBody && row.watch_installed);
  const uniqueCandidate = state.callbackCandidates.size === 1;
  const captureValid = uniqueCandidate && selectedConstructor !== undefined &&
    state.tickSamples > 0 && state.errors.length === 0 &&
    state.unclassifiedEvents === 0;
  const invalidReasons = [];
  if (!uniqueCandidate) invalidReasons.push("player_callback_identity_not_unique");
  if (!selectedConstructor) invalidReasons.push("selected_constructor_watch_missing");
  if (state.tickSamples === 0) invalidReasons.push("selected_integrator_not_observed");
  if (state.errors.length > 0) invalidReasons.push("watch_errors_present");
  if (state.unclassifiedEvents > 0) invalidReasons.push("unclassified_watch_events_present");
  send({
    event: "pcar_source_field_watch_done",
    elapsed_ms: Date.now() - started,
    selected_body: selectedBody,
    selected_ordinal_510: state.selectedOrdinal,
    callback_candidate_count: state.callbackCandidates.size,
    callback_candidates: Array.from(state.callbackCandidates.values(), candidate => ({
      body: candidate.body.toString(),
      ordinal_510: candidate.ordinal,
    })),
    selected_constructor_matched: selectedConstructor !== undefined,
    constructor_rows: state.constructorRows,
    integrator_samples: state.tickSamples,
    write_events: state.writeEvents,
    unclassified_events: state.unclassifiedEvents,
    errors: state.errors,
    capture_valid: captureValid,
    invalid_reasons: invalidReasons,
    limitations: [
      "hardware watchpoints are per-thread; threads are armed at PCAR construction or integrator entry",
      "capture_valid is false unless exactly one PCAR callback candidate was observed",
      "a debugger/hypervisor restriction may prevent hardware watchpoint installation",
    ],
  });
}, 15000);

rpc.exports = {
  status() {
    return {
      selected_body: state.selected ? state.selected.toString() : null,
      selected_ordinal_510: state.selectedOrdinal,
      callback_candidate_count: state.callbackCandidates.size,
      callback_candidates: Array.from(state.callbackCandidates.values(), candidate => ({
        body: candidate.body.toString(),
        ordinal_510: candidate.ordinal,
      })),
      constructor_rows: state.constructorRows.length,
      integrator_samples: state.tickSamples,
      write_events: state.writeEvents,
      unclassified_events: state.unclassifiedEvents,
      errors: state.errors.slice(),
    };
  },
};
