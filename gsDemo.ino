//GroveStreams/IoT demo for Flint Area Coding Meetup
//Jack Christensen 20Jul2014
//This work by Jack Christensen is licensed under CC BY-SA 4.0,
//http://creativecommons.org/licenses/by-sa/4.0/

#include <utility/w5100.h>
#include <Dns.h>
#include <Ethernet.h>               //http://arduino.cc/en/Reference/Ethernet
#include <extEEPROM.h>
#include <MCP980X.h>                //http://github.com/JChristensen/MCP980X
#include <SPI.h>                    //http://arduino.cc/en/Reference/SPI
#include <Streaming.h>              //http://arduiniana.org/libraries/streaming/
#include <Wire.h>                   //http://arduino.cc/en/Reference/Wire
#include "GroveStreams.h"           //part of this project

//pin assignments
const uint8_t WAIT_LED = 8;        //waiting for server response
const uint8_t HB_LED = 9;          //heartbeat LED
const uint8_t POT_PIN = A2;
const uint8_t LDR_PIN = A1;

//object instantiations
const char* gsServer = "grovestreams.com";
PROGMEM const char gsApiKey[] = "cbc8d222-6f25-3e26-9f6e-edfc3364d7fd";
char gsCompID[9];                          //read from external EEPROM
GroveStreams GS(gsServer, (const __FlashStringHelper *) gsApiKey, WAIT_LED);
EthernetClient client;
MCP980X mcp9802(0);
extEEPROM eep(kbits_2, 1, 8);

void setup(void)
{
    //pin inits
    pinMode(HB_LED, OUTPUT);
    pinMode(WAIT_LED, OUTPUT);
    pinMode(LDR_PIN, INPUT_PULLUP);
    Serial.begin(115200);
    mcp9802.begin();
    mcp9802.writeConfig(ADC_RES_12BITS);
    eep.begin();
    eep.read(0, (uint8_t*)gsCompID, 9);    //get the component ID from EEPROM
    delay(1000);                           //give W5100 some bootup time

    //get MAC address & display
    uint8_t mac[6];
    eep.read(0xFA, mac, 6);
    Serial << F("MAC addr=");
    for (int i=0; i<6; ++i) {
        if (mac[i] < 16) Serial << '0';
        Serial << _HEX( mac[i] );
    }
    Serial << endl;

    //start Ethernet, display IP
    Ethernet.begin(mac);                   //DHCP
    Serial << millis() << F(" Ethernet started, IP=") << Ethernet.localIP() << endl;

    //connect to GroveStreams, display IP
    GS.begin();
}

enum STATE_t { WAIT, XMIT, RECV } STATE;

void loop(void)
{
    const unsigned long msXmitInterval = 10000;
    static unsigned long msLastXmit;
    const unsigned long msRenewInterval = 3600 * 1000;
    static unsigned long msLastRenew;
    static unsigned long msLastSecond;
    static unsigned long msLastLED;
    static char buf[80];
    static int tF10, ldr, pot;
    static bool ledState;
    unsigned long ms = millis();

    ethernetStatus_t gsStatus = GS.run();   //run the GroveStreams state machine

    switch (STATE)
    {
    case WAIT:
        if (ms - msLastSecond >= 1000) {
            msLastSecond += 1000;
            tF10 = mcp9802.readTempF10(AMBIENT);
            ldr = map( analogRead(LDR_PIN), 0, 1024, 100, 0 );
            pot = map( analogRead(POT_PIN), 0, 1024, 0, 100 );
        }
        
        if (ms - msLastXmit >= msXmitInterval) {
            msLastXmit += msXmitInterval;
            STATE = XMIT;
        }
        
        if ( ms - msLastRenew >= msRenewInterval) {
            msLastRenew += msRenewInterval;
            uint8_t mStat = Ethernet.maintain();
            Serial << ms << ' ' << F("Ethernet.maintain=") << mStat << endl;
        }
        break;
        
    case XMIT:
        sprintf(buf,"&s=%u&f=%i.%i&l=%u&p=%u", ++GS.seq, tF10/10, tF10%10, ldr, pot);
//        Serial << buf << endl;
        Serial << ms << F(" XMIT");
        if ( GS.send(gsCompID, buf) == SEND_ACCEPTED ) {
            Serial << F(" OK");
            STATE = RECV;
        }
        else {
            Serial << F(" FAIL");
            STATE = WAIT;
        }
        Serial << F(" seq=") << GS.seq << F(" tF10=") << tF10 << F(" ldr=") << ldr << F(" pot=") << pot << endl;
        break;
        
    case RECV:
        if (gsStatus == HTTP_OK) {
            Serial << ms << F(" HTTP OK") << endl;
            STATE = WAIT;
        }
        else if ( ms - msLastXmit >= 7000 ) {
            Serial << ms << F(" GroveStreams timeout") << endl;
            STATE = WAIT;
        }
        break;
    }
    if (ms - msLastLED > 1000) {        //run the heartbeat LED
        msLastLED += 1000;
        digitalWrite(HB_LED, ledState = !ledState);
    }
}

