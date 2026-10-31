#ifndef ATTACKS_H
#define ATTACKS_H
#include <Arduino.h>
void initAttacks();
void startDeauth(uint8_t* bssid, uint8_t channel);
void startEvilTwin(String ssid, uint8_t* bssid, uint8_t channel);
void startBeaconSpam(String baseName, int count);
void stopAllAttacks();
#endif
