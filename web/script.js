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
        showNotification('Failed to connect to server', 'error');
    }
}

async function refreshClients() {
    try {
        const response = await fetch('/api/clients');
        const data = await response.text();
        
        // Parse the response (assuming it returns IP addresses)
        const clientsContainer = document.getElementById('clientsList');
        const messageTarget = document.getElementById('messageTarget');
        
        if (data.includes('error') || data.trim() === '') {
            clientsContainer.innerHTML = '<div class="client-item">No clients connected</div>';
            // Reset target dropdown
            messageTarget.innerHTML = '<option value="all">All Clients</option>';
        } else {
            // Parse IP addresses from response
            const ips = data.split('\n').filter(ip => ip.trim() && !ip.includes('error'));
            clients = ips;
            
            clientsContainer.innerHTML = ips.map(ip => `
                <div class="client-item">
                    <span class="client-ip">${ip.trim()}</span>
                    <span class="client-status">ONLINE</span>
                </div>
            `).join('');
            
            // Update target dropdown
            messageTarget.innerHTML = '<option value="all">All Clients</option>' +
                ips.map(ip => `<option value="${ip.trim()}">${ip.trim()}</option>`).join('');
        }
    } catch (error) {
        console.error('Failed to refresh clients:', error);
        document.getElementById('clientsList').innerHTML = 
            '<div class="client-item">Error loading clients</div>';
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
        const command = target === 'all' ? 
            `message ${content}` : 
            `message_to ${target} ${content}`;
        
        const response = await fetch('/api/message', {
            method: 'POST',
            headers: {'Content-Type': 'text/plain'},
            body: command
        });
        
        const result = await response.text();
        
        if (response.ok) {
            showNotification('Message sent successfully', 'success');
            document.getElementById('messageContent').value = '';
            refreshLogs();
        } else {
            showNotification('Failed to send message', 'error');
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
            headers: {'Content-Type': 'text/plain'},
            body: command
        });
        
        const result = await response.text();
        
        if (response.ok) {
            showNotification(`Command "${command}" executed`, 'success');
            refreshClients();
            refreshLogs();
        } else {
            showNotification(`Failed to execute "${command}"`, 'error');
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
        logsContainer.textContent = data.logs || 'No logs available';
        
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