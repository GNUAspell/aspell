#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from word_api import Aspell

parser = argparse.ArgumentParser()
parser.add_argument('--local', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
base = ['aspell', '--lang=sv', '--encoding=utf-8', '--conf=/dev/null', '--per-conf=/dev/null']
directory = None
if args.local:
    directory = args.local.resolve()
    base += ['--local-data-dir='+str(directory), '--dict-dir='+str(directory), '--master=sv.rws']
vocabulary = json.loads((root/'tests/vocabulary.json').read_text())
corpus = json.loads((root/'tests/swedish-rules.json').read_text())
word_cases = [case for case in corpus['cases'] if case['scope'] == 'word']
with tempfile.TemporaryDirectory(prefix='aspell-sv-check-') as temp:
    engine = Aspell(directory, Path(temp))
    try:
        rejected = [word for word in vocabulary['positive'] if not engine.spell(word)]
        accepted = [word for word in vocabulary['negative'] if engine.spell(word)]
        assert not rejected, ('Correct forms rejected', rejected)
        assert not accepted, ('Misspellings accepted', accepted)
        failed_cases = [case['id'] for case in word_cases
                        if engine.spell(case['text']) != case['expected_accept']]
        assert not failed_cases, ('Sourced word cases failed', failed_cases)
    finally:
        engine.close()
expected = set((root/'sv.wl').read_text().splitlines())
dump = subprocess.run(base+['dump','master'], text=True, capture_output=True, check=True)
assert set(dump.stdout.splitlines()) == expected
corrections = json.loads((root/'lexical-corrections.json').read_text())['corrections']
assert not expected & {c['word'] for c in corrections}
print(f'PASS: {len(vocabulary["positive"])} positive, {len(vocabulary["negative"])} negative; {len(expected)} compiled forms')
print(f'PASS: {len(word_cases)} sourced word cases; context examples are not scored')
