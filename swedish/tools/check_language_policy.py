#!/usr/bin/env python3
"""Validate reviewed rule metadata offline; source interpretation needs review.

Kept identical in hunspell-sv/tools and aspell-sv/swedish/tools.
"""
import json
from pathlib import Path
from urllib.parse import urlsplit


def is_sprakradet_source(url):
    parsed = urlsplit(url)
    return parsed.scheme == 'https' and parsed.hostname in {
        'isof.se', 'www.isof.se', 'frageladan.isof.se',
        'isof.diva-portal.org', 'sprakochfolkminnen.diva-portal.org',
    } and parsed.path not in ('', '/')


def validate(catalogue, corrections, cases=()):
    """Reject unsourced rules and context rules used as spelling prohibitions."""
    if catalogue.get('norm_source') != 'Språkrådet (Isof)':
        raise ValueError('The rule catalogue must use Språkrådet (Isof) as norm source')
    rules = {}
    for rule in catalogue['rules']:
        key = rule['id']
        if key in rules:
            raise ValueError(f'Duplicate rule: {key}')
        if rule.get('dictionary_scope') not in ('word', 'context'):
            raise ValueError(f'{key}: missing dictionary scope')
        if not any(is_sprakradet_source(url) for url in rule.get('sources', [])):
            raise ValueError(f'{key}: missing Språkrådet primary source')
        rules[key] = rule

    def reviewed_rule(entry, word_level):
        key = entry['rule']
        if key not in rules:
            raise ValueError(f'Unknown rule: {key}')
        rule = rules[key]
        sources = entry.get('sources', [])
        if not sources or not set(sources) <= set(rule['sources']):
            raise ValueError(f'{key}: evidence must be recorded in the rule catalogue')
        if word_level and rule['dictionary_scope'] != 'word':
            raise ValueError(f'{key}: context rule cannot decide word acceptance')

    def whole_word(word):
        if not isinstance(word, str) or not word or any(c.isspace() for c in word):
            raise ValueError(f'Expected a whole word, got {word!r}')

    forbidden = set()
    replacements = set()
    for entry in corrections:
        reviewed_rule(entry, True)
        whole_word(entry['word'])
        whole_word(entry['replacement'])
        if entry['word'] in forbidden:
            raise ValueError(f'Duplicate correction: {entry["word"]}')
        forbidden.add(entry['word'])
        replacements.add(entry['replacement'])
    if forbidden & replacements:
        raise ValueError('A reviewed replacement is also forbidden')

    ids = set()
    accepted = {}
    for case in cases:
        if case['id'] in ids:
            raise ValueError(f'Duplicate case: {case["id"]}')
        ids.add(case['id'])
        scope = case['scope']
        if scope not in ('word', 'context'):
            raise ValueError(f'Unknown case scope: {scope}')
        reviewed_rule(case, scope == 'word')
        if scope == 'word':
            whole_word(case['text'])
            expected = case.get('expected_accept')
            if type(expected) is not bool:
                raise ValueError('Word cases require a boolean expected_accept')
            previous = accepted.setdefault(case['text'], expected)
            if previous != expected:
                raise ValueError(f'Conflicting word cases: {case["text"]}')
        elif 'expected_accept' in case:
            raise ValueError('Context examples must not be scored as spelling tests')


def validate_project(root):
    catalogue = json.loads((root / 'docs/swedish-rules.json').read_text(encoding='utf-8'))
    corrections = json.loads((root / 'lexical-corrections.json').read_text(encoding='utf-8'))
    corpus = root / 'tests/swedish-rules.json'
    cases = json.loads(corpus.read_text(encoding='utf-8'))['cases']
    validate(catalogue, corrections['corrections'], cases)
    return len(catalogue['rules']), len(corrections['corrections']), len(cases)


if __name__ == '__main__':
    counts = validate_project(Path(__file__).resolve().parents[1])
    print('PASS: %d sourced rules, %d corrections, %d cases' % counts)
