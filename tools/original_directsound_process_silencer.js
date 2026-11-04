// Process-local silent capture helper for the original 32-bit DirectSound
// client. It preserves buffer state transitions: SetVolume calls are clamped
// to DSBVOLUME_MIN, and Play calls are forwarded after applying that clamp.
// This changes only the instrumented target process and is not gameplay code.

if (Process.arch !== "ia32" || Process.pointerSize !== 4)
  throw new Error("expected the 32-bit Viper Racing engine");

const volumeMinimum = -10000;
const installed = new Map();
const hookedInterfaces = new Set();
const counters = { devices: 0, buffers: 0, play_calls: 0, volume_calls: 0,
  duplicate_buffers: 0, suppressed_unmutable_plays: 0, errors: [] };
const originals = new Map();
const callbacks = [];
const safe = (label, action) => {
  try { return action(); }
  catch (error) {
    counters.errors.push(`${label}: ${String(error)}`);
    send({ event: "directsound_silencer_error", label, error: String(error) });
    return null;
  }
};

function replaceVolume(address) {
  const key = address.toString();
  if (installed.has(`volume:${key}`)) return;
  const original = new NativeFunction(address, "int", ["pointer", "int"], "stdcall");
  originals.set(`volume:${key}`, original);
  const callback = new NativeCallback((self, _requested) => {
    counters.volume_calls++;
    const result = original(self, volumeMinimum);
    if (counters.volume_calls <= 12) {
      send({ event: "directsound_volume_clamped", buffer: self.toString(),
        requested: _requested, applied: volumeMinimum, result });
    }
    return result;
  }, "int", ["pointer", "int"], "stdcall");
  callbacks.push(callback);
  Interceptor.replace(address, callback);
  installed.set(`volume:${key}`, true);
}

function replacePlay(address) {
  const key = address.toString();
  if (installed.has(`play:${key}`)) return;
  const volumeAddress = installed.get(`volume_address:${key}`);
  if (!volumeAddress) throw new Error("buffer volume method was not installed");
  const original = new NativeFunction(address, "int",
    ["pointer", "uint32", "uint32", "uint32"], "stdcall");
  const setVolume = originals.get(`volume:${volumeAddress.toString()}`);
  if (!setVolume) throw new Error("original buffer volume method is unavailable");
  originals.set(`play:${key}`, original);
  const callback = new NativeCallback((self, reserved, priority, flags) => {
    counters.play_calls++;
    const muteResult = safe("mute_before_play", () =>
      setVolume(self, volumeMinimum));
    if (typeof muteResult !== "number" || muteResult < 0) {
      counters.suppressed_unmutable_plays++;
      send({ event: "directsound_play_suppressed_unmutable_buffer",
        buffer: self.toString(), mute_result: muteResult,
        ordinal: counters.play_calls });
      return 0;
    }
    if (counters.play_calls <= 40) {
      send({ event: "directsound_play_forwarded_silent", buffer: self.toString(),
        reserved, priority, flags, ordinal: counters.play_calls });
    }
    return original(self, reserved, priority, flags);
  }, "int", ["pointer", "uint32", "uint32", "uint32"], "stdcall");
  callbacks.push(callback);
  Interceptor.replace(address, callback);
  installed.set(`play:${key}`, true);
}

function patchBuffer(buffer, source) {
  if (buffer.isNull()) return false;
  const identity = buffer.toString();
  if (hookedInterfaces.has(identity)) return true;
  const table = buffer.readPointer();
  const playAddress = table.add(12 * 4).readPointer();
  const volumeAddress = table.add(15 * 4).readPointer();
  if (playAddress.isNull() || volumeAddress.isNull())
    throw new Error("DirectSound buffer vtable is incomplete");

  // Install the volume wrapper first so the Play wrapper can call its original
  // method directly at minimum volume without recursing through the clamp.
  replaceVolume(volumeAddress);
  const originalVolume = originals.get(`volume:${volumeAddress.toString()}`);
  const muteResult = safe("initial_buffer_mute", () =>
    originalVolume(buffer, volumeMinimum));
  installed.set(`volume_address:${playAddress.toString()}`, volumeAddress);
  replacePlay(playAddress);
  hookedInterfaces.add(identity);
  counters.buffers++;
  send({ event: "directsound_buffer_silenced", buffer: identity, source,
    set_volume_result: muteResult, play_method: playAddress.toString(),
    volume_method: volumeAddress.toString() });
  return true;
}

