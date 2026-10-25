import argparse
import csv
import json
import math
from collections import Counter, defaultdict
from pathlib import Path


def distribution(values):
    if not values:
        return {"count": 0}
    values = sorted(values)
    return {
        "count": len(values),
        "mean_ms": sum(values) / len(values),
        "p50_ms": values[math.ceil(len(values) * 0.50) - 1],
        "p95_ms": values[math.ceil(len(values) * 0.95) - 1],
        "p99_ms": values[math.ceil(len(values) * 0.99) - 1],
        "max_ms": values[-1],
    }


def analyze_capture(path, session_filter=None, instrumented=False):
    metrics = {}
    counters = Counter()
    sessions = set()
    with Path(path).open(newline="", encoding="utf-8-sig") as source:
        rows = list(csv.DictReader(source))
    if instrumented:
        session_filter = {row["session"] for row in rows if row["name"] == "frame_interval"}
    for row in rows:
        if session_filter is not None and row["session"] not in session_filter:
            continue
        sessions.add(row["session"])
        name = row["name"]
        if row["kind"] == "counter":
            counters[name] += int(row["value"])
        elif row["kind"] == "timing":
            count = int(row["count"])
            mean = float(row["mean_ms"])
            p99 = float(row["p99_ms"])
            maximum = float(row["max_ms"])
            if count < 0 or not all(math.isfinite(x) and x >= 0 for x in (mean, p99, maximum)):
                raise ValueError(f"Invalid timing: {name}")
            bucket = metrics.setdefault(name, {"count": 0, "total_ms": 0.0, "max_ms": 0.0, "worst_window_p99_ms": 0.0})
            bucket["count"] += count
            bucket["total_ms"] += mean * count
            bucket["max_ms"] = max(bucket["max_ms"], maximum)
            bucket["worst_window_p99_ms"] = max(bucket["worst_window_p99_ms"], p99)
    for bucket in metrics.values():
        bucket["mean_ms"] = bucket["total_ms"] / bucket["count"] if bucket["count"] else 0.0
    result = {"capture": str(path), "sessions": len(sessions), "session_ids": sorted(sessions), "metrics": metrics, "counters": dict(counters)}
    event_path = Path(str(path) + ".events.csv")
    if event_path.exists():
        result["events"] = analyze_events(event_path, session_filter)
    detail_path = Path(str(path) + ".details.csv")
    if detail_path.exists():
        result["slow_event_details"] = analyze_event_details(detail_path, session_filter)
    return result


def analyze_event_details(path, session_filter=None):
    groups = defaultdict(list)
    with Path(path).open(newline="", encoding="utf-8-sig") as source:
        for row in csv.DictReader(source):
            if session_filter is not None and row["session"] not in session_filter:
                continue
            if row["metric"] not in {"event_callback", "event_recovery"}:
                continue
            duration = float(row["milliseconds"])
            if not math.isfinite(duration) or duration < 0:
                raise ValueError("Invalid event detail duration")
            groups[(row["metric"], row["resource"])].append(duration)
    return [{"metric": metric, "event_name": name, "total_observed_ms": sum(values), **distribution(values)}
        for (metric, name), values in sorted(groups.items())]


