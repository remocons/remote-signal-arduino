/*
 *  Remote Signal Example. 
 *  DFRobot Xboard Relay ( Leonardo + W5100 ethernet )
 * 
 *  Dongeun Lee <sixgen@gmail.com>
 *  https://github.com/congtrol/remote-signal-arduino
 *
 *  MIT License
 */


#include <SPI.h>
#include <Ethernet.h>
#include <string.h>
#include <RemoteSignal.h>

#define RELAY1 7
#define RELAY2 8

#define TCP_PORT 55488
const char *server = "tt.congtrol.com";
const char *deviceId = "deviceid";
const char *deviceKey = "devicekey";

// If you have multiple devices, you'll need to change the MAC address.
byte mac[]{0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x19};
// IPAddress ip(192, 168, 1, 3);
EthernetClient client;
RemoteSignal remote;
const char *ui = "on,off,toggle";

void setup() {
  
  Serial.begin(115200);
  Serial.println("Init..");
  
  Ethernet.init(10);
  Ethernet.begin(mac); // DHCP
  // Ethernet.begin(mac , ip); //static IP
  
  Serial.print("IP:");
  Serial.println(Ethernet.localIP());
  
  remote.setRxBuffer( 80 );
  remote.setStream( &client );

  // If you have a device ID and device key
  // remote.auth(deviceId, deviceKey); 

  remote.onReady( &onReadyHandler );
  remote.onMessage( &onMessageHandler );
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
}

void deviceOn(){
   digitalWrite( RELAY1, HIGH);
   digitalWrite( RELAY2, HIGH);
  remote.signal("@$state", "on" );
}

void deviceOff(){
   digitalWrite(RELAY1, LOW);
   digitalWrite(RELAY2, LOW);
  remote.signal("@$state", "off" );
}

void deviceToggle(){
  int nextState = !digitalRead(RELAY1);
  digitalWrite(RELAY1, nextState);
  digitalWrite(RELAY2, nextState);
   if(nextState){
    remote.signal("@$state", "on" );
   }else{
    remote.signal("@$state", "off" );
   }
}


void loop() {
      if( client.connected() ){
      remote.update();
 
      uint8_t conditionCode = remote.update();
      if(conditionCode != 0 ){ 
          // some warning or error. 
          // Serial.print("E");
          // Serial.println( conditionCode);
        if(conditionCode >= 250){
          // big issue.
          Serial.println("disconnect!");
          client.stop();
        }          
      } 
  

    }else{
      remote.clear();
      delay(3000); 
      if(client.connect( server , TCP_PORT) ){
        Serial.println("Server connected.");
      }else{
        Serial.print("s");
      }
    }

}


void onReadyHandler()
{
  Serial.print(F("onReady cid: "));
  Serial.println( remote.cid );
  remote.signal("@$state", "off" );
  remote.signal("@$ui", ui );
  remote.signal("#notify", remote.cid );
  remote.subscribe("#search");
  
}

void onMessageHandler( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{
  Serial.print(">> signal tag: " );
  Serial.print( tag );
  Serial.print(" type: " );
  Serial.print( payloadType );
  Serial.print(" size: " );
  Serial.println( payloadSize );

  if( payloadType == RemoteSignal::PAYLOAD_TYPE::TEXT ){  
    Serial.print(F("string payload: ") );
    Serial.println( (char *)payload  );
  }

  if( strcmp(tag, "#search") == 0){
      remote.signal( "#notify", remote.cid );
  }
      
  if( strcmp(tag, "@ui") == 0){
      remote.signal2( payload, "@ui", remote.cid , ui);
  }
      
  if( strcmp(tag, "@") == 0){
    if( strcmp(payload, "on") == 0){
      deviceOn();
    }else if( strcmp(payload, "off") == 0){
      deviceOff();
    }else if( strcmp(payload, "toggle") == 0){
      deviceToggle();
    }
  }
      
}


