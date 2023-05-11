This tool has the following files:

    nllbTranslate.py: The main script that gets resources from Transifex and uses different translation methods to translate strings.

    trMethods.py: This file contains the translation methods used in the tool, Google Translate and NLLB.

    tools.py: This file contains helper classes and functions used in the tool.

    dictionary.py: This file contains a dictionary mapping language codes to their NLLB code equivalents.

## Requirements

To run this tool, you will need Python 3.7+ and the following packages:

    googletrans
    transformers
    transifex-api
    pandas

## Usage

To use this tool, you need to do the following:

1. Install all required packages using pip:

pip install googletrans transformers transifex-api pandas

2. Run the main script:

python nllbTranslate.py

This will start the translation process. The script will fetch resources from Transifex, get the strings to be translated, translate them using the specified translation method, and then save the translated strings back to Transifex.

Note: You need to replace TOKEN in tools.py with your actual Transifex API token.
Note 2 : You need to replace LANGUAGE in nllbTranslate.py with the language you want to translate to.


## How it works

The main() function in nllbTranslate.py is the entry point of the tool. It creates instances of the Transifex and trMethods classes, gets resources from Transifex, and then iterates over each resource to translate its strings.

The trMethods class in trMethods.py provides two methods for translating strings: translate_nllb() and translate_googleTrad(). These methods take a text string and a target language code as input and return the translated text.

The Transifex class in tools.py is a wrapper around the Transifex API. It provides methods for authenticating to the API, getting resources and languages from a project, and creating resource files.

The dictionary.py file contains a dictionary that maps language codes to their equivalents in the NLLB model. This is used by the translate_nllb() method in trMethods.py to set the target language for translation.

Note: Google Translate's translation method has been added so it can be compared to the nllb model translations.


## Source of files and code

Google translate => https://pypi.org/project/googletrans/
Meta NLLB => https://huggingface.co/docs/transformers/model_doc/nllb
Dictionary Source => https://github.com/facebookresearch/flores/tree/main/flores200
