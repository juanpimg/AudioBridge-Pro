# AudioBridge Pro (Low-Latency UDP Engine) 🚀🎧

A high-performance, real-time audio routing system designed to send zero-latency audio from a Digital Audio Workstation (DAW) to any external sound card or device on Windows.

> **Note:** The core logic, network architecture, and graphical interface of this project were largely designed and written with the assistance of Artificial Intelligence (LLM), serving as an exploration of modern AI-assisted software engineering in real-time audio systems.

## 🧠 Architecture Overview
The system is split into two independent parts:
1. **The Sender (VST3 Plugin):** Built with the JUCE framework. It captures the audio stream and fires it as fast UDP packets to the local network.
2. **The Receiver (Standalone App):** A native Windows GUI application written in C++ using `miniaudio`. It features a network thread to catch UDP packets and an elastic **Jitter Buffer** to feed the WASAPI driver smoothly.

## ✨ Key Technical Features
* **Lock-Free UDP Networking:** Leverages the Windows kernel network stack to queue packets safely, preventing thread starvation.
* **Elastic Jitter Buffer:** Compensates for physical clock drift between the DAW's ASIO driver and the external WASAPI soundcard.
* **Hot-Swappable Routing:** Change output devices on the fly via the Win32 GUI without restarting.
* **Deep Copy Hardware ID:** Prevents silent fallback crashes common in WASAPI.

## 🛠️ Stack & Technologies
* **C++ (C++17/C++20)**
* **[JUCE Framework](https://juce.com/) & [Miniaudio](https://miniaud.io/)**
* **Win32 API & Winsock2**