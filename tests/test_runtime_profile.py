import importlib.util
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("runtime_profile", Path(__file__).parents[1] / "tools/analyze_runtime_profile.py")
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


class RuntimeProfileTest(unittest.TestCase):
    def test_weighted_means_and_window_percentiles(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.csv"
            path.write_text(
                "session,kind,name,count,mean_ms,p99_ms,max_ms,value\n"
                "1,timing,frame_work,10,10,20,30,0\n"
                "1,timing,frame_work,30,20,25,40,0\n"
                "1,counter,sector_cache_misses,0,0,0,0,3\n"
                "2,counter,sector_cache_misses,0,0,0,0,4\n", encoding="utf-8")
            result = profile.analyze_capture(path)
            self.assertEqual(result["sessions"], 2)
            self.assertEqual(result["metrics"]["frame_work"]["mean_ms"], 17.5)
            self.assertEqual(result["metrics"]["frame_work"]["worst_window_p99_ms"], 25)
            self.assertEqual(result["counters"]["sector_cache_misses"], 7)

    def test_combat_pauses_sessions_and_turn_based(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.csv"
            rows = [
                "1,0,pause_state,0,0,0,0", "1,10,attack_begin,42,8,30,0",
                "1,50,attack_release,42,8,0,0", "1,90,attack_release,42,8,0,0",
                "1,100,attack_end,42,8,0,0", "1,110,attack_begin,42,9,30,0",
                "1,120,pause_state,0,0,1,0", "1,900,attack_end,42,9,0,0",
                "1,910,pause_state,0,0,0,0", "1,920,attack_begin,42,10,30,1",
                "1,1000,attack_end,42,10,0,1", "2,10,attack_end,42,8,0,0",
            ]
            path.write_text("session,timestamp_ms,event,subject,correlation,value,auxiliary\n" + "\n".join(rows), encoding="utf-8")
            result = profile.analyze_events(path)
            self.assertEqual(result["realtime_animation_spans"]["count"], 1)
            self.assertEqual(result["realtime_animation_spans"]["mean_ms"], 90)
            self.assertEqual(result["realtime_release_intervals_same_run"]["mean_ms"], 40)
            self.assertEqual(result["ends_without_begin"], 1)

    def test_session_selection(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.csv"
            path.write_text("session,kind,name,count,mean_ms,p99_ms,max_ms,value\n"
                "1,timing,frame_work,1,100,100,100,0\n"
                "2,timing,frame_interval,1,10,10,10,0\n"
                "2,timing,frame_work,1,12,12,12,0\n", encoding="utf-8")
            result = profile.analyze_capture(path, instrumented=True)
            self.assertEqual(result["session_ids"], ["2"])
            self.assertEqual(result["metrics"]["frame_work"]["mean_ms"], 12)
            explicit = profile.analyze_capture(path, {"1"})
            self.assertEqual(explicit["metrics"]["frame_work"]["mean_ms"], 100)

    def test_slow_callbacks_units_domains_and_details(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.csv"
            path.write_text("session,kind,name,count,mean_ms,p99_ms,max_ms,value\n"
                "1,timing,frame_interval,1,10,10,10,0\n", encoding="utf-8")
            Path(str(path) + ".events.csv").write_text(
                "session,timestamp_ms,event,subject,correlation,value,auxiliary\n"
                "1,10,slow_callback,10,1,12500,0\n"
                "1,20,slow_callback,10,1,25000,1\n"
                "1,30,slow_callback,1,2,6000,0\n"
                "2,40,slow_callback,10,1,999999,0\n", encoding="utf-8")
            Path(str(path) + ".details.csv").write_text(
                "session,interval,metric,milliseconds,resource\n"
                "1,0,event_callback,12.5,AI\n"
                "1,0,event_callback,25,AI\n"
                "1,0,event_recovery,6,Anim\n"
                "2,0,event_callback,999,AI\n", encoding="utf-8")
            result = profile.analyze_capture(path, {"1"})
            ai = next(x for x in result["events"]["slow_callbacks"] if x["event_type"] == "10")
            self.assertEqual(ai["time_domain"], "1")
            self.assertEqual(ai["count"], 2)
            self.assertEqual(ai["mean_ms"], 18.75)
            detail = next(x for x in result["slow_event_details"] if x["event_name"] == "AI")
            self.assertEqual(detail["total_observed_ms"], 37.5)
            self.assertEqual(detail["count"], 2)

    def test_invalid_timing_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.csv"
            path.write_text("session,kind,name,count,mean_ms,p99_ms,max_ms,value\n1,timing,path,1,nan,0,0,0\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                profile.analyze_capture(path)

    def test_utility_decisions_and_fatigue_forecast(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.csv"
            path.write_text("session,timestamp_ms,event,subject,correlation,value,auxiliary\n"
                "1,10,utility_ai,42,43,12,160\n"
                "1,10,utility_ai_detail,42,43,4,8\n"
                "1,20,utility_ai,42,42,-1,170\n"
                "1,20,utility_ai_detail,42,42,5,2\n"
                "1,30,utility_ai,42,43,-2,35\n"
                "1,30,utility_ai_detail,42,43,0,-2\n"
                "1,40,utility_ai,42,43,20,-120\n"
                "1,45,utility_ai,42,43,-3,85\n"
                "1,45,utility_ai_detail,42,43,8,4\n"
                "1,46,utility_ai,42,43,-4,120\n"
                "1,46,utility_ai_detail,42,43,9,8\n"
                "2,50,utility_ai,42,43,20,120\n", encoding="utf-8")
            result = profile.analyze_events(path, {"1"})["utility_ai"]
            self.assertEqual(result["decisions_by_mode"]["active"], {"spell": 1, "item": 1, "normal_attack": 1, "retreat": 1, "release_maintenance": 1})
            self.assertEqual(result["decisions_by_mode"]["observation"], {"spell": 1})
            self.assertEqual(result["action_counts"], {"heal": 1, "recover_fatigue": 1, "normal_attack": 1, "retreat": 1, "release_maintenance": 1})
            self.assertEqual(result["projected_fatigue"]["minimum_points"], -2)
            self.assertAlmostEqual(result["projected_fatigue"]["mean_points"], 4)


if __name__ == "__main__":
    unittest.main()
