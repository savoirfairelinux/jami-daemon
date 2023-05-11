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
from languages import *


class Translator:
    def __init__(self):
        self.language = "all"
        try:
            self.language = os.environ['LANGUAGE']
        except:
            self.language = "all"
        self.languages = [self.language] if self.language != "all" else languagesList.keys()
        self.sflTransifex = Transifex()
        self.translator = trMethods(True)
        self.sflResources = sflTransifex.get_resources()

    def translate(self):
        for l in self.languages:
            value = languagesList.get(l)
            if value is not None:
                printc(f"Using language: {l}")
                self.translate_resources()

    def translate_resources(self):
        for r in self.sflResources:
            translateStrings = []
            rslug = r.slug
            printc(f"\n######## Translating resource: {rslug} ########")
            try:
                [toTranslate, translated] = self.sflTransifex.get_toTranslate_strings(
                    rslug, l
                ) # type: ignore

                for t in toTranslate:
                    source_string_other = t.resource_string.strings["other"]
                    translated_string_other = self.translator.translate_nllb(source_string_other, value)

                    if t.resource_string.pluralized:
                        source_string_one = t.resource_string.strings["one"]
                        translated_string_one = self.translator.translate_nllb(source_string_one, value)
                        t.save(strings={'one': translated_string_one, 'other': translated_string_other})
                    else:
                        t.save(strings={'other': translated_string_other})
                    translateStrings.append(t)

                if len(translateStrings) > 0:
                    self.sflTransifex.create_resource_file(rslug, translateStrings, l)

            except Exception as e:
                traceback.print_exc()
                printc(f"Translation failed: {e}", bcolors.FAIL)


if __name__ == "__main__":
    printc("Starting program...")
    t = Translator()
    t.translate()
    printc("Program finished")
