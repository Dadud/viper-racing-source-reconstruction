#!/usr/bin/env node
// Convert a valid original PCAR controlled-capture JSONL log into the
// reconstruction's fixed-step play-trace CSV schedule. This transfers inputs
// only; it does not assert state or physics parity.
"use strict";

const fs = require("node:fs");

const fail = message => {
    console.error(message);
    process.exitCode = 1;
};

if (process.argv.length !== 4) {
    fail("usage: node tools/controlled_capture_to_trace.js <capture.jsonl> <output.csv>");
} else {
    const [, , inputPath, outputPath] = process.argv;
    try {
        const rows = fs.readFileSync(inputPath, "utf8").split(/\r?\n/)
            .filter(line => line.trim().length > 0)
            .map((line, index) => {
                try {
                    const event = JSON.parse(line);
                    return event.payload && typeof event.payload === "object"
                        ? event.payload : event;
                } catch (error) {
                    throw new Error(`invalid JSON on line ${index + 1}: ${error.message}`);
                }
            });

        const selected = rows.find(event => event.event === "pcar_selected_player");
        const completion = rows.find(event => event.event === "controlled_pcar_capture_done");
        const samples = rows.filter(event => event.event === "pcar_integrator_sample");
        if (!selected || !completion) throw new Error("capture is missing selection or completion evidence");
        if (completion.captureValid !== true) throw new Error("capture is marked invalid");
        if (!completion.configuredBindings ||
            completion.configuredBindings.throttle?.type !== 1 ||
            completion.configuredBindings.brake?.type !== 1) {
            throw new Error("capture does not contain two verified type-1 keyboard bindings");
        }
        if (!Number.isInteger(completion.capturedWheelRows) || completion.capturedWheelRows < 20) {
            throw new Error("capture lacks the required wheel sample coverage");
        }
        const response = completion.controlResponseComparedToNeutral;
        if (!response ||
            !(response.throttle?.active > response.throttle?.neutral + 1.0e-4) ||
            !(response.brake?.active > response.brake?.neutral + 1.0e-4)) {
            throw new Error("capture does not show phase-matched throttle and brake responses");
        }
        const requiredPhases = [
            "neutral_before", "throttle", "neutral_mid", "brake", "neutral_after",
        ];
        const counts = completion.sampledIntegratorsByPhase || {};
        for (const phase of requiredPhases) {
            if (!Number.isInteger(counts[phase]) || counts[phase] < 4) {
                throw new Error(`capture lacks required ${phase} samples`);
            }
        }
        if (samples.length === 0 || samples.some(sample => sample.body !== selected.body)) {
            throw new Error("integrator sample body does not match selected PCAR identity");
        }
        const windows = completion.phaseWindowsMs;
        if (!Array.isArray(windows) || windows.length !== requiredPhases.length) {
            throw new Error("capture is missing its five-phase schedule");
        }
        const byPhase = new Map(windows.map(window => [window.phase, window]));
        for (const phase of requiredPhases) {
            const window = byPhase.get(phase);
            if (!window || !Number.isFinite(window.start) || !Number.isFinite(window.end) ||
                window.start < 0 || window.end <= window.start) {
                throw new Error(`capture has an invalid ${phase} window`);
            }
        }
        const throttle = byPhase.get("throttle");
        const brake = byPhase.get("brake");
        const durationMs = Math.max(...windows.map(window => window.end));
        const frameDuration = 0.016;
        const lastFrame = Math.ceil(durationMs / (frameDuration * 1000));
        const phaseAt = elapsedMs => {
            const window = windows.find(item => elapsedMs >= item.start && elapsedMs < item.end);
            if (!window) return "neutral";
            return window.phase;
        };
        const lines = ["frame,elapsed_seconds,delta_seconds,steer_left,steer_right,throttle,brake,phase"];
        for (let frame = 0; frame <= lastFrame; frame++) {
            const elapsed = frame * frameDuration;
            const elapsedMs = elapsed * 1000;
            const throttleDown = elapsedMs >= throttle.start && elapsedMs < throttle.end;
            const brakeDown = elapsedMs >= brake.start && elapsedMs < brake.end;
            const phase = phaseAt(elapsedMs);
            lines.push([
                frame,
                elapsed.toFixed(3),
                frameDuration.toFixed(3),
                0,
                0,
                throttleDown ? 1 : 0,
                brakeDown ? 1 : 0,
                phase,
            ].join(","));
        }
        fs.writeFileSync(outputPath, `${lines.join("\n")}\n`, "utf8");
        console.log(JSON.stringify({
            output: outputPath,
            selectedBody: selected.body,
            frameCount: lastFrame + 1,
            durationMs,
            routeMotion: false,
            parityClaim: false,
        }));
    } catch (error) {
        fail(error.message);
    }
}
