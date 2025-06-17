Overview
A Linux-based server-client system with:

Java web server (bridges GUI to C++ backend)

C++ backend server with client management

Web-based GUI (HTML/CSS/JS)

C++ client application

Components
1. Server Components
server.cpp features:

Auto-update broadcast to all connected clients

Client management functions:

message() - Send messages to clients

stop() - Gracefully shutdown server

kill_switch() - Force disconnect all clients

show_ips() - Display all connected client IPv4 addresses

Comprehensive logging:

Format: [(YYYY-MM-DD)(HH:MM:SS)] send to [IP] {"Message content..."}

Logs all commands, connections, and messages

Java Web Server:

Acts as bridge between web GUI and C++ server

Exposes all server.cpp functions via API endpoints

Handles web requests and communicates with C++ backend

2. Client Application (client.cpp)
Connection behavior:

Initial server ping (1 minute interval when server offline)

Connection announcement when server available

Regular 3-second ping when connected

Functionality:

Displays server messages

Logs all commands received

Responds to IP requests

Gracefully handles kill_switch command

3. Web GUI
HTML/CSS/JS interface for:

Viewing connected clients

Sending messages

Executing server commands

Viewing server logs

System Requirements
Linux environment

Java Runtime (version X or higher)

C++ compiler (g++ recommended)

Web browser with JavaScript support

Installation
Compile C++ components:

bash
g++ -o server server.cpp [additional flags]
g++ -o client client.cpp [additional flags]
Configure Java web server properties

Deploy HTML/CSS/JS files to web server directory

Usage
Start the C++ server:

bash
./server
Launch Java web server

Access GUI at http://localhost:[port]

Deploy client application to target machines

Network Configuration
Default port: [specify your port]

Firewall requirements:

Allow TCP on server port

Allow HTTP/HTTPS for web interface

Logging
Server logs to server.log

Client logs to client.log

Web server logs access attempts and API calls

Security Considerations
Implement authentication for web interface

Validate all client-server communications

Consider TLS for sensitive operations

Rate limiting for client connections

Maintenance
Log rotation for server logs

Regular updates for security patches

Monitoring script for server process

Troubleshooting
Common issues:

Port conflicts: Verify no other service is using the server port

Connection failures: Check firewall settings

Compilation errors: Verify all dependencies are installed

Contributors
[AngyrFlop/justanyordinarydude@gmail.com]

Future Enhancements
Client auto-update capability

Server clustering for high availability

Mobile-friendly web interface

Enhanced security features

Project Name - Cross-Platform Server/Client System
Overview
A Linux-based server with Windows clients, featuring:

Java web server (bridges GUI to C++ backend)

C++ backend server with client management

Web-based GUI (HTML/CSS/JS)

Windows C++ client application

Plans:
New features:
-Call function (Pings Address)
-RAID start/shutdown (Opens Applications randomly(eg., cmd))
-DELETE (uninstalls itself on the spefic client)
-SILENCE (Temporally silences output on the client side)
-Encryption toggle (Cipher encryption)

Reworks:
-Log system (Should'nt log everything, only Errors)
-GUI (slight GUI improvements, like creating a CC (command center(All actions/Everything))) and a VC (Viewing Center(Only readable stuff like what clients join'd))
    -On top of the website will be 2 buttons, 1 CC, 2 VC (To switch between centers)
-API (Improve responsiveness and make all functions work)
-Client side (Window should'nt show at all, only in debug versions. Also trying to decrease system usage.)

Known Problems:
-Messages currently being reported as error's, even tho they work
-Message system sends the given message to all clients, even when only having a spefic client selected
-Im fucking stupid and cant code.

Roadmap:
1. Fixing known problems.
2. Reworking API and client side.
3. adding new functions.
4. Reworking GUI.
5. Reveal Project to Jemmy.
6. Test outside local network.
7. Test in unplanned scenes.
8. Simulating stress to server.
9. Add even more features.


