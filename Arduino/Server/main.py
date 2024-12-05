"""
This file is part of the open-clock Project.

Copyright (C) 2024 gl0p

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
"""

from flask import Flask, Response, send_from_directory, render_template_string, request, send_from_directory, jsonify
import os
import time
import glob
import yt_dlp
import subprocess
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
import threading
from flask_cors import CORS


app = Flask(__name__)
CORS(app, resources={r"/*": {"origins": "*"}})# Configuration
MUSIC_DIRECTORY = './music'  # Directory where MP3 files are stored
current_youtube_url = "https://youtu.be/El90OBILFBw"  # Default URL
UPDATES_DIRECTORY = './updates'  # Directory to store firmware files


@app.route('/esp32-update', methods=['GET', 'HEAD'])
def update():
    """
    Serve the latest firmware file to the ESP32.
    On HEAD requests, return the filename in headers.
    On GET requests, return the firmware file.
    """
    firmware_files = glob.glob(os.path.join(UPDATES_DIRECTORY, '*.bin'))
    if not firmware_files:
        return Response("No firmware available", status=404)

    latest_firmware = max(firmware_files, key=os.path.getctime)
    firmware_name = os.path.basename(latest_firmware)

    if request.method == 'HEAD':
        # Send filename in the header without the content
        response = Response(status=200)
        response.headers['Firmware-Name'] = firmware_name
        return response
    else:
        # Send the firmware file on GET request
        return send_from_directory(UPDATES_DIRECTORY, firmware_name, mimetype='application/octet-stream')


def get_mp3_files(directory=MUSIC_DIRECTORY):
    """
    Retrieve a sorted list of all MP3 files in the specified directory.
    """
    mp3_pattern = os.path.join(directory, '*.mp3')
    mp3_files = glob.glob(mp3_pattern)
    mp3_files.sort()
    return mp3_files

# Initialize MP3 files list
mp3_files = get_mp3_files()


def generate_stream():
    """
    Generator function to stream MP3 files continuously.
    """
    while True:  # Loop indefinitely for continuous streaming
        for mp3_file in mp3_files:
            if os.path.exists(mp3_file):
                try:
                    with open(mp3_file, 'rb') as f:
                        print(f"Streaming {mp3_file}")
                        while True:
                            data = f.read(4096)  # Read in 4KB chunks
                            if not data:
                                break  # Move to the next file
                            yield data
                            # Removed sleep to maintain a steady stream
                except Exception as e:
                    print(f"Error streaming {mp3_file}: {e}")
                # Optional: Add a short delay between songs to prevent overlapping
                time.sleep(0.5)
            else:
                print(f"File {mp3_file} not found! Skipping.")
                continue  # Skip to the next file if not found


def get_youtube_audio_stream_urls(youtube_url):
    """
    Retrieve all audio stream URLs from a YouTube link (playlist or single video).
    """
    ydl_opts = {
        'format': 'bestaudio/best',
        'quiet': True,
        'noplaylist': False,
        'socket_timeout': 30,
        'nocheckcertificate': True,
    }

    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        info = ydl.extract_info(youtube_url, download=False)

        if 'entries' in info:  # It's a playlist
            print(f"Playlist detected with {len(info['entries'])} videos.")
            audio_urls = []
            for entry in info['entries']:
                # Get individual audio stream URLs for each video in the playlist
                video_url = entry['url']
                audio_info = ydl.extract_info(video_url, download=False)
                audio_urls.append(audio_info['url'])
            return audio_urls
        else:  # It's a single video
            audio_url = info['url']
            if not audio_url.startswith("https://"):
                audio_url = audio_url.replace("http://", "https://")
            return [audio_url]  # Return as a list to keep consistency


def generate_youtube_playlist_stream(youtube_url):
    """
    Generator to stream audio from a YouTube playlist or single video.
    """
    audio_urls = get_youtube_audio_stream_urls(youtube_url)

    for audio_url in audio_urls:
        print(f"Streaming from: {audio_url}")

        # Set up ffmpeg to stream audio from each URL
        command = [
            'ffmpeg',
            '-i', audio_url,
            '-f', 'mp3',
            '-codec:a', 'libmp3lame',
            '-b:a', '192k',
            '-content_type', 'audio/mpeg',
            '-af', 'volume=1.2',  # Increase volume (2.0 = 200%)
            '-hide_banner',
            '-loglevel', 'error',
            '-'
        ]
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        try:
            while True:
                data = process.stdout.read(4096)
                if not data:
                    break  # Move to the next audio stream
                yield data
        except GeneratorExit:
            process.kill()
            break
        except Exception as e:
            print(f"Error streaming audio: {e}")
            process.kill()
            break


