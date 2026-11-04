const assert = require("node:assert/strict");
const crypto = require("node:crypto");
const { execFileSync, spawnSync } = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

const root = fs.mkdtempSync(path.join(os.tmpdir(), "viper-capture-trace-"));
const capturePath = path.join(root, "capture.jsonl");
const tracePath = path.join(root, "replay.csv");
const gameReplayPath = path.join(root, "game-replay.csv");
const tickTracePath = path.join(root, "game-ticks.csv");
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
        controls: {
            throttle: window.phase === "throttle" ? 0.8 :
                window.phase === "neutral_before" || window.phase === "neutral_mid" ||
                window.phase === "neutral_after" ? 0.02 : 0,
            brake: window.phase === "brake" ? 0.7 :
                window.phase === "neutral_before" || window.phase === "neutral_mid" ||
                window.phase === "neutral_after" ? 0.01 : 0,
        },
        wheels: [{}],
    }))),
    {
        event: "controlled_pcar_capture_done",
        captureValid: true,
        configuredBindings: {
            throttle: { type: 1, selector: 0xc8 },
            brake: { type: 1, selector: 0xd0 },
        },
        keyStats: {
            throttle: { binding_type: 1, selector: 0xc8, reads: 8, patched: 8 },
            brake: { binding_type: 1, selector: 0xd0, reads: 8, patched: 8 },
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

    const mismatchedSummary = [...events];
    mismatchedSummary[mismatchedSummary.length - 1] = {
        ...mismatchedSummary[mismatchedSummary.length - 1],
        controlResponseComparedToNeutral: {
            throttle: { active: 0.9, neutral: 0.02 },
            brake: { active: 0.7, neutral: 0.01 },
        },
    };
    fs.writeFileSync(capturePath,
        mismatchedSummary.map(event => JSON.stringify(event)).join("\n"));
    const mismatched = spawnSync(process.execPath, [converter, capturePath, tracePath], {
        encoding: "utf8",
    });
    assert.notEqual(mismatched.status, 0);
    assert.match(mismatched.stderr, /phase response does not match sampled PCAR controls/);

    const gameExe = process.env.VIPER_GAME_EXE;
    const dataDir = process.env.VIPER_DATA_DIR;
    if (gameExe && dataDir) {
        const game = spawnSync(gameExe, [
            "--data-dir", dataDir,
            "--track", "bemidji",
            "--car", "viper",
            "--software",
            "--diagnostics-only",
            "-grid",
            "--play-trace", tracePath,
            "--record-trace", gameReplayPath,
            "--record-tick-trace", tickTracePath,
        ], { encoding: "utf8", timeout: 45000, windowsHide: true });
        assert.equal(game.error, undefined, game.error?.message);
        assert.equal(game.status, 0,
            `viper_game rejected generated schedule: ${game.stderr || game.stdout}`);
        const gameReplayLines = fs.readFileSync(gameReplayPath, "utf8")
            .trimEnd().split(/\r?\n/);
        assert(gameReplayLines.length > 1, "viper_game recorded no replay frames");
        const replayHeader = gameReplayLines[0].split(",");
        const replayRows = gameReplayLines.slice(1).map(line => line.split(","));
        const replayColumn = name => replayHeader.indexOf(name);
        assert(replayColumn("throttle") >= 0 && replayColumn("brake") >= 0 &&
            replayColumn("route_motion_step_count") >= 0);
        assert(replayRows.some(row => row[replayColumn("throttle")] === "1"));
        assert(replayRows.some(row => row[replayColumn("brake")] === "1"));
        assert(replayRows.every(row => row[replayColumn("route_motion_step_count")] === "0"),
            "converted playback invoked ideal-line route motion");
        const tickLines = fs.readFileSync(tickTracePath, "utf8").trimEnd().split(/\r?\n/);
        assert(tickLines.length > 1, "viper_game produced no tick records from generated schedule");
        const tickHeader = tickLines[0].split(",");
        const tickRows = tickLines.slice(1).map(line => line.split(","));
        const tickColumn = name => tickHeader.indexOf(name);
        assert(tickColumn("throttle") >= 0 && tickColumn("brake") >= 0);
        const peak = name => Math.max(...tickRows.map(row =>
            Number(row[tickColumn(name)])).filter(Number.isFinite));
        assert(peak("throttle") > 0.05, "replayed throttle did not reach PCAR controls");
        assert(peak("brake") > 0.05, "replayed brake did not reach PCAR controls");
    }

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
