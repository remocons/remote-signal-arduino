/*
 *  Remote Signal Example. 
 *  SparkFun ESP8266 Thing - Dev Board + Button
 *  You can use general ESP8266 board too.
 *
 *  Dongeun Lee <sixgen@gmail.com>
 *  https://github.com/congtrol/remote-signal-arduino
 *
 *  MIT License
 */


#include <Arduino.h>
#include <ESP8266WiFiMulti.h>
#include <RemoteSignal.h>

#define TCP_PORT 55488
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_AP_PASSPHRASE";
const char *server = "tt.congtrol.com"; 
const char *deviceId = "deviceid";
const char *deviceKey = "devicekey";

ESP8266WiFiMulti wifiMulti;
WiFiClient client;
RemoteSignal remote;
const char *ui = "on,off,toggle";

const int buttonPin = 2;
int lastButtonState = HIGH; 

void deviceOn(){
  digitalWrite(LED_BUILTIN, LOW); // Low means ON
  remote.signal("@$state", "on" );
}

void deviceOff(){
  digitalWrite(LED_BUILTIN, HIGH);
  remote.signal("@$state", "off" );
}

void deviceToggle(){
  int nextState = !digitalRead(LED_BUILTIN);
  digitalWrite(LED_BUILTIN, nextState);
   if( nextState){
    remote.signal("@$state", "off" );
   }else{
    remote.signal("@$state", "on" );
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

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(ssid, password);
  // wifiMulti.addAP("SECOND_SSID", "AP_KEY");  
  // You can register multiple APs.

  Serial.begin(115200);
  remote.setRxBuffer( 200 );

  // If you have a device ID and device key
  // remote.auth(deviceId, deviceKey );

  remote.setStream( &client );
  remote.onReady( &onReadyHandler );
  remote.onMessage( &onMessageHandler );

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
    
    } else if( WiFi.status() != WL_CONNECTED ){ 

      if( wifiMulti.run() == WL_CONNECTED ){
        Serial.println("WiFi connected.");
      }else{
        Serial.print("w");
        delay(2000); 
      }

    }else{ 
      remote.clear();
      delay(2000); 
      if(client.connect( server , TCP_PORT) ){
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