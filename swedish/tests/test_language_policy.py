"""Guard source provenance and the boundary between spelling and text advice."""
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    'language_policy', ROOT / 'tools/check_language_policy.py')
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)


class LanguagePolicyTests(unittest.TestCase):
    def setUp(self):
        self.catalogue = json.loads((ROOT / 'docs/swedish-rules.json').read_text())
        self.corrections = json.loads((ROOT / 'lexical-corrections.json').read_text())['corrections']

    def test_reviewed_project(self):
        policy.validate_project(ROOT)

    def test_missing_or_impostor_primary_source_is_rejected(self):
        for sources in ([], ['https://isof.se.example.org/faqs/31080'],
                        ['https://example.org/?source=https://isof.se'],
                        ['https://www.isof.se/']):
            with self.subTest(sources=sources):
                self.catalogue['rules'][0]['sources'] = sources
                with self.assertRaisesRegex(ValueError, 'primary source'):
                    policy.validate(self.catalogue, [])

    def test_context_rule_cannot_become_a_word_correction_or_test(self):
        rule = next(r for r in self.catalogue['rules'] if r['id'] == 'R25')
        entry = {'rule': 'R25', 'sources': rule['sources'], 'word': '7', 'replacement': 'sju'}
        with self.assertRaisesRegex(ValueError, 'context rule'):
            policy.validate(self.catalogue, [entry])
        case = dict(entry, id='example', scope='word', text='7', expected_accept=False)
        with self.assertRaisesRegex(ValueError, 'context rule'):
            policy.validate(self.catalogue, [], [case])

    def test_context_case_cannot_have_a_spelling_verdict(self):
        rule = next(r for r in self.catalogue['rules'] if r['id'] == 'R25')
        case = {'id': 'example', 'rule': 'R25', 'sources': rule['sources'],
                'scope': 'context', 'text': '7 kg', 'expected_accept': False}
        with self.assertRaisesRegex(ValueError, 'must not be scored'):
            policy.validate(self.catalogue, [], [case])
        del case['expected_accept']
        policy.validate(self.catalogue, [], [case])

    def test_unknown_rule_and_unrecorded_evidence_are_rejected(self):
        for update, message in (({'rule': 'missing'}, 'Unknown rule'),
                                ({'sources': []}, 'evidence'),
                                ({'sources': ['https://example.org/']}, 'evidence')):
            with self.subTest(update=update):
                entry = dict(self.corrections[0], **update)
                with self.assertRaisesRegex(ValueError, message):
                    policy.validate(self.catalogue, [entry])

    def test_conflicting_corrections_are_rejected(self):
        entry = self.corrections[0]
        with self.assertRaisesRegex(ValueError, 'Duplicate correction'):
            policy.validate(self.catalogue, [entry, entry])
        reverse = dict(entry, word=entry['replacement'], replacement=entry['word'])
        with self.assertRaisesRegex(ValueError, 'also forbidden'):
            policy.validate(self.catalogue, [entry, reverse])

    def test_conflicting_word_expectations_are_rejected(self):
        entry = self.corrections[0]
        case = dict(entry, id='one', scope='word', text=entry['word'], expected_accept=False)
        opposite = dict(case, id='two', expected_accept=True)
        with self.assertRaisesRegex(ValueError, 'Conflicting word cases'):
            policy.validate(self.catalogue, [], [case, opposite])


if __name__ == '__main__':
    unittest.main()
