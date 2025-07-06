### Functionality

# Display and Touchscreen
1. Initialise Touchscreen(CST816 via I2C)
2. Initialise display (ST77916)
2.1. Use double buffering in PSRAM
2.1. Add touchscreen to lvgl handler: lv_indev_set_read_cb(indev, Lvgl_Touchpad_Read);

# MP3 Player
### ESP32

1. Get list of files from SDCARD from /mp3 folder
2. Play selected MP3 file using Audio-I2S library via PCM5101 (Digital Analog Converter)

# Internet radio
### ESP32

1. Get list of internet radio from SDCARD from /internet_radios.json
2. Play selected stream URL using Audio-I2S library via PCM5101 (Digital Analog Converter)


# AI Assistent
### ESP32
1. Capture the sound from Mic (24-bit INMP441) via I2S
2. Band-pass filtering (ESP-DSP) and Automatic Gain Control (AGC)
2a. Storing into WAV file on SDCARD
2a1. Upload WAV file to Python server via HTTP POST
2b. Stream sound data via Websocket to Python server

### Webservice
3a. Receive a file from HTTP POST and store it in /recordings
3b. Receive sound data via Websocket, reconstruct WAV file and store it in /recordings


### Assistent Agent 
4. Send WAV file to Whisper using openai api and receive transcript 
5. Create LLM prompt with transcript
6. Send prompt to ChatGPT using openai api
7. Receive text response from ChatGPT
8. Send text response back via Websocket


# ESP32
9. Receive text response from Python server via Websocket
10. Send text response to GoogleTranslate using Audio-I2S library
11. Play received audio stream via PCM5101 (Digital Analog Converter)