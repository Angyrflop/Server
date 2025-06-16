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

public class WebServerBridge {
    private static final int WEB_PORT = 8080;
    private static final String CPP_SERVER_HOST = "localhost";
    private static final int CPP_SERVER_PORT = 9999;
    private static final String WEB_ROOT = "./web";
    private static final String LOG_FILE = "server.log";
    
    private HttpServer server;
    private Socket cppSocket;
    private PrintWriter cppOut;
    private BufferedReader cppIn;
    private boolean cppConnected = false;
    
    public static void main(String[] args) {
        new WebServerBridge().start();
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
                        cppSocket = new Socket(CPP_SERVER_HOST, CPP_SERVER_PORT);
                        cppOut = new PrintWriter(cppSocket.getOutputStream(), true);
                        cppIn = new BufferedReader(new InputStreamReader(cppSocket.getInputStream()));
                        cppConnected = true;
                        System.out.println("Connected to C++ server");
                    }
                    Thread.sleep(5000);
                } catch (Exception e) {
                    cppConnected = false;
                    System.err.println("C++ server connection failed: " + e.getMessage());
                    try {
                        Thread.sleep(10000); // Retry every 10 seconds
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
    
    private String sendCommandToCpp(String command) {
        if (!cppConnected) {
            return "{\"error\": \"C++ server not connected\"}";
        }
        
        try {
            cppOut.println(command);
            StringBuilder response = new StringBuilder();
            String line;
            long timeout = System.currentTimeMillis() + 5000; // 5 second timeout
            
            while (System.currentTimeMillis() < timeout) {
                if (cppIn.ready()) {
                    line = cppIn.readLine();
                    if (line != null) {
                        response.append(line).append("\n");
                        if (line.equals("END_RESPONSE")) break;
                    }
                }
                Thread.sleep(50);
            }
            
            return response.toString().trim();
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
                    sendJsonResponse(exchange, 500, "{\"error\": \"Failed to read logs\"}");
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
            String status = "{\"server_connected\": " + cppConnected + ", \"timestamp\": \"" + 
                           new SimpleDateFormat("yyyy-MM-dd HH:mm:ss").format(new Date()) + "\"}";
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
        
        exchange.sendResponseHeaders(statusCode, response.getBytes().length);
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(response.getBytes());
        }
    }
    
    private String escapeJson(String text) {
        return text.replace("\\", "\\\\")
                  .replace("\"", "\\\"")
                  .replace("\n", "\\n")
                  .replace("\r", "\\r")
                  .replace("\t", "\\t");
    }
}
