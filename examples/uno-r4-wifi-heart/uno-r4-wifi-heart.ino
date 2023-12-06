/*
 *  Remote Signal Example. 
 *  Arduino Uno R4 WiFi
 *
 *  Lee Dongeun <sixgen@gmail.com>
 *  https://github.com/remocons/remote-signal-arduino
 *
 *  MIT License
 */


#include "WiFiS3.h"
#include <Arduino.h>
#include <RemoteSignal.h>
#include "Arduino_LED_Matrix.h"

#define SERVER_URL "io.remocon.kr"
#define SERVER_PORT 55488

ArduinoLEDMatrix matrix;

// WiFi
const char* ssid = "WIFI_SSID";
const char* pass = "WIFI_PASS";

WiFiClient client;
RemoteSignal remote;
const char *name = "UnoR4-WiFi:HOME";
const char *ui = "on,off,toggle";

const int buttonPin = 2;
int lastButtonState = HIGH; 

const uint32_t heart[] = { 0x3184a444, 0x44042081, 0x100a0040 };
const uint32_t fullOff[] = { 0, 0, 0 };

void deviceOn(){
  digitalWrite(LED_BUILTIN, HIGH);
  matrix.loadFrame(heart);
  remote.signal("@$state", "on" );
}

void deviceOff(){
  digitalWrite(LED_BUILTIN, LOW);
  matrix.loadFrame(fullOff);
  remote.signal("@$state", "off" );
}

void deviceToggle(){
  int nextState = !digitalRead(LED_BUILTIN);
   if( nextState){
      deviceOn();
   }else{
      deviceOff();
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

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  WiFi.begin(ssid, pass);
  Serial.begin(115200);
  matrix.begin();
  matrix.loadFrame(heart);
  delay(500);

  remote.setRxBuffer( 200 );

  // device authentication.
  // type1. If you have a deviceId and a deviceKey.
  // remote.auth( "deviceId", "deviceKey" );

  // type2. If you have one id_key string.
  // remote.auth( "id_key" );


  remote.setClient( &client );
  remote.onReady( &onReadyHandler );
  remote.onMessage( &onMessageHandler );

}


void loop() {
  
    if( client.connected() ){

      uint8_t conditionCode = remote.update();
      if(conditionCode != 0 ){ 
          // some warning or error. 
          Serial.print("E");
          Serial.println( conditionCode);
        if(conditionCode >= 250){
          // big issue.
          Serial.println(F("disconnect!"));
          client.stop();
        }          
      } 
      

      if(isPressed()){
        Serial.println(F("pressed"));
        deviceToggle();
      // type 1. Multicasting to a public channel
        // remote.signal("public_button", "click" );  // simple channel
        // remote.signal("public#button", "click" );  // Separate channel names and a topic with # marks.

      // type 2. Multicasting to a Private HOME_CHANNEL 
      // Omitting the channel name allows devices with the same global IP address to communicate.
        // remote.signal("#button", "click" );  // Omit the channel name and separate it from the topic with a # marker.
        remote.signal("#screen", "playToggle" );
        // remote.signal("#screen", "next" );

      // type 3. To make a uni-cast transmission, you need to know the CID of the recipient.
        // remote.signal("cid@", "click" );   // Follow the recipient's CID with the @ character.
        // remote.signal("cid@button", "click" ); // You can add a topic after the @.
      }
    
    } else if( WiFi.status() != WL_CONNECTED ){ 

      if( WiFi.begin(ssid, pass) == WL_CONNECTED ){
        Serial.println("WiFi connected.");
      }else{
        Serial.print("w");
        delay(2000); 
      }

    }else{ 
      remote.clear();
      delay(2000); 
      if(client.connect( SERVER_URL , SERVER_PORT) ){
        Serial.println("Server connected.");
      }else{
        Serial.print("s");
      }
    }

}




void onReadyHandler()
{
  Serial.print("onReady cid: ");
  Serial.println( remote.cid );
  remote.signal("@$state", "off" );
  remote.signal("@$name", name );
  remote.signal("@$ui", ui );
  remote.signal("#notify", remote.cid );
  remote.subscribe("#search");
  
}

void onMessageHandler( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{

  // signal message info
  Serial.print(">> signal tag: " );
  Serial.print( tag );
  Serial.print(" type: " );
  Serial.print( payloadType );
  Serial.print(" size: " );
  Serial.println( payloadSize );

  if( payloadType == RemoteSignal::PAYLOAD_TYPE::TEXT ){  
    Serial.print("string payload: " );
    Serial.println( (char *)payload  );
  }

  if( strcmp(tag, "#search") == 0){
    remote.signal( "#notify", remote.cid );
  }

  if( strcmp(tag, "@ui") == 0){
    remote.signal2( (char *)payload, "@ui", remote.cid , ui );
  }
      
  if( strcmp(tag, "@") == 0){
    if( strcmp((char *)payload, "on") == 0){
      deviceOn();
    }else if( strcmp((char *)payload, "off") == 0){
      deviceOff();
    }else if( strcmp((char *)payload, "toggle") == 0){
      deviceToggle();
    }
  }
      
}

