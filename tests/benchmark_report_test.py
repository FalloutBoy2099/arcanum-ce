import csv
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import benchmark_report as report


class ReportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.phases = []
        self.samples = []
        for run in ("100", "200"):
            for interval, name in enumerate(report.PHASES_V2):
                self.phases.append(dict(run=run, phase=name, status="ok", first_interval=interval * 2,
                                        last_interval=interval * 2 + 1, frames=180))
                for offset, count, mean in ((0, 2, 10), (1, 6, 2)):
                    self.samples.append(dict(session=run, interval=interval * 2 + offset, kind="timing",
                                             name="world_render", count=count, mean_ms=mean if run == "200" else 999,
                                             max_ms=20 if run == "200" else 999, value=0))
                    self.samples.append(dict(session=run, interval=interval * 2 + offset, kind="counter",
                                             name="text_cache_hits", count=0, mean_ms=0, max_ms=0, value=7))
            self.phases.append(dict(run=run, phase="complete", status="finished", first_interval=31,
                                    last_interval=32, frames=0))
        self.meta = "".join(f"run={run}\nscenario=2\nsave=slot\nmap=1\nseed=fixed\nplatform=Windows\nconversations=2\nfile.Save/Current/data.sav=crc32:1234:50\nsetting.resolution=1280x960\n" for run in ("100", "200"))
        self.write()

    def write(self):
        for name, values in (("benchmark-phases.csv", self.phases), ("benchmark-performance.csv", self.samples)):
            with (self.root / name).open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=list(values[0]))
                writer.writeheader()
                writer.writerows(values)
        (self.root / "benchmark-runs.txt").write_text(self.meta)

    def test_sessions_and_weighted_means(self):
        result = report.analyze(self.root)
        self.assertEqual(result["run"], "200")
        phase = result["phases"]["walk_1"]
        self.assertEqual(phase["timings"]["world_render"]["mean_ms"], 4)
        self.assertEqual(phase["timings"]["world_render"]["count"], 8)
        self.assertEqual(phase["counters"]["text_cache_hits"], 14)
        self.assertEqual(report.comparison_errors(result, report.analyze(self.root, "100")), [])

    def test_failed_runs_and_absent_metrics(self):
        self.phases[-1]["status"] = "aborted"
        self.write()
        self.assertEqual(report.analyze(self.root)["run"], "100")
        with self.assertRaises(ValueError):
            report.analyze(self.root, "200")

    def test_incomplete_duplicate_and_overlap(self):
        original = [row.copy() for row in self.phases]
        self.phases.pop(-2)
        self.write()
        with self.assertRaises(ValueError): report.analyze(self.root)
        self.phases = [row.copy() for row in original]
        self.phases[-2]["first_interval"] = 0
        self.write()
        with self.assertRaises(ValueError): report.analyze(self.root)
        self.phases = [row.copy() for row in original]
        self.phases.insert(-1, self.phases[-2].copy())
        self.write()
        with self.assertRaises(ValueError): report.analyze(self.root)

    def test_missing_samples(self):
        self.samples = [row for row in self.samples if not (row["session"] == "200" and row["interval"] == 5)]
        self.write()
        with self.assertRaises(ValueError): report.analyze(self.root)

    def test_duplicate_samples(self):
        self.samples.append(self.samples[-1].copy())
        self.write()
        with self.assertRaises(ValueError): report.analyze(self.root)

    def test_invalid_timing(self):
        self.samples[-2]["mean_ms"] = "nan"
        self.write()
        with self.assertRaises(ValueError): report.analyze(self.root)

    def test_incompatible_input_and_missing_fingerprint(self):
        result = report.analyze(self.root)
        baseline = report.analyze(self.root, "100")
        baseline["metadata"]["setting.resolution"] = "800x600"
        self.assertTrue(report.comparison_errors(result, baseline))
        baseline["save_verified"] = False
        self.assertIn("Comparaison refusée", report.markdown(result, baseline))
        result["save_verified"] = False
        self.assertIn("non vérifiable", report.markdown(result))

    def test_settings_unused_by_scenario(self):
        result = report.analyze(self.root)
        baseline = report.analyze(self.root, "100")
        for key in ("splash", "barter_color_better", "barter_color_worse", "barter_color_equal"):
            result["metadata"]["setting." + key] = "new value"
        self.assertEqual(report.comparison_errors(result, baseline), [])
        for key in ("lighting", "modern_font_size", "zoom_filter", "vsync_mode"):
            result["metadata"]["setting." + key] = "different"
            self.assertTrue(report.comparison_errors(result, baseline))
            del result["metadata"]["setting." + key]

    def test_slow_resources_follow_run_and_phase(self):
        path = self.root / "benchmark-performance.csv.details.csv"
        with path.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=("session", "interval", "metric", "milliseconds", "resource"))
            writer.writeheader()
            writer.writerows([
                dict(session="100", interval=16, metric="art_load", milliseconds=99, resource="wrong-run.art"),
                dict(session="200", interval=16, metric="art_load", milliseconds=12.5, resource='art/slow,"quoted".art'),
                dict(session="200", interval=999, metric="art_load", milliseconds=99, resource="outside-phase.art"),
            ])
        result = report.analyze(self.root)
        self.assertEqual(len(result["slow_resources"]), 1)
        self.assertEqual(result["slow_resources"][0]["phase"], "zoom_075")
        self.assertEqual(result["slow_resources"][0]["resource"], 'art/slow,"quoted".art')
        self.assertIn("12.5000", report.markdown(result))
        self.assertNotIn("wrong-run.art", report.markdown(result))
        self.assertNotIn("outside-phase.art", report.markdown(result))
        path.write_text("session,interval,metric,milliseconds,resource\n200,16,art_load,nan,bad.art\n")
        with self.assertRaises(ValueError): report.analyze(self.root)

    def test_v1_archive(self):
        self.phases = [row for row in self.phases if row["phase"] not in report.PHASES_V2[13:]]
        self.meta = self.meta.replace("scenario=2", "scenario=1")
        self.write()
        self.assertEqual(len(report.analyze(self.root)["phases"]), 13)


if __name__ == "__main__":
    unittest.main()
