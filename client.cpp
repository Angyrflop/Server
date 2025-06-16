#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <atomic>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <shlobj.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

class WindowsClient {
private:
    SOCKET clientSocket;
    std::atomic<bool> connected;
    std::atomic<bool> running;
    std::string serverHost;
    int serverPort;
    std::string logFilePath;
    
    // System tray
    NOTIFYICONDATA nid;
    HWND hwnd;
    static const UINT WM_TRAYICON = WM_USER + 1;
    static const UINT ID_TRAY_EXIT = 1001;
    
public:
    WindowsClient(const std::string& host = "127.0.0.1", int port = 9998) 
        : serverHost(host), serverPort(port), connected(false), running(true), clientSocket(INVALID_SOCKET) {
        
        // Initialize Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            logMessage("Failed to initialize Winsock");
            exit(1);
        }
        
        // Setup log file path
        setupLogPath();
        
        // Setup system tray
        setupSystemTray();
        
        logMessage("Windows Client initialized");
    }
    
    ~WindowsClient() {
        Shell_NotifyIcon(NIM_DELETE, &nid);
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
        }
        WSACleanup();
    }
    
    void start() {
        logMessage("Starting client connection attempts...");
        
        // Start connection thread
        std::thread connectionThread(&WindowsClient::connectionLoop, this);
        connectionThread.detach();
        
        // Start message loop for system tray
        MSG msg;
        while (running.load()) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    running = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            Sleep(100);
        }
        
        logMessage("Client shutting down...");
    }
    
