// Host-specific startup workaround for the verified ISO race.bin.
// Change only the first successful local video-memory total query. This runs
// before the original process resumes and does not modify game files.
const engine = Process.findModuleByName("race.bin");
if (!engine) throw new Error("race.bin is not mapped");
if (Process.arch !== "ia32") throw new Error("expected 32-bit x86 race.bin");
const at = va => engine.base.add(va - 0x400000);
const readU32 = pointer => {
  try { return pointer.readU32(); } catch (_) { return null; }
};
let queries = 0;
let applied = false;
Interceptor.attach(at(0x458ce0), {
  onEnter() {
    queries++;
    try {
      const stack = this.context.esp;
      this.capsPointer = stack.add(4).readPointer();
      this.totalPointer = stack.add(8).readPointer();
      this.freePointer = stack.add(12).readPointer();
      this.caps = readU32(this.capsPointer);
    } catch (error) {
      this.readError = String(error);
    }
  },
  onLeave() {
    try {
      const status = readU32(engine.base.add(0x109fc4));
      const totalBefore = readU32(this.totalPointer);
      const free = readU32(this.freePointer);
      if (!applied && this.caps === 0x4000 && status === 0 &&
          this.totalPointer && !this.totalPointer.isNull()) {
        this.totalPointer.writeU32(0x00200000);
        applied = true;
        send({
          event: "startup_vram_override",
          status: "applied",
          query: queries,
          caps: this.caps,
          hresult: status,
          total_before: totalBefore,
          total_after: 0x00200000,
          free,
          module_base: engine.base.toString(),
        });
        Interceptor.detachAll();
      } else if (queries <= 3) {
        send({
          event: "startup_vram_override",
          status: "query_only",
          query: queries,
          caps: this.caps,
          hresult: status,
          total: totalBefore,
          free,
          read_error: this.readError || null,
        });
      }
    } catch (error) {
      send({ event: "startup_vram_override", status: "hook_error", error: String(error) });
      Interceptor.detachAll();
    }
  },
});
send({
  event: "startup_vram_override",
  status: "hook_ready",
  module_base: engine.base.toString(),
  query_va: "0x458CE0",
});
