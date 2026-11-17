"""Run the frozen, headless SID register-log comparison (exit 1 means excluded)."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "src/tests/sid_register_logs.json"
FIXTURE_SHA256 = "64dc216cd5d7b80d0bd2fd9c20f70be9e4233432446acac1b4bfe08602ed3c8c"
LIMITS = {"8580": {"relative_rms_error": 0.10, "uncentered_correlation": 0.995},
          "6581": {"relative_rms_error": 0.25, "uncentered_correlation": 0.97}}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def fixture() -> dict:
    if sha256(FIXTURE) != FIXTURE_SHA256:
        raise ValueError("register-log fixture differs from frozen contract revision 1")
    data = json.loads(FIXTURE.read_text(encoding="utf-8"))
    if (data["schema"], data["contract"], data["clock_hz"]) != (1, 1, 985248):
        raise ValueError("unsupported fixture contract")
    ids: set[str] = set()
    for log in data["logs"]:
        if log["id"] in ids or not all(c.isalnum() or c == "-" for c in log["id"]):
            raise ValueError("invalid or duplicate fixture id")
        ids.add(log["id"])
        if len(log["voices"]) != 3 or not 8192 <= log["cycles"] <= 2_000_000:
            raise ValueError("invalid fixture dimensions")
        for voice in log["voices"]:
            if len(voice) != 5 or not (0 <= voice[0] <= 65535 and 0 <= voice[1] <= 4095
                                      and all(0 <= v <= 255 for v in voice[2:])):
                raise ValueError("invalid voice setup")
        previous = 20
        for event in log["events"]:
            cycle, kind, reg = event[:3]
            if not (previous < cycle < log["cycles"] and kind in ("w", "r") and 0 <= reg <= 31):
                raise ValueError("fixture must contain one ordered bus access per cycle")
            if len(event) != (4 if kind == "w" else 3) or (kind == "w" and not 0 <= event[3] <= 255):
                raise ValueError("invalid register access")
            previous = cycle
        segment = log["audio_segment_cycles"]
        if log["analog"] != bool(segment) or (segment and log["cycles"] % segment):
            raise ValueError("audio segments must cover the complete log")
    return data


def events_text(log: dict, hz: int) -> str:
    """Initial voice setup consumes 21 real cycles; AD/SR precede gate."""
    lines = [f'{log["cycles"]} {log["audio_segment_cycles"]} {hz}']
    cycle = 0
    for voice, (frequency, width, control, ad, sr) in enumerate(log["voices"]):
        setup = [(0, frequency & 255), (1, frequency >> 8), (2, width & 255),
                 (3, width >> 8), (5, ad), (6, sr), (4, control)]
        for reg, value in setup:
            lines.append(f"{cycle} w {voice * 7 + reg} {value}")
            cycle += 1
    for event in log["events"]:
        at, kind, reg = event[:3]
        value = event[3] if kind == "w" else 0
        lines.append(f"{at} {kind} {reg} {value}")
    return "\n".join(lines) + "\n"


def qualify(result: dict, log: dict) -> dict:
    expected_checks = {"phase24", "noise23", "envelope8", "osc3_per_voice8",
                       "env3_per_voice8", "guest_reads", "save_load"}
    if (result.get("schema") != 1 or result.get("completed") is not True
            or result.get("cycles") != log["cycles"]
            or set(result.get("digital", {})) != expected_checks):
        raise ValueError("incomplete native report")
    digital = all(check["mismatches"] == 0 for check in result["digital"].values())
    waveform12 = result.get("waveform12", {}).get("status") == "exact" and (
        result["waveform12"].get("checked") == 3 * log["cycles"]
        and result["waveform12"].get("mismatches") == 0)
    if result["continuation_cycles"] < 4096:
        raise ValueError("insufficient exact candidate continuation")
    limits = LIMITS[str(result["model"])]
    expected_segments = log["cycles"] // log["audio_segment_cycles"] if log["analog"] else 0
    if len(result["audio"]) != expected_segments:
        raise ValueError("missing audio segments")
    audio = True
    for index, segment in enumerate(result["audio"]):
        if (segment["begin"] != index * log["audio_segment_cycles"]
                or segment["cycles"] != log["audio_segment_cycles"]):
            raise ValueError("audio segment timing changed")
        segment["passed"] = (segment["relative_rms_error"] <= limits["relative_rms_error"]
                             and segment["uncentered_correlation"] >= limits["uncentered_correlation"])
        audio = audio and segment["passed"]
    return {"observable_digital": digital, "waveform12": waveform12,
            "audio": audio if log["analog"] else None,
            "qualified": digital and waveform12 and audio}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "work/build-x64/sid_compare.exe")
    parser.add_argument("--out", type=Path, default=ROOT / "work/sid-comparison")
    parser.add_argument("--validate", action="store_true", help="validate the frozen fixture without running an engine")
    args = parser.parse_args()
    data = fixture()
    if args.validate:
        print(f'SID comparison contract 1: {len(data["logs"])} logs, fixture SHA-256 {FIXTURE_SHA256}')
        return 0
    binary = args.binary.resolve()
    if not binary.is_file():
        raise ValueError(f"native comparator missing: {binary}")
    output = args.out.resolve()
    if not output.is_relative_to((ROOT / "work").resolve()):
        raise ValueError("comparison output must stay under this repository's ignored work directory")
    output.mkdir(parents=True, exist_ok=True)
    report = {"schema": 1, "contract": 1, "fixture_sha256": FIXTURE_SHA256,
              "contract_sha256": sha256(ROOT / "docs/SID-COMPARISON.md"),
              "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
              "limits": LIMITS, "clock_hz": data["clock_hz"], "results": []}
    for log in data["logs"]:
        events = output / f'{log["id"]}.txt'
        events.write_text(events_text(log, data["clock_hz"]), encoding="ascii")
        for model in (6581, 8580):
            native = output / f'{log["id"]}-{model}.json'
            subprocess.run([str(binary), "--log", str(events), "--model", str(model),
                            "--out", str(native)], cwd=output, check=True, timeout=120)
            result = json.loads(native.read_text(encoding="ascii"))
            if result.get("model") != model:
                raise ValueError("native report has wrong model")
            result["id"] = log["id"]
            result["events_sha256"] = sha256(events)
            result["gates"] = qualify(result, log)
            report["results"].append(result)
            print(f'{model} {log["id"]}: digital={result["gates"]["observable_digital"]} '
                  f'waveform12={result["waveform12"]["status"]} audio={result["gates"]["audio"]}', flush=True)
    report["completed"] = True
    report["qualified"] = all(row["gates"]["qualified"] for row in report["results"])
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f'Comparison complete; candidate {"qualified" if report["qualified"] else "excluded"}. Report: {output / "report.json"}')
    return 0 if report["qualified"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit(f"SID comparison error: {error}") from error
