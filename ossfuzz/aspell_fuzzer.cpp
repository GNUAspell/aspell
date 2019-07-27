#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <aspell.h>

#define FUZZ_LANG_CODE_LEN              5
#define FUZZ_ENCODING_LEN               1
#define NUM_COMMAND_BYTES               (FUZZ_LANG_CODE_LEN + \
                                         FUZZ_ENCODING_LEN)

static int enable_diags;

#define FUZZ_DEBUG(FMT, ...)                                                  \
        if (enable_diags) {                                                   \
          fprintf(stderr, FMT, ##__VA_ARGS__);                                \
          fprintf(stderr, "\n");                                              \
        }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  AspellCanHaveError *possible_err = NULL;
  AspellSpeller *spell_checker = NULL;
  AspellConfig *spell_config = NULL;
  AspellDocumentChecker *doc_checker = NULL;
  AspellCanHaveError *doc_err = NULL;
  AspellToken token;
  const char *data_str = reinterpret_cast<const char *>(data);

  char language_code[FUZZ_LANG_CODE_LEN + 1];
  const char *encoding;

  // Enable or disable diagnostics based on the FUZZ_VERBOSE environment flag.
  enable_diags = (getenv("FUZZ_VERBOSE") != NULL);

  if (size < NUM_COMMAND_BYTES) {
    goto EXIT_LABEL;
  }

  // Copy the first five bytes as a language code and null-terminate it.
  memcpy(language_code, data, FUZZ_LANG_CODE_LEN);
  language_code[FUZZ_LANG_CODE_LEN] = 0;

  data += FUZZ_LANG_CODE_LEN;

  // Determine the encoding.
  switch (data[0] % 4) {
    case 0:
      encoding = "iso-8859-1";
      break;

    case 1:
      encoding = "utf-8";
      break;

    case 2:
      encoding = "ucs-2";
      break;

    case 3:
      encoding = "ucs-4";
      break;
  }

  data += FUZZ_ENCODING_LEN;

  // After parsing the command bytes, reduce the size by that amount.
  size -= NUM_COMMAND_BYTES;

  // Create a new configuration class.
  spell_config = new_aspell_config();

  FUZZ_DEBUG("Language code: %s", language_code);
  aspell_config_replace(spell_config, "lang", language_code);

  FUZZ_DEBUG("Encoding: %s", encoding);
  aspell_config_replace(spell_config, "encoding", encoding);

  possible_err = new_aspell_speller(spell_config);
  if (aspell_error_number(possible_err) != 0) {
    // Failed on configuration.
    FUZZ_DEBUG("Failed to create speller: %s",
               aspell_error_message(possible_err));
    delete_aspell_can_have_error(possible_err);
    goto EXIT_LABEL;
  }

  // Create a spell checker.
  spell_checker = to_aspell_speller(possible_err);

  // Convert the spell checker to a document checker.
  doc_err = new_aspell_document_checker(spell_checker);
  if (aspell_error(doc_err) != 0) {
    // Failed to convert to a document checker.
    FUZZ_DEBUG("Failed to create document checker: %s",
               aspell_error_message(doc_err));
    delete_aspell_can_have_error(doc_err);
    goto EXIT_LABEL;
  }

  doc_checker = to_aspell_document_checker(doc_err);

  // Process the remainder of the document.
  aspell_document_checker_process(doc_checker, data_str, size);

  // Iterate over all misspellings.
  token = aspell_document_checker_next_misspelling(doc_checker);

  FUZZ_DEBUG("Token len %d", token.len);

  for (;
       token.len != 0;
       token = aspell_document_checker_next_misspelling(doc_checker))
  {
    // Get spelling suggestions for the misspelling.
    auto word_list = aspell_speller_suggest(spell_checker,
                                            data_str + token.offset,
                                            token.len);

    // Iterate over the suggested replacement words in the word list.
    AspellStringEnumeration *els = aspell_word_list_elements(word_list);

    for (const char *word = aspell_string_enumeration_next(els);
         word != 0;
         word = aspell_string_enumeration_next(els))
    {
      // Conditionally print out the suggested replacement words.
      FUZZ_DEBUG("Suggesting replacement for word at offset %d len %d: %s",
                 token.offset,
                 token.len,
                 word);
    }
    delete_aspell_string_enumeration(els);
  }

EXIT_LABEL:

  if (doc_checker != NULL) {
    delete_aspell_document_checker(doc_checker);
  }

  if (spell_checker != NULL) {
    delete_aspell_speller(spell_checker);
  }

  if (spell_config != NULL) {
    delete_aspell_config(spell_config);
  }

  return 0;
}
