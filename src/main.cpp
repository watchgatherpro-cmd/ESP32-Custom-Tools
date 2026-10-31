#include <Arduino.h>
#include <WiFi.h>
#include "attacks.h"
#include "web_server.h"
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Free_WiFi_Update", "");
    initAttacks();
    initWebServer();
}
void loop() {
    handleWebServer();
}
