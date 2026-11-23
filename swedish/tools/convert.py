#!/usr/bin/env python3
"""Conservative conversion for the inspected hunspell-sv affix format.
Usage: python3 convert.py HUNSPELL_SOURCE ASPELL_SOURCE OUTPUT_DIRECTORY
Requires aspell, word-list-compress and libhunspell 1.7.
"""
import collections, ctypes, ctypes.util, json, pathlib, re, shutil, subprocess, sys
from check_language_policy import validate

hun, old, out = map(lambda s: pathlib.Path(s).resolve(), sys.argv[1:])
package = pathlib.Path(__file__).resolve().parents[1]
catalogue = json.loads((package/'docs/swedish-rules.json').read_text())
corrections = json.loads((hun/'lexical-corrections.json').read_text())['corrections']
# Validate source evidence before creating output or applying spelling exclusions.
# Use the bundled, reviewed catalogue so the historical source pin still works.
validate(catalogue, corrections)
out.mkdir(parents=True, exist_ok=True)
aff = hun.joinpath('sv_SE.aff').read_text()
rules = collections.defaultdict(list)
for line in aff.splitlines():
    p = line.split()
    if not p or p[0].startswith('#'):
        continue
    if p[0] in {'PFX', 'FLAG', 'AF', 'ICONV', 'OCONV', 'COMPLEXPREFIXES'}:
        raise ValueError('Unsupported feature: ' + line)
    if p[0] == 'SFX' and len(p) >= 5:
        strip, _, continuation = p[3].partition('/')
        rules[p[1]].append((p[2] if p[2] != '0' else '', strip if strip != '0' else '', continuation, re.compile('(?:' + p[4] + ')$')))
for group in rules.values():
    for _, _, flags, _ in group:
        if set(flags) & rules.keys():
            raise ValueError('Chained suffix rules need an extended converter')
lib = ctypes.CDLL(ctypes.util.find_library('hunspell-1.7'))
lib.Hunspell_create.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
lib.Hunspell_create.restype = ctypes.c_void_p
lib.Hunspell_spell.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
lib.Hunspell_spell.restype = ctypes.c_int
lib.Hunspell_destroy.argtypes = [ctypes.c_void_p]
handle = lib.Hunspell_create(str(hun/'sv_SE.aff').encode(), str(hun/'sv_SE.dic').encode())
assert handle
reviewed_forbidden = {entry['word'] for entry in corrections}
candidates, forbidden = set(), set(reviewed_forbidden)
rows = (hun/'sv_SE.dic').read_text().splitlines()
assert int(rows[0]) == len(rows)-1
for line in rows[1:]:
    word, _, flags = line.split('\t')[0].partition('/')
    if '%' in flags:
        forbidden.add(word)
        continue
    # Conservatively omit nosuggest, compound-only and forced-case entries.
    if set(flags) & set('!Zc'):
        continue
    if '¤' not in flags:
        candidates.add(word)
    for flag in flags:
        for strip, add, continuation, condition in rules.get(flag, []):
            if set(continuation) & set('Z!%¤c'):
                continue
            if condition.search(word) and (not strip or word.endswith(strip)):
                candidates.add((word[:-len(strip)] if strip else word) + add)
# Preserve reviewed project vocabulary, including finite compound spellings.
# Every entry must still pass Hunspell; this does not enable free compounding.
reviewed_path = hun/'test-words.txt'
reviewed = set(reviewed_path.read_text().split()) if reviewed_path.exists() else set()
candidates.update(reviewed)
accepted = {w for w in candidates-forbidden if lib.Hunspell_spell(handle, w.encode())}
lib.Hunspell_destroy(handle)
raw = subprocess.run(['word-list-compress', 'd'], input=(old/'sv.cwl').read_bytes(), capture_output=True, check=True).stdout
baseline = set(raw.decode('iso8859-1').splitlines())
merged = (baseline | accepted) - forbidden
# Keep Aspell's existing Swedish charset. Record every excluded word.
usable, excluded = set(), []
for word in sorted(merged):
    try:
        word.encode('iso8859-1')
    except UnicodeEncodeError:
        excluded.append(word)
        continue
    if word not in baseline and not re.fullmatch(r"[^\W\d_]+(?:['.:-][^\W\d_]+)*[.]?", word):
        excluded.append(word)
        continue
    if 'µ' in word:
        excluded.append(word)
        continue
    usable.add(word)
for filename, words in [('merged.txt', usable), ('excluded.txt', excluded)]:
    (out/filename).write_text('\n'.join(sorted(words))+'\n')
for filename in ['sv.dat', 'sv_phonet.dat']:
    shutil.copy2(old/filename, out/filename)
(out/'sv.dat').write_text((out/'sv.dat').read_text().replace('run-together true', 'run-together false'))
for filename, source in [('COPYING.aspell', old/'COPYING'), ('Copyright.aspell', old/'Copyright'), ('LICENSE.hunspell', hun/'LICENSE')]:
    shutil.copy2(source, out/filename)
basecmd = ['aspell', '--lang=sv', '--encoding=utf-8', '--local-data-dir='+str(out), '--dict-dir='+str(out)]
for name, words in [('sv', usable), ('sv-baseline', baseline)]:
    result = subprocess.run(basecmd+['create', 'master', str(out/(name+'.rws'))], input='\n'.join(sorted(words))+'\n', text=True, capture_output=True)
    (out/(name+'-build.log')).write_text(result.stderr)
    result.check_returncode()
    if result.stderr:
        raise RuntimeError('Aspell build emitted warnings: ' + result.stderr)
    dumped = subprocess.run(basecmd + ['--master=' + str(out/(name+'.rws')), 'dump', 'master'],
                            text=True, capture_output=True, check=True)
    if set(dumped.stdout.splitlines()) != set(words):
        raise RuntimeError('Compiled dictionary differs from exported words: ' + name)
(out/'sv.multi').write_text('add sv.rws\n')
stats = {'hunspell_commit': subprocess.check_output(['git', '-C', str(hun), 'rev-parse', 'HEAD'], text=True).strip(), 'hunspell_entries':len(rows)-1,'aspell_original_forms':len(baseline),'hunspell_generated_candidates':len(candidates),'hunspell_validated_forms':len(accepted),'merged_exported_forms':len(usable),'new_vs_original':len(usable-baseline),'excluded_forms':len(excluded),'original_forms_excluded':len(baseline-usable),'reviewed_corrections':len(reviewed_forbidden),'reviewed_legacy_forms_excluded':len(baseline & reviewed_forbidden)}
(out/'stats.json').write_text(json.dumps(stats, indent=2)+'\n')
print(json.dumps(stats, indent=2))
