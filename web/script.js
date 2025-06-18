// Server Management Interface JavaScript
let clients = [];
let updateInterval;

// Initialize the interface
document.addEventListener('DOMContentLoaded', function() {
    updateStatus();
    refreshClients();
    refreshLogs();
    
    // Start auto-refresh every 10 seconds
    updateInterval = setInterval(() => {
        updateStatus();
        refreshClients();
    }, 10000);
});

async function updateStatus() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        
        const statusDot = document.getElementById('statusDot');
        const statusText = document.getElementById('statusText');
        const lastUpdate = document.getElementById('lastUpdate');
        
        if (data.server_connected) {
            statusDot.className = 'status-dot connected';
            statusText.textContent = 'CONNECTED';
        } else {
            statusDot.className = 'status-dot disconnected';
            statusText.textContent = 'DISCONNECTED';
        }
        
        lastUpdate.textContent = `Last update: ${data.timestamp}`;
    } catch (error) {
        console.error('Failed to update status:', error);
        const statusDot = document.getElementById('statusDot');
        const statusText = document.getElementById('statusText');
        statusDot.className = 'status-dot disconnected';
        statusText.textContent = 'API OFFLINE';
        showNotification('Failed to connect to API server', 'error');
    }
}

async function refreshClients() {
    try {
        const response = await fetch('/api/clients');
        const data = await response.json();
        
        const clientsContainer = document.getElementById('clientsList');
        const messageTarget = document.getElementById('messageTarget');
        
        // Handle different response formats
        if (data.error) {
            clientsContainer.innerHTML = `<div class="client-item error">Error: ${data.error}</div>`;
            messageTarget.innerHTML = '<option value="all">All Clients (0)</option>';
            return;
        }
        
        let clientList = [];
        
        // Parse the clients array if it exists
        if (data.clients && Array.isArray(data.clients)) {
            clientList = data.clients;
        } else if (data.clients && typeof data.clients === 'string') {
            // Handle comma-separated string
            clientList = data.clients.split(',').map(ip => ip.trim()).filter(ip => ip);
        } else {
            // Fallback: look for any IP-like strings in the response
            const ipRegex = /\b(?:\d{1,3}\.){3}\d{1,3}\b/g;
            const responseText = JSON.stringify(data);
            const matches = responseText.match(ipRegex);
            if (matches) {
                clientList = [...new Set(matches)]; // Remove duplicates
            }
        }
        
        // Update clients global variable
        clients = clientList;
        
        if (clientList.length === 0) {
            clientsContainer.innerHTML = '<div class="client-item">No clients connected</div>';
            messageTarget.innerHTML = '<option value="all">All Clients (0)</option>';
        } else {
            // Display clients
            clientsContainer.innerHTML = clientList.map((ip, index) => `
                <div class="client-item">
                    <span class="client-ip">${ip}</span>
                    <span class="client-status">ONLINE</span>
                </div>
            `).join('');
            
            // Update target dropdown
            messageTarget.innerHTML = `<option value="all">All Clients (${clientList.length})</option>` +
                clientList.map(ip => `<option value="${ip}">${ip}</option>`).join('');
        }
        
        console.log('Clients updated:', clientList);
        
    } catch (error) {
        console.error('Failed to refresh clients:', error);
        document.getElementById('clientsList').innerHTML = 
            '<div class="client-item error">Error loading clients</div>';
        document.getElementById('messageTarget').innerHTML = 
            '<option value="all">All Clients (Error)</option>';
    }
}

async function sendMessage() {
    const target = document.getElementById('messageTarget').value;
    const content = document.getElementById('messageContent').value;
    
    if (!content.trim()) {
        showNotification('Please enter a message', 'error');
        return;
    }
    
    try {
        const response = await fetch('/api/message', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ target: target, message: content })
        });
        
        const result = await response.json();
        
        if (response.ok && !result.error) {
            const sentCount = result.sent_to || 'unknown';
            showNotification(`Message sent to ${sentCount} clients`, 'success');
            document.getElementById('messageContent').value = '';
            refreshLogs();
        } else {
            showNotification(`Failed to send message: ${result.error || 'Unknown error'}`, 'error');
        }
    } catch (error) {
        console.error('Failed to send message:', error);
        showNotification('Error sending message', 'error');
    }
}

async function executeCommand(command) {
    if (command === 'kill_switch' || command === 'stop') {
        if (!confirm(`Are you sure you want to execute "${command}"? This action cannot be undone.`)) {
            return;
        }
    }
    
    try {
        const response = await fetch('/api/command', {
            method: 'POST',
            // Set the correct Content-Type to application/json
            headers: {'Content-Type': 'application/json'}, 
            // Send the command within a JSON object
            body: JSON.stringify({ command: command }) 
        });
        
        const result = await response.json();
        
        if (response.ok && !result.error) {
            let message = `Command "${command}" executed successfully`;
            if (result.disconnected) {
                message += ` (${result.disconnected} clients affected)`;
            }
            showNotification(message, 'success');
            refreshClients();
            refreshLogs();
        } else {
            showNotification(`Failed to execute "${command}": ${result.error || 'Unknown error'}`, 'error');
        }
    } catch (error) {
        console.error('Failed to execute command:', error);
        showNotification('Error executing command', 'error');
    }
}

async function refreshLogs() {
    try {
        const response = await fetch('/api/logs');
        const data = await response.json();
        
        const logsContainer = document.getElementById('logsContainer');
        
        if (data.error) {
            logsContainer.textContent = `Error: ${data.error}`;
        } else {
            logsContainer.textContent = data.logs || 'No logs available';
        }
        
        // Auto-scroll to bottom
        logsContainer.scrollTop = logsContainer.scrollHeight;
    } catch (error) {
        console.error('Failed to refresh logs:', error);
        document.getElementById('logsContainer').textContent = 'Error loading logs';
    }
}

function showNotification(message, type) {
    const notification = document.getElementById('notification');
    notification.textContent = message;
    notification.className = `notification ${type}`;
    notification.classList.add('show');
    
    setTimeout(() => {
        notification.classList.remove('show');
    }, 4000);
}

// Handle page visibility for auto-refresh
document.addEventListener('visibilitychange', function() {
    if (document.hidden) {
        clearInterval(updateInterval);
    } else {
        updateStatus();
        refreshClients();
        updateInterval = setInterval(() => {
            updateStatus();
            refreshClients();
        }, 10000);
    }
});

// Debug function to test API endpoints
window.debugAPI = function(endpoint) {
    fetch(`/api/${endpoint}`)
        .then(response => response.json())
        .then(data => console.log(`${endpoint}:`, data))
        .catch(error => console.error(`${endpoint} error:`, error));
};