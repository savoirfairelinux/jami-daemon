import re
from transifex.api import transifex_api
import datetime
import os
import traceback
import pandas as pd


class bcolors:
    OK = "\033[92m"
    WARNING = "\033[93m"
    FAIL = "\033[91m"
    ENDC = "\033[0m"


def printc(text, color=bcolors.OK):
    print(color + text + bcolors.ENDC)


class Transifex:
    def __init__(self, organization_="savoirfairelinux", project_="jami"):

        try:
            print("Authenticating to Transifex API...")
            transifex_api.setup(auth=TOKEN) #Put your token here
            self.organization = transifex_api.Organization.get(slug=organization_)
            printc(self.organization.slug + " fetched successfully")

            self.project = self.organization.fetch("projects").get(slug=project_)
            print(self.project.slug)

        except Exception as e:
            traceback.print_exc()
            printc(f"Exiting program from transifex {e}", bcolors.FAIL)
            exit()

    def get_languages(self):
        self.languages = self.project.fetch("languages")
        for l in self.languages:
            print(l.code)
        return self.languages

    def get_resources(self):
        self.resources = self.project.fetch("resources")
        for r in self.resources:
            print(r.slug)
        return self.resources

    def get_toTranslate_strings(self, resource_name, languageCode):
        toTransalte = []
        translated = []
        toTranslateCount = 0
        translatedCount = 0
        resource = None

        for r in self.resources:
            if r.slug == resource_name:
                resource = r
                break

        try:
            language = transifex_api.Language.get(code=languageCode)
            print("Resource fetched")
            translations = transifex_api.ResourceTranslation.filter(
                resource=resource, language=language
            ).include("resource_string")

            for t in translations.all():
                if t.strings and t.strings.get("other") is not None:
                    translatedCount += 1
                    translated.append(t)
                else:
                    toTranslateCount += 1
                    toTransalte.append(t)

            printc(f"{translatedCount} strings translated")
            printc(f"{toTranslateCount} strings to translate", bcolors.WARNING)
            return toTransalte, translated

        except Exception as e:
            traceback.print_exc()
            printc(f"Problem detected in get_toTranslate_strings: {e}", bcolors.FAIL)

    def create_resource_file(self, resource_name, translated_strings, language):
        directory = resource_name  # Nom du répertoire à créer
        date = datetime.datetime.now().strftime("%Y-%m-%d")
        if not os.path.exists(directory):
            os.makedirs(directory)  # Créer le répertoire s'il n'existe pas déjà

        filename = f"{language}_translated_strings_{date}.xlsx"
        filepath = os.path.join(
            directory, filename
        )

        df = pd.DataFrame(
            {
                "stringToTranslate": [
                    item.resource_string.strings["other"] for item in translated_strings
                ],
                "translatedString": [
                    item.strings["other"] for item in translated_strings
                ],
            }
        )

        # Ajouter les informations d'en-tête
        header = pd.DataFrame(
            {
                "Organization": ["savoirfairelinux"],
                "Project": ["jami"],
                "Resource": [resource_name],
                "Date": [datetime.datetime.now().strftime("%Y-%m-%d")],
                "Time": [datetime.datetime.now().strftime("%H:%M:%S")],
            }
        )

        # Ajouter le header et les colonnes au dataframe
        df = pd.concat([header, df], axis=1)

        # Écrire le DataFrame dans le fichier Excel
        df.to_excel(filepath, index=False)
