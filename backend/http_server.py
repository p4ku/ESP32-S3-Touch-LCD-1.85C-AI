import os
import shutil
import logging
from fastapi import FastAPI, UploadFile, File, Form
from fastapi.responses import JSONResponse, FileResponse, HTMLResponse


logging.basicConfig(level=logging.INFO)
agent_in_queue = None  # will be set externally

def create_app(in_queue):
    global agent_in_queue
    agent_in_queue = in_queue

    app = FastAPI()

    @app.post("/upload")
    async def upload_wav(
        file: UploadFile = File(...),
        session_id: str = Form(None)  # Get session_id from form field
    ):
        """Upload a WAV file and process it."""
        try:
            filename = os.path.basename(file.filename)
            path = os.path.join(UPLOAD_DIR, filename)

            logging.info(f"[UPLOAD] Receiving file: {filename}")
            logging.info(f"[UPLOAD] Content-Type: {file.content_type}")
            logging.info(f"[UPLOAD] From session_id: {session_id}")

            with open(path, "wb") as f:
                shutil.copyfileobj(file.file, f)

            size = os.path.getsize(path)
            logging.info(f"[UPLOAD] Saved to: {path} ({size} bytes)")

            # Add file to agent queue
            agent_in_queue.put({
                "type": "ASSISTANT_PROCESS_WAVE",
                "fname": path,
                "sender": session_id if session_id else None,
            })

            return JSONResponse(content={"status": "ok", "filename": filename, "size": size})
        except Exception as e:
            logging.exception("[UPLOAD] Upload failed")
            return JSONResponse(status_code=500, content={"status": "error", "message": str(e)})

    @app.get("/", response_class=HTMLResponse)
    async def serve_index():
        """Serve a simple HTML page listing recorded WAV files."""
        html = """
    <html>
    <head>
        <title>WAV Recordings</title>
        <style>
            body { font-family: Arial, sans-serif; padding: 20px; background: #f8f8f8; }
            h1 { color: #333; }
            .recording { margin-bottom: 15px; padding: 10px; background: #fff; border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
            audio { display: block; margin-top: 5px; }
        </style>
    </head>
    <body>
        <h1>Recorded WAV Files</h1>
        %s
    </body>
    </html>
        """ % "\n".join(
            f'''
        <div class="recording">
            <strong>{f}</strong><br/>
            <audio controls>
                <source src="/recordings/{f}" type="audio/wav">
                Your browser does not support the audio element.
            </audio>
        </div>
            '''
            for f in os.listdir(UPLOAD_DIR) if f.endswith(".wav")
        )
        return HTMLResponse(content=html)

    @app.get("/recordings")
    async def list_recordings():
        """List all recorded WAV files."""
        files = [f for f in os.listdir(UPLOAD_DIR) if f.endswith(".wav")]
        return {"files": files}

    @app.get("/recordings/{filename}")
    async def get_recording(filename: str):
        """Serve a recorded WAV file."""
        filepath = os.path.join(UPLOAD_DIR, filename)
        if not os.path.isfile(filepath):
            return JSONResponse(status_code=404, content={"error": "File not found"})
        return FileResponse(filepath, media_type="audio/wav")

    return app
