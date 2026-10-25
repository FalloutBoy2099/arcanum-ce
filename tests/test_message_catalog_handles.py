from collections import Counter
from pathlib import Path
import re
import unittest


def duplicate_handles(source):
    names = re.findall(r"^static\s+mes_file_handle_t\s+(\w+)\s*;", source, re.MULTILINE)
    return {name for name, count in Counter(names).items() if count > 1}


class MessageCatalogHandleTest(unittest.TestCase):
    def test_detects_tentative_declarations_sharing_storage(self):
        source = "static mes_file_handle_t spell_catalog;\nstatic mes_file_handle_t spell_catalog;\n"
        self.assertEqual(duplicate_handles(source), {"spell_catalog"})
        self.assertFalse(duplicate_handles("static mes_file_handle_t spell_text;\nstatic mes_file_handle_t spell_rules;\n"))

    def test_repository_catalog_handles_have_distinct_storage(self):
        for path in (Path(__file__).parents[1] / "src").rglob("*.c"):
            with self.subTest(path=str(path)):
                self.assertFalse(duplicate_handles(path.read_text(encoding="utf-8")))


if __name__ == "__main__":
    unittest.main()
