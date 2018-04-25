#ifndef __WHYFI_EEPROM_H__
#define __WHYFI_EEPROM_H__

#include <EEPROM.h>
#include <assert.h>
#include "structures.h"

#define EEPROM_MAX_NAME_LEN   10
#define EEPROM_MAX_AP_LEN     32
#define EEPROM_MAX_PASS_LEN   32
#define EEPROM_MAX_NUM_MAC_NAMES ((512 - EEPROM_MAX_AP_LEN - EEPROM_MAX_PASS_LEN - 8 - ETH_MAC_LEN) / (EEPROM_MAX_NAME_LEN + ETH_MAC_LEN))
  
struct eeprom_mac_name
{
  uint8_t mac[ETH_MAC_LEN];
  char szname[EEPROM_MAX_NAME_LEN];
};

/*
 * 512 bytes
 * 
 *  64 bytes --> accesspoint credentials 
 *  
 *  16 bytes --> mac & name combination
 *  x 28
 *  ==
 *  512
 */
struct eeprom_whyfi
{
  char ap_name[EEPROM_MAX_AP_LEN];
  char ap_pass[EEPROM_MAX_PASS_LEN];
  int  ap_chan;
  uint8 ap_mac[ETH_MAC_LEN];
  int  mac_name_pos;
  struct eeprom_mac_name mac_names[EEPROM_MAX_NUM_MAC_NAMES];
};

struct eeprom_whyfi _eeprom_whyfi;

void print_mac_name_pair(int index)
{
  char buf[64];
  uint8_t *mac = _eeprom_whyfi.mac_names[index].mac;
  char *szname = _eeprom_whyfi.mac_names[index].szname;

  snprintf(buf, sizeof(buf), "MAC:%02x.%02x.%02x.%02x.%02x.%02x --> %s",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], szname);  
  Serial.println(buf);
}

void print_mac_name_pairs()
{
  Serial.println("=== MAC-NAME Pairs ===");
  for(int i=0; i<_eeprom_whyfi.mac_name_pos; i++)
    print_mac_name_pair(i);
  Serial.println("======================");
}


void eeprom_clear()
{
  Serial.print("Clearing EEPROM...");
  //TODO - make button to clear EEPROM
  EEPROM.begin(512);
  for (int i = 0; i < 512; i++)
    EEPROM.write(i, 0);
  EEPROM.commit();
  delay(200);
  EEPROM.end();
  Serial.println("done");
}

/*
 * Dump the existing eeprom_whify struct to eeprom
 */
void eeprom_write()
{
  uint8_t *ptr = (uint8_t *) &_eeprom_whyfi;

  Serial.print("Current Size:");
  Serial.println(sizeof(_eeprom_whyfi));
  delay(100);
  assert(sizeof(_eeprom_whyfi) <= 512);
  
  Serial.print("Writing EEPROM memory...");
  EEPROM.begin(512);
  for (int i = 0; i < 512; i++)
    EEPROM.write(i, *(ptr + i));
  EEPROM.commit();
  delay(200);
  EEPROM.end();
  Serial.println("done");
}

/*
 * Read eeprom_whify from eeprom
 */
void eeprom_read()
{
  uint8_t *ptr = (uint8_t *) &_eeprom_whyfi;

  Serial.print("Reading EEPROM memory...");
  EEPROM.begin(512);
  for (int i = 0; i < 512; i++)
    *(ptr + i) = EEPROM.read(i);
  EEPROM.end();
  Serial.println("done");

  if(_eeprom_whyfi.ap_name[0] != '\0'){
    Serial.print("Current Access Point: ");
    Serial.println(_eeprom_whyfi.ap_name);
  }

  print_mac_name_pairs();
}

int whyfi_eeprom_find_station(const uint8_t *station)
{
  for(int i=0; i<_eeprom_whyfi.mac_name_pos; i++)
  {
    if(memcmp(station, _eeprom_whyfi.mac_names[i].mac, ETH_MAC_LEN) == 0)
    {
      Serial.print("Found station at ");
      Serial.println(i);
      return i;
    }
  }
  return -1;
}

void whyfi_eeprom_del_pair(int index)
{
  for(int i=index; i < _eeprom_whyfi.mac_name_pos; i++)
  {
    memcpy(_eeprom_whyfi.mac_names[i].mac, _eeprom_whyfi.mac_names[i+1].mac, ETH_MAC_LEN);
    memcpy(_eeprom_whyfi.mac_names[i].szname, _eeprom_whyfi.mac_names[i+1].szname, EEPROM_MAX_NAME_LEN);
  }  
  _eeprom_whyfi.mac_name_pos--;

  for(int i=_eeprom_whyfi.mac_name_pos; i < EEPROM_MAX_NUM_MAC_NAMES; i++)
  {
    memset(_eeprom_whyfi.mac_names[i].mac, ETH_MAC_LEN, 0);
    memset(_eeprom_whyfi.mac_names[i].szname, EEPROM_MAX_NAME_LEN, 0);
  }
}

bool whyfi_eeprom_add_pair(const uint8_t *station /*[ETH_MAC_LEN]*/, const char *szvalue)
{
  //check if the station is already in the list
  int index = whyfi_eeprom_find_station(station);

  if(index < 0)
  {
    //skip if entry unlabeled
    if(szvalue[0] == '\0')
    {      
      Serial.println("Skipping empty");
      return false;
    }
 
    if(_eeprom_whyfi.mac_name_pos >= EEPROM_MAX_NUM_MAC_NAMES)
    {
      Serial.println("WARN: MAC table is full.  Cannot save mac/name pair.");
      return false; 
    }
    
    //add to back of list
    memcpy(_eeprom_whyfi.mac_names[_eeprom_whyfi.mac_name_pos].mac, station, ETH_MAC_LEN);
    snprintf(_eeprom_whyfi.mac_names[_eeprom_whyfi.mac_name_pos].szname, EEPROM_MAX_NAME_LEN, szvalue);
    _eeprom_whyfi.mac_name_pos++;
    Serial.print("Added Pair - ");
    print_mac_name_pair(_eeprom_whyfi.mac_name_pos-1);
  }
  else
  {
    //delete entry if description empty
    if(szvalue[0] == '\0')
    {      
      Serial.println("Deleting empty pair");
      whyfi_eeprom_del_pair(index);
    }
    else
    {
      //overwrite existing entry (update description)
      Serial.println("Rename existing pair");
      snprintf(_eeprom_whyfi.mac_names[index].szname, EEPROM_MAX_NAME_LEN, szvalue);
      _eeprom_whyfi.mac_name_pos++;
    }
  }
  
  return true;
}

#endif /* __WHYFI_EEPROM_H__ */
