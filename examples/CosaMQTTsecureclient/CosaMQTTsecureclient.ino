/**
 * @file CosaMQTTsecureclient.ino
 * @version 1.0
 *
 * @section License
 * Copyright (C) 2015,      Alexander Sehlström (CosaMQTTsecureclient example)
 * Copyright (C) 2014-2015, Mikael Patel (Cosa core and related libraries)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * @section Description
 * W5100 Ethernet Controller device drive example code; MQTT client
 * switching imaginary NEXA switches on and off.
 * The MQTT client connects to the MQTT broker using username/password.
 * It also specifies a last will.
 * 
 * @section Circuit
 * This sketch is designed for the Ethernet Shield.
 * @code
 *                       W5100/ethernet
 *                       +------------+
 * (D10)--------------29-|CSN         |
 * (D11)--------------28-|MOSI        |
 * (D12)--------------27-|MISO        |
 * (D13)--------------30-|SCK         |
 * (D2)-----[ ]-------56-|IRQ         |
 *                       +------------+
 *
 * @endcode
 * 
 * @section Reference
 * 1. Cosa
 *    Copyright (c) 2012-2015, Mikael Patel
 *    https://github.com/mikaelpatel/Cosa
 *    
 * 2. Cosa-MQTT
 *    Copyright (c) 2014-2015, Mikael Patel
 *    https://github.com/mikaelpatel/Cosa-MQTT
 *    
 * This file is part of the Arduino Che Cosa project
 */

#include <DHCP.h>
#include <DNS.h>
#include <MQTT.h>
#include <W5100.h>

#include "Cosa/Watchdog.hh"
#include "Cosa/Trace.hh"
#include "Cosa/IOStream/Driver/UART.hh"
#include "Cosa/String.hh"

#define DEVMODE

// W5100 Ethernet Controller with MAC-address
static const uint8_t mac[6] __PROGMEM = { 0xde, 0xad, 0xbe, 0xef, 0xfe, 0xed };
W5100 ethernet(mac);

// MQTT broker configuration
#define LOCALBROKER "10.0.1.191"
#define BROKER LOCALBROKER

// MQTT client configurations
static const char CLIENT[] __PROGMEM = "Cosa-MQTT secure client";       // Client name
static const char WILL_TOPIC[] __PROGMEM = "node/gone-offline";         // Last will topic
static const char WILL_MSG[] __PROGMEM = "Cosa-MQTT secure client";     // Last will message
static const uint16_t WILL_QOS = 0;                                     // Last will QoS
static const char USER[] __PROGMEM = "Cosa-MQTT";                       // Username for connection to broker
static const char PASSWD[] __PROGMEM = "mypwd";                         // Password for connection to broker
static const uint8_t flag = MQTT::Client::WILL_FLAG |                   // Connection flags; combine flag for will, username and password
                            MQTT::Client::USER_NAME_FLAG |
                            MQTT::Client::PASSWORD_FLAG;

/*
 * MQTT client; Parses incoming topic/message
 */
class MQTTClient : public MQTT::Client {
public:
  virtual void on_publish(char* topic, void* buf, size_t count);
};

/*
 * Parse incomming topic/message.
 * The following topics will be parsed with "ON", "1", "OFF"
 * and "0" beeing valid messages.
 * nexa/switch/[device]
 * nexa/group/[group]
 */
void
MQTTClient::on_publish(char* topic, void* buf, size_t count)
{
  String toparse((char*) topic);
  String partition;
  String payload((char*) buf);
  
  #if defined(DEVMODE)
  // Print the topic and value to trace stream
  trace << PSTR("on_publish::count = ") << count << endl;
  trace << PSTR("  topic = ") << topic << endl;
  trace << PSTR("  payload = ") << payload << endl;
  #endif
  
  // Parse NEXA Switches
  String key(toparse.substring(0, 12));
  if (key.equals("nexa/switch/"))
  {
    String val(toparse.substring(12, toparse.length()));
    uint8_t len = val.length();
    uint16_t device = 0;
    for(int i=0; i<len; i++){
      device = device * 10 + ( val[i] - '0' );
    }
    
    if (payload.equals("ON") || payload.equals("1"))
    {
      trace << PSTR("  NEXA Switch ") << device << PSTR(" turned ON") << endl;
    }
    else if(payload.equals("OFF") || payload.equals("0"))
    {
      trace << PSTR("  NEXA Switch ") << device << PSTR(" turned OFF") << endl;
    }
    
    return;
  } // end parsing NEXA Switches
  
  // Parse NEXA Groups
  key = toparse.substring(0, 11);
  if (key.equals("nexa/group/"))
  {
    String val(toparse.substring(11, toparse.length()));
    uint8_t len = val.length();
    uint16_t group = 0;
    for(int i=0; i<len; i++){
      group = group * 10 + ( val[i] - '0' );
    }
    
    if (payload.equals("ON") || payload.equals("1"))
    {
      trace << PSTR("  NEXA Group ") << group << PSTR(" turned ON") << endl;
    }
    else if(payload.equals("OFF") || payload.equals("0"))
    {
      trace << PSTR("  NEXA Group ") << group << PSTR(" turned OFF") << endl;
    }
  } // end parsing NEXA Groups
}

// MQTT client
MQTTClient client;

void setup() {
  // The program start when the serial debug terminal is connected
  uart.begin(9600);
  trace.begin(&uart, PSTR("Cosa MQTT secure client: started"));
  Watchdog::begin();
  
  // Start the Ethernet Controller and request network address for hostname
  ASSERT(ethernet.begin_P(CLIENT));

  // Start the MQTT client
  mqttStart();
}

void loop()
{
  // Service incoming publish messages until error
  trace << PSTR("Service incomming messages") << endl;

  int service = 0;
  do
  {
    TRACE(service = client.service());
  }
  while (service > -1 );
  
  // Restart the client; after client is stoped wait a few ms before starting again
  trace << PSTR("Cosa-MQTT secure client: restarting") << endl;
  mqttStop();
  delay(10);
  mqttStart();
}

void mqttStart()
{
  // Start MQTT client with socket and connect to server using will, username and password
  ASSERT(client.begin(ethernet.socket(Socket::TCP)));
  ASSERT(!client.connect(BROKER, CLIENT, 600, flag, WILL_TOPIC, WILL_MSG, WILL_QOS, USER, PASSWD));
  
  // Publish data about the client
  client.publish_P(PSTR("cosamqttsecureclient/client"), CLIENT, sizeof(CLIENT));
  
  // Subscribe to topics
  mqttSub();

  // Client has started
  trace << PSTR("Cosa-MQTT secure client: started") << endl;
}

void mqttStop()
{
  // Unsubscribe to topics
  mqttUnsub();
  
  // Disconnect from server and end MQTT client with socket
  ASSERT(!client.disconnect());
  ASSERT(client.end());

  // Client has stoped
  trace << PSTR("Cosa-MQTT secure client: stoped") << endl;
}

void mqttSub()
{
  // Subscribe the following topics
  TRACE(client.subscribe(PSTR("nexa/switch/+")));
  TRACE(client.subscribe(PSTR("nexa/group/0")));
}

void mqttUnsub()
{
  // Unsubscribe the following topics
  TRACE(client.unsubscribe(PSTR("nexa/switch/+")));
  TRACE(client.unsubscribe(PSTR("nexa/group/0")));
}
