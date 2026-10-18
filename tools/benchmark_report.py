"""Summarize completed CE captures without mixing sessions or interval quantiles."""

import argparse
import csv
import json
import html
import math
from pathlib import Path

PHASES_V1 = ("warmup", "idle_opaque_full", "walk_1", "walk_2", "walk_3", "walk_4",
             "idle_opaque_hidden", "idle_translucent_full", "zoom_075", "rumors",
             "conversations", "quest_index", "schematics")
PHASES_V2 = PHASES_V1 + ("quest_index_repeat", "conversations_repeat", "schematics_repeat")


def metadata(path):
    runs = {}
    current = None
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key == "run":
            current = runs.setdefault(value, {})
        elif current is not None:
            current[key] = value
    return runs


def rows(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def analyze(directory, run_id=None):
    directory = Path(directory)
    phase_rows = rows(directory / "benchmark-phases.csv")
    finished = [row["run"] for row in phase_rows if row["phase"] == "complete" and row["status"] == "finished"]
    if run_id is None:
        if not finished:
            raise ValueError("Aucun scénario terminé : aucun gain ne peut être calculé.")
        run_id = finished[-1]
    run_id = str(run_id)
    if run_id not in finished:
        raise ValueError(f"Le scénario {run_id} n'est pas terminé avec finished.")
    terminal = [row for row in phase_rows if row["run"] == run_id and row["phase"] == "complete"]
    if len(terminal) != 1 or terminal[0]["status"] != "finished":
        raise ValueError("Fin de scénario répétée ou contradictoire.")
    info = metadata(directory / "benchmark-runs.txt").get(run_id, {})
    expected = {"1": PHASES_V1, "2": PHASES_V2}.get(info.get("scenario"))
    if expected is None:
        raise ValueError("Version du scénario absente ou non prise en charge.")
    selected = [row for row in phase_rows if row["run"] == run_id and row["phase"] != "complete"]
    if tuple(row["phase"] for row in selected) != expected or any(row["status"] != "ok" for row in selected):
        raise ValueError("Phases manquantes, répétées, réordonnées ou en échec.")
    samples = [row for row in rows(directory / "benchmark-performance.csv") if row["session"] == run_id]
    phases = {}
    used = set()
    interval_phases = {}
    for phase in selected:
        first, last = int(phase["first_interval"]), int(phase["last_interval"])
        if first < 0 or first > last or last - first > 100000 or int(phase["frames"]) <= 0:
            raise ValueError("Plage d'intervalles ou nombre de frames invalide.")
        intervals = set(range(first, last + 1))
        if used & intervals:
            raise ValueError("Intervalles partagés entre plusieurs phases.")
        used |= intervals
        interval_phases.update((interval, phase["phase"]) for interval in intervals)
        capture = [row for row in samples if int(row["interval"]) in intervals]
        if {int(row["interval"]) for row in capture} != intervals:
            raise ValueError("Mesures manquantes dans une phase terminée.")
        timings, counters = {}, {}
        seen = set()
        for row in capture:
            name = row["name"]
            identity = (row["interval"], row["kind"], name)
            if identity in seen:
                raise ValueError("Mesure répétée dans le même intervalle.")
            seen.add(identity)
            if row["kind"] == "counter":
                value = int(row["value"])
                if value < 0: raise ValueError("Compteur négatif.")
                counters[name] = counters.get(name, 0) + value
            elif row["kind"] == "timing":
                count = int(row["count"])
                mean, maximum = float(row["mean_ms"]), float(row["max_ms"])
                if count <= 0 or not math.isfinite(mean) or not math.isfinite(maximum) or mean < 0 or maximum < mean:
                    raise ValueError("Durée ou nombre d'appels invalide.")
                bucket = timings.setdefault(name, {"count": 0, "total_ms": 0.0, "max_ms": 0.0})
                bucket["count"] += count
                bucket["total_ms"] += count * mean
                bucket["max_ms"] = max(bucket["max_ms"], maximum)
        for bucket in timings.values():
            bucket["mean_ms"] = bucket["total_ms"] / bucket["count"] if bucket["count"] else 0.0
        phases[phase["phase"]] = {"frames": int(phase["frames"]), "timings": timings, "counters": counters}
    slow_resources = []
    detail_path = directory / "benchmark-performance.csv.details.csv"
    if detail_path.exists():
        for row in rows(detail_path):
            if row["session"] != run_id:
                continue
            interval = int(row["interval"])
            if interval not in interval_phases:
                continue
            duration = float(row["milliseconds"])
            if not math.isfinite(duration) or duration < 5.0:
                raise ValueError("Durée de diagnostic ART invalide.")
            slow_resources.append({"phase": interval_phases[interval], "interval": interval,
                                   "metric": row["metric"], "milliseconds": duration, "resource": row["resource"]})
    verified = info.get("file.Save/Current/data.sav", "").startswith("crc32:")
    return {"run": run_id, "metadata": info, "phases": phases, "save_verified": verified,
            "ignored_runs": sorted({row["run"] for row in phase_rows if row["run"] != run_id}),
            "slow_resources": slow_resources}


def comparison_errors(current, baseline):
    errors = []
    if not current["save_verified"] or not baseline["save_verified"]:
        errors.append("Empreinte data.sav absente : identité des sauvegardes non vérifiable.")
    a, b = current["metadata"], baseline["metadata"]
    keys = {"scenario", "save", "map", "seed", "platform", "conversations"}
    keys |= {key for key in a.keys() | b.keys() if key.startswith(("setting.", "file.", "waypoint"))}
    # No barter phase or startup screen occurs inside the fixed scenario.
    # Keep all scene/render/font/gameplay settings strict.
    keys -= {"setting.performance_logging", "setting.benchmark_autorun", "setting.splash",
             "setting.barter_color_better", "setting.barter_color_worse", "setting.barter_color_equal"}
    for key in sorted(keys):
        if key not in a or key not in b or a[key] != b[key] or (key.startswith("file.") and a[key].startswith("error")):
            errors.append(f"Entrée différente, absente ou illisible : {key}.")
    return errors


def markdown(report, baseline=None):
    text = [f"# Relevé de performances — scénario {report['run']}", "",
            "Seules les phases validées du scénario sélectionné sont regroupées.",
            "Les moyennes sont pondérées par le nombre d'appels ; les maxima restent des maxima.",
            "Les percentiles d'intervalles ne sont pas transformés en percentiles globaux.",
            "Les temps sont inclusifs ; la présentation comprend l'attente VSync.", ""]
    if not report["save_verified"]:
        text += ["**Empreinte de sauvegarde absente : comparaison avant/après non vérifiable.**", ""]
    errors = comparison_errors(report, baseline) if baseline is not None else []
    if errors:
        text += ["Comparaison refusée :", ""] + [f"- {error}" for error in errors] + [""]
    elif baseline is not None:
        text += [f"Référence compatible : {baseline['run']}. Comparer aussi machine, fichiers du jeu et versions d'exécutable.", ""]
    for name, phase in report["phases"].items():
        if name == "warmup":
            continue
        text += [f"## {name}", "", "| Mesure | Appels | Moyenne (ms) | Maximum (ms) |",
                 "|---|---:|---:|---:|"]
        for metric, values in phase["timings"].items():
            text.append(f"| {metric} | {values['count']} | {values['mean_ms']:.4f} | {values['max_ms']:.4f} |")
        nonzero = "; ".join(f"{key}={value}" for key, value in phase["counters"].items() if value)
        text += ["", "Compteurs non nuls : " + (nonzero or "aucun") + ".", ""]
        if baseline is not None and not errors and name in baseline["phases"]:
            text += ["Écarts de moyennes par appel (nouvelle version moins référence) :", ""]
            for metric, values in phase["timings"].items():
                old = baseline["phases"][name]["timings"].get(metric)
                if old is not None:
                    text.append(f"- {metric} : {values['mean_ms'] - old['mean_ms']:+.4f} ms ; appels {old['count']} → {values['count']}.")
            text += [""]
    if report.get("slow_resources"):
        text += ["## Opérations ART lentes (au moins 5 ms)", "",
                 "Ces durées sont inclusives et peuvent se chevaucher : ne pas les additionner.", "",
                 "| Phase | Intervalle | Opération | Durée (ms) | Ressource |", "|---|---:|---|---:|---|"]
        for row in sorted(report["slow_resources"], key=lambda row: row["milliseconds"], reverse=True):
            resource = html.escape(row["resource"]).replace("|", "&#124;").replace("\n", " ")
            text.append(f"| {row['phase']} | {row['interval']} | {row['metric']} | {row['milliseconds']:.4f} | {resource} |")
        text += [""]
    return "\n".join(text) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--run")
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--baseline-run")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = analyze(args.capture, args.run)
        baseline = analyze(args.baseline, args.baseline_run) if args.baseline else None
    except (ValueError, KeyError, OSError) as error:
        parser.exit(2, f"Rapport impossible : {error}\n")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(markdown(report, baseline), encoding="utf-8")
    args.output.with_suffix(".json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Rapport : {args.output.resolve()}")


if __name__ == "__main__":
    main()
