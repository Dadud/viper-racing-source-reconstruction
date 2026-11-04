// Passive startup trace for the original x86 DirectDraw initialization path.
// It records recovered function boundaries and termination/exception signals
// without changing returns, memory, display mode, or input.

if (Process.arch !== "ia32" || Process.pointerSize !== 4)
  throw new Error("expected the 32-bit Viper Racing engine");

const engine = Process.findModuleByName("race.bin");
if (!engine) throw new Error("race.bin is not mapped");
const at = va => engine.base.add(va - 0x400000);
const graphicsPoints = [
  0x44E080, // application graphics initialization
  0x44E460, // adapter selection
  0x44E560, // device enumeration
  0x44E4D0, // fallback adapter selection
  0x458470, // DirectDraw object creation
  0x458CB0, // capability query
  0x44EEB0, // display-mode enumeration
  0x44EE00, // mode callback
  0x458D60, // EnumDisplayModes wrapper
  0x458D20, // SetCooperativeLevel wrapper
  0x44EC00, // available-video-memory classification
  0x458CE0, // GetAvailableVidMem wrapper
];

for (const va of graphicsPoints) {
  try {
    Interceptor.attach(at(va), {
      onEnter(args) {
        this.ordinal = (this.ordinal || 0) + 1;
        const captured = [];
        for (let i = 0; i < 4; i++) captured.push(args[i].toString());
        send({ event: "graphics_startup_enter",
          address: `0x${va.toString(16)}`, args: captured });
      },
      onLeave(result) {
        send({ event: "graphics_startup_return",
          address: `0x${va.toString(16)}`,
          result: result.toUInt32(),
          return_low_byte: result.toUInt32() & 0xff });
      },
    });
    send({ event: "graphics_startup_hook_ready",
      address: `0x${va.toString(16)}` });
  } catch (error) {
    send({ event: "graphics_startup_hook_unavailable",
      address: `0x${va.toString(16)}`, error: String(error) });
  }
}

for (const api of ["NtTerminateProcess", "NtRaiseException", "RtlRaiseException"]) {
  try {
    const address = Module.getGlobalExportByName(api);
    Interceptor.attach(address, {
      onEnter(args) {
        if (api === "NtTerminateProcess") {
          this.termination = {
            process_handle: args[0].toString(),
            exit_status: args[1].toUInt32(),
            exit_status_hex: `0x${args[1].toUInt32().toString(16).padStart(8, "0")}`,
          };
          send({ event: "native_termination_call", api, ...this.termination,
            return_address: this.returnAddress.toString() });
          return;
        }
        const record = args[0];
        let code = null;
        let addressValue = null;
        try {
          if (!record.isNull()) {
            code = record.readU32();
            addressValue = record.add(12).readPointer().toString();
          }
        } catch (_) {}
        send({ event: "native_exception_raise", api, code,
          exception_address: addressValue,
          first_chance: api === "NtRaiseException" ? args[2].toUInt32() : null,
          return_address: this.returnAddress.toString() });
      },
      onLeave(result) {
        if (api === "NtTerminateProcess") {
          send({ event: "native_termination_return",
            status: result.toUInt32(),
            termination: this.termination || null });
        }
      },
    });
    send({ event: "native_startup_hook_ready", api,
      address: address.toString() });
  } catch (error) {
    send({ event: "native_startup_hook_unavailable", api,
      error: String(error) });
  }
}

try {
  Process.setExceptionHandler(details => {
    const context = details.context || {};
    send({ event: "unhandled_native_exception",
      type: details.type || null,
      address: details.address ? details.address.toString() : null,
      pc: context.pc ? context.pc.toString() : null,
      sp: context.sp ? context.sp.toString() : null,
      memory: details.memory ? {
        operation: details.memory.operation || null,
        address: details.memory.address
          ? details.memory.address.toString() : null,
      } : null });
    return false;
  });
  send({ event: "native_exception_handler_ready" });
} catch (error) {
  send({ event: "native_exception_handler_unavailable", error: String(error) });
}
