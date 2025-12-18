/*
MIT License

Copyright (c) 2024 MSc Games Engineering Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <Windows.h>
#include <map>
#include <string>
#include <xaudio2.h>

// FourCC codes for WAV file parsing (big-Endian)
#ifdef _XBOX
#define fourccRIFF 'RIFF'
#define fourccDATA 'data'
#define fourccFMT 'fmt '
#define fourccWAVE 'WAVE'
#define fourccXWMA 'XWMA'
#define fourccDPDS 'dpds'
#endif

// FourCC codes for WAV file parsing (little-endian)
#ifndef _XBOX
#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'
#endif

// The Sound class handles loading and playing WAV audio files
class Sound {
private:
  XAUDIO2_BUFFER buffer;                 // Audio buffer
  IXAudio2SourceVoice *sourceVoice[128]; // Array of source voices for playback
  int index;                             // Current index for source voices

  // Helper function to find a chunk in the WAV file
  HRESULT FindChunk(HANDLE hFile, DWORD fourcc, DWORD &dwChunkSize,
                    DWORD &dwChunkDataPosition) {
    HRESULT hr = S_OK;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, 0, NULL, FILE_BEGIN))
      return HRESULT_FROM_WIN32(GetLastError());

    DWORD dwChunkType;
    DWORD dwChunkDataSize;
    DWORD dwRIFFDataSize = 0;
    DWORD dwFileType;
    DWORD bytesRead = 0;
    DWORD dwOffset = 0;

    while (hr == S_OK) {
      DWORD dwRead;
      if (0 == ReadFile(hFile, &dwChunkType, sizeof(DWORD), &dwRead, NULL))
        hr = HRESULT_FROM_WIN32(GetLastError());

      if (0 == ReadFile(hFile, &dwChunkDataSize, sizeof(DWORD), &dwRead, NULL))
        hr = HRESULT_FROM_WIN32(GetLastError());

      switch (dwChunkType) {
      case fourccRIFF:
        dwRIFFDataSize = dwChunkDataSize;
        dwChunkDataSize = 4;
        if (0 == ReadFile(hFile, &dwFileType, sizeof(DWORD), &dwRead, NULL))
          hr = HRESULT_FROM_WIN32(GetLastError());
        break;

      default:
        if (INVALID_SET_FILE_POINTER ==
            SetFilePointer(hFile, dwChunkDataSize, NULL, FILE_CURRENT))
          return HRESULT_FROM_WIN32(GetLastError());
      }

      dwOffset += sizeof(DWORD) * 2;

      if (dwChunkType == fourcc) {
        dwChunkSize = dwChunkDataSize;
        dwChunkDataPosition = dwOffset;
        return S_OK;
      }

      dwOffset += dwChunkDataSize;
      if (bytesRead >= dwRIFFDataSize)
        return S_FALSE;
    }

    return S_OK;
  }

  // Helper function to read chunk data from the WAV file
  HRESULT ReadChunkData(HANDLE hFile, void *buffer, DWORD buffersize,
                        DWORD bufferoffset) {
    HRESULT hr = S_OK;
    if (INVALID_SET_FILE_POINTER ==
        SetFilePointer(hFile, bufferoffset, NULL, FILE_BEGIN))
      return HRESULT_FROM_WIN32(GetLastError());
    DWORD dwRead;
    if (0 == ReadFile(hFile, buffer, buffersize, &dwRead, NULL))
      hr = HRESULT_FROM_WIN32(GetLastError());
    return hr;
  }

public:
  // Loads a WAV file into the audio buffer
  bool loadWAV(IXAudio2 *xaudio, std::string filename) {
    WAVEFORMATEXTENSIBLE wfx = {0};
    // Open the file
    HANDLE hFile = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, 0, NULL);

    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    DWORD dwChunkSize;
    DWORD dwChunkPosition;

    // Check the file type; it should be 'WAVE'
    FindChunk(hFile, fourccRIFF, dwChunkSize, dwChunkPosition);
    DWORD filetype;
    ReadChunkData(hFile, &filetype, sizeof(DWORD), dwChunkPosition);
    if (filetype != fourccWAVE)
      return FALSE;

    // Read the 'fmt ' chunk to get the format
    FindChunk(hFile, fourccFMT, dwChunkSize, dwChunkPosition);
    ReadChunkData(hFile, &wfx, dwChunkSize, dwChunkPosition);

    // Read the 'data' chunk to get the audio data
    FindChunk(hFile, fourccDATA, dwChunkSize, dwChunkPosition);
    BYTE *pDataBuffer = new BYTE[dwChunkSize];
    ReadChunkData(hFile, pDataBuffer, dwChunkSize, dwChunkPosition);

    // Fill the XAUDIO2_BUFFER structure
    buffer.AudioBytes = dwChunkSize;
    buffer.pAudioData = pDataBuffer;
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    HRESULT hr;
    // Create multiple source voices for concurrent playback
    for (int i = 0; i < 128; i++) {
      if (FAILED(hr = xaudio->CreateSourceVoice(&sourceVoice[i],
                                                (WAVEFORMATEX *)&wfx))) {
        return false;
      }
    }

    // Submit the audio buffer to the first source voice
    if (FAILED(hr = sourceVoice[0]->SubmitSourceBuffer(&buffer))) {
      return false;
    }

    index = 0; // Reset the index
    return true;
  }

  // Plays the sound once
  void play() {
    sourceVoice[index]->SubmitSourceBuffer(&buffer);
    sourceVoice[index]->Start(0);
    index++;
    if (index == 128) {
      index = 0;
    }
  }

  // Plays the sound in an infinite loop (for music)
  void playMusic() {
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    sourceVoice[index]->SubmitSourceBuffer(&buffer);
    sourceVoice[index]->Start(0);
  }
};

// The SoundManager class manages multiple Sound instances
class SoundManager {
private:
  IXAudio2 *xaudio;                          // XAudio2 interface
  IXAudio2MasteringVoice *xaudioMasterVoice; // Mastering voice
  std::map<std::string, Sound *> sounds;     // Map of sounds
  Sound *music = NULL;                       // Music sound

  // Helper function to find a sound by filename
  Sound *find(std::string filename) {
    auto it = sounds.find(filename);
    if (it != sounds.end()) {
      return it->second;
    }
    return NULL;
  }

public:
  // Constructor that initializes XAudio2
  SoundManager() {
    HRESULT comResult;
    comResult = XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
    comResult = xaudio->CreateMasteringVoice(&xaudioMasterVoice);
  }

  // Loads a sound effect
  void load(std::string filename) {
    if (find(filename) == NULL) {
      Sound *sound = new Sound();
      if (sound->loadWAV(xaudio, filename)) {
        sounds[filename] = sound;
      }
    }
  }

  // Plays a loaded sound effect
  void play(std::string filename) {
    Sound *sound = find(filename);
    if (sound != NULL) {
      sound->play();
    }
  }

  // Loads a music track
  void loadMusic(std::string filename) {
    music = new Sound();
    music->loadWAV(xaudio, filename);
  }

  // Plays the loaded music track
  void playMusic() { music->playMusic(); }

  // Destructor to release resources
  ~SoundManager() { xaudio->Release(); }
};
