import os
import json
import time
import logging
import threading
from queue import Queue
from queue import Empty
import openai
from openai import OpenAI


def run_agent_thread(input_queue: Queue, output_queue: Queue, shutdown_event: threading.Event):
    openai_client = OpenAI(api_key=os.getenv("OPENAI_API_KEY"))
    logging.info("[Agent] Agent thread started")

    while not shutdown_event.is_set():
        try:
            task = input_queue.get(timeout=1)
            if not task:
                continue

            type = task.get("type")
            sender = task.get("sender")
            lang = task.get("lang", "en")
            mode = task.get("mode", "chat")

            if type == "HELLO":
                logging.info(f"[Agent] Processing HELLO from sender: {sender}")
                logging.info(f"[Agent] Adding HELLO response to the output_queue")
                output_queue.put({
                    "type": "HELLO",
                    "content": "Hello back from server.",
                    "language": "en",
                    "mode": "",
                    "sender": sender
               })
            if type == "ASSISTANT_PROCESS_WAVE":
                filename = task.get("fname")
                logging.info(f"[Agent] Processing {filename} from sender: {sender}")
                try:
                    # --- 1. Transcribe ---
                    with open(filename, "rb") as f:
                        whisper_result = openai_client.audio.transcriptions.create(
                            model="whisper-1",
                            file=f
                        )
                        text = whisper_result.text

                    logging.info(f"[Whisper] Transcript: {text}")

                    # --- 2. LLM Processing ---
                    prompt = (
                          "You are a smart, concise voice assistant running on a low-power ESP32-S3 device with Raspberry Pi server assistance. "
                          "Your user just said the following. Understand and interpret the intent clearly. "
                          "Reply briefly and helpfully. If it's a **command**, treat it as ready to act. If it's a **question**, answer it. "
                          "If you don't understand or can't help, say so. "
                          "Respond in **this strict JSON format**:\n\n"
                          "{ \"reply\": \"Your reply here\", \"lang\": \"language_code\", \"type\": \"command|text|unable_to_answer\" }\n\n"
                          "Use ISO 639-1 codes for 'lang'.\n"
                          f"User said: \"{text}\""
                    )
                    response = openai_client.chat.completions.create(
                        model="gpt-4",
                        messages=[{"role": "user", "content": prompt}]
                    )
                    # --- Parse GPT-4 Response ---
                    try:
                        reply_json = json.loads(response.choices[0].message.content)
                        reply = reply_json.get("reply", "").strip()
                        lang = reply_json.get("lang", "en").strip()
                        rtype = reply_json.get("type", "text").strip()  # default to text if missing
                    except (json.JSONDecodeError, AttributeError) as e:
                        logging.warning(f"[GPT-4] Failed to parse response JSON: {e}")
                        reply = response.choices[0].message.content.strip()
                        lang = "en"  # fallback
                        rtype = "error"

                    logging.info(f"[GPT-4] Response: {reply}")

                    # --- 3. Send back via output_queue if sender exists---
                    output_queue.put({
                        "type": "ASSISTANT_TEXT_RESPONSE",
                        "content": reply,
                        "language": lang,
                        "mode": mode,
                        "sender": str(sender)
                    })

                except openai.RateLimitError as e:
                    logging.error("[Agent] Rate limit or quota exceeded.")
                    if sender:
                        output_queue.put({
                            "sender": sender,
                            "text": "[ERROR] Rate limit exceeded. Please wait and try again later."
                        })

                except Exception as e:
                    logging.exception("[Agent] Failed to process audio task")
                    if sender:
                        output_queue.put({
                            "sender": sender,
                            "text": "[ERROR] Failed to process audio."
                        })

                # except Exception as e:
                #     logging.exception("[Agent] Failed to process audio task")

        except Empty:
            continue

        except Exception as e:
            logging.exception("[Agent] Fatal error")

    logging.info("[Agent] Agent thread finished")
