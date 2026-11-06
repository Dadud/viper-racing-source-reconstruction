#!/usr/bin/env python3
"""Compare selected numeric fields from two tick-indexed reconstruction traces.

This tool reports measured deltas; passing its configured numeric tolerances is
not, by itself, proof of original-game parity. Scenario identity and evidence
quality must be reviewed separately.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


@dataclass(frozen=True)
class FieldSpec:
    name: str
    absolute_tolerance: float
    relative_tolerance: float


def parse_field_spec(value: str) -> FieldSpec:
    parts = value.split(":")
    if len(parts) != 3 or not parts[0]:
        raise argparse.ArgumentTypeError(
            "field must be NAME:ABS_TOL:REL_TOL (example body_vel_x:0.01:0.02)"
        )
    try:
        absolute, relative = float(parts[1]), float(parts[2])
    except ValueError as error:
        raise argparse.ArgumentTypeError("tolerances must be numbers") from error
    if not math.isfinite(absolute) or not math.isfinite(relative) or absolute < 0 or relative < 0:
        raise argparse.ArgumentTypeError("tolerances must be finite and non-negative")
    return FieldSpec(parts[0], absolute, relative)


def read_trace(path: Path, key: str, fields: Sequence[FieldSpec]) -> dict[int, dict[str, float]]:
    rows: dict[int, dict[str, float]] = {}
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        headers = reader.fieldnames or []
        required = {key, *(field.name for field in fields)}
        missing_headers = sorted(required.difference(headers))
        if missing_headers:
            raise ValueError(f"{path}: missing columns: {', '.join(missing_headers)}")
        for line_number, row in enumerate(reader, start=2):
            try:
                sample_key = int(row[key])
            except (TypeError, ValueError) as error:
                raise ValueError(f"{path}:{line_number}: invalid {key}") from error
            if sample_key in rows:
                raise ValueError(f"{path}:{line_number}: duplicate {key}={sample_key}")
            values: dict[str, float] = {}
            for field in fields:
                raw = row[field.name]
                try:
                    number = float(raw)
                except (TypeError, ValueError) as error:
                    raise ValueError(
                        f"{path}:{line_number}: invalid {field.name}={raw!r}"
                    ) from error
                if not math.isfinite(number):
                    raise ValueError(
                        f"{path}:{line_number}: non-finite {field.name}={raw!r}"
                    )
                values[field.name] = number
            rows[sample_key] = values
    if not rows:
        raise ValueError(f"{path}: no trace samples")
    return rows


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def compare(
    reference: dict[int, dict[str, float]],
    candidate: dict[int, dict[str, float]],
    fields: Sequence[FieldSpec],
) -> dict[str, object]:
    ref_keys, candidate_keys = set(reference), set(candidate)
    shared = sorted(ref_keys & candidate_keys)
    missing_reference = sorted(candidate_keys - ref_keys)
    missing_candidate = sorted(ref_keys - candidate_keys)
    field_results: dict[str, object] = {}
    all_passed = not missing_reference and not missing_candidate
    for field in fields:
        deltas = [candidate[tick][field.name] - reference[tick][field.name] for tick in shared]
        limits = [
            field.absolute_tolerance
            + field.relative_tolerance * abs(reference[tick][field.name])
            for tick in shared
        ]
        failures = [tick for tick, delta, limit in zip(shared, deltas, limits) if abs(delta) > limit]
        maximum = max((abs(delta) for delta in deltas), default=None)
        mean = sum(deltas) / len(deltas) if deltas else None
        rmse = math.sqrt(sum(delta * delta for delta in deltas) / len(deltas)) if deltas else None
        passed = bool(shared) and not failures and not missing_reference and not missing_candidate
        all_passed = all_passed and passed
        field_results[field.name] = {
            "absolute_tolerance": field.absolute_tolerance,
            "relative_tolerance": field.relative_tolerance,
            "samples": len(shared),
            "max_absolute_delta": maximum,
            "mean_signed_delta": mean,
            "rmse": rmse,
            "failed_tick_count": len(failures),
            "first_failed_ticks": failures[:20],
            "passed": passed,
        }
    return {
        "schema": "viper-tick-trace-comparison-v1",
        "key": "tick_index",
        "reference_sample_count": len(reference),
        "candidate_sample_count": len(candidate),
        "matched_sample_count": len(shared),
        "missing_reference_ticks": missing_reference[:100],
        "missing_candidate_ticks": missing_candidate[:100],
        "fields": field_results,
        "passed": all_passed,
        "parity_claimed": False,
        "interpretation": (
            "Numeric tolerance result only. Confirm scenario identity, input schedule, "
            "capture validity, and original evidence independently."
        ),
    }


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="tick-indexed reference CSV")
    parser.add_argument("candidate", type=Path, help="tick-indexed candidate CSV")
    parser.add_argument("--key", default="tick_index", help="integer alignment column")
    parser.add_argument(
        "--field", action="append", type=parse_field_spec, required=True,
        help="numeric comparison NAME:ABS_TOL:REL_TOL; repeat for each field",
    )
    parser.add_argument("--output", type=Path, help="write JSON report to this path")
    args = parser.parse_args(argv)
    if len({field.name for field in args.field}) != len(args.field):
        parser.error("each --field name must be unique")
    try:
        reference = read_trace(args.reference, args.key, args.field)
        candidate = read_trace(args.candidate, args.key, args.field)
        report = compare(reference, candidate, args.field)
        report["key"] = args.key
        report["reference"] = {"path": str(args.reference), "sha256": sha256(args.reference)}
        report["candidate"] = {"path": str(args.candidate), "sha256": sha256(args.candidate)}
        output = json.dumps(report, indent=2, sort_keys=True) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(output, encoding="utf-8")
        else:
            sys.stdout.write(output)
        return 0 if report["passed"] else 2
    except (OSError, ValueError) as error:
        print(f"trace comparison failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
