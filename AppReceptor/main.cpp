// main.cpp
// --- Directivas para modernizar la ventana y ocultar la consola ---
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ws2_32.lib")

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "NetworkAudio.h"

#include <thread>
#include <atomic>
#include <string>
#include <vector>

// --- VARIABLES DE RED Y AUDIO ---
float ringBuffer[NUM_CHANNELS * RING_BUFFER_SIZE];
std::atomic<uint32_t> writeIndex(0);
std::atomic<uint32_t> readIndex(0);

ma_context context;
ma_device device;
bool isAudioRunning = false;
bool isAppRunning = true;
int targetPreBuffer = 4096;
bool isBuffering = true;
std::vector<ma_device_id> deviceIds;

// --- CONTROLES DE LA INTERFAZ GRÁFICA ---
HWND hCombo, hBtnApply, hBtnPlus, hBtnMinus, hStatus;

// IDs para identificar los botones internamente
#define ID_COMBO 101
#define ID_BTN_APPLY 102
#define ID_BTN_PLUS 103
#define ID_BTN_MINUS 104

// 1. HILO DE RED (El estable que recibe datos de FL Studio)
void udpListenerThread() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in recvAddr;
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(UDP_PORT);
    recvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(recvSocket, (SOCKADDR*)&recvAddr, sizeof(recvAddr));

    AudioPacket packet;
    while (isAppRunning) {
        int bytesReceived = recv(recvSocket, (char*)&packet, sizeof(AudioPacket), 0);
        if (bytesReceived > 0) {
            uint32_t currentWrite = writeIndex.load(std::memory_order_relaxed);
            for (uint32_t i = 0; i < packet.numFrames * NUM_CHANNELS; ++i) {
                ringBuffer[currentWrite] = packet.samples[i];
                currentWrite = (currentWrite + 1) % (RING_BUFFER_SIZE * NUM_CHANNELS);
            }
            writeIndex.store(currentWrite, std::memory_order_release);
        }
    }
    closesocket(recvSocket);
    WSACleanup();
}

// 2. REPRODUCTOR AUDIO (WASAPI)
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    float* outBuffer = (float*)pOutput;
    uint32_t currentRead = readIndex.load(std::memory_order_relaxed);
    uint32_t currentWrite = writeIndex.load(std::memory_order_acquire);

    int availableSamples = (currentWrite - currentRead) / NUM_CHANNELS;
    if (availableSamples < 0) availableSamples += RING_BUFFER_SIZE;

    // Lógica del colchón anti-tirones
    if (isBuffering) {
        if (availableSamples >= targetPreBuffer) isBuffering = false;
        else {
            for (ma_uint32 i = 0; i < frameCount * NUM_CHANNELS; ++i) outBuffer[i] = 0.0f;
            return;
        }
    }

    if (availableSamples < (int)frameCount) {
        isBuffering = true;
        for (ma_uint32 i = 0; i < frameCount * NUM_CHANNELS; ++i) outBuffer[i] = 0.0f;
        return;
    }

    for (ma_uint32 i = 0; i < frameCount * NUM_CHANNELS; ++i) {
        outBuffer[i] = ringBuffer[currentRead];
        currentRead = (currentRead + 1) % (RING_BUFFER_SIZE * NUM_CHANNELS);
    }
    readIndex.store(currentRead, std::memory_order_release);
}

// Actualiza el texto de información de la ventana
void updateStatus() {
    std::string text = "Estado: Reproduciendo\nColchon anti-tirones: " + std::to_string(targetPreBuffer) + " frames";
    SetWindowTextA(hStatus, text.c_str());
}

// Reinicia la tarjeta de sonido
void startAudio(ma_device_id* devId) {
    if (isAudioRunning) { ma_device_uninit(&device); isAudioRunning = false; }
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID = devId;
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = NUM_CHANNELS;
    deviceConfig.sampleRate        = 48000;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.periodSizeInFrames = 256;

    if (ma_device_init(&context, &deviceConfig, &device) == MA_SUCCESS) {
        ma_device_start(&device);
        isAudioRunning = true;
    }
}

