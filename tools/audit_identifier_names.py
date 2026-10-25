import argparse
import bisect
import json
import re
from collections import Counter
from pathlib import Path

SKIPPED = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
CATEGORIES = {
    'inherited_address': re.compile(r'\b(?:sub|byte|word|dword|qword|unk|stru|off)_[0-9A-Fa-f]+\b'),
    'offset_field': re.compile(r'\bfield_[0-9A-Fa-f]+\b'),
    'anonymous_parameter': re.compile(r'\ba\d+\b'),
    'anonymous_temporary': re.compile(r'\bv\d+\b'),
    'stub': re.compile(r'\b\w+_stub\b'),
    'numeric_constant': re.compile(r'\b(?:[A-Z][A-Z0-9_]*_0x[0-9A-Fa-f]+|MM_WINDOW_\d+|[A-Z][A-Z0-9_]*_STATE_\d+)\b'),
}

def mask_non_code(source):
    return SKIPPED.sub(lambda match: re.sub(r'[^\r\n]', ' ', match.group()), source)

def function_spans(masked):
    spans = []
    declaration = re.compile(r'(?m)^[ \t]*(?:static\s+)?[A-Za-z_]\w*[ \t*]+(\w+)\s*\(([^;{}]*)\)\s*([;{])')
    for match in declaration.finditer(masked):
        if match.group(3) != '{':
            continue
        depth = 1
        end = match.end()
        while end < len(masked) and depth:
            depth += (masked[end] == '{') - (masked[end] == '}')
            end += 1
        spans.append((match.start(), end, match.group(1)))
    return spans

def audit(root):
    entries = []
    for directory in ('src', 'first_party'):
        base = root / directory
        if not base.exists():
            continue
        for path in sorted(base.rglob('*')):
            if path.suffix not in ('.c', '.h'):
                continue
            source = path.read_bytes().decode('utf-8')
            masked = mask_non_code(source)
            spans = function_spans(masked)
            line_starts = [0] + [match.end() for match in re.finditer('\n', masked)]
            for category, pattern in CATEGORIES.items():
                for match in pattern.finditer(masked):
                    entries.append({'category': category, 'file': path.relative_to(root).as_posix(), 'line': bisect.bisect_right(line_starts, match.start()), 'symbol': match.group(), 'function': next((name for start, end, name in spans if start <= match.start() < end), None)})
    counts = Counter(entry['category'] for entry in entries)
    summary = {category: {'occurrences': counts[category], 'distinct_spellings': len({entry['symbol'] for entry in entries if entry['category'] == category}), 'files': len({entry['file'] for entry in entries if entry['category'] == category})} for category in CATEGORIES}
    return {'scope': ['src/**/*.c', 'src/**/*.h', 'first_party/**/*.c', 'first_party/**/*.h'], 'counting': 'Lexical occurrences outside comments and string/character literals. Repeated spellings can identify different scoped entities. Categories are review candidates, not confirmed defects.', 'summary': summary, 'entries': entries}

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result['summary'], indent=2))
