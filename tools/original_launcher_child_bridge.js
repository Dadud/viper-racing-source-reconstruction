// Attach to the isolated 32-bit launcher before resume. Suspend only its
// race.bin child so the host can install the bounded observers before game
// startup continues. The launcher retains ownership of PROCESS_INFORMATION.

const expectedCwd = __EXPECTED_CWD_JSON__;
const createSuspended = 0x00000004;
const resumeDelayMs = 30000;
const maxResumeCalls = 16;

if (Process.arch !== "ia32" || Process.pointerSize !== 4)
  throw new Error("expected the 32-bit Viper Racing launcher");

const resumeThread = new NativeFunction(
  Module.getGlobalExportByName("ResumeThread"), "uint32", ["pointer"]);
const launcher = Process.enumerateModules().find(module =>
  module.name.toLowerCase() === "viper racing.exe");
if (!launcher) throw new Error("verified Viper Racing launcher module is not mapped");
const at = va => launcher.base.add(va - 0x400000);

function textArg(pointer, wide) {
  try {
    if (pointer.isNull()) return "";
    return (wide ? pointer.readUtf16String() : pointer.readCString()) || "";
  } catch (_) { return ""; }
}

function normalize(value) {
  return value.replace(/\//g, "\\").replace(/\\+$/, "").toLowerCase();
}

for (const name of ["ExitProcess", "TerminateProcess", "RtlExitUserProcess"]) {
  try {
    const address = Module.getGlobalExportByName(name);
    Interceptor.attach(address, {
      onEnter(args) {
        const isTerminate = name === "TerminateProcess";
        const code = isTerminate ? args[1] : args[0];
        send({event: "launcher_exit_api", api: name,
          exitCode: code.toUInt32(), returnAddress: this.returnAddress.toString()});
      },
    });
  } catch (error) {
    send({event: "launcher_exit_hook_unavailable", api: name,
      error: String(error)});
  }
}

for (const name of ["MessageBoxA", "MessageBoxW"]) {
  try {
    const address = Module.getGlobalExportByName(name);
    Interceptor.attach(address, {
      onEnter(args) {
        const wide = name.endsWith("W");
        this.body = textArg(args[1], wide);
        this.caption = textArg(args[2], wide);
      },
      onLeave() {
        send({event: "launcher_message_box", api: name,
          text: this.body, caption: this.caption});
      },
    });
  } catch (error) {
    send({event: "launcher_message_box_hook_unavailable", api: name,
      error: String(error)});
  }
}

function watch(name, wide) {
  const address = Module.getGlobalExportByName(name);
  Interceptor.attach(address, {
    onEnter(args) {
      const application = textArg(args[0], wide);
      const command = textArg(args[1], wide);
      const cwd = textArg(args[7], wide);
      const target = `${application} ${command}`.trim();
      this.api = name;
      this.application = application;
      this.command = command;
      this.cwd = cwd;
      this.flags = args[5].toUInt32();
      send({event: "launcher_process_create_call", api: name,
        application, command, cwd, creation_flags: this.flags,
        return_address: this.returnAddress.toString()});
      this.capture = /(?:^|[\\/\s"])(?:data[\\/])?race\.bin(?:[\s"]|$)/i.test(target) &&
        normalize(cwd) === normalize(expectedCwd);
      this.processInfo = args[9];
      this.cwd = cwd;
      this.target = target;
      if (/race\.bin/i.test(target) && normalize(cwd) !== normalize(expectedCwd)) {
        send({event: "race_child_not_captured", api: name, target, cwd,
          expectedCwd});
      }
      if (this.capture) {
        this.flagsBefore = args[5].toUInt32();
        args[5] = ptr(this.flagsBefore | createSuspended);
        send({event: "race_child_create_suspended", api: name,
          flagsBefore: this.flagsBefore,
          flagsAfter: this.flagsBefore | createSuspended, target, cwd});
      }
    },
    onLeave(result) {
      send({event: "launcher_process_create_return", api: this.api,
        application: this.application, command: this.command, cwd: this.cwd,
        creation_flags: this.flags, succeeded: result.toInt32() !== 0,
        result: result.toInt32()});
      if (!this.capture) return;
      if (result.toInt32() === 0 || this.processInfo.isNull()) {
        send({event: "race_child_create_failed", api: name});
        return;
      }
      const threadHandle = this.processInfo.add(4).readPointer();
      const pid = this.processInfo.add(8).readU32();
      const tid = this.processInfo.add(12).readU32();
      send({event: "race_child_ready_for_attach", pid, tid,
        target: this.target, cwd: this.cwd, resumeDelayMs});
      setTimeout(() => {
        let previous = null;
        for (let calls = 1; calls <= maxResumeCalls; calls++) {
          previous = resumeThread(threadHandle);
          if (previous === 0xffffffff) {
            send({event: "race_child_resume_failed", pid, tid, calls});
            return;
          }
          if (previous <= 1) {
            send({event: "race_child_resumed", pid, tid, calls,
              previousSuspendCount: previous,
              alreadyRunning: previous === 0});
            return;
          }
        }
        send({event: "race_child_resume_incomplete", pid, tid,
          lastPreviousSuspendCount: previous});
      }, resumeDelayMs);
    },
  });
  send({event: "race_child_bridge_hook_ready", api: name,
    address: address.toString(), expectedCwd, resumeDelayMs});
}

watch("CreateProcessA", false);
watch("CreateProcessW", true);

try {
  Interceptor.attach(Module.getGlobalExportByName("WinExec"), {
    onEnter(args) {
      this.command = textArg(args[0], false);
      this.show = args[1].toUInt32();
      send({event: "launcher_winexec_call", command: this.command,
        show: this.show, return_address: this.returnAddress.toString()});
    },
    onLeave(result) {
      send({event: "launcher_winexec_return", command: this.command,
        result: result.toUInt32()});
    },
  });
} catch (error) {
  send({event: "launcher_winexec_hook_unavailable", error: String(error)});
}

Interceptor.attach(at(0x401880), {
  onEnter(args) { this.pathBuffer = args[0]; },
  onLeave(result) {
    let path = null;
    const returnLowByte = result.toUInt32() & 0xff;
    try { path = this.pathBuffer.readCString(); } catch (_) {}
    send({event: "launcher_data_directory_result",
      succeeded: returnLowByte !== 0, return_low_byte: returnLowByte, path,
      path_buffer: this.pathBuffer.toString()});
  },
});

for (const [api, stringIndex] of [["GetDriveTypeA", 0], ["GetFileAttributesA", 0]]) {
  try {
    Interceptor.attach(Module.getGlobalExportByName(api), {
      onEnter(args) { this.path = textArg(args[stringIndex], false); },
      onLeave(result) {
        send({event: "launcher_path_probe", api, path: this.path,
          result: result.toUInt32()});
      },
    });
  } catch (error) {
    send({event: "launcher_path_probe_hook_unavailable", api,
      error: String(error)});
  }
}