// 3. LA VENTANA GRÁFICA (Win32 API)
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

            // Crear Textos y Botones
            HWND hLabel = CreateWindowA("STATIC", "Selecciona tu Tarjeta de Sonido:", WS_VISIBLE | WS_CHILD, 20, 20, 300, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

            hCombo = CreateWindowA("COMBOBOX", "", CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_VSCROLL, 20, 45, 300, 200, hwnd, (HMENU)ID_COMBO, NULL, NULL);
            SendMessage(hCombo, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnApply = CreateWindowA("BUTTON", "Aplicar Salida", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 20, 80, 300, 35, hwnd, (HMENU)ID_BTN_APPLY, NULL, NULL);
            SendMessage(hBtnApply, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnMinus = CreateWindowA("BUTTON", "- Reducir Buffer", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 130, 140, 35, hwnd, (HMENU)ID_BTN_MINUS, NULL, NULL);
            SendMessage(hBtnMinus, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBtnPlus = CreateWindowA("BUTTON", "+ Aumentar Buffer", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 180, 130, 140, 35, hwnd, (HMENU)ID_BTN_PLUS, NULL, NULL);
            SendMessage(hBtnPlus, WM_SETFONT, (WPARAM)hFont, TRUE);

            hStatus = CreateWindowA("STATIC", "Iniciando...", WS_VISIBLE | WS_CHILD, 20, 180, 300, 40, hwnd, NULL, NULL, NULL);
            SendMessage(hStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Rellenar el menú desplegable con las tarjetas
            SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)"[Predeterminado de Windows]");
            ma_device_info* pPlaybackInfos;
            ma_uint32 playbackCount;
            if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, NULL, NULL) == MA_SUCCESS) {
                for (ma_uint32 i = 0; i < playbackCount; ++i) {
                    SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)pPlaybackInfos[i].name);
                    deviceIds.push_back(pPlaybackInfos[i].id);
                }
            }
            SendMessage(hCombo, CB_SETCURSEL, 0, 0); // Seleccionar la primera por defecto
            return 0;
        }
        case WM_COMMAND: {
            // Lógica al hacer clic en los botones
            if (LOWORD(wParam) == ID_BTN_APPLY) {
                int index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
                if (index == 0) startAudio(NULL);
                else if (index > 0 && index - 1 < deviceIds.size()) startAudio(&deviceIds[index - 1]);
                updateStatus();
            }
            if (LOWORD(wParam) == ID_BTN_PLUS) {
                if (targetPreBuffer < RING_BUFFER_SIZE / 4) targetPreBuffer += 512;
                isBuffering = true;
                updateStatus();
            }
            if (LOWORD(wParam) == ID_BTN_MINUS) {
                if (targetPreBuffer > 512) targetPreBuffer -= 512;
                updateStatus();
            }
            return 0;
        }
        case WM_DESTROY: {
            isAppRunning = false; // Detener bucles infinitos
            PostQuitMessage(0);   // Cerrar programa
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// 4. PUNTO DE ENTRADA (Aquí arranca la App nativa sin terminal negra)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) return -1;
    
    // Iniciar hilo de red en segundo plano
    std::thread networkThread(udpListenerThread);

    // Crear la ventana principal
    const char* CLASS_NAME = "AudioBridgeClass";
    WNDCLASSA wc = { };
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0, CLASS_NAME, "Audio Bridge Receiver", 
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // Estilo de ventana
        CW_USEDEFAULT, CW_USEDEFAULT, 360, 270, // Tamaño
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);
    startAudio(NULL); // Arrancar audio predeterminado al abrir la app
    updateStatus();

    // Bucle de mensajes de Windows (Mantiene la ventana viva)
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Limpieza al cerrar la ventana
    if (isAudioRunning) ma_device_uninit(&device);
    ma_context_uninit(&context);
    networkThread.join();
    return 0;
}