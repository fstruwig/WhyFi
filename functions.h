#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <ESP8266WiFi.h>
#include <Time.h>
#include "./structures.h"
#include "whyfi_eeprom.h"

#define MAX_CLIENTS_TRACKED 50
clientinfo clients_known[MAX_CLIENTS_TRACKED];            // Array to save MACs of known CLIENTs
int clients_known_count = 0;                              // Number of known CLIENTs


/*
 * Order array based on data_bytes
 */
void order_clients_known()
{
  clientinfo holder;
  
  for(int x = 0; x < clients_known_count-1; x++)
   for(int y = x+1; y < clients_known_count; y++)
     if(clients_known[y].data_bytes > clients_known[x].data_bytes) {
       holder = clients_known[x];
       clients_known[x] = clients_known[y];
       clients_known[y] = holder;
     }  
}


int register_client(clientinfo *ci, RxControl *rxcontrol)
{
  int known = 0;   // Clear known flag
    
  for (int u = 0; u < clients_known_count; u++)
  {
    if (! memcmp(clients_known[u].station, ci->station, ETH_MAC_LEN)) {
      //This is a client we know - update
      int32 t = millis()/1000;
      
      known = 1;
      //TODO - track sequence numbers for lost/missed packets
      
      clients_known[u].timed_data_bytes += rxcontrol->legacy_length;
      clients_known[u].data_bytes += rxcontrol->legacy_length;
      ci->data_bytes = clients_known[u].data_bytes;

      //if time bigger than 10 seconds, calculate rate
      if(t - clients_known[u].start_time > 10)
      {        
        clients_known[u].data_rate = clients_known[u].timed_data_bytes / (10 * 1024);
        clients_known[u].timed_data_bytes = 0;
        clients_known[u].start_time = t;
      }
      
      order_clients_known();
      break;
    }
  }
  if (! known)
  {
    if (clients_known_count == MAX_CLIENTS_TRACKED)
      return known;
  
    memset(ci->szname, '\0', EEPROM_MAX_NAME_LEN);
     
    //find in known name list
    int index = whyfi_eeprom_find_station(ci->station);
    if(index >= 0)
      memcpy(ci->szname, _eeprom_whyfi.mac_names[index].szname, EEPROM_MAX_NAME_LEN);
              
    ci->start_time = millis()/1000;
    ci->timed_data_bytes = 0;
    ci->data_rate = 0;
    memcpy(&clients_known[clients_known_count], ci, sizeof(*ci));
    clients_known_count++;

    if ((unsigned int) clients_known_count >=
        sizeof (clients_known) / sizeof (clients_known[0]) ) {
      Serial.printf("exceeded max clients_known\n");
      clients_known_count = 0;
    }
  }
  return known;
}

void print_client(clientinfo ci)
{
  int u = 0;
  int known = 0;   // Clear known flag
  if (ci.err != 0) 
  {
    // nothing
  } else 
  {
    Serial.printf("DEVICE: ");
    for (int i = 0; i < 6; i++) Serial.printf("%02x", ci.station[i]);
    Serial.printf(" ==> ");

    Serial.printf("%2s", " ");
    for (int i = 0; i < 6; i++) Serial.printf("%02x", ci.ap[i]);

    Serial.printf("    %dKB", ci.data_bytes/1024);
    Serial.println();
  }
}

void promisc_cb(uint8_t *buf, uint16_t len)
{
  if (len == 12) {
    //struct RxControl *sniffer = (struct RxControl*) buf;
  } else if (len == 128) {
    //struct sniffer_buf2 *sniffer = (struct sniffer_buf2*) buf;
    //struct beaconinfo beacon = parse_beacon(sniffer->buf, 112, sniffer->rx_ctrl.rssi);
  } else {
    struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
    
    //Is data or QOS?
    if ((sniffer->buf[0] == 0x08) || (sniffer->buf[0] == 0x88)) {
      struct clientinfo ci = parse_data(sniffer->buf, 36, sniffer->rx_ctrl.rssi, sniffer->rx_ctrl.channel);
      //filter on our SSID only - only if we don't connect to the network (promisc only)
      if (memcmp(ci.bssid, _eeprom_whyfi.ap_mac, ETH_MAC_LEN))
        return;
      //TODO: Check what this is for  
      if (memcmp(ci.bssid, ci.station, ETH_MAC_LEN)) {
        if (register_client(&ci, &(sniffer->rx_ctrl)) == 0) {
          print_client(ci);
        }
      }
    }
  }
}

#endif /* __FUNCTIONS_H__ */
