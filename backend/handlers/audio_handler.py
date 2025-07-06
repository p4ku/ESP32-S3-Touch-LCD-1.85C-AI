import wave
import os
import logging
from utils.wav_helpers import write_wav_header_placeholder, finalize_wav_file

class AudioStreamSession:
    def __init__(self, filepath: str, sample_rate=16000, num_channels=1, bits_per_sample=16):
        self.filepath = filepath
        self.sample_rate = sample_rate
        self.num_channels = num_channels
        self.bits_per_sample = bits_per_sample

        logging.info(f"[AudioStreamSession] Initializing session: {filepath}")
        try:
            self.wave_file = open(filepath, "wb")
            write_wav_header_placeholder(self.wave_file, sample_rate, num_channels, bits_per_sample)
            self.data_length = 0
            logging.info(f"[AudioStreamSession] WAV header written (rate={sample_rate}, channels={num_channels}, bits={bits_per_sample})")
        except Exception as e:
            logging.exception(f"[AudioStreamSession] Failed to initialize WAV stream: {e}")
            raise

    def write_chunk(self, data: bytes):
        try:
            self.wave_file.write(data)
            self.wave_file.flush()  # ensure data is written to disk
            os.fsync(self.wave_file.fileno())  # flush to disk
            self.data_length += len(data)
            logging.debug(f"[AudioStreamSession] Wrote chunk ({len(data)} bytes), total so far: {self.data_length} bytes")
        except Exception as e:
            logging.exception(f"[AudioStreamSession] Failed to write audio chunk: {e}")

    def close(self):
        try:
            if not self.wave_file.closed:
                self.wave_file.close()
                finalize_wav_file(self.filepath, self.data_length)
                logging.info(f"[AudioStreamSession] WAV file closed and finalized: {self.filepath} ({self.data_length} bytes data)")
        except Exception as e:
            logging.exception(f"[AudioStreamSession] Failed to finalize WAV file: {e}")
