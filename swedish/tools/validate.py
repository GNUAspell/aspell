#!/usr/bin/env python3
"""Run local integration checks: validate.py HUNSPELL_REPO [ASPELL_PACKAGE]."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

hun = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[1]
package = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else root
base = ['aspell', '--lang=sv', '--encoding=utf-8', '--conf=/dev/null', '--per-conf=/dev/null', '--personal=/dev/null', '--local-data-dir='+str(package), '--dict-dir='+str(package), '--master=sv.rws']
vocabulary = json.loads((root/'tests/vocabulary.json').read_text())
positive, negative = set(vocabulary['positive']), set(vocabulary['negative'])
report = {'positive_words':sorted(positive), 'negative_words':sorted(negative), 'checks':{}}
for name, command in [('hunspell', ['hunspell','-i','UTF-8','-d',str(hun/'sv_SE'),'-p','/dev/null','-l']), ('aspell',base+['list'])]:
    result = subprocess.run(command, input='\n'.join(sorted(positive|negative))+'\n', text=True, capture_output=True, check=True)
    rejected = set(result.stdout.splitlines())
    report['checks'][name] = {'correct_words_rejected':sorted(rejected & positive), 'misspellings_accepted':sorted(negative - rejected), 'stderr':result.stderr}
    assert not (negative - rejected), report['checks'][name]
    assert not result.stderr, result.stderr
    assert not (rejected & positive), report['checks'][name]
# Check every compiled spelling, not just the count.
dump = subprocess.run(base+['dump','master'], text=True, capture_output=True, check=True)
word_file = 'sv.wl' if (package/'sv.wl').exists() else 'merged.txt'
expected = set((package/word_file).read_text().splitlines())
actual = set(dump.stdout.splitlines())
assert actual == expected
report['compiled_forms_verified'] = len(actual)
# Affix file headers must agree with the number of rules in each group.
for path in hun.glob('*.aff'):
    headers, counts = {}, {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if fields and fields[0] in ('SFX','PFX'):
            key = tuple(fields[:2])
            if len(fields) == 4:
                assert key not in headers
                headers[key] = int(fields[3])
            else:
                counts[key] = counts.get(key,0)+1
    assert headers == counts, (path.name,headers,counts)
report['affix_rule_counts'] = 'both affix files match their headers'
report['sha256'] = {name:hashlib.sha256((package/name).read_bytes()).hexdigest() for name in [word_file,'sv.rws','sv.dat']}
(package/'verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
print(json.dumps(report['checks'],ensure_ascii=False,indent=2))
print('Compiled forms verified:',len(actual))