# Status Page
@app.route('/status')
def status():
    current_file = mp3_files[0] if mp3_files else 'None'
    html = f"""
    <h1>MP3 Streaming Server Status</h1>
    <p>Number of MP3 files: {len(mp3_files)}</p>
    <p>Currently Streaming: {os.path.basename(current_file) if current_file != 'None' else 'None'}</p>
    <ul>
        {''.join(f"<li>{os.path.basename(f)}</li>" for f in mp3_files)}
    </ul>
    """
    return render_template_string(html)

# Index Page
@app.route('/')
def index():
    return """<h1>Welcome to the MP3 and YouTube Streaming Server!</h1>
              <ul>
                <li>Visit <a href='/stream'>/stream</a> to listen to local MP3 files.</li>
                <li>Visit <a href='/status'>/status</a> for server status.</li>
                <li>Visit <a href='/stream_youtube?url=https://youtu.be/El90OBILFBw'>/stream_youtube?url=[YouTube_URL]</a> to stream audio from YouTube.</li>
              </ul>"""

# Streaming Endpoint for Local MP3s
@app.route('/stream')
def stream_mp3():
    return Response(generate_stream(), mimetype="audio/mpeg")


@app.route('/youtube_search')
def youtube_search():
    print("Beginning youtube search...")
    query = request.args.get('query')
    if not query:
        return jsonify({"error": "No query provided"}), 400

    search_url = f"ytsearch5:{query}"  # Search for the top 5 results
    with yt_dlp.YoutubeDL({'quiet': True}) as ydl:
        try:
            info = ydl.extract_info(search_url, download=False)
            videos = [
                {
                    "title": entry["title"],
                    "url": entry["webpage_url"],
                    "duration": entry["duration"],
                    "thumbnail": entry.get("thumbnail", "")
                }
                for entry in info.get("entries", [])
            ]
            print(f'Found Videos: {videos}')
            return jsonify(videos)
        except Exception as e:
            print(f"Exception occurred: {str(e)}")
            return jsonify({"error": str(e)}), 500


# Streaming Endpoint for YouTube Audio
@app.route('/stream_youtube')
def stream_youtube():
    print("Streaming YouTube audio...")
    youtube_url = request.args.get('url')
    user_id = request.args.get('user')

    if youtube_url:
        print(f"Streaming for user {user_id}: {youtube_url}")
        return Response(
            generate_youtube_playlist_stream(youtube_url),
            mimetype="audio/mpeg",
            headers={'Connection': 'close'}
        )
    else:
        return Response("Invalid YouTube URL", status=400)


# Watchdog Event Handler
class MP3Handler(FileSystemEventHandler):
    def on_created(self, event):
        if event.src_path.lower().endswith('.mp3'):
            print(f"MP3 file created: {event.src_path}")
            global mp3_files
            mp3_files = get_mp3_files()

    def on_deleted(self, event):
        if event.src_path.lower().endswith('.mp3'):
            print(f"MP3 file deleted: {event.src_path}")
            global mp3_files
            mp3_files = get_mp3_files()

def start_watcher(directory=MUSIC_DIRECTORY):
    event_handler = MP3Handler()
    observer = Observer()
    observer.schedule(event_handler, path=directory, recursive=False)
    observer.start()
    try:
        while True:
            time.sleep(1)  # Keep the thread alive
    except KeyboardInterrupt:
        observer.stop()
    observer.join()

@app.route('/landingpage')
def landing_page():
    try:
        # Ensure the path is correct relative to this script
        return send_from_directory('www','welcomepage.html')
    except Exception as e:
        return Response(f"Error loading landing page: {e}", status=500)

if __name__ == '__main__':
    # Ensure the music directory exists
    if not os.path.isdir(MUSIC_DIRECTORY):
        os.makedirs(MUSIC_DIRECTORY)
        print(f"Created music directory at {MUSIC_DIRECTORY}")
    if not os.path.isdir(UPDATES_DIRECTORY):
        os.makedirs(UPDATES_DIRECTORY)
        print(f"Created updates directory at {UPDATES_DIRECTORY}")

    # Print the list of MP3 files at startup
    if mp3_files:
        print("MP3 files to stream:")
        for f in mp3_files:
            print(f" - {f}")
    else:
        print("No MP3 files found in the directory.")

    # Start the file watcher in a separate thread
    watcher_thread = threading.Thread(target=start_watcher, args=(MUSIC_DIRECTORY,), daemon=True)
    watcher_thread.start()

    # Start the Flask server
    app.run(host='0.0.0.0', port=5000, threaded=True)
