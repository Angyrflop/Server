import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpExchange;
import java.io.*;
import java.net.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.Executors;
import java.text.SimpleDateFormat;
import java.util.regex.Pattern;
import java.util.regex.Matcher;

public class API {
    private static final int WEB_PORT = 8080;
    private static final String CPP_SERVER_HOST = "localhost";
    private static final int CPP_SERVER_PORT = 9999;
    private static final String WEB_ROOT = "./web";
    private static final String LOG_FILE = "server.log";
    
    private HttpServer server;
    private Socket cppSocket;
    private PrintWriter cppOut;
    private BufferedReader cppIn;
    private volatile boolean cppConnected = false;
    private long lastConnectionAttempt = 0;
    private static final long CONNECTION_RETRY_DELAY = 5000; // 5 seconds
    
    public static void main(String[] args) {
        new API().start();
    }
    
    public void start() {
        try {
            // Create HTTP server
            server = HttpServer.create(new InetSocketAddress(WEB_PORT), 0);
            server.setExecutor(Executors.newCachedThreadPool());
            
            // Setup endpoints
            server.createContext("/", new StaticFileHandler());
            server.createContext("/api/clients", new ClientsHandler());
            server.createContext("/api/message", new MessageHandler());
            server.createContext("/api/command", new CommandHandler());
            server.createContext("/api/logs", new LogsHandler());
            server.createContext("/api/status", new StatusHandler());
            
            server.start();
            System.out.println("Web server started on port " + WEB_PORT);
            System.out.println("Access GUI at: http://localhost:" + WEB_PORT);
            
            // Try to connect to C++ server
            connectToCppServer();
            
        } catch (Exception e) {
            System.err.println("Failed to start web server: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private void connectToCppServer() {
        Thread connectionThread = new Thread(() -> {
            while (true) {
                try {
                    if (!cppConnected) {
                        long currentTime = System.currentTimeMillis();
                        if (currentTime - lastConnectionAttempt > CONNECTION_RETRY_DELAY) {
                            lastConnectionAttempt = currentTime;
                            
                            // Close existing socket if any
                            closeCppConnection();
                            
                            // Try to connect
                            cppSocket = new Socket();
                            cppSocket.setSoTimeout(3000); // 3 second timeout
                            cppSocket.connect(new InetSocketAddress(CPP_SERVER_HOST, CPP_SERVER_PORT), 3000);
                            
                            cppOut = new PrintWriter(cppSocket.getOutputStream(), true);
                            cppIn = new BufferedReader(new InputStreamReader(cppSocket.getInputStream()));
                            cppConnected = true;
                            System.out.println("Connected to C++ server");
                        }
                    } else {
                        // Test connection with a ping
                        if (!testConnection()) {
                            cppConnected = false;
                            closeCppConnection();
                            System.err.println("Lost connection to C++ server");
                        }
                    }
                    Thread.sleep(5000);
                } catch (Exception e) {
                    cppConnected = false;
                    closeCppConnection();
                    System.err.println("C++ server connection failed: " + e.getMessage());
                    try {
                        Thread.sleep(5000);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                }
            }
        });
        connectionThread.setDaemon(true);
        connectionThread.start();
    }
    
    private boolean testConnection() {
        try {
            if (cppSocket != null && !cppSocket.isClosed() && cppSocket.isConnected()) {
                return true;
            }
        } catch (Exception e) {
            // Connection is bad
        }
        return false;
    }
    
    private void closeCppConnection() {
        try {
            if (cppOut != null) {
                cppOut.close();
                cppOut = null;
            }
            if (cppIn != null) {
                cppIn.close();
                cppIn = null;
            }
            if (cppSocket != null && !cppSocket.isClosed()) {
                cppSocket.close();
            }
        } catch (Exception e) {
            // Ignore cleanup errors
        }
    }
    
    private synchronized String sendCommandToCpp(String command) {
        if (!cppConnected || cppOut == null || cppIn == null) {
            return "{\"error\": \"C++ server not connected\"}";
        }
        
        try {
            cppOut.println(command);
            cppOut.flush();
            
            StringBuilder response = new StringBuilder();
            String line;
            long timeout = System.currentTimeMillis() + 10000; // 10 second timeout
            boolean foundEnd = false;
            
            while (System.currentTimeMillis() < timeout && !foundEnd) {
                if (cppIn.ready()) {
                    line = cppIn.readLine();
                    if (line != null) {
                        if (line.equals("END_RESPONSE")) {
                            foundEnd = true;
                            break;
                        }
                        if (response.length() > 0) {
                            response.append("\n");
                        }
                        response.append(line);
                    }
                } else {
                    Thread.sleep(50);
                }
            }
            
            if (!foundEnd && response.length() == 0) {
                cppConnected = false;
                return "{\"error\": \"No response from C++ server\"}";
            }
            
            String result = response.toString().trim();
            return result.isEmpty() ? "{\"error\": \"Empty response\"}" : result;
            
        } catch (Exception e) {
            cppConnected = false;
            return "{\"error\": \"Communication failed: " + e.getMessage() + "\"}";
        }
    }
    
    // Static file handler for serving HTML/CSS/JS
    class StaticFileHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            String path = exchange.getRequestURI().getPath();
            if (path.equals("/")) path = "/index.html";
            
            File file = new File(WEB_ROOT + path);
            
            if (file.exists() && file.isFile()) {
                String contentType = getContentType(path);
                exchange.getResponseHeaders().set("Content-Type", contentType);
                exchange.sendResponseHeaders(200, file.length());
                
                try (FileInputStream fis = new FileInputStream(file);
                     OutputStream os = exchange.getResponseBody()) {
                    byte[] buffer = new byte[8192];
                    int bytesRead;
                    while ((bytesRead = fis.read(buffer)) != -1) {
                        os.write(buffer, 0, bytesRead);
                    }
                }
            } else {
                String response = "404 Not Found";
                exchange.sendResponseHeaders(404, response.length());
                exchange.getResponseBody().write(response.getBytes());
                exchange.getResponseBody().close();
            }
        }
        
        private String getContentType(String path) {
            if (path.endsWith(".html")) return "text/html";
            if (path.endsWith(".css")) return "text/css";
            if (path.endsWith(".js")) return "application/javascript";
            if (path.endsWith(".json")) return "application/json";
            return "text/plain";
        }
    }
    
    // API endpoint handlers
    class ClientsHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("GET".equals(exchange.getRequestMethod())) {
                String response = sendCommandToCpp("show_ips");
                
                // Ensure we always return valid JSON
                if (!response.startsWith("{") && !response.startsWith("[")) {
                    response = "{\"error\": \"Invalid response format\", \"raw\": \"" + escapeJson(response) + "\"}";
                }
                
                sendJsonResponse(exchange, 200, response);
            } else {
                sendJsonResponse(exchange, 405, "{\"error\": \"Method not allowed\"}");
            }
        }
    }
    
    class MessageHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("POST".equals(exchange.getRequestMethod())) {
                String body = readRequestBody(exchange);
                String response = sendCommandToCpp("message " + body);
                
                // Ensure valid JSON response
                if (!response.startsWith("{") && !response.startsWith("[")) {
                    response = "{\"error\": \"Invalid response format\", \"raw\": \"" + escapeJson(response) + "\"}";
                }
                
                sendJsonResponse(exchange, 200, response);
            } else {
                sendJsonResponse(exchange, 405, "{\"error\": \"Method not allowed\"}");
            }
        }
    }
    
    class CommandHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("POST".equals(exchange.getRequestMethod())) {
                String body = readRequestBody(exchange);
                String response = sendCommandToCpp(body);
                
                // Ensure valid JSON response
                if (!response.startsWith("{") && !response.startsWith("[")) {
                    response = "{\"success\": true, \"message\": \"" + escapeJson(response) + "\"}";
                }
                
                sendJsonResponse(exchange, 200, response);
            } else {
                sendJsonResponse(exchange, 405, "{\"error\": \"Method not allowed\"}");
            }
        }
    }
    
    class LogsHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("GET".equals(exchange.getRequestMethod())) {
                try {
                    String logs = readLogFile();
                    sendJsonResponse(exchange, 200, "{\"logs\": \"" + escapeJson(logs) + "\"}");
                } catch (Exception e) {
                    sendJsonResponse(exchange, 500, "{\"error\": \"Failed to read logs: " + escapeJson(e.getMessage()) + "\"}");
                }
            } else {
                sendJsonResponse(exchange, 405, "{\"error\": \"Method not allowed\"}");
            }
        }
        
        private String readLogFile() throws IOException {
            File logFile = new File(LOG_FILE);
            if (!logFile.exists()) return "No logs available";
            
            List<String> lines = Files.readAllLines(logFile.toPath());
            // Get last 100 lines
            int start = Math.max(0, lines.size() - 100);
            return String.join("\\n", lines.subList(start, lines.size()));
        }
    }
    
    class StatusHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            String timestamp = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss").format(new Date());
            String status = String.format(
                "{\"server_connected\": %s, \"timestamp\": \"%s\", \"last_attempt\": %d}", 
                cppConnected, timestamp, lastConnectionAttempt
            );
            sendJsonResponse(exchange, 200, status);
        }
    }
    
    // Helper methods
    private String readRequestBody(HttpExchange exchange) throws IOException {
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(exchange.getRequestBody()))) {
            StringBuilder body = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                body.append(line);
            }
            return body.toString();
        }
    }
    
    private void sendJsonResponse(HttpExchange exchange, int statusCode, String response) throws IOException {
        exchange.getResponseHeaders().set("Content-Type", "application/json");
        exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type");
        
        byte[] responseBytes = response.getBytes();
        exchange.sendResponseHeaders(statusCode, responseBytes.length);
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(responseBytes);
        }
    }
    
    private String escapeJson(String text) {
        if (text == null) return "";
        return text.replace("\\", "\\\\")
                  .replace("\"", "\\\"")
                  .replace("\n", "\\n")
                  .replace("\r", "\\r")
                  .replace("\t", "\\t");
    }
}