from gtts import gTTS
import io

def text_to_speech(text):
    tts = gTTS(text)
    buf = io.BytesIO()
    tts.write_to_fp(buf)
    buf.seek(0)
    return buf.read()