def analyze_events(path, session_filter=None):
    begins = {}
    releases = {}
    paused = {}
    spans = []
    intervals = []
    counts = Counter()
    slow_callbacks = defaultdict(list)
    utility_decisions = defaultdict(Counter)
    utility_actions = Counter()
    utility_fatigue = []
    utility_action_names = {0: "normal_attack", 1: "offensive", 2: "defensive", 3: "summon",
        4: "heal", 5: "recover_fatigue", 6: "cure_poison", 7: "resurrect", 8: "retreat", 9: "release_maintenance"}
    missing_begin = 0
    with Path(path).open(newline="", encoding="utf-8-sig") as source:
        for row in csv.DictReader(source):
            if session_filter is not None and row["session"] not in session_filter:
                continue
            event = row["event"]
            counts[event] += 1
            session = row["session"]
            key = (session, row["subject"], row["correlation"])
            timestamp = float(row["timestamp_ms"])
            if not math.isfinite(timestamp) or timestamp < 0:
                raise ValueError("Invalid event timestamp")
            if event == "utility_ai":
                mode = "active" if int(row["auxiliary"]) >= 0 else "observation"
                spell = int(row["value"])
                utility_decisions[mode]["release_maintenance" if spell == -4 else "retreat" if spell == -3
                    else "normal_attack" if spell == -2 else "item" if spell == -1 else "spell"] += 1
                continue
            if event == "utility_ai_detail":
                action = int(row["value"])
                utility_actions[utility_action_names.get(action, "unknown")] += 1
                utility_fatigue.append(int(row["auxiliary"]))
                continue
            if event == "slow_callback":
                duration = float(row["value"]) / 1000.0
                if not math.isfinite(duration) or duration < 0:
                    raise ValueError("Invalid callback duration")
                slow_callbacks[(row["subject"], row["correlation"])].append(duration)
                continue
            if event == "pause_state":
                paused[session] = int(row["value"]) != 0
                begins = {k: v for k, v in begins.items() if k[0] != session}
                releases = {k: v for k, v in releases.items() if k[0] != session}
                continue
            if event not in {"attack_begin", "attack_release", "attack_end"}:
                continue
            if int(row["auxiliary"]) != 0 or paused.get(session, False):
                begins.pop(key, None)
                releases.pop(key, None)
                continue
            if event == "attack_begin":
                begins[key] = timestamp
                releases.pop(key, None)
            elif event == "attack_release":
                previous = releases.get(key)
                if previous is not None and timestamp >= previous:
                    intervals.append(timestamp - previous)
                releases[key] = timestamp
            else:
                begin = begins.pop(key, None)
                if begin is not None and timestamp >= begin:
                    spans.append(timestamp - begin)
                else:
                    missing_begin += 1
                releases.pop(key, None)
    return {
        "counts": dict(counts),
        "utility_ai": {
            "decisions_by_mode": {mode: dict(counts) for mode, counts in utility_decisions.items()},
            "action_counts": dict(utility_actions),
            "projected_fatigue": {"count": len(utility_fatigue),
                "minimum_points": min(utility_fatigue) if utility_fatigue else None,
                "mean_points": sum(utility_fatigue) / len(utility_fatigue) if utility_fatigue else None},
        },
        "slow_callbacks": [{"event_type": event_type, "time_domain": domain, **distribution(values)}
            for (event_type, domain), values in sorted(slow_callbacks.items())],
        "realtime_animation_spans": distribution(spans),
        "realtime_release_intervals_same_run": distribution(intervals),
        "unfinished_or_interrupted_begins": len(begins),
        "ends_without_begin": missing_begin,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--output", type=Path)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--session", action="append")
    selection.add_argument("--instrumented", action="store_true")
    args = parser.parse_args()
    report = {
        "notes": [
            "Timings are inclusive; nested totals must not be added.",
            "Worst window p99 is not a global percentile.",
            "Frame work includes presentation/VSync and is not CPU-only FPS.",
            "Animation spans may include multiple shots; release intervals cover a continuous run only.",
            "Combat event timings exclude turn-based samples and spans crossing recorded pauses.",
            "Nonzero trace_events_dropped means event distributions are incomplete.",
            "trace_sampled_out counts intentional AI/utility/late-dispatch omissions, not lost attack traces.",
            "Slow callback details cover recorded slow operations, not all callbacks of that type.",
        ],
        "captures": [analyze_capture(path, set(args.session) if args.session else None, args.instrumented) for path in args.captures],
    }
    output = json.dumps(report, indent=2, ensure_ascii=False)
    if args.output:
        args.output.write_text(output + "\n", encoding="utf-8")
    else:
        print(output)


if __name__ == "__main__":
    main()
