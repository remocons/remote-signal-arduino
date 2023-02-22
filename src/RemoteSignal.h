
#ifndef RemoteSignal_h
#define RemoteSignal_h

#include <Arduino.h>
#include <CongPacket.h>
#include <Boho.h>

#define DEFAULT_TX_BUF_SIZE 50
#define MAX_CID_LEN 20

// USE_PSRAM: If you need large size memory on ESP boards which has PSRAM.
// #define USE_PSRAM   

    enum class STATES{
      OPENING,   // 0
      OPEN,         // 1
      CLOSING,      // 2
      CLOSED,       // 3
      SERVER_READY, // 4
      AUTH_FAIL,    // 5
      AUTH_READY,   // 6
      READY         // 7
    };

class RemoteSignal final: public Boho
{
  public:
    RemoteSignal(void);
    ~RemoteSignal();
    uint8_t state;



     enum MsgType: uint8_t {
      /* ###############
      * Not recommended 0~127dec.
      * 0~31: control code
      * 32~126: ascii charactor
      * 127: DEL
      */
     
      ADMIN_REQ = 0xA0,
      ADMIN_RES = 0xA1,

  // B. Boho is using Bx.
      // AUTH_REQ = 0xB0,
      // AUTH_NONCE,
      // AUTH_HMAC,
      // AUTH_ACK,
      // AUTH_FAIL,
      // AUTH_EXT,
      // ENC_PACK, 
      // ENC_E2E,  
      // ENC_488,   

  // C. RemoteSignal status contorl.
      SERVER_READY = 0xC0,
      CID_REQ = 0xC1, 
      CID_RES = 0xC2,  
      QUOTA_LEVEL = 0xC3,
      SERVER_CLEAR_AUTH = 0xC4, 
        // ..
      LOOP = 0xCB,
      ECHO = 0xCC,
      PING = 0xCD,  
      PONG = 0xCE,
      CLOSE = 0xCF,
        // ~CF

        // D. RemoteSignal data signaling
      SIGNAL = 0xD0,  
      SIGNAL_REQ = 0xD1, 
      SIGNAL_E2E = 0xD2, 
      SUBSCRIBE = 0xD3,
      SUBSCRIBE_REQ = 0xD4, 
      UNSUBSCRIBE = 0xD5, 
      SERVER_SIGNAL = 0xD6,
        // ..
      IAM = 0xD9,
      IAM_RES = 0xDA,
        //.. 
      SET = 0xDB,   // setting server database.
      RESPONSE_CODE = 0xDC,   
      RESPONSE_MBP = 0xDD,   
      REQUEST = 0xDE,
      RESPONSE = 0xDF,
        // ~DF


        // F. Framing Flow control related framing protocol.(CongPacket)
      FLOW_MODE = 0xF0,
      WAIT = 0xF1,
      RESUME = 0xF2,
        //..
      TIME_OUT = 0xFD,
      OVER_SIZE = 0xFE,
      OVER_FLOW = 0xFF

      };

 enum PAYLOAD_TYPE: uint8_t {
      EMPTY = 0,
      TEXT = 1,
      BINARY = 2,
      OBJECT = 3, // one stringify able object. no buffer.
      MJSON = 4, // multiple stringify able obejct.  JSON string. with top levle array , no buffer
      MBA = 5 // can contains buffer.
 };

 enum ENC_MODE: uint8_t {
      NO = 0,
      YES = 1, 
      AUTO = 2 
 };

  void update();
  bool overflow();
  void setStream( Stream* client );
 
  void write(const uint8_t* buffer, uint32_t size);

  void send(const uint8_t* buffer, uint32_t size);
  void send_enc_mode(const uint8_t* buffer, uint32_t size);

  void ping();
  void pong();

  void close( uint8_t reason );

  void login( char* auth_id, char* auth_key );
  void auth( char* auth_id, char* auth_key );
  uint8_t useAuth;

  void set( char* setString );
  void subscribe( char* tag );

  void signal( char* tag );
  void signal( char* tag, const char* data  );
  void signal( char* tag, const char* data1 , const char* data2  );
  void signal( char* tag, const uint8_t* data , uint32_t dataLen );
  void signal_e2e( char* tag, const uint8_t* data , uint32_t dataLen , const char *dataKey);

  void signal2( char* target ,char* topic, const char* data  );
  void signal2( char* target ,char* topic, const char* data1, const char* data2 );
  void signal2( char* target ,char* topic, const uint8_t* data , uint32_t dataLen );
  void signal2_e2e( char* target ,char* topic, const uint8_t* data , uint32_t dataLen , const char *dataKey);

  void onMessage(void (*messageCallback)( char*, uint8_t, uint8_t*, size_t));
  void onReady(void (*readyCallback)( void ));
  void clear(void);
  void setRxBuffer( size_t size);

  uint8_t _buffer[DEFAULT_TX_BUF_SIZE];
  Stream *stream;

  union u32buf4 packetLength;

  char cid[MAX_CID_LEN + 1 ];
  CongPacket cong;
  uint32_t lastTxRxTime = 0; //second : internal unixTime
  uint32_t pingPeriod = 30; //second

  uint8_t encMode = RemoteSignal::ENC_MODE::AUTO;

  private:
  void (*messageCallback)( char*, uint8_t, uint8_t*, size_t);
  void (*readyCallback)( void);

  
};

#endif