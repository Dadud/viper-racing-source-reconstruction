// Passive startup trace for the original x86 DirectDraw initialization path.
// It records recovered function boundaries and termination/exception signals
// without changing returns, memory, display mode, or input.

if (Process.arch !== "ia32" || Process.pointerSize !== 4)
  throw new Error("expected the 32-bit Viper Racing engine");

const engine = Process.findModuleByName("race.bin");
if (!engine) throw new Error("race.bin is not mapped");
const at = va => engine.base.add(va - 0x400000);
function readU32(address) {
  try { return address.readU32(); } catch (_) { return null; }
}
function readWords(address, count) {
  if (!address || address.isNull()) return null;
  const words = [];
  try {
    for (let index = 0; index < count; index++)
      words.push(address.add(index * 4).readU32());
    return words;
  } catch (_) { return null; }
}
const knownDirectDrawObjects = new Set();
const hookedQueryInterfaceMethods = new Set();
const hookedEnumDisplayModesMethods = new Set();

function moduleName(address) {
  try {
    const module = Process.findModuleByAddress(address);
    return module ? module.name : null;
  } catch (_) { return null; }
}

function hookDirectDrawInterface(object, discoveredBy) {
  try {
    const objectKey = object.toString();
    if (knownDirectDrawObjects.has(objectKey)) return;
    const vtable = object.readPointer();
    const queryInterface = vtable.readPointer();
    // IDirectDraw and IDirectDraw2 inherit EnumDisplayModes at slot 8.
    const enumDisplayModes = vtable.add(8 * Process.pointerSize).readPointer();
    knownDirectDrawObjects.add(objectKey);
    send({ event: "directdraw_interface_observed", object: objectKey,
      vtable: vtable.toString(), discovered_by: discoveredBy,
      query_interface: queryInterface.toString(),
      query_interface_module: moduleName(queryInterface),
      enum_display_modes: enumDisplayModes.toString(),
      enum_display_modes_module: moduleName(enumDisplayModes) });

    const queryKey = queryInterface.toString();
    if (!hookedQueryInterfaceMethods.has(queryKey)) {
      Interceptor.attach(queryInterface, {
        onEnter(args) {
          // DirectDraw exposes COM's x86 STDMETHODCALLTYPE ABI: this is
          // argument zero on the stack, not the ECX register.
          this.queryObject = args[0].toString();
          if (!knownDirectDrawObjects.has(this.queryObject)) return;
          this.output = args[2];
          this.capture = true;
          send({ event: "directdraw_query_interface_enter",
            object: this.queryObject,
            iid_words: readWords(args[1], 4) });
        },
        onLeave(result) {
          if (!this.capture) return;
          const hr = result.toUInt32();
          let returnedObject = null;
          try {
            if (hr === 0 && this.output && !this.output.isNull()) {
              const pointer = this.output.readPointer();
              if (!pointer.isNull()) {
                returnedObject = pointer.toString();
                hookDirectDrawInterface(pointer, "IDirectDraw::QueryInterface");
              }
            }
          } catch (_) {}
          send({ event: "directdraw_query_interface_return", object: this.queryObject,
            hresult: hr, returned_object: returnedObject });
        },
      });
      hookedQueryInterfaceMethods.add(queryKey);
    }

    const enumKey = enumDisplayModes.toString();
    if (!hookedEnumDisplayModesMethods.has(enumKey)) {
      Interceptor.attach(enumDisplayModes, {
        onEnter(args) {
          // IDirectDraw methods also use STDMETHODCALLTYPE; stack args are
          // [this, flags, descriptor, callback context, callback].
          this.enumObject = args[0].toString();
          if (!knownDirectDrawObjects.has(this.enumObject)) return;
          this.capture = true;
          send({ event: "directdraw_enum_display_modes_enter",
            object: this.enumObject,
            flags: args[1].toUInt32(),
            descriptor: args[2].isNull() ? null : args[2].toString(),
            descriptor_words: args[2].isNull() ? null : readWords(args[2], 27),
            callback_context: args[3].toString(),
            callback: args[4].toString() });
        },
        onLeave(result) {
          if (!this.capture) return;
          send({ event: "directdraw_enum_display_modes_return",
            object: this.enumObject,
            hresult: result.toUInt32(),
            hresult_hex: `0x${result.toUInt32().toString(16).padStart(8, "0")}` });
        },
      });
      hookedEnumDisplayModesMethods.add(enumKey);
    }
  } catch (error) {
    send({ event: "directdraw_interface_hook_error",
      object: object.toString(), discovered_by: discoveredBy,
      error: String(error) });
  }
}

try {
  const createAddress = Module.getGlobalExportByName("DirectDrawCreate");
  Interceptor.attach(createAddress, {
    onEnter(args) { this.output = args[1]; },
    onLeave(result) {
      const hr = result.toUInt32();
      let object = null;
      try {
        if (hr === 0 && this.output && !this.output.isNull()) {
          const pointer = this.output.readPointer();
          if (!pointer.isNull()) {
            object = pointer.toString();
            hookDirectDrawInterface(pointer, "DirectDrawCreate");
          }
        }
      } catch (error) {
        send({ event: "directdraw_create_object_read_error", error: String(error) });
      }
      send({ event: "directdraw_create_return", hresult: hr,
        hresult_hex: `0x${hr.toString(16).padStart(8, "0")}`, object });
    },
  });
  send({ event: "directdraw_create_hook_ready", address: createAddress.toString() });
} catch (error) {
  send({ event: "directdraw_create_hook_unavailable", error: String(error) });
}
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
        const captured = [];
        for (let i = 0; i < 4; i++) captured.push(args[i].toString());
        const event = { event: "graphics_startup_enter",
          address: `0x${va.toString(16)}`, args: captured };
        if (va === 0x44EEB0)
          event.mode_640x480_flag_0x509c52 = readU32(engine.base.add(0x109c52));
        if (va === 0x44EE00) {
          event.mode_descriptor_words = readWords(args[0], 27);
          event.callback_context = args[1].toString();
        }
        if (va === 0x458D60) {
          const interfacePointer = this.context.ecx;
          event.registers = {
            ecx: interfacePointer.toString(),
            esi: this.context.esi.toString(),
            edi: this.context.edi.toString(),
          };
          event.wrapper_words = readWords(interfacePointer, 16);
          event.argument_words = [];
          for (let index = 0; index < 4; index++)
            event.argument_words.push(readWords(args[index], 4));
          event.enum_mode_descriptor_words = readWords(args[0], 27);
        }
        send(event);
      },
      onLeave(result) {
        send({ event: "graphics_startup_return",
          address: `0x${va.toString(16)}`,
          result: result.toUInt32(),
          return_low_byte: result.toUInt32() & 0xff,
          mode_640x480_flag_0x509c52: va === 0x44EEB0 || va === 0x458D60
            ? readU32(engine.base.add(0x109c52)) : undefined });
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
