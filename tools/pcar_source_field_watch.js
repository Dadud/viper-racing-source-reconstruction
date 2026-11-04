// Exact field watch for the PCAR constructor inputs used by 0x434700.
// Attaches to an already-running 32-bit race.bin process; it does not launch
// the game or modify input. The PCAR factory's 0xEF0 allocation is identified
// by its return site at 0x4226E0, then two per-thread hardware write watches
// remain armed through the first race updates.
const engine = Process.findModuleByName("race.bin");
if (!engine) throw new Error("race.bin is not mapped");
if (Process.arch !== "ia32") throw new Error("expected 32-bit x86 race.bin");
const at = va => engine.base.add(va - 0x400000);
const safe = fn => { try { return fn(); } catch (e) { return { error: String(e) }; } };
const fields = [0x5b0, 0x8f8];
const state = {
  body: null,
  watchedThreads: new Map(),
  initial: null,
  last: null,
  armed: false,
  writes: 0,
  samples: 0,
  errors: [],
};
const readValues = () => state.body ? fields.map(offset =>
  safe(() => state.body.add(offset).readFloat())) : null;
const formatAddress = address => {
  const module = Process.findModuleByAddress(address);
  return module ? `${module.name}+0x${address.sub(module.base).toString()}` : address.toString();
};
const armThread = thread => {
  if (state.watchedThreads.has(thread.id)) return true;
  const results = fields.map((offset, index) => safe(() => {
    thread.setHardwareWatchpoint(index, state.body.add(offset), 4, "w");
    return true;
  }));
  if (results.some(result => result !== true)) {
    state.errors.push(`watchpoint_arm_failed_thread_${thread.id}: ${JSON.stringify(results)}`);
    fields.forEach((_, index) => safe(() => thread.unsetHardwareWatchpoint(index)));
    return false;
  }
  state.watchedThreads.set(thread.id, thread);
  return true;
};
const writeRow = details => {
  const current = readValues();
  const changedOffsets = current ? fields.filter((_, index) =>
    JSON.stringify(current[index]) !== JSON.stringify(state.last[index])) : [];
  const row = {
    event: "pcar_source_field_write",
    elapsed_ms: Date.now() - started,
    thread_id: Process.getCurrentThreadId(),
    exception_type: details.type,
    instruction: safe(() => details.context.pc.toString()),
    instruction_x86: safe(() => details.context.eip.toString()),
    watched_address: safe(() => details.memory.address.toString()),
    memory_operation: safe(() => details.memory.operation),
    body: state.body.toString(),
    offsets: fields,
    changed_offsets: changedOffsets,
    values: current,
    backtrace: safe(() => Thread.backtrace(details.context, Backtracer.FUZZY)
      .map(formatAddress)),
  };
  state.last = current;
  state.writes++;
  send(row);
};
const started = Date.now();

Process.setExceptionHandler(details => {
  const threadId = Process.getCurrentThreadId();
  const thread = state.watchedThreads.get(threadId);
  if (!state.armed || !thread ||
      !["breakpoint", "single-step"].includes(details.type)) return false;
  writeRow(details);
  fields.forEach((_, index) => safe(() => thread.unsetHardwareWatchpoint(index)));
  state.watchedThreads.delete(threadId);
  if (!armThread(thread)) state.watchedThreads.delete(threadId);
  return true;
});

const allocator = Interceptor.attach(at(0x414170), {
  onEnter(args) {
    this.callSite = this.returnAddress;
    this.isPcarBody = this.returnAddress.equals(at(0x4226e0)) &&
      args[0].toUInt32() === 0xef0;
  },
  onLeave(retval) {
    if (!this.isPcarBody || retval.isNull() || state.body) return;
    state.body = retval;
    state.initial = readValues();
    state.last = state.initial;
    const thread = Process.enumerateThreads().find(
      thread => thread.id === Process.getCurrentThreadId());
    if (!thread) {
      state.errors.push("allocator_thread_not_found");
      return;
    }
    if (!armThread(thread)) return;
    state.armed = true;
    send({
      event: "pcar_source_field_watch_armed",
      body: state.body.toString(),
      thread_id: thread.id,
      initial_values: state.initial,
      watched_addresses: fields.map(offset => state.body.add(offset).toString()),
      allocator_return: this.callSite.toString(),
      write_watchpoints: fields.length,
    });
  },
});

const integrator = Interceptor.attach(at(0x431150), {
  onEnter() {
    if (!state.body || !this.context.ecx.equals(state.body)) return;
    const threadId = Process.getCurrentThreadId();
    const thread = state.watchedThreads.get(threadId) || Process.enumerateThreads().find(
      item => item.id === threadId);
    if (!thread) return;
    if (!state.armed) state.armed = true;
    if (!state.watchedThreads.has(threadId)) armThread(thread);
    this.body = state.body;
    this.before = readValues();
    state.samples++;
  },
  onLeave() {
    if (!this.body) return;
    send({
      event: "pcar_source_field_tick_sample",
      elapsed_ms: Date.now() - started,
      tick_sample: state.samples,
      before: this.before,
      after: readValues(),
      body: this.body.toString(),
    });
  },
});

send({
  event: "pcar_source_field_watch_ready",
  module: engine.name,
  image_size: engine.size,
  allocation_size: 0xef0,
  allocation_return_site: "0x4226E0",
  duration_ms: 15000,
  route_or_ideal_line_input: false,
});

setTimeout(() => {
  for (const thread of state.watchedThreads.values()) {
    fields.forEach((_, index) => safe(() => thread.unsetHardwareWatchpoint(index)));
  }
  state.armed = false;
  allocator.detach();
  integrator.detach();
  Process.setExceptionHandler(null);
  send({
    event: "pcar_source_field_watch_done",
    elapsed_ms: Date.now() - started,
    body: state.body ? state.body.toString() : null,
    initial_values: state.initial,
    final_values: readValues(),
    integrator_samples: state.samples,
    write_events: state.writes,
    watched_thread_ids: Array.from(state.watchedThreads.keys()),
    errors: state.errors,
    limitations: [
      "watchpoints are installed on the allocator thread and the selected PCAR integrator thread",
      "the allocation return-site match identifies a candidate PCAR by size and call site",
      "a debugger/hypervisor restriction may prevent hardware watchpoint installation",
    ],
  });
}, 15000);
