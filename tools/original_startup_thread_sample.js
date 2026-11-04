// Bounded passive sample of race.bin's launcher-created main thread.
// Records the thread's instruction pointer and stack pointer at 10 Hz for up
// to 10 seconds; it does not change registers or single-step the target.

if (Process.arch !== "ia32" || Process.pointerSize !== 4)
  throw new Error("expected the 32-bit Viper Racing engine");

const requestedTid = Number("__TARGET_TID__");
const maximumSamples = 100;
let ordinal = 0;
let unavailable = 0;
let timer = null;

function sampleThread() {
  if (ordinal >= maximumSamples) {
    if (timer !== null) clearInterval(timer);
    send({ event: "startup_thread_sample_done", samples: ordinal,
      unavailable });
    return;
  }
  ordinal++;
  try {
    const threads = Process.enumerateThreads();
    let thread = requestedTid !== 0
      ? threads.find(candidate => candidate.id === requestedTid)
      : null;
    if (!thread) {
      thread = threads.find(candidate => candidate.state === "running") ||
        threads[0] || null;
    }
    if (!thread) throw new Error("no target thread is visible");
    const context = thread.context || {};
    const pc = context.eip || context.pc;
    const sp = context.esp || context.sp;
    if (!pc || !sp) throw new Error("thread context has no x86 PC/SP");
    const module = Process.findModuleByAddress(pc);
    send({ event: "startup_thread_sample", ordinal,
      target_tid: requestedTid || thread.id,
      state: thread.state,
      pc: pc.toString(),
      sp: sp.toString(),
      module: module ? module.name : null,
      module_offset: module ? pc.sub(module.base).toString() : null });
  } catch (error) {
    unavailable++;
    if (unavailable <= 5) {
      send({ event: "startup_thread_sample_unavailable", ordinal,
        error: String(error) });
    }
  }
}

send({ event: "startup_thread_sampler_ready", target_tid: requestedTid,
  interval_ms: 100, maximum_samples: maximumSamples });
timer = setInterval(sampleThread, 100);