function hookDevice(device, source) {
  if (device.isNull()) return;
  const identity = device.toString();
  if (hookedInterfaces.has(`device:${identity}`)) return;
  hookedInterfaces.add(`device:${identity}`);
  counters.devices++;
  const table = device.readPointer();
  const createBuffer = table.add(3 * 4).readPointer();
  const duplicateBuffer = table.add(5 * 4).readPointer();
  for (const [method, methodAddress, outputIndex] of [
    ["CreateSoundBuffer", createBuffer, 2],
    ["DuplicateSoundBuffer", duplicateBuffer, 2],
  ]) {
    if (methodAddress.isNull()) {
      send({ event: "directsound_device_method_unavailable", device: identity,
        source, method });
      continue;
    }
    const hookKey = `${method}:${methodAddress.toString()}`;
    if (installed.has(hookKey)) continue;
    Interceptor.attach(methodAddress, {
      onEnter(args) { this.output = args[outputIndex]; },
      onLeave(result) {
        if (result.toInt32() < 0 || !this.output || this.output.isNull()) return;
        const buffer = safe("read_created_buffer", () => this.output.readPointer());
        if (!buffer || buffer.isNull()) return;
        if (method === "DuplicateSoundBuffer") counters.duplicate_buffers++;
        safe(`patch_${method}`, () => patchBuffer(buffer, method));
      },
    });
    installed.set(hookKey, true);
  }
  send({ event: "directsound_device_observed", device: identity, source });
}

function hookCreateExport(module, name) {
  const address = module.findExportByName(name);
  if (!address) return false;
  const key = `export:${address.toString()}`;
  if (installed.has(key)) return true;
  Interceptor.attach(address, {
    onEnter(args) { this.output = args[1]; },
    onLeave(result) {
      if (result.toInt32() < 0 || !this.output || this.output.isNull()) return;
      const device = safe(`read_${name}_device`, () => this.output.readPointer());
      if (device && !device.isNull()) hookDevice(device, name);
    },
  });
  installed.set(key, true);
  send({ event: "directsound_create_hooked", api: name,
    address: address.toString(), module: module.name });
  return true;
}

const observer = Process.attachModuleObserver({
  onAdded(module) {
    if (module.name.toLowerCase() !== "dsound.dll") return;
    const create = hookCreateExport(module, "DirectSoundCreate");
    const create8 = hookCreateExport(module, "DirectSoundCreate8");
    if (!create && !create8) {
      counters.errors.push("no_directsound_creation_export");
      send({ event: "directsound_silencer_error",
        label: "module_exports", error: "no DirectSoundCreate export found" });
    }
  },
});

// Load the same system audio module the engine will use while it is still
// suspended. This guarantees that the Create/duplicate hooks are in place
// before the first engine call can create an audible buffer.
const directSoundModule = Process.findModuleByName("dsound.dll") ||
  Module.load("dsound.dll");
const createHooked = hookCreateExport(directSoundModule, "DirectSoundCreate");
const create8Hooked = hookCreateExport(directSoundModule, "DirectSoundCreate8");
if (!createHooked && !create8Hooked)
  throw new Error("DirectSound creation exports could not be hooked before resume");

rpc.exports = {
  status() {
    return { ...counters, installed_hooks: installed.size,
      observed_buffers: hookedInterfaces.size };
  },
};
send({ event: "directsound_process_silencer_ready",
  suppression: "clamp buffer volume to DSBVOLUME_MIN and forward Play",
  create_exports_hooked: [createHooked, create8Hooked],
  target_arch: Process.arch });
