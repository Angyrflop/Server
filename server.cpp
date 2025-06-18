#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <map>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>

class ServerManager {
private:
    struct Client {
        int socket;
        std::string ip;
        time_t lastPing;
        bool connected;
        
        Client(int s, std::string i) : socket(s), ip(i), lastPing(time(nullptr)), connected(true) {}
    };
    
    std::vector<Client> clients;
    std::mutex clientsMutex;
    std::mutex logMutex;
    
    int serverSocket;
    int javaSocket;
    bool running;
    const int CLIENT_PORT = 9998;
    const int JAVA_PORT = 9999;
    const std::string LOG_FILE = "server.log";
        // Add this private function to ServerManager class
std::string sendMessageToClient(const std::string& targetIp, const std::string& message) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto& client : clients) {
        if (client.connected && client.ip == targetIp) {
            std::string fullMessage = "MSG:" + message;
            if (send(client.socket, fullMessage.c_str(), fullMessage.length(), 0) > 0) {
                logMessage("Send to " + client.ip + " {\"" + message + "\"}");
                return "{\"sent_to\": \"" + targetIp + "\", \"status\": \"success\"}";
            } else {
                client.connected = false; // Mark as disconnected if send fails
                logMessage("Failed to send to " + client.ip);
                return "{\"error\": \"Failed to send to " + targetIp + "\"}";
            }
        }
    }
    return "{\"error\": \"Client " + targetIp + " not found or not connected\"}";
}
public:
    ServerManager() : running(false), serverSocket(-1), javaSocket(-1) {}
    
    void start() {
        running = true;
        
        // Setup signal handlers
        signal(SIGINT, [](int) { exit(0); });
        
        // Start client server
        std::thread clientThread(&ServerManager::clientServerLoop, this);
        clientThread.detach();
        
        // Start Java bridge server
        std::thread javaThread(&ServerManager::javaBridgeLoop, this);
        javaThread.detach();
        
        // Ping monitoring thread
        std::thread pingThread(&ServerManager::monitorPings, this);
        pingThread.detach();
        
        // Auto-update broadcast thread
        std::thread updateThread(&ServerManager::autoUpdateBroadcast, this);
        updateThread.detach();
        
        // Command line interface
        std::string command;
        std::cout << "Server started. Available commands:\n";
        std::cout << "- message <text>: Send message to all clients\n";
        std::cout << "- show_ips: Display connected clients\n";
        std::cout << "- kill_switch: Disconnect all clients\n";
        std::cout << "- stop: Shutdown server\n";
        std::cout << "- help: Show commands\n\n";
        
        while (running) {
            std::cout << "> ";
            std::getline(std::cin, command);
            processCommand(command);
        }
    }
    
