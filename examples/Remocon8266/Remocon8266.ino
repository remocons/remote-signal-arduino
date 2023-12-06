/*
 *  RemoteSignal Example. 
 *  Remocon8266 board

 *  Lee Dongeun <sixgen@gmail.com>
 *  https://github.com/remocons/remote-signal-arduino
 *
 *  MIT License
 */


#include <ESP8266WiFiMulti.h>
#include <RemoteSignal.h>
#include <Bounce2.h>
#include <U8g2lib.h>
#include <Wire.h>

// Buttons
#define BTN_UP     16   // D0  SW1 *EXTERNAL HARDWARE PULLUP
#define BTN_LEFT    0   // D3  SW2
#define BTN_DOWN   14   // D5  SW3
#define BTN_RIGHT  13   // D7  SW4

//I2C  OLED & else
#define PIN_SDA     4   // D2
#define PIN_SDL     5   // D1

//OUTPUT
#define OUT1       15   // D8  LED, NeoPixel, Servo or else.
#define OUT2       12   // D6 

#define TCP_PORT 55488
const char *server = "io.remocon.kr"; 

ESP8266WiFiMulti wifiMulti;
WiFiClient client;
RemoteSignal remote;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// Define button objects
Bounce2::Button upBtn = Bounce2::Button();
Bounce2::Button leftBtn = Bounce2::Button();
Bounce2::Button downBtn = Bounce2::Button();
Bounce2::Button rightBtn = Bounce2::Button();


const char *name = "Remocon8266:HOME";
const char *ui = "L,A,B";
char states[] = "100";
char last_states[] = "100";
uint8_t pinMap[] = { LED_BUILTIN , OUT1, OUT2 };


void stateChange(int i){
  //local state
  char state = states[i];
  if( state != last_states[i] ){
    digitalWrite( pinMap[i] , states[i] - '0' );
    last_states[i] = state;
  }
  Serial.print("states: ");
  Serial.println( states);

  //publish device states
  remote.signal("@$state", states );

  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Remote Info");
  u8g2.setCursor(0, 25);
  u8g2.print("CID: ");
  u8g2.print( remote.cid);
  u8g2.setCursor(0, 40);
  u8g2.print("States: ");
  u8g2.print( states);
  u8g2.sendBuffer();
}


void toggle(int i){
  char state = states[i];
   if( state == '1'){
      states[i] = '0';
   }else{
      states[i] = '1';
   }
   stateChange(i);
}


void check_events(void) {
  upBtn.update();
  leftBtn.update();
  downBtn.update();
  rightBtn.update();
}

void handle_events(void) {
  // 0 = not pushed, 1 = pushed
  if (upBtn.pressed()) {
    Serial.println("up");
    remote.signal("#screen","playToggle");
  } else if (leftBtn.pressed()) {
    Serial.println("left");
    remote.signal("#screen","left");
  } else if (downBtn.pressed()) {
    Serial.println("down");
    remote.signal("#screen","down");
  } else if (rightBtn.pressed()) {
    Serial.println("right");
    remote.signal("#screen","right");
    // remote.signal("cid@","D");
  }
}

void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

// logo
uint16_t p[][2] = {
  { 42, 32 },
  { 61, 32 },
  { 67, 7 },
  { 76, 17 },
  { 66, 56 },
  { 76, 46 }
};

void draw_logo() {
  // 128x64 frame
  u8g2.drawTriangle(p[0][0], p[0][1], p[1][0], p[1][1], p[2][0], p[2][1]);
  u8g2.drawTriangle(p[1][0], p[1][1], p[2][0], p[2][1], p[3][0], p[3][1]);
  u8g2.drawTriangle(p[0][0], p[0][1], p[1][0], p[1][1], p[4][0], p[4][1]);
  u8g2.drawTriangle(p[1][0], p[1][1], p[4][0], p[4][1], p[5][0], p[5][1]);
  u8g2.drawDisc(84, 30, 5, U8G2_DRAW_ALL);
}

void setup() {
  Serial.begin(115200);
  Serial.flush();

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP( "WIFI_SSID", "WIFI_PASS");
  // wifiMulti.addAP( "twesomego", "qwer1234");  
  // You can add multiple APs.

  upBtn.attach(BTN_UP, INPUT); // external pullup
  upBtn.interval(5);           
  upBtn.setPressedState(LOW);  

  leftBtn.attach(BTN_LEFT, INPUT_PULLUP); 
  leftBtn.interval(5);              
  leftBtn.setPressedState(LOW);     

  downBtn.attach(BTN_DOWN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  

  rightBtn.attach(BTN_RIGHT, INPUT_PULLUP);
  rightBtn.interval(5);           
  rightBtn.setPressedState(LOW);  


  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // active low 

  pinMode(OUT1, OUTPUT);
  digitalWrite(OUT1, LOW); 

  pinMode(OUT2, OUTPUT);
  digitalWrite(OUT2, LOW); 


  u8g2.begin();
  u8g2_prepare();

  u8g2.clearBuffer(); 
    draw_logo();
    u8g2.sendBuffer();
    delay(800);
 
  u8g2.clearBuffer(); 
    u8g2.setFont(u8g2_font_profont22_mf);
    u8g2.drawStr(0, 20, "REMOCON.KR");
    u8g2.sendBuffer();
    delay(2000);

  // u8g2.setFont(u8g2_font_ncenB08_tr);  // choose a suitable font
  u8g2.setFont(u8g2_font_t0_13_mr);  // choose a suitable font
  u8g2.clearBuffer(); 
    u8g2.drawStr(0, 20, "Connecting..");
    u8g2.sendBuffer();

  remote.setRxBuffer( 200 );

  // RemoteSignal Authenticaton. 
  // remote.auth( "ID_KEY" ); 
  
  
  remote.setClient( &client );
  remote.onReady( &onReadyHandler );
  remote.onMessage( &onMessageHandler );

}

  
void loop() {

    if( client.connected() ){

      uint8_t state = remote.update();
      if(state != 0 ){ 
          // some warning or error. 
           Serial.print("E");
           Serial.println( state);
        if(state >= 250){
          // big issue.
          Serial.println(F("disconnect!"));
          client.stop();
        }          
      } 
      
      check_events();
      handle_events();


    } else if( WiFi.status() != WL_CONNECTED ){ 
      // Serial.println("WiFi disconnected.");
      if( wifiMulti.run() == WL_CONNECTED ){
        Serial.println("WiFi connected.");
      }else{
        Serial.print("w");
        delay(2000); 
      }

    }else{ 
      delay(2000); 
      remote.clear();
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
  remote.signal("@$state", states );
  remote.signal("@$ui", ui );
  remote.signal("@$name", name );
  remote.signal("#notify", remote.cid );
  remote.subscribe("#search");
  
  u8g2.clearBuffer();
    u8g2.setCursor(0, 0);
    u8g2.print("Ready CID: ");
    u8g2.print(remote.cid);
    u8g2.drawStr(0, 15, "web app url:");
    u8g2.drawStr(0, 30, "https://remocon.kr");
    u8g2.drawStr(0, 45, "remote control");
    u8g2.sendBuffer();
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

  if( strcmp(tag, "@msg") == 0){

    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "msg");
    u8g2.setCursor(0, 25);
    u8g2.print( (char *)payload );
    u8g2.sendBuffer();

  }
      
  if( strcmp(tag, "@") == 0){
    if( strcmp((char *)payload, "L") == 0){
      toggle(0);
    }else if( strcmp((char *)payload, "A") == 0){
      toggle(1);
    }else if( strcmp((char *)payload, "B") == 0){
      toggle(2);
    }
  }
      
}
