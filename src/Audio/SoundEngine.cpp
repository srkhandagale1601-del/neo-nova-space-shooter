#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "Audio/SoundEngine.hpp"
#include "Core/Config.hpp"

#include <cmath>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iostream>

const int AUDIO_SAMPLE_RATE = 44100;
const int AUDIO_BUFFER_SAMPLES = 1024;
const int AUDIO_NUM_BUFFERS = 4;

struct SfxVoice
{
    bool active;
    SfxType type;
    int samplePos;
    int totalSamples;
    float param1;
    float param2;
    float volume;
};

const int MAX_SFX_VOICES = 16;
static SfxVoice sfxVoices[MAX_SFX_VOICES];
static std::mutex sfxMutex;

static std::atomic<bool> audioRunning(false);
static std::thread audioThread;
static HWAVEOUT hWaveOut = NULL;
static short audioBuffers[AUDIO_NUM_BUFFERS][AUDIO_BUFFER_SAMPLES];
static WAVEHDR waveHeaders[AUDIO_NUM_BUFFERS];

// Music Sequencer State
static int musicBeat = 0;
static int musicSampleCounter = 0;
int currentMusicTempo = 125; // BPM
int sectorThemeIndex = 0;

void playSfx(SfxType type, float vol)
{
    std::lock_guard<std::mutex> lock(sfxMutex);
    for (int i = 0; i < MAX_SFX_VOICES; i++)
    {
        if (!sfxVoices[i].active)
        {
            sfxVoices[i].active = true;
            sfxVoices[i].type = type;
            sfxVoices[i].samplePos = 0;
            sfxVoices[i].volume = vol;

            switch (type)
            {
                case SFX_LASER_DEFAULT:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.08f);
                    break;
                case SFX_LASER_SPREAD:
                case SFX_LASER_PENTA:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.11f);
                    break;
                case SFX_LASER_ENEMY:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.10f);
                    break;
                case SFX_EXPLOSION_SMALL:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.22f);
                    break;
                case SFX_EXPLOSION_LARGE:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.55f);
                    break;
                case SFX_POWERUP:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.32f);
                    break;
                case SFX_SHIELD_HIT:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.18f);
                    break;
                case SFX_SPEED_BOOST:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.40f);
                    break;
                case SFX_EMP_BOMB:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.85f);
                    break;
                case SFX_LEVEL_UP:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.65f);
                    break;
                case SFX_GAME_OVER:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.90f);
                    break;
                case SFX_UI_CLICK:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.03f);
                    break;
                case SFX_ALARM:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.45f);
                    break;
                default:
                    sfxVoices[i].totalSamples = (int)(AUDIO_SAMPLE_RATE * 0.1f);
                    break;
            }
            break;
        }
    }
}

