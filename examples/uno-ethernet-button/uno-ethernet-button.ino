/*
 *  Remote Signal Example. 
 *  Arduino Uno + Ethernet shield W5100 + Button
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

#define TCP_PORT 55488
const char *server = "tt.congtrol.com";  
const char *deviceId = "deviceid";
const char *deviceKey = "devicekey";

// If you have multiple devices, you'll need to change the MAC address.
byte mac[]{0, 0, 0, 0, 0, 0x07}; 
// IPAddress ip(192, 168, 1, 3);
EthernetClient client;
RemoteSignal remote;
const char *ui = "on,off,toggle";

const int buttonPin = 2;
int lastButtonState = HIGH; 

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println(F("Init.."));
  Ethernet.init(10);

  Ethernet.begin(mac); // DHCP
  // Ethernet.begin(mac , ip); //static IP

  Serial.print(F("IP:"));
  Serial.println(Ethernet.localIP());
  
  remote.setRxBuffer( 80 );
  remote.setStream( &client );

  // If you have a device ID and device key
  // remote.auth(deviceId, deviceKey); 

  remote.onReady( &onReadyHandler );
  remote.onMessage( &onMessageHandler );
  pinMode(5, OUTPUT); // LED
  pinMode(6, OUTPUT); // Relay
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);
}

void deviceOn(){
   digitalWrite( 5, HIGH);
   digitalWrite( 6, HIGH);
  remote.signal("@$state", "on" );
}

void deviceOff(){
   digitalWrite(5, LOW);
   digitalWrite(6, LOW);
  remote.signal("@$state", "off" );
}

void deviceToggle(){
  int nextState = !digitalRead(5);
  digitalWrite(5, nextState);
  digitalWrite(6, nextState);
   if(nextState){
    remote.signal("@$state", "on" );
   }else{
    remote.signal("@$state", "off" );
   }
}

int isPressed(){
  int currentState = digitalRead(buttonPin);
  if(lastButtonState == HIGH && currentState == LOW){
    lastButtonState = LOW;
    return 1;
  } 
  else{
    lastButtonState = currentState;
    return 0;
  } 
}

void loop() {
    if( client.connected() ){

      uint8_t conditionCode = remote.update();
      if(conditionCode != 0 ){ 
          // some warning or error. 
          // Serial.print("E");
          // Serial.println( conditionCode);
        if(conditionCode >= 250){
          // big issue.
          Serial.println(F("disconnect!"));
          client.stop();
        }          
      } 
      

      if(isPressed()){
        Serial.println(F("pressed"));

      // type 1. Multicasting to a public channel
        // remote.signal("public_button", "click" );  // simple channel
        // remote.signal("public#button", "click" );  // Separate channel names and a topic with # marks.

      // type 2. Multicasting to a Private HOME_CHANNEL 
      // Omitting the channel name allows devices with the same global IP address to communicate.
        // remote.signal("#button", "click" );  // Omit the channel name and separate it from the topic with a # marker.
        remote.signal("#screen", "next" );

      // type 3. To make a uni-cast transmission, you need to know the CID of the recipient.
        // remote.signal("cid@", "click" );   // Follow the recipient's CID with the @ character.
        // remote.signal("cid@button", "click" ); // You can add a topic after the @.
      }


    }else{
      remote.clear();
      delay(3000); 
      if(client.connect( server , TCP_PORT) ){
        Serial.println(F("Server connected."));
      }else{
        Serial.print(F("s"));
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
  Serial.print(F(">> signal tag: ") );
  Serial.print( tag );
  Serial.print(F(" type: ") );
  Serial.print( payloadType );
  Serial.print(F(" size: " ));
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


