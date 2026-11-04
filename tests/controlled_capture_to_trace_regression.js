const assert = require("node:assert/strict");
const { execFileSync, spawnSync } = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

const root = fs.mkdtempSync(path.join(os.tmpdir(), "viper-capture-trace-"));
const capturePath = path.join(root, "capture.jsonl");
const tracePath = path.join(root, "replay.csv");
const converter = path.join(__dirname, "..", "tools", "controlled_capture_to_trace.js");
const windows = [
    { phase: "neutral_before", start: 1000, end: 2500 },
    { phase: "throttle", start: 2500, end: 4000 },
    { phase: "neutral_mid", start: 4000, end: 5500 },
    { phase: "brake", start: 5500, end: 7000 },
    { phase: "neutral_after", start: 7000, end: 8500 },
];
const events = [
    { event: "pcar_selected_player", body: "0x101000" },
    ...["neutral_before", "throttle", "neutral_mid", "brake", "neutral_after"]
        .map((phase, index) => ({
            event: "pcar_integrator_sample",
            phase,
            body: "0x101000",
            integrator: index,
        })),
    {
        event: "controlled_pcar_capture_done",
        captureValid: true,
        configuredBindings: {
            throttle: { type: 1, selector: 0xc8 },
            brake: { type: 1, selector: 0xd0 },
        },
        capturedWheelRows: 20,
        sampledIntegratorsByPhase: Object.fromEntries(
            windows.map(window => [window.phase, 4])),
        phaseWindowsMs: windows,
        controlResponseComparedToNeutral: {
            throttle: { active: 0.8, neutral: 0.02 },
            brake: { active: 0.7, neutral: 0.01 },
        },
    },
];

try {
    fs.writeFileSync(capturePath, events.map(event => JSON.stringify(event)).join("\n"));
    execFileSync(process.execPath, [converter, capturePath, tracePath], { stdio: "pipe" });
    const lines = fs.readFileSync(tracePath, "utf8").trimEnd().split(/\r?\n/);
    assert.equal(lines[0],
        "frame,elapsed_seconds,delta_seconds,steer_left,steer_right,throttle,brake,phase");
    const parsed = lines.slice(1).map(line => line.split(","));
    const throttle = parsed.find(row => row[7] === "throttle");
    const brake = parsed.find(row => row[7] === "brake");
    assert(throttle && throttle[5] === "1" && throttle[6] === "0");
    assert(brake && brake[5] === "0" && brake[6] === "1");
    assert(parsed.every(row => row[3] === "0" && row[4] === "0"));

    const invalid = [...events];
    invalid[invalid.length - 1] = {
        ...invalid[invalid.length - 1],
        captureValid: false,
    };
    fs.writeFileSync(capturePath, invalid.map(event => JSON.stringify(event)).join("\n"));
    const rejected = spawnSync(process.execPath, [converter, capturePath, tracePath], {
        encoding: "utf8",
    });
    assert.notEqual(rejected.status, 0);
    assert.match(rejected.stderr, /capture is marked invalid/);
    console.log("Controlled capture to replay trace regression passed.");
} finally {
    fs.rmSync(root, { recursive: true, force: true });
}
