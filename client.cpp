#include <iostream>
#include <string>
#include <chrono>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <atomic>
#include <vector>
#include <cstring>

// Windows includes
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <shlobj.h>
#include <process.h>

// Link required libraries
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

// Safe string copy function for cross-platform compatibility
void safe_strcpy(char* dest, size_t dest_size, const char* src) {
    size_t len = strlen(src);
    if (len >= dest_size) {
        len = dest_size - 1;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
}

class WindowsClient {
private:
    // Network
    SOCKET clientSocket;
    volatile bool connected;
    volatile bool shouldRun;
    std::string serverHost;
    int serverPort;
    
    // Logging
    std::string logFilePath;
    CRITICAL_SECTION logMutex;
    
    // System Tray
    HWND hwnd;
    NOTIFYICONDATAA nid;
    static const UINT WM_TRAYICON = WM_USER + 1;
    static const UINT ID_TRAY_EXIT = 1001;
    
    // Threads
    HANDLE connectionThread;
    HANDLE pingThread;
    HANDLE messageThread;
    
public:
    WindowsClient(const std::string& host = "127.0.0.1", int port = 9998) 
        : serverHost(host), serverPort(port), connected(false), shouldRun(true), 
          clientSocket(INVALID_SOCKET), hwnd(nullptr),
          connectionThread(NULL), pingThread(NULL), messageThread(NULL) {
        
        InitializeCriticalSection(&logMutex);
        initializeWinsock();
        setupLogFile();
        setupSystemTray();
        
        log("Client initialized - " + serverHost + ":" + std::to_string(serverPort));
    }
    
    ~WindowsClient() {
        cleanup();
        DeleteCriticalSection(&logMutex);
    }
    
    void start() {
        log("Starting client...");
        
        // Start connection thread
        connectionThread = (HANDLE)_beginthreadex(NULL, 0, connectionThreadProc, this, 0, NULL);
        if (connectionThread == NULL) {
            log("Failed to create connection thread");
            return;
        }
        
        // Message loop for system tray
        MSG msg;
        while (shouldRun) {
            BOOL result = GetMessage(&msg, nullptr, 0, 0);
            if (result <= 0) break;
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        cleanup();
    }
    
private:
    void initializeWinsock() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            exit(1);
        }
    }
    
    void setupLogFile() {
        char appData[MAX_PATH];
        if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData) == S_OK) {
            logFilePath = std::string(appData) + "\\WindowsClient.log";
        } else {
            logFilePath = "WindowsClient.log";
        }
    }
    
    void setupSystemTray() {
        // Register window class
        WNDCLASSA wc = {};
        wc.lpfnWndProc = windowProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "WindowsClientTray";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        
        RegisterClassA(&wc);
        
        // Create message window
        hwnd = CreateWindowA("WindowsClientTray", "Windows Client", 0, 
                            0, 0, 0, 0, HWND_MESSAGE, nullptr, 
                            GetModuleHandleA(nullptr), this);
        
        if (!hwnd) {
            log("Failed to create window");
            return;
        }
        
        // Setup tray icon
        memset(&nid, 0, sizeof(nid));
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        safe_strcpy(nid.szTip, sizeof(nid.szTip), "Client - Disconnected");
        
        Shell_NotifyIconA(NIM_ADD, &nid);
    }
    
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WindowsClient* client = nullptr;
        
        if (msg == WM_CREATE) {
            CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
            client = (WindowsClient*)cs->lpCreateParams;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)client);
        } else {
            client = (WindowsClient*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
        }
        
        if (client) {
            return client->handleMessage(msg, wParam, lParam);
        }
        
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_TRAYICON:
                if (LOWORD(lParam) == WM_RBUTTONUP) {
                    showContextMenu();
                }
                return 0;
                
            case WM_COMMAND:
                if (LOWORD(wParam) == ID_TRAY_EXIT) {
                    shouldRun = false;
                    PostQuitMessage(0);
                }
                return 0;
                
            default:
                return DefWindowProcA(hwnd, msg, wParam, lParam);
        }
    }
    
    void showContextMenu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;
        
        AppendMenuA(menu, MF_STRING, ID_TRAY_EXIT, "Exit");
        
        POINT cursor;
        GetCursorPos(&cursor);
        SetForegroundWindow(hwnd);
        
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd, nullptr);
        DestroyMenu(menu);
    }
    
    void updateTrayIcon(bool isConnected) {
        if (isConnected) {
            safe_strcpy(nid.szTip, sizeof(nid.szTip), "Client - Connected");
        } else {
            safe_strcpy(nid.szTip, sizeof(nid.szTip), "Client - Disconnected");
        }
        Shell_NotifyIconA(NIM_MODIFY, &nid);
    }
    
    // Thread procedures
    static unsigned __stdcall connectionThreadProc(void* param) {
        WindowsClient* client = static_cast<WindowsClient*>(param);
        client->connectionLoop();
        return 0;
    }
    
    static unsigned __stdcall pingThreadProc(void* param) {
        WindowsClient* client = static_cast<WindowsClient*>(param);
        client->pingLoop();
        return 0;
    }
    
    static unsigned __stdcall messageThreadProc(void* param) {
        WindowsClient* client = static_cast<WindowsClient*>(param);
        client->messageLoop();
        return 0;
    }
    
    void connectionLoop() {
        while (shouldRun) {
            if (!connected) {
                log("Attempting to connect...");
                if (connectToServer()) {
                    connected = true;
                    updateTrayIcon(true);
                    log("Connected successfully");
                    
                    // Send initial message
                    sendMessage("CLIENT_CONNECTED");
                    
                    // Start communication threads
                    pingThread = (HANDLE)_beginthreadex(NULL, 0, pingThreadProc, this, 0, NULL);
                    messageThread = (HANDLE)_beginthreadex(NULL, 0, messageThreadProc, this, 0, NULL);
                    
                    // Wait for threads to finish
                    HANDLE threads[] = { pingThread, messageThread };
                    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
                    
                    if (pingThread) {
                        CloseHandle(pingThread);
                        pingThread = NULL;
                    }
                    if (messageThread) {
                        CloseHandle(messageThread);
                        messageThread = NULL;
                    }
                    
                } else {
                    log("Connection failed, retrying in 30 seconds...");
                    Sleep(30000);
                }
            }
            Sleep(100);
        }
    }
    
    bool connectToServer() {
        // Clean up existing socket
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
        }
        
        // Create socket
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            log("Failed to create socket: " + std::to_string(WSAGetLastError()));
            return false;
        }
        
        // Set timeout
        DWORD timeout = 5000; // 5 seconds
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
        
        // Setup server address
        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(serverPort);
        
        // Convert IP address string to binary form
        unsigned long addr = inet_addr(serverHost.c_str());
        if (addr == INADDR_NONE) {
            log("Invalid server address: " + serverHost);
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
            return false;
        }
        serverAddr.sin_addr.s_addr = addr;
        
        // Connect
        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            log("Connection failed: " + std::to_string(WSAGetLastError()));
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
            return false;
        }
        
        return true;
    }
    
    void pingLoop() {
        while (connected && shouldRun) {
            if (!sendMessage("PING")) {
                log("Ping failed - disconnecting");
                break;
            }
            Sleep(3000);
        }
        disconnected();
    }
    
    void messageLoop() {
        char buffer[1024];
        
        while (connected && shouldRun) {
            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytesReceived <= 0) {
                if (bytesReceived == 0) {
                    log("Server closed connection");
                } else {
                    log("Receive error: " + std::to_string(WSAGetLastError()));
                }
                break;
            }
            
            std::string message(buffer, bytesReceived);
            processMessage(message);
        }
        disconnected();
    }
    
    bool sendMessage(const std::string& message) {
        if (clientSocket == INVALID_SOCKET || !connected) {
            return false;
        }
        
        int result = send(clientSocket, message.c_str(), (int)message.length(), 0);
        if (result == SOCKET_ERROR) {
            log("Send failed: " + std::to_string(WSAGetLastError()));
            return false;
        }
        
        return true;
    }
    
    void processMessage(const std::string& message) {
        log("Received: " + message);
        
        if (message == "PONG") {
            // Normal ping response
            return;
        }
        else if (message == "KILL_SWITCH") {
            log("Kill switch received");
            shouldRun = false;
            PostMessage(hwnd, WM_QUIT, 0, 0);
        }
        else if (message == "SERVER_SHUTDOWN") {
            log("Server shutdown notification");
            disconnected();
        }
        else if (message.length() > 4 && message.substr(0, 4) == "MSG:") {
            std::string msg = message.substr(4);
            log("Server message: " + msg);
            showNotification("Server Message", msg);
        }
        else if (message == "AUTO_UPDATE_CHECK") {
            log("Auto-update check received");
            // Implement auto-update logic here
        }
    }
    
    void showNotification(const std::string& title, const std::string& message) {
        // Simple notification using the tooltip instead of balloon tips
        // since older Windows versions may not support balloon notifications
        NOTIFYICONDATAA tempNid = nid;
        
        // Create a combined message for the tooltip
        std::string combinedMsg = title + ": " + message;
        if (combinedMsg.length() > 127) {
            combinedMsg = combinedMsg.substr(0, 124) + "...";
        }
        
        safe_strcpy(tempNid.szTip, sizeof(tempNid.szTip), combinedMsg.c_str());
        Shell_NotifyIconA(NIM_MODIFY, &tempNid);
        
        // Also log the notification
        log("Notification - " + title + ": " + message);
    }
    
    void disconnected() {
        if (connected) {
            connected = false;
            updateTrayIcon(false);
            
            if (clientSocket != INVALID_SOCKET) {
                closesocket(clientSocket);
                clientSocket = INVALID_SOCKET;
            }
            
            log("Disconnected from server");
        }
    }
    
    void cleanup() {
        shouldRun = false;
        
        // Wait for and close threads
        if (connectionThread) {
            WaitForSingleObject(connectionThread, 5000);
            CloseHandle(connectionThread);
            connectionThread = NULL;
        }
        if (pingThread) {
            WaitForSingleObject(pingThread, 1000);
            CloseHandle(pingThread);
            pingThread = NULL;
        }
        if (messageThread) {
            WaitForSingleObject(messageThread, 1000);
            CloseHandle(messageThread);
            messageThread = NULL;
        }
        
        // Cleanup tray icon
        Shell_NotifyIconA(NIM_DELETE, &nid);
        
        // Cleanup socket
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
        }
        
        // Cleanup Winsock
        WSACleanup();
        
        log("Client shutdown complete");
    }
    
    void log(const std::string& message) {
        EnterCriticalSection(&logMutex);
        
        time_t now = time(0);
        struct tm* timeinfo = localtime(&now);
        
        std::ostringstream oss;
        oss << "[" << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S") << "] " << message;
        
        std::string logEntry = oss.str();
        
        // Console output
        std::cout << logEntry << std::endl;
        
        // File output
        std::ofstream file(logFilePath, std::ios::app);
        if (file.is_open()) {
            file << logEntry << std::endl;
        }
        
        LeaveCriticalSection(&logMutex);
    }
};

// Entry points
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Enable console in debug mode
    #ifdef _DEBUG
    AllocConsole();
    FILE* pCout;
    FILE* pCerr;
    freopen_s(&pCout, "CONOUT$", "w", stdout);
    freopen_s(&pCerr, "CONOUT$", "w", stderr);
    #endif
    
    std::string host = "127.0.0.1";
    int port = 9998;
    
    // Parse command line
    if (strlen(lpCmdLine) > 0) {
        std::string cmd(lpCmdLine);
        size_t pos = cmd.find(':');
        if (pos != std::string::npos) {
            host = cmd.substr(0, pos);
            port = std::stoi(cmd.substr(pos + 1));
        } else {
            host = cmd;
        }
    }
    
    WindowsClient client(host, port);
    client.start();
    
    return 0;
}

int main(int argc, char* argv[]) {
    std::string host = "192.168.2.145";
    int port = 9998;
    
    if (argc > 1) {
        std::string arg(argv[1]);
        size_t pos = arg.find(':');
        if (pos != std::string::npos) {
            host = arg.substr(0, pos);
            port = std::stoi(arg.substr(pos + 1));
        } else {
            host = arg;
        }
    }
    
    WindowsClient client(host, port);
    client.start();
    
    return 0;
}