// Procedural sample synthesizer
static float generateSfxSample(SfxVoice &v)
{
    float t = (float)v.samplePos / (float)AUDIO_SAMPLE_RATE;
    float progress = (float)v.samplePos / (float)v.totalSamples;
    float sample = 0.0f;

    switch (v.type)
    {
        case SFX_LASER_DEFAULT:
        {
            float freq = 1200.0f * (1.0f - progress * 0.8f);
            float phase = t * freq * 2.0f * 3.14159f;
            float wave = sinf(phase);
            float env = 1.0f - progress;
            sample = wave * env * 0.6f;
            break;
        }
        case SFX_LASER_SPREAD:
        {
            float freq = 1500.0f * (1.0f - progress * 0.75f);
            float phase = t * freq * 2.0f * 3.14159f;
            float wave = 0.7f * sinf(phase) + 0.3f * sinf(phase * 1.5f);
            float env = 1.0f - progress;
            sample = wave * env * 0.7f;
            break;
        }
        case SFX_LASER_PENTA:
        {
            float freq = 1700.0f * (1.0f - progress * 0.78f);
            float phase = t * freq * 2.0f * 3.14159f;
            float wave = (sinf(phase) > 0.0f ? 0.6f : -0.6f) + 0.4f * sinf(phase * 2.0f);
            float env = 1.0f - progress;
            sample = wave * env * 0.65f;
            break;
        }
        case SFX_LASER_ENEMY:
        {
            float freq = 420.0f * (1.0f - progress * 0.65f);
            float phase = t * freq * 2.0f * 3.14159f;
            float wave = (sinf(phase) > 0.1f ? 0.8f : -0.8f);
            float env = 1.0f - progress;
            sample = wave * env * 0.45f;
            break;
        }
        case SFX_EXPLOSION_SMALL:
        {
            float noise = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
            float env = expf(-progress * 6.5f);
            float rumble = sinf(t * 80.0f * 2.0f * 3.14159f);
            sample = (noise * 0.75f + rumble * 0.25f) * env * 0.7f;
            break;
        }
        case SFX_EXPLOSION_LARGE:
        {
            float noise = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
            float env = expf(-progress * 4.5f);
            float subBass = sinf(t * 55.0f * 2.0f * 3.14159f);
            sample = (noise * 0.6f + subBass * 0.4f) * env * 0.85f;
            break;
        }
        case SFX_POWERUP:
        {
            float step = progress * 4.0f;
            float freq = 523.25f;
            if (step >= 3.0f) freq = 1046.5f;
            else if (step >= 2.0f) freq = 783.99f;
            else if (step >= 1.0f) freq = 659.25f;

            float phase = t * freq * 2.0f * 3.14159f;
            float wave = sinf(phase) + 0.3f * sinf(phase * 2.0f);
            float env = (1.0f - progress);
            sample = wave * env * 0.55f;
            break;
        }
        case SFX_SHIELD_HIT:
        {
            float freq = 880.0f * expf(-progress * 3.0f);
            float phase = t * freq * 2.0f * 3.14159f;
            float wave = sinf(phase) * sinf(t * 120.0f * 2.0f * 3.14159f);
            float env = expf(-progress * 7.0f);
            sample = wave * env * 0.75f;
            break;
        }
        case SFX_SPEED_BOOST:
        {
            float freq = 180.0f + 500.0f * progress;
            float noise = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
            float phase = t * freq * 2.0f * 3.14159f;
            float env = sinf(progress * 3.14159f);
            sample = (sinf(phase) * 0.5f + noise * 0.5f) * env * 0.65f;
            break;
        }
        case SFX_EMP_BOMB:
        {
            float freq = 200.0f * expf(-progress * 2.5f) + 30.0f;
            float phase = t * freq * 2.0f * 3.14159f;
            float noise = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
            float wave = sinf(phase);
            float env = expf(-progress * 3.5f);
            sample = (wave * 0.7f + noise * 0.3f) * env * 0.95f;
            break;
        }
        case SFX_LEVEL_UP:
        {
            float step = progress * 4.0f;
            float freq = 349.23f;
            if (step >= 3.0f) freq = 698.46f;
            else if (step >= 2.0f) freq = 523.25f;
            else if (step >= 1.0f) freq = 440.00f;

            float phase = t * freq * 2.0f * 3.14159f;
            float wave = sinf(phase) + 0.35f * sinf(phase * 2.0f);
            float env = 1.0f - progress * 0.6f;
            sample = wave * env * 0.6f;
            break;
        }
        case SFX_GAME_OVER:
        {
            float step = progress * 4.0f;
            float freq = 293.66f;
            if (step >= 3.0f) freq = 146.83f;
            else if (step >= 2.0f) freq = 196.00f;
            else if (step >= 1.0f) freq = 233.08f;

            float phase = t * freq * 2.0f * 3.14159f;
            float wave = sinf(phase) * 0.7f;
            float env = 1.0f - progress;
            sample = wave * env * 0.6f;
            break;
        }
        case SFX_UI_CLICK:
        {
            float freq = 850.0f;
            float phase = t * freq * 2.0f * 3.14159f;
            float env = 1.0f - progress;
            sample = sinf(phase) * env * 0.45f;
            break;
        }
        case SFX_ALARM:
        {
            float freq = ((v.samplePos / (AUDIO_SAMPLE_RATE / 6)) % 2 == 0) ? 680.0f : 520.0f;
            float phase = t * freq * 2.0f * 3.14159f;
            float wave = (sinf(phase) > 0.0f ? 0.6f : -0.6f);
            float env = 1.0f - progress * 0.3f;
            sample = wave * env * 0.5f;
            break;
        }
    }

    v.samplePos++;
    if (v.samplePos >= v.totalSamples)
    {
        v.active = false;
    }

    return sample * v.volume;
}