private:
    void clientServerLoop() {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            logMessage("Failed to create client socket");
            return;
        }
        
        int opt = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(CLIENT_PORT);
        
        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            logMessage("Failed to bind client socket to port " + std::to_string(CLIENT_PORT));
            return;
        }
        
        if (listen(serverSocket, 10) < 0) {
            logMessage("Failed to listen on client socket");
            return;
        }
        
        logMessage("Client server listening on port " + std::to_string(CLIENT_PORT));
        
        while (running) {
            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
            
            if (clientSocket >= 0) {
                std::string clientIP = inet_ntoa(clientAddr.sin_addr);
                
                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    clients.emplace_back(clientSocket, clientIP);
                }
                
                logMessage("Client connected from " + clientIP);
                
                // Handle client in separate thread
                std::thread clientHandler(&ServerManager::handleClient, this, clientSocket, clientIP);
                clientHandler.detach();
            }
        }
    }
    
    void javaBridgeLoop() {
        javaSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (javaSocket < 0) {
            logMessage("Failed to create Java bridge socket");
            return;
        }
        
        int opt = 1;
        setsockopt(javaSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        sockaddr_in javaAddr{};
        javaAddr.sin_family = AF_INET;
        javaAddr.sin_addr.s_addr = INADDR_ANY;
        javaAddr.sin_port = htons(JAVA_PORT);
        
        if (bind(javaSocket, (struct sockaddr*)&javaAddr, sizeof(javaAddr)) < 0) {
            logMessage("Failed to bind Java bridge socket to port " + std::to_string(JAVA_PORT));
            return;
        }
        
        if (listen(javaSocket, 5) < 0) {
            logMessage("Failed to listen on Java bridge socket");
            return;
        }
        
        logMessage("Java bridge listening on port " + std::to_string(JAVA_PORT));
        
        while (running) {
            sockaddr_in javaClientAddr{};
            socklen_t javaClientLen = sizeof(javaClientAddr);
            int javaClientSocket = accept(javaSocket, (struct sockaddr*)&javaClientAddr, &javaClientLen);
            
            if (javaClientSocket >= 0) {
                logMessage("Java bridge connected");
                
                // Handle Java bridge communication
                std::thread javaHandler(&ServerManager::handleJavaBridge, this, javaClientSocket);
                javaHandler.detach();
            }
        }
    }
    
    void handleClient(int clientSocket, const std::string& clientIP) {
        char buffer[1024];
        
        while (running) {
            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytesReceived <= 0) {
                break; // Client disconnected
            }
            
            std::string message(buffer, bytesReceived);
            
            // Update last ping time
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                auto it = std::find_if(clients.begin(), clients.end(),
                    [clientSocket](const Client& c) { return c.socket == clientSocket; });
                if (it != clients.end()) {
                    it->lastPing = time(nullptr);
                }
            }
            
            // Process client message
            if (message == "PING") {
                send(clientSocket, "PONG", 4, 0);
            } else if (message == "CLIENT_CONNECTED") {
                logMessage("Client announcement from " + clientIP);
            } else {
                logMessage("Received from " + clientIP + ": " + message);
            }
        }
        
        // Clean up disconnected client
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.erase(std::remove_if(clients.begin(), clients.end(),
                [clientSocket](const Client& c) { return c.socket == clientSocket; }),
                clients.end());
        }
        
        close(clientSocket);
        logMessage("Client disconnected: " + clientIP);
    }
    
    void handleJavaBridge(int javaClientSocket) {
        char buffer[2048];
        
        while (running) {
            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(javaClientSocket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytesReceived <= 0) {
                break; // Java bridge disconnected
            }
            
            std::string command(buffer, bytesReceived);
            command.erase(command.find_last_not_of(" \n\r\t") + 1); // trim
            
            logMessage("Java bridge command: " + command);
            
            std::string response = processCommand(command);
            response += "\nEND_RESPONSE\n";
            
            send(javaClientSocket, response.c_str(), response.length(), 0);
        }
        
        close(javaClientSocket);
        logMessage("Java bridge disconnected");
    }
    
std::string processCommand(const std::string& command) {
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        
        if (cmd == "message_all") { // Changed to 'if'
            std::string message = command.substr(cmd.length());
            if (!message.empty() && message[0] == ' ') {
                message = message.substr(1);
            }
            return sendMessageToClients(message);
        }
        else if (cmd == "message_single") {
            std::string args = command.substr(cmd.length() + 1);
            size_t firstSpace = args.find(' ');
            if (firstSpace == std::string::npos) {
                return "{\"error\": \"Invalid message_single command format\"}";
            }
            std::string targetIp = args.substr(0, firstSpace);
            std::string message = args.substr(firstSpace + 1);
            return sendMessageToClient(targetIp, message);
        }
        // You should add handling for other commands here (e.g., "show_ips", "kill_switch", "stop", "help")
        // Example:
        else if (cmd == "show_ips") {
            return showConnectedIPs();
        }
        else if (cmd == "kill_switch") {
            return killSwitch(); // Make sure killSwitch returns a string
        }
        else if (cmd == "stop") {
            stop(); // stop() calls exit(0), so it doesn't return a string
            return "{\"status\": \"Server stopping\"}"; // Or handle appropriately
        }
        else if (cmd == "help") {
             return "{\"info\": \"Available commands: message_all <text>, message_single <ip> <text>, show_ips, kill_switch, stop, help\"}";
        }
        else {
            return "{\"error\": \"Unknown command\"}";
        }
    }
    
