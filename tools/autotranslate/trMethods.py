
import time
from googletrans import LANGUAGES, Translator
from transformers import AutoModelForSeq2SeqLM, AutoTokenizer


class trMethods:
    def __init__(self, isNLLB=False):
        if isNLLB:
            print("Loading NLLB model...")
            self.tokenizer = AutoTokenizer.from_pretrained(
                "facebook/nllb-200-distilled-600M"
            )
            self.model = AutoModelForSeq2SeqLM.from_pretrained(
                "facebook/nllb-200-distilled-600M"
            )
            print("NLLB model loaded successfully")

    def translate_nllb(self, text, target_language):
        inputs = self.tokenizer(text, return_tensors="pt")

        translated_tokens = self.model.generate(
            **inputs,
            forced_bos_token_id=self.tokenizer.lang_code_to_id[target_language],
            max_length=500
        )
        translated = self.tokenizer.batch_decode(
            translated_tokens, skip_special_tokens=True
        )[0]

        print(text + " <=> " + translated)
        return translated

    def translate_googleTrad(self, text, target_language):

        translator_ = Translator()
        text = text.replace("&quot;", "'")
        for i in range(3):
            try:
                translation = translator_.translate(text, dest=target_language)
                break
            except:
                print("Error translating. Retrying in 30 seconds...")
                time.sleep(30)
        if translation is None:
            raise Exception("Failed to translate text after 3 attempts.")
            return False
        else:
            print(text + " <=> " + translation.text)
        return translation.text
