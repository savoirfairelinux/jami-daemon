"""
Copyright (C) 2023 Savoir-faire Linux Inc.
Author: Fadi Shehadeh <fadi.shehadeh@savoirfairelinux.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

import os
from fnmatch import translate
from tools import *
from trMethods import *
from dictionary import *

def main():
    printc("Starting program...")
    language = "all"
    try:
        language = os.environ['LANGUAGE']
    except:
        language = "all"
    languages = [language] if language != "all" else dictionnaireLangues.keys()
    sflTransifex = Transifex()
    translator = trMethods(True)
    sflResources = sflTransifex.get_resources()

    for l in languages:
        value = dictionnaireLangues.get(l)
        if value is not None:
            printc(f"Using language: {l}")
            for r in sflResources:
                translateStrings = []
                rslug = r.slug
                printc("\n######## Translating resource: " + rslug + " ########")
                try:
                    [toTranslate, translated] = sflTransifex.get_toTranslate_strings(
                        rslug, l
                    ) # type: ignore

                    for t in toTranslate:

                        source_string_other = t.resource_string.strings["other"]
                        translated_string_other = translator.translate_nllb(source_string_other, value)

                        if t.resource_string.pluralized:
                            source_string_one = t.resource_string.strings["one"]
                            translated_string_one = translator.translate_nllb(source_string_one, value)
                            t.save(strings={'one': translated_string_one, 'other': translated_string_other})

                        else:
                            t.save(strings={'other': translated_string_other})

                        translateStrings.append(t)

                    if len(translateStrings) > 0:
                        sflTransifex.create_resource_file(rslug, translateStrings, l)

                except Exception as e:
                    traceback.print_exc()
                    printc(f"Problem detected in main: {e}", bcolors.FAIL)

    printc("Program finished")


if __name__ == "__main__":
    main()
