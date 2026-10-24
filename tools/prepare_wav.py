#!/usr/bin/env python3
"""Convert audio the user has a right to use into a 16-bit PCM WAV for scanning.
Usage: python prepare_wav.py input.mp3 song_0.wav
Requires ffmpeg executable. Does not download/distribute any copyrighted music.
"""
import subprocess, sys
if len(sys.argv)!=3:
    sys.exit('usage: python prepare_wav.py <input_audio> <output_song_N.wav>')
subprocess.run(['ffmpeg','-y','-i',sys.argv[1],'-ar','22050','-ac','1','-c:a','pcm_s16le',sys.argv[2]],check=True)
print('WAV ready:',sys.argv[2])
