/*
 *  ESP8266 D1 mini
 *  Lee Dongeun <sixgen@gmail.com>
 *  https://github.com/remocons/remocon-signal-arduino
 *
 *  MIT License
 */



#include <ESP8266WiFiMulti.h>
#include <RemoteSignal.h>
#include <Bounce2.h>

#define TCP_PORT 55488
#define BUTTON_PIN 13
// #define BUILTIN_LED 2

const char *server = "io.remocon.kr"; 

ESP8266WiFiMulti wifiMulti;
WiFiClient client;
RemoteSignal remote;

Bounce2::Button aBtn = Bounce2::Button();

const char *name = "D1 mini:esp8266";
const char *ui = "on,off,toggle";


void deviceOn(){
  digitalWrite(BUILTIN_LED, LOW);
  remote.signal("@$state", "on" );
}

void deviceOff(){
  digitalWrite(BUILTIN_LED, HIGH);
  remote.signal("@$state", "off" );
}

void deviceToggle(){
  int nextState = !digitalRead(BUILTIN_LED);
  digitalWrite(BUILTIN_LED, nextState);
   if( nextState){
    remote.signal("@$state", "off" );
   }else{
    remote.signal("@$state", "on" );
   }
}




void setup() {
  
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP( "WIFI_SSID", "WIFI_PASS");
  // wifiMulti.addAP( "twesomego", "qwer1234");  
  // You can add multiple APs.
  
  pinMode( BUILTIN_LED , OUTPUT);
  digitalWrite( BUILTIN_LED , HIGH);  //
  
  aBtn.attach( BUTTON_PIN, INPUT_PULLUP ); 
  aBtn.interval(5); // debounce interval in milliseconds
  aBtn.setPressedState(LOW); 
  
  remote.setClient( &client );
  remote.setRxBuffer( 200 );
  remote.onReady( &onReady );
  remote.onMessage( &onMessage );
  Serial.begin(115200);

}


void loop() {

    if( client.connected() ){
      uint8_t conditionCode = remote.update();
      if(conditionCode != 0 ){ 
        if(conditionCode >= 250){
          client.stop();
        }          
      } else{
        aBtn.update();
        if ( aBtn.pressed() ) {
          deviceToggle();
          delay(100);
        }  

      }


    } else if( WiFi.status() != WL_CONNECTED ){ 
      Serial.println("WiFi disconnected.");

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




void onReady()
{
  Serial.print("onReady cid: ");
  Serial.println( remote.cid );
  remote.signal("@$state", "off" );
  remote.signal("@$ui", ui );
  remote.signal("@$name", name );
  remote.signal("#notify", remote.cid );
  remote.subscribe("#search");
  
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
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
