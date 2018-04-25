/*#include <ESP8266WiFi.h>
#include "structures.h"

void promisc_cb(uint8_t *buf, uint16_t len)
{
  //TODO: Understand what the different len values mean i.t.o. the frame type etc.
  if (len == 12) {
    struct RxControl *sniffer = (struct RxControl*) buf;
  } else if (len == 128) {
    struct sniffer_buf2 *sniffer = (struct sniffer_buf2*) buf;
    struct beaconinfo beacon = parse_beacon(sniffer->buf, 112, sniffer->rx_ctrl.rssi);

    //TODO: We probably already have this info
    if(find_mac_of_king_ssid(beacon))
    {    
      if (register_beacon(beacon) == 0) 
      {
        print_beacon(beacon);
        nothing_new = 0;
      }
    }
  } else {
    struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
    
    //Is data or QOS?
    if ((sniffer->buf[0] == 0x08) || (sniffer->buf[0] == 0x88)) {
      struct clientinfo ci = parse_data(sniffer->buf, 36, sniffer->rx_ctrl.rssi, sniffer->rx_ctrl.channel);
      //filter on our SSID only - only if we don't connect to the network (promisc only)
      if (memcmp(ci.bssid, elvis, ETH_MAC_LEN))
        return;
      //TODO: Check what this is for  
      if (memcmp(ci.bssid, ci.station, ETH_MAC_LEN)) {
        if (register_client(&ci, &(sniffer->rx_ctrl)) == 0) {
          print_client(ci);
          nothing_new = 0;
        }
      }
    }
  }
}
*/