std::string sendMessageToClients(const std::string& message) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        int sentCount = 0;
        
        for (auto& client : clients) {
            if (client.connected) {
                std::string fullMessage = "MSG:" + message;
                if (send(client.socket, fullMessage.c_str(), fullMessage.length(), 0) > 0) {
                    logMessage("Send to " + client.ip + " {\"" + message + "\"}");
                    sentCount++;
                } else {
                    client.connected = false;
                    logMessage("Failed to send to " + client.ip);
                }
            }
        }
        
        return "{\"sent_clients\": " + std::to_string(sentCount) + "}"; // Corrected JSON
    }
    
    std::string showConnectedIPs() {
        std::lock_guard<std::mutex> lock(clientsMutex);
        std::ostringstream oss;
        oss << "{\"clients\": [";
        
        bool first = true;
        for (const auto& client : clients) {
            if (client.connected) {
                if (!first) oss << ",";
                oss << "\"" << client.ip << "\"";
                first = false;
            }
        }
        
        oss << "], \"count\": " << clients.size() << "}";
        return oss.str();
    }
    
std::string killSwitch() {
        std::lock_guard<std::mutex> lock(clientsMutex);
        int disconnectedCount = 0; // This variable is correctly defined within killSwitch
        
        for (auto& client : clients) {
            if (client.connected) {
                send(client.socket, "KILL_SWITCH", 11, 0);
                close(client.socket);
                client.connected = false;
                disconnectedCount++;
                logMessage("Force disconnected: " + client.ip);
            }
        }
        
        clients.clear();
        return "{\"disconnected_clients\": " + std::to_string(disconnectedCount) + "}"; // Corrected return
    }
    
    void stop() {
        running = false;
        
        // Send graceful shutdown to all clients
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto& client : clients) {
                if (client.connected) {
                    send(client.socket, "SERVER_SHUTDOWN", 15, 0);
                    close(client.socket);
                }
            }
            clients.clear();
        }
        
        if (serverSocket >= 0) close(serverSocket);
        if (javaSocket >= 0) close(javaSocket);
        
        logMessage("Server stopped gracefully");
        exit(0);
    }
    
    void monitorPings() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
            std::lock_guard<std::mutex> lock(clientsMutex);
            time_t now = time(nullptr);
            
            clients.erase(std::remove_if(clients.begin(), clients.end(),
                [now](Client& c) {
                    if (now - c.lastPing > 30) { // 30 second timeout
                        close(c.socket);
                        return true;
                    }
                    return false;
                }), clients.end());
        }
    }
    
    void autoUpdateBroadcast() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::minutes(5));
            
            sendMessageToClients("AUTO_UPDATE_CHECK");
            logMessage("Auto-update broadcast sent to all clients");
        }
    }
    
    void logMessage(const std::string& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << "[(" << std::put_time(&tm, "%Y-%m-%d")
            << ")(" << std::put_time(&tm, "%H:%M:%S")
            << ")] " << message;
        
        std::string logEntry = oss.str();
        
        // Print to console
        std::cout << logEntry << std::endl;
        
        // Write to log file
        std::ofstream logFile(LOG_FILE, std::ios::app);
        if (logFile.is_open()) {
            logFile << logEntry << std::endl;
            logFile.close();
        }
    }
};

int main() {
    std::cout << "Starting C++ Server (Linux)..." << std::endl;
    
    ServerManager server;
    server.start();
    
    return 0;
}
