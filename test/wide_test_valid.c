#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aspell.h>

const uint16_t test_word[] = {'c','a','f', 0x00E9, 0};
const uint16_t test_incorrect[] = {'c','a','f', 'e', 0};
const uint16_t test_doc[] = {'T', 'h', 'e', ' ', 'c','a','f', 'e', '.', 0};

int fail = 0;

int main() {
  AspellConfig * spell_config = new_aspell_config();
  aspell_config_replace(spell_config, "master", "en_US-w_accents");
  aspell_config_replace(spell_config, "encoding", "ucs-2");
  AspellCanHaveError * possible_err = new_aspell_speller(spell_config);
  AspellSpeller * spell_checker = 0;
  if (aspell_error_number(possible_err) != 0) {
    fprintf(stderr, "%s", aspell_error_message(possible_err));
    return 2;
  } else {
    spell_checker = to_aspell_speller(possible_err);
  }
  int correct = aspell_speller_check_w(spell_checker, test_word, -1);
  if (!correct) {
    fprintf(stderr, "%s", "fail: expected word to be correct\n");
    fail = 1;
  }
  correct = aspell_speller_check_w(spell_checker, test_incorrect, -1);
  if (correct) {
    fprintf(stderr, "%s", "fail: expected word to be incorrect\n");
    fail = 1;
  }
  const AspellWordList * suggestions = aspell_speller_suggest_w(spell_checker, test_incorrect, -1);
  AspellStringEnumeration * elements = aspell_word_list_elements(suggestions);
  const uint16_t * word = aspell_string_enumeration_next_w(uint16_t, elements);
  if (memcmp(word, test_word, sizeof(test_incorrect)) != 0) {
    fprintf(stderr, "%s", "fail: first suggesion is not what is expected (suggest)\n");
    fail = 1;
  }
  delete_aspell_string_enumeration(elements);

  AspellSuggestions * sugs = aspell_speller_suggestions_w(spell_checker, test_incorrect, -1, NULL);
  unsigned len;
  const uint16_t * * words = aspell_suggestions_words_w(uint16_t, sugs, &len);
  if (len < 1 || memcmp(words[0], test_word, sizeof(test_incorrect)) != 0) {
    fprintf(stderr, "%s", "fail: first suggesion is not what is expected (suggestions)\n");
    fail = 1;
  }
  
  possible_err = new_aspell_document_checker(spell_checker);
  if (aspell_error(possible_err) != 0) {
    fprintf(stderr, "Error: %s\n",aspell_error_message(possible_err));
    return 2;
  }
  AspellDocumentChecker * checker = to_aspell_document_checker(possible_err);
  aspell_document_checker_process_w(checker, test_doc, -1);

  AspellToken token = aspell_document_checker_next_misspelling_w(uint16_t, checker);
  if (4 != token.len) {
    fprintf(stderr, "fail: size of first misspelling (%d) is not what is expected (%d)\n",
            token.len, 4);
    fail = 1;
  } else if (memcmp(test_incorrect, test_doc + token.offset, token.len) != 0) {
    fprintf(stderr, "%s", "fail: first misspelling is not what is expected\n");
    fail = 1;
  }
  if (fail) {
    printf("not ok\n");
    return 1;
  } else {
    printf("ok\n");
    return 0;
  }
}
