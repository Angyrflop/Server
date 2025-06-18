# Cross-Platform Server/Client System

## Overview

This project delivers a robust, Linux-based server and web-controlled management system for Windows client applications. It provides a centralized platform for remote client management, communication, and system monitoring.

The system comprises:

* **Java Web Server:** Acts as a bridge, connecting the web-based GUI to the C++ backend.
* **C++ Backend Server:** Manages connected clients and executes administrative functions.
* **Web-based GUI:** An intuitive HTML/CSS/JS interface for system control and monitoring.
* **Windows C++ Client Application:** Deployed on target machines to receive commands and provide system information.

## Components

### 1. Server Components

#### `server.cpp` (C++ Backend Server)

The core backend server responsible for client communication and management.

* **Update Notification Broadcast:** Notifies all connected clients about pending updates.
* **Client Management Functions:**
    * `message()`: Sends custom messages to connected clients.
    * `stop()`: Initiates a graceful server shutdown.
    * `kill_switch()`: Forces disconnection of all active clients.
    * `show_ips()`: Displays the IPv4 addresses of all currently connected clients.
* **Comprehensive Logging:** Maintains detailed logs of commands, connections, and messages.
    * Format: `[(YYYY-MM-DD)(HH:MM:SS)] send to [IP] {"Message content..."}`

#### Java Web Server

This component serves as the intermediary between the web GUI and the C++ backend.

* **API Exposure:** Exposes all `server.cpp` functionalities as API endpoints.
* **Web Request Handling:** Manages incoming web requests and facilitates communication with the C++ backend.

### 2. Client Application (`client.cpp`)

A lightweight Windows C++ application designed for deployment on target machines.

* **Connection Management:**
    * Initiates a server ping at 1-minute intervals when the server is offline.
    * Announces its connection status upon server availability.
    * Maintains a regular 3-second ping when connected.
* **Core Functionality:**
    * Displays messages received from the server.
    * Logs all commands executed.
    * Responds to IP address requests.
    * Gracefully handles the `kill_switch` command.
* **Optimization:** Designed to operate without a visible window in release versions to minimize system footprint.

### 3. Web GUI

A user-friendly web interface built with HTML, CSS, and JavaScript.

* **Client Monitoring:** Provides a clear view of all connected clients.
* **Command Execution:** Allows for sending messages and executing server commands.
* **Log Viewing:** Displays real-time server logs.
* **Interface Rework (Planned):** Future enhancements include a "Command Center" (CC) for all actions and a "Viewing Center" (VC) for read-only information, accessible via dedicated buttons on the top of the interface.

## System Requirements

* **Operating System:** Linux environment for the server.
* **Java Runtime:** Java Runtime Environment (JRE) version 11 or higher.
* **C++ Compiler:** G++ (GNU C++ Compiler) recommended.
* **Web Browser:** Modern web browser with JavaScript support.
* **Client Operating System:** Windows for the client application.

## Installation

1.  **Compile C++ Components:**
    ```bash
    g++ -o server server.cpp [additional flags]
    g++ -o client client.cpp [additional flags] -lws2_32 # Add Winsock library for Windows client
    ```
2.  **Configure Java Web Server:** Adjust properties as needed.
3.  **Deploy Web Files:** Place HTML/CSS/JS files in the web server's designated directory.

## Usage

1.  **Start the C++ Server:**
    ```bash
    ./server
    ```
2.  **Launch Java Web Server:** Execute the Java web server application.
3.  **Access GUI:** Open your web browser and navigate to `http://localhost:[Java_Web_Server_Port]`. (Default likely 8080 or configurable)
4.  **Deploy Clients:** Distribute the compiled `client.exe` to target Windows machines.

## Network Configuration

* **Server Ports:**
    * Java Web Server: Typically `8080` (or configured port for HTTP/HTTPS).
    * C++ Server Listener: `9998` (for client connections).
    * C++ Server Internal: `9999` (potentially for internal communication or specific client-server interactions).
* **Firewall Requirements:**
    * Allow TCP traffic on the Java Web Server port (e.g., 8080) for web interface access.
    * Allow TCP traffic on the C++ server port (`9998`) for client connections.
    * Allow HTTP/HTTPS traffic for web interface access if deployed externally.

## Logging

* **Server Logs:** `server.log`
* **Client Logs:** `client.log`
* **Web Server Logs:** Records access attempts and API calls.

## Security Considerations

* Implement robust authentication mechanisms for the web interface.
* Thoroughly validate all client-server communications to prevent malicious inputs.
* Consider implementing TLS/SSL for encrypting sensitive operations.
* Apply rate limiting to client connections to mitigate denial-of-service attacks.
* **Warning:** Certain planned features like "RAID start/shutdown" and "DELETE (self-uninstall)" may trigger antivirus software and require elevated permissions, potentially being classified as malware behavior. Exercise caution and ensure proper authorization and testing in controlled environments.

## Maintenance

* Implement log rotation for `server.log` to manage file size.
* Regularly apply security patches to all components.
* Develop a monitoring script to ensure the server process remains active.

## Troubleshooting

* **Port Conflicts:** Verify that no other services are utilizing the required server ports (`8080`, `9998`, `9999`).
* **Connection Failures:** Check firewall settings on both server and client machines.
* **Compilation Errors:** Ensure all necessary dependencies are installed and compiler flags are correct.

## Contributors

* AngyrFlop / justanyordinarydude@gmail.com

## Future Enhancements (Roadmap)

### Planned Features

* **Call Function:** Ability to ping target client IP addresses.
* **RAID Start/Shutdown:** Capability to randomly open/close applications on the client (e.g., `cmd`). *Note: See Security Considerations regarding potential antivirus flagging.*
* **DELETE:** Functionality for the client application to self-uninstall from a specific target machine. *Note: This requires elevated permissions and platform-specific handling.*
* **SILENCE:** Temporarily suppress output on the client side.
* **Encryption Toggle:** Implement Cipher encryption for client-server communication.

### Reworks

* **Logging System:** Refine the logging to focus primarily on errors, rather than logging all events.
* **GUI:**
    * Introduce "Command Center" (CC) for all actions and "Viewing Center" (VC) for read-only information.
    * Add dedicated buttons on the top of the website to switch between CC and VC.
* **API:** Improve responsiveness and ensure all exposed functions operate correctly.
* **Client-Side:**
    * Ensure the client window remains hidden in release versions.
    * Focus on decreasing overall system resource usage.

### Known Problems

* Messages are currently reported as errors, despite successful delivery.
* The message system sends the given message to all clients, even when a specific client is selected.

### Roadmap

1.  Address and resolve current known problems.
2.  Rework the API and optimize the client-side application.
3.  Implement new features.
4.  Rework and enhance the Web GUI.
5.  Project reveal and demonstration to Jemmy.
6.  Thorough testing outside the local network.
7.  Add Whitelist on who will be able to access the website.
8.  Testing in unplanned scenarios.
9.  Stress testing and simulation on the server.
10.  Further feature additions and refinements.
