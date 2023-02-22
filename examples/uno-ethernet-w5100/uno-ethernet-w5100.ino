/*
 *  Remote Signal Example. 
 *  Arduino Uno + Ethernet shield W5100
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

#define TCP_PORT 8888
const char *host = "192.168.0.101";  // url or ip of remote_signal_server  
byte mac[]{0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x03};
// The mac must be changed if you use multiple devices.

// IPAddress ip(192, 168, 1, 3);
EthernetClient client;
RemoteSignal remote;
const char *ui = "on,off";

uint32_t lastTime = 0;

void setup() {
  
  Serial.begin(115200);
  while (!Serial)
    ;

  Serial.println(F("Init.."));
  Ethernet.init(10);

  Ethernet.begin(mac );// , ip);
  Serial.print(F("IP:"));
  Serial.println(Ethernet.localIP());
  
  remote.setRxBuffer( 80 );
  remote.setStream( &client );

  // if you have deviceId and key.
  // remote.auth("deviceid","devicekey");  
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
   Serial.println(nextState);
}


void loop() {
      if( client.connected() ){
      remote.update();
 
      if(remote.overflow() ){
        Serial.println(F("reading error. disconnect!"));
        remote.close( 0xff );
        client.stop();
      }

   
      if( ( remote.getUnixTime() - lastTime ) > 3 ){
        lastTime = remote.getUnixTime();
        // getUnixtTime() returns: <seconds>
        // if authorized:  Unixtime from server.
        // else: running time after boot.
        boho_print_time( lastTime);
      }

    }else{
      remote.clear();
      delay(3000); 
      if(client.connect( host , TCP_PORT) ){
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
    }
  }
      
}


