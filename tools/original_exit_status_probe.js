// Read-only process-exit observer for a bounded original race.bin session.
// Records explicit termination API arguments before the process exits.
const hooks = [];
for (const name of ["ExitProcess", "TerminateProcess", "RtlExitUserProcess"]) {
  try {
    const address = Module.getGlobalExportByName(name);
    hooks.push(Interceptor.attach(address, {
      onEnter(args) {
        const terminateProcess = name === "TerminateProcess";
        const code = terminateProcess ? args[1] : args[0];
        const module = Process.findModuleByAddress(this.returnAddress);
        send({
          event: "original_process_exit_api",
          api: name,
          exit_code: code.toUInt32(),
          return_address: this.returnAddress.toString(),
          caller_module: module ? module.name : null,
          caller_module_offset: module
            ? this.returnAddress.sub(module.base).toString() : null,
        });
      },
    }));
    send({ event: "original_process_exit_hook_ready", api: name,
      address: address.toString() });
  } catch (error) {
    send({ event: "original_process_exit_hook_unavailable", api: name,
      error: String(error) });
  }
}