private:
    void setupLogPath() {
        char appDataPath[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath) == S_OK) {
            logFilePath = std::string(appDataPath) + "\\client_log.txt";
        } else {
            logFilePath = "client_log.txt";
        }
    }
    
    void setupSystemTray() {
        // Create invisible window for message handling
        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"ClientTrayWindow";
        RegisterClass(&wc);
        
        hwnd = CreateWindow(L"ClientTrayWindow", L"Client", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), this);
        
        // Setup system tray icon
        memset(&nid, 0, sizeof(nid));
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        strcpy_s(nid.szTip, "Server Client - Disconnected");
        
        Shell_NotifyIcon(NIM_ADD, &nid);
    }
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        WindowsClient* client = nullptr;
        
        if (uMsg == WM_CREATE) {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            client = (WindowsClient*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)client);
        } else {
            client = (WindowsClient*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }
        
        if (client && uMsg == client->WM_TRAYICON) {
            if (lParam == WM_RBUTTONUP) {
                client->showContextMenu();
            }
            return 0;
        }
        
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    
    void showContextMenu() {
        HMENU hMenu = CreatePopupMenu();
        AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
        
        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(hwnd);
        
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
        
        if (cmd == ID_TRAY_EXIT) {
            running = false;
            PostQuitMessage(0);
        }
        
        DestroyMenu(hMenu);
    }
    
    void updateTrayIcon(bool isConnected) {
        if (isConnected) {
            strcpy_s(nid.szTip, "Server Client - Connected");
        } else {
            strcpy_s(nid.szTip, "Server Client - Disconnected");
        }
        Shell_NotifyIcon(NIM_MODIFY, &nid);
    }
    
    void connectionLoop() {
        while (running.load()) {
            if (!connected.load()) {
                // Try to connect
                if (connectToServer()) {
                    connected = true;
                    updateTrayIcon(true);
                    
                    // Send connection announcement
                    sendMessage("CLIENT_CONNECTED");
                    logMessage("Connected to server and announced presence");
                    
                    // Start ping and message handling
                    std::thread pingThread(&WindowsClient::pingLoop, this);
                    std::thread msgThread(&WindowsClient::messageLoop, this);
                    
                    pingThread.detach();
                    msgThread.detach();
                } else {
                    logMessage("Failed to connect, retrying in 60 seconds...");
                    Sleep(60000); // 1 minute retry interval
                }
            } else {
                Sleep(3000); // Connected, check every 3 seconds
            }
        }
    }
    
    bool connectToServer() {
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            logMessage("Failed to create socket: " + std::to_string(WSAGetLastError()));
            return false;
        }
        
        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(serverPort);
        
        if (inet_pton(AF_INET, serverHost.c_str(), &serverAddr.sin_addr) <= 0) {
            logMessage("Invalid server address: " + serverHost);
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
            return false;
        }
        
        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
            return false;
        }
        
        return true;
    }
    
    void pingLoop() {
        while (connected.load() && running.load()) {
            if (!sendMessage("PING")) {
                logMessage("Ping failed - connection lost");
                break;
            }
            Sleep(3000); // 3 second ping interval
        }
        
        // Connection lost
        connected = false;
        updateTrayIcon(false);
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }
    }
    
    void messageLoop() {
        char buffer[1024];
        
        while (connected.load() && running.load()) {
            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytesReceived <= 0) {
                logMessage("Server disconnected");
                break;
            }
            
            std::string message(buffer, bytesReceived);
            processMessage(message);
        }
        
        // Connection lost
        connected = false;
        updateTrayIcon(false);
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }
    }
    
    bool sendMessage(const std::string& message) {
        if (clientSocket == INVALID_SOCKET || !connected.load()) {
            return false;
        }
        
        int result = send(clientSocket, message.c_str(), message.length(), 0);
        return result != SOCKET_ERROR;
    }
    
    void processMessage(const std::string& message) {
        logMessage("Received: " + message);
        
        if (message == "PONG") {
            // Ping response - normal operation
            return;
        }
        else if (message == "KILL_SWITCH") {
            logMessage("Kill switch received - shutting down");
            running = false;
            PostQuitMessage(0);
            return;
        }
        else if (message == "SERVER_SHUTDOWN") {
            logMessage("Server shutdown notification received");
            connected = false;
            updateTrayIcon(false);
            return;
        }
        else if (message.substr(0, 4) == "MSG:") {
            std::string actualMessage = message.substr(4);
            logMessage("Server message: " + actualMessage);
            
            // Show message in system notification
            showNotification("Server Message", actualMessage);
            return;
        }
        else if (message == "AUTO_UPDATE_CHECK") {
            logMessage("Auto-update check received");
            // Here you could implement auto-update logic
            return;
        }
        
        // Log any other messages
        logMessage("Unknown message format: " + message);
    }
    
    void showNotification(const std::string& title, const std::string& message) {
        // Update notification icon data
        nid.uFlags = NIF_INFO;
        nid.dwInfoFlags = NIIF_INFO;
        
        // Convert strings to wide char for Windows API
        MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nid.szInfoTitle, sizeof(nid.szInfoTitle) / sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nid.szInfo, sizeof(nid.szInfo) / sizeof(wchar_t));
        
        Shell_NotifyIcon(NIM_MODIFY, &nid);
        
        // Reset flags
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    }
    
    void logMessage(const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << "[(" << std::put_time(&tm, "%Y-%m-%d")
            << ")(" << std::put_time(&tm, "%H:%M:%S")
            << ")] " << message;
        
        std::string logEntry = oss.str();
        
        // Print to console (if console is available)
        std::cout << logEntry << std::endl;
        
        // Write to log file
        std::ofstream logFile(logFilePath, std::ios::app);
        if (logFile.is_open()) {
            logFile << logEntry << std::endl;
            logFile.close();
        }
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Allocate console for debugging (optional)
    #ifdef _DEBUG
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
    #endif
    
    std::string serverHost = "127.0.0.1"; // Default localhost
    int serverPort = 9998;
    
    // Parse command line arguments if provided
    if (strlen(lpCmdLine) > 0) {
        std::string cmdLine(lpCmdLine);
        size_t colonPos = cmdLine.find(':');
        if (colonPos != std::string::npos) {
            serverHost = cmdLine.substr(0, colonPos);
            serverPort = std::stoi(cmdLine.substr(colonPos + 1));
        } else {
            serverHost = cmdLine;
        }
    }
    
    WindowsClient client(serverHost, serverPort);
    client.start();
    
    return 0;
}

// Alternative main for console applications
int main(int argc, char* argv[]) {
    std::string serverHost = "127.0.0.1";
    int serverPort = 9998;
    
    if (argc > 1) {
        std::string arg(argv[1]);
        size_t colonPos = arg.find(':');
        if (colonPos != std::string::npos) {
            serverHost = arg.substr(0, colonPos);
            serverPort = std::stoi(arg.substr(colonPos + 1));
        } else {
            serverHost = arg;
        }
    }
    
    WindowsClient client(serverHost, serverPort);
    client.start();
    
    return 0;
}
