#include <Fsm.h>
#include <ESP8266WiFi.h>
#include "./functions.h"
#include "whyfi_eeprom.h"
#include <Wire.h>
#include "OLED.h"
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <assert.h>

#define __ASSERT_USE_STDERR

#define BTN_NEXT_PIN    0
#define BTN_SELECT_PIN  5

#define RST_OLED D2
OLED display(SDA, SCL);

#define DISPLAY_SNIFF_REFRESH_MS    (5*1000)

#define disable 0
#define enable  1

void on_sniff_enter();
void on_sniff();
void on_sniff_exit();
void on_captive();

State state_sniffing(on_sniff_enter, on_sniff, on_sniff_exit);
State state_captive(NULL, on_captive, NULL);

Fsm fsm(&state_sniffing);

#define EVT_START_SNIFF       1
#define EVT_CAPTIVE_PRESSED   2
#define EVT_CAPTIVE_DONE      3

void on_sniff_enter()
{
  Serial.println(__FUNCTION__);
  start_sniffing();
}

void on_sniff()
{
  static uint32_t last_refresh = millis();
  uint32_t current_time = millis();

  if (last_refresh + DISPLAY_SNIFF_REFRESH_MS < current_time)
  {
    last_refresh = current_time;
    show_sniffing();
  }
}

void on_sniff_exit()
{
  Serial.println(__FUNCTION__);
  wifi_promiscuous_enable(disable);
}

void on_captive()
{ 
  Serial.println(__FUNCTION__); 
  display.clear(); 
  display.print("Enter mobile AP",1);
  display.print("to configure...",2);
  start_captive_ap(); 
  fsm.trigger(EVT_CAPTIVE_DONE);
  Serial.println(__FUNCTION__); 
}

void setup() {
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);

  pinMode(RST_OLED, OUTPUT);
  digitalWrite(RST_OLED, LOW);   // turn D2 low to reset OLED
  delay(50);
  digitalWrite(RST_OLED, HIGH);    // while OLED is running, must set D2 in high
  display.begin();
  display.print("Snooping...");

  Serial.begin(115200);
  eeprom_read();

  pinMode(BTN_NEXT_PIN,INPUT_PULLUP);
  pinMode(BTN_SELECT_PIN,INPUT_PULLUP);

  fsm.add_transition(&state_sniffing, &state_captive, EVT_CAPTIVE_PRESSED, NULL);
  fsm.add_transition(&state_captive,  &state_sniffing, EVT_CAPTIVE_DONE, NULL);
}

void start_sniffing()
{
  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(_eeprom_whyfi.ap_chan);
  wifi_promiscuous_enable(disable);
  wifi_set_promiscuous_rx_cb(promisc_cb);   // Set up promiscuous callback
  wifi_promiscuous_enable(enable);
}


void loop() {
  bool next_btn_pressed = false;
  bool select_btn_pressed = false;
  
  delay(100);
  ESP.wdtFeed();
  
  next_btn_pressed = digitalRead(BTN_NEXT_PIN) == LOW;
  select_btn_pressed = digitalRead(BTN_SELECT_PIN) == LOW;
  
  fsm.trigger(EVT_START_SNIFF);
  fsm.run_machine();
  
  if(next_btn_pressed || select_btn_pressed)
  {
    Serial.println("Button Press Detected");
    fsm.trigger(EVT_CAPTIVE_PRESSED);
  }
}

void show_sniffing()
{
  display.clear();
  display.print("-MAC-|-KBS-|-MB-", (uint8_t) 0);
  for (int u = 0; u < clients_known_count; u++)
  {
    print_client(clients_known[u]);
    display_client(clients_known[u], u);
  }
  Serial.println();
}


void display_client(clientinfo ci, int line)
{
  char buf[32];
  int u = 0;
  int known = 0;   // Clear known flag

  line += 1;

  if (ci.err != 0) {
    // nothing
  } else {
    if (ci.szname[0] != '\0')
    {
      snprintf(buf, MAX_NAME_LEN, "%s", ci.szname);
    }
    else
    {
      for (int i = 0; i < 3; i++) snprintf(buf + i * 2, 3, "%02x", ci.station[i]);
    }
    display.print(buf, (uint8_t) line);
    //snprintf(buf, sizeof(buf), "...==>%3dMB", ci.data_bytes/(1024*1024));
    snprintf(buf, sizeof(buf), "|  %3d| %3d", ci.data_rate, ci.data_bytes / (1024 * 1024));
    display.print(buf, (uint8_t) line, 5);
  }
}


WiFiManager wifiManager;

void start_captive_ap()
{
  char human[MAX_CLIENTS_TRACKED][EEPROM_MAX_NAME_LEN];
  char placeholders[MAX_CLIENTS_TRACKED][18];
  char paramIds[MAX_CLIENTS_TRACKED][4];
  WiFiManagerParameter wifiMacParam[MAX_CLIENTS_TRACKED];

  for (int u = 0; u < (clients_known_count > EEPROM_MAX_NUM_MAC_NAMES ? EEPROM_MAX_NUM_MAC_NAMES : clients_known_count); u++)
  {
    snprintf(paramIds[u], 4, "%d", u);
    for (int i = 0; i < 6; i++) 
    {
      snprintf(&placeholders[u][i * 2], 4, "%02x-", clients_known[u].station[i]);
    }
    wifiMacParam[u].init(paramIds[u], placeholders[u], clients_known[u].szname, EEPROM_MAX_NAME_LEN, "");
    wifiManager.addParameter(&wifiMacParam[u]);
  }
  
  wifiManager.setSaveConfigCallback(saveConfigCb);
  wifiManager.setResetCallback(resetCb);
  wifiManager.setDebugOutput(true);
  if (!wifiManager.startConfigPortal("WhyFi")) {
    Serial.println("failed to connect and hit timeout");
    delay(500);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("We're happy with the connection.  Resuming sniffing.");

  //Close WifiConnection and free up explicitly
  wifiManager.~WiFiManager();
}

void resetCb()
{
  eeprom_clear();
}

void saveConfigCb()
{
  char buf[13];
  Serial.println("Saving configuration");

  //save AP info
  String ssid = wifiManager.getSSID();
  String pass = wifiManager.getPassword();
  assert(ssid != NULL);
  assert(pass != NULL);

  strncpy(_eeprom_whyfi.ap_name, ssid.c_str(), EEPROM_MAX_AP_LEN);
  strncpy(_eeprom_whyfi.ap_pass, pass.c_str(), EEPROM_MAX_PASS_LEN);

  _eeprom_whyfi.ap_chan = WiFi.channel();
  memcpy(_eeprom_whyfi.ap_mac, WiFi.BSSID(), ETH_MAC_LEN); 
  
  Serial.print("Saved SSID:");
  Serial.println(_eeprom_whyfi.ap_name);
  Serial.print("Saved Password:");
  Serial.println(_eeprom_whyfi.ap_pass);
  Serial.print("Channel:");
  Serial.println(_eeprom_whyfi.ap_chan);

  //save MAC Name pairs
  int count;
  WiFiManagerParameter** params = wifiManager.getParams(&count);

  Serial.print("Received parameters:");
  Serial.println(count);

  for (int i = 0; i < count; i++)
  {
    //TODO: The order of things may get thrown out if the client list is accessed during the config period.
    //must lock it down.  Assuming that the list has stayed the same.
    whyfi_eeprom_add_pair(clients_known[i].station, params[i]->getValue());
    params[i]->getID();
  }
  eeprom_write();
}


