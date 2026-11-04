#!/usr/bin/env node
// Convert a valid original PCAR controlled-capture JSONL log into the
// reconstruction's fixed-step play-trace CSV schedule. This transfers inputs
// only; it does not assert state or physics parity.
"use strict";

const fs = require("node:fs");
const crypto = require("node:crypto");

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
        const ready = rows.find(event => event.event === "controlled_pcar_capture_ready");
        const armed = rows.find(event => event.event === "controlled_pcar_capture_armed");
        const completion = rows.find(event => event.event === "controlled_pcar_capture_done");
        const samples = rows.filter(event => event.event === "pcar_integrator_sample");
        if (!selected || !ready || !armed || !completion) {
            throw new Error("capture is missing ready, selected-player, armed, or completion evidence");
        }
        if (ready.startCondition !== "first_selected_pcar_callback_0x4384d0" ||
            armed.body !== selected.body) {
            throw new Error("capture did not arm on the selected PCAR identity");
        }
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
            const observed = samples.filter(sample => sample.phase === phase).length;
            if (!Number.isInteger(counts[phase]) || counts[phase] < 4 ||
                observed < 4 || observed !== counts[phase]) {
                throw new Error(`capture lacks required ${phase} samples`);
            }
        }
        if (samples.length === 0 || samples.some(sample => sample.body !== selected.body)) {
            throw new Error("integrator sample body does not match selected PCAR identity");
        }
        const observedWheelRows = samples.reduce((sum, sample) =>
            sum + (Array.isArray(sample.wheels) ? sample.wheels.length : 0), 0);
        if (observedWheelRows < 20 || observedWheelRows !== completion.capturedWheelRows) {
            throw new Error("wheel sample rows do not match the completion summary");
        }
        const windows = completion.phaseWindowsMs;
        if (!Array.isArray(windows) || windows.length !== requiredPhases.length) {
            throw new Error("capture is missing its five-phase schedule");
        }
        const byPhase = new Map(windows.map(window => [window.phase, window]));
        if (byPhase.size !== requiredPhases.length ||
            windows.some((window, index) => index > 0 &&
                window.start < windows[index - 1].end)) {
            throw new Error("capture phase windows are duplicated, overlapping, or unordered");
        }
        if (!Array.isArray(armed.phaseWindowsMs) ||
            JSON.stringify(armed.phaseWindowsMs) !== JSON.stringify(windows)) {
            throw new Error("armed and completed phase schedules do not match");
        }
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
        const csvText = `${lines.join("\n")}\n`;
        const sourceText = fs.readFileSync(inputPath, "utf8");
        const sha256 = value => crypto.createHash("sha256").update(value).digest("hex");
        const frameForMs = value => Math.ceil(value / (frameDuration * 1000));
        const sidecar = {
            schema: "viper-controlled-capture-trace-v1",
            sourceCaptureSha256: sha256(sourceText),
            replayTraceSha256: sha256(csvText),
            sourceModule: {
                name: ready.module,
                base: ready.base,
                imageSize: ready.imageSize,
            },
            selectedPcar: {
                body: selected.body,
                ordinal: selected.ordinal_0x510 ?? null,
            },
            inputEvidence: {
                syntheticInput: true,
                routeMotionInput: false,
                configuredBindings: completion.configuredBindings,
                controlResponseComparedToNeutral: completion.controlResponseComparedToNeutral,
                sampledIntegratorsByPhase: completion.sampledIntegratorsByPhase,
                capturedWheelRows: completion.capturedWheelRows,
            },
            replaySchedule: {
                origin: "first-selected-pcar-callback",
                frameDeltaSeconds: frameDuration,
                durationMs,
                windows: windows.map(window => ({
                    phase: window.phase,
                    inputMs: { start: window.start, end: window.end },
                    frameRangeExclusive: {
                        start: frameForMs(window.start),
                        end: frameForMs(window.end),
                    },
                })),
                stateOrPhysicsParityClaimed: false,
            },
        };
        fs.writeFileSync(outputPath, csvText, "utf8");
        fs.writeFileSync(`${outputPath}.json`, `${JSON.stringify(sidecar, null, 2)}\n`, "utf8");
        console.log(JSON.stringify({
            output: outputPath,
            sidecar: `${outputPath}.json`,
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