// Procedural Retro Space Chiptune Music Generator
static float generateMusicSample()
{
    if (musicVolume <= 0.01f || masterVolume <= 0.01f)
        return 0.0f;

    int samplesPerBeat = (AUDIO_SAMPLE_RATE * 60) / (currentMusicTempo * 4);
    int step = (musicSampleCounter / samplesPerBeat) % 16;
    int stepSample = musicSampleCounter % samplesPerBeat;
    float stepProgress = (float)stepSample / (float)samplesPerBeat;
    float t = (float)musicSampleCounter / (float)AUDIO_SAMPLE_RATE;

    musicSampleCounter++;
    if (musicSampleCounter >= samplesPerBeat * 16)
    {
        musicSampleCounter = 0;
        musicBeat = (musicBeat + 1) % 4;
    }

    float bassNotes[16] = {
        110.0f, 110.0f, 130.81f, 110.0f,
        98.0f,  98.0f,  123.47f, 98.0f,
        87.31f, 87.31f, 110.0f,  87.31f,
        82.41f, 98.0f,  110.0f,  123.47f
    };

    if (sectorThemeIndex == 1)
    {
        bassNotes[0] = 110.0f; bassNotes[4] = 116.54f;
        bassNotes[8] = 98.0f;  bassNotes[12] = 82.41f;
    }
    else if (sectorThemeIndex == 3)
    {
        bassNotes[0] = 130.81f; bassNotes[4] = 98.0f;
        bassNotes[8] = 110.0f;  bassNotes[12] = 87.31f;
    }

    float bFreq = bassNotes[step];
    float bPhase = t * bFreq * 2.0f * 3.14159f;
    float bWave = (sinf(bPhase) > 0.0f ? 0.35f : -0.35f) + 0.25f * sinf(bPhase);
    float bEnv = expf(-stepProgress * 4.0f);
    float bassTrack = bWave * bEnv * 0.35f;

    float leadFreqs[16] = {
        440.0f, 523.25f, 659.25f, 880.0f,
        523.25f, 659.25f, 783.99f, 659.25f,
        349.23f, 440.0f, 523.25f, 698.46f,
        392.00f, 493.88f, 587.33f, 783.99f
    };
    float lFreq = leadFreqs[step];
    float lPhase = t * lFreq * 2.0f * 3.14159f;
    float lWave = sinf(lPhase) + 0.2f * sinf(lPhase * 2.0f);
    float lEnv = expf(-stepProgress * 6.0f);
    float leadTrack = lWave * lEnv * 0.18f;

    float drumTrack = 0.0f;
    if (step % 4 == 0)
    {
        float kickFreq = 120.0f * expf(-stepProgress * 8.0f) + 35.0f;
        float kickPhase = t * kickFreq * 2.0f * 3.14159f;
        drumTrack += sinf(kickPhase) * expf(-stepProgress * 6.0f) * 0.55f;
    }
    if (step == 4 || step == 12)
    {
        float noise = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
        drumTrack += noise * expf(-stepProgress * 7.0f) * 0.35f;
    }
    if (step % 2 == 1)
    {
        float noise = ((float)(rand() % 2000) - 1000.0f) / 1000.0f;
        drumTrack += noise * expf(-stepProgress * 18.0f) * 0.12f;
    }

    return (bassTrack + leadTrack + drumTrack) * musicVolume;
}

// Background Audio Mixer Thread Loop
static void audioMixerThreadFunc()
{
    while (audioRunning.load())
    {
        for (int b = 0; b < AUDIO_NUM_BUFFERS; b++)
        {
            if ((waveHeaders[b].dwFlags & WHDR_DONE) || waveHeaders[b].dwUser == 0)
            {
                if (waveHeaders[b].dwUser != 0)
                {
                    waveOutUnprepareHeader(hWaveOut, &waveHeaders[b], sizeof(WAVEHDR));
                }

                {
                    std::lock_guard<std::mutex> lock(sfxMutex);
                    for (int s = 0; s < AUDIO_BUFFER_SAMPLES; s++)
                    {
                        float mixed = 0.0f;

                        mixed += generateMusicSample();

                        for (int v = 0; v < MAX_SFX_VOICES; v++)
                        {
                            if (sfxVoices[v].active)
                            {
                                mixed += generateSfxSample(sfxVoices[v]) * sfxVolume;
                            }
                        }

                        float finalVal = mixed * masterVolume;
                        if (finalVal > 0.98f) finalVal = 0.98f;
                        if (finalVal < -0.98f) finalVal = -0.98f;

                        audioBuffers[b][s] = (short)(finalVal * 32767.0f);
                    }
                }

                waveHeaders[b].lpData = (LPSTR)audioBuffers[b];
                waveHeaders[b].dwBufferLength = AUDIO_BUFFER_SAMPLES * sizeof(short);
                waveHeaders[b].dwFlags = 0;
                waveHeaders[b].dwUser = 1;
                waveOutPrepareHeader(hWaveOut, &waveHeaders[b], sizeof(WAVEHDR));
                waveOutWrite(hWaveOut, &waveHeaders[b], sizeof(WAVEHDR));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(6));
    }
}

void initAudio()
{
    for (int i = 0; i < MAX_SFX_VOICES; i++)
    {
        sfxVoices[i].active = false;
    }

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = AUDIO_SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = AUDIO_SAMPLE_RATE * 2;
    wfx.cbSize = 0;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR)
    {
        audioRunning.store(true);
        audioThread = std::thread(audioMixerThreadFunc);
    }
    else
    {
        std::cout << "[AUDIO] Warning: waveOutOpen failed, running in silent mode." << std::endl;
    }
}

void shutdownAudio()
{
    if (audioRunning.load())
    {
        audioRunning.store(false);
        if (audioThread.joinable())
        {
            audioThread.join();
        }
        if (hWaveOut != NULL)
        {
            waveOutReset(hWaveOut);
            for (int b = 0; b < AUDIO_NUM_BUFFERS; b++)
            {
                if (waveHeaders[b].dwUser != 0)
                {
                    waveOutUnprepareHeader(hWaveOut, &waveHeaders[b], sizeof(WAVEHDR));
                }
            }
            waveOutClose(hWaveOut);
            hWaveOut = NULL;
        }
    }
}
