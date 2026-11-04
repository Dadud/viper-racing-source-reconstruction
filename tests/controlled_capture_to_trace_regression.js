const assert = require("node:assert/strict");
const crypto = require("node:crypto");
const { execFileSync, spawnSync } = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

const root = fs.mkdtempSync(path.join(os.tmpdir(), "viper-capture-trace-"));
const capturePath = path.join(root, "capture.jsonl");
const tracePath = path.join(root, "replay.csv");
const converter = path.join(__dirname, "..", "tools", "controlled_capture_to_trace.js");
const sidecarPath = `${tracePath}.json`;
const windows = [
    { phase: "neutral_before", start: 1000, end: 2500, control: null },
    { phase: "throttle", start: 2500, end: 4000, control: "throttle" },
    { phase: "neutral_mid", start: 4000, end: 5500, control: null },
    { phase: "brake", start: 5500, end: 7000, control: "brake" },
    { phase: "neutral_after", start: 7000, end: 8500, control: null },
];
const events = [
    {
        event: "controlled_pcar_capture_ready",
        module: "race.bin",
        base: "0x400000",
        imageSize: 0x1c2000,
        startCondition: "first_selected_pcar_callback_0x4384d0",
    },
    { event: "pcar_selected_player", body: "0x101000", ordinal_0x510: 0 },
    { event: "controlled_pcar_capture_armed", body: "0x101000", phaseWindowsMs: windows },
    ...windows.flatMap(window => Array.from({ length: 4 }, (_, index) => ({
        event: "pcar_integrator_sample",
        phase: window.phase,
        body: "0x101000",
        integrator: index,
        wheels: [{}],
    }))),
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
    const sidecar = JSON.parse(fs.readFileSync(sidecarPath, "utf8"));
    assert.equal(sidecar.selectedPcar.body, "0x101000");
    assert.equal(sidecar.sourceModule.name, "race.bin");
    assert.equal(sidecar.replaySchedule.origin, "first-selected-pcar-callback");
    assert.equal(sidecar.replaySchedule.windows[1].frameRangeExclusive.start, 157);
    assert.equal(sidecar.replaySchedule.stateOrPhysicsParityClaimed, false);
    const digest = value => crypto.createHash("sha256").update(value).digest("hex");
    assert.equal(sidecar.sourceCaptureSha256,
        digest(fs.readFileSync(capturePath, "utf8")));
    assert.equal(sidecar.replayTraceSha256,
        digest(fs.readFileSync(tracePath, "utf8")));

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
