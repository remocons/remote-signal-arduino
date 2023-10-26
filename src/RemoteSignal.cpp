
#include "RemoteSignal.h"
#include <Arduino.h>
#include <string.h>
#include "Client.h"

RemoteSignal::RemoteSignal()
{
  state = static_cast<int>(STATES::OPENING);
  setTime(0);
  packetLength.u32 = 0;
  cong = CongPacket();
}


// return code: 
// 0: okay  
// 1: mem_alloc error 
// 2: decryption error
// 250~ : packet error
uint8_t RemoteSignal::update()
{
  refreshTime();

  if ((getUnixTime() - lastTxRxTime) > pingPeriod)
  {
    ping();
    lastTxRxTime = getUnixTime();
  }

  cong.run();

  if(cong._state >= 250){
    close( cong._state);
    return cong._state;
  } 
  if (!cong.ready()) return 0; 

  lastTxRxTime = getUnixTime();

  uint8_t *tmpbuf = NULL;
  uint32_t len = cong._payloadLength;
  uint8_t *message = (uint8_t *)cong._buffer;

  if (message[0] == Boho::MsgType::ENC_488)
  {
// Serial.println(F(">> E"));
#ifdef USE_PSRAM
    tmpbuf = (uint8_t *)ps_malloc(len);
#else
    tmpbuf = (uint8_t *)malloc(len);
#endif

    if (tmpbuf == NULL)
    {
      cong.clear();
      return 1;
    }

    uint32_t decLen = decrypt_488(tmpbuf, (uint8_t *)cong._buffer, len);
    if (decLen)
    {
      message = tmpbuf;
      len = decLen;
    }
    else
    {
      if (tmpbuf != NULL) free(tmpbuf);
      cong.clear(); 
      return 2;
    }
  }
  else if (message[0] == Boho::MsgType::ENC_E2E)
  {
    /*
    !! End to End Encryption Message decryption.
    ** payload is enc_pack.
    ** application should decrypt payload with e2e key. use decrypt_e2e()
    */

    // Serial.println(F(">> E2"));
    memcpy(packetLength.buf, cong._buffer + 1, 4);
    int encHeaderSize = packetLength.u32;
#ifdef USE_PSRAM
    tmpbuf = (uint8_t *)ps_malloc(encHeaderSize);
#else
    tmpbuf = (uint8_t *)malloc(encHeaderSize);
#endif

    if (tmpbuf == NULL)
    {
      cong.clear();
      return 1;
    }

    uint32_t decLen = decrypt_488(tmpbuf, (uint8_t *)cong._buffer, len);
    if (decLen)
    {
      memcpy(cong._buffer + 21, tmpbuf, encHeaderSize);
      message = cong._buffer + 21;
      len = len - 21;
    }
    else
    {
      if (tmpbuf != NULL) free(tmpbuf);
      cong.clear();
      return 2;  
    }
  }

  uint8_t hd = message[0];

  switch (hd)
  {
  case RemoteSignal::MsgType::PING:
  {
    pong();
    break;
  }

  case RemoteSignal::MsgType::PONG:
  {
    break;
  }

  case RemoteSignal::MsgType::SIGNAL_E2E:
  case RemoteSignal::MsgType::SIGNAL:
  {
    int tagLen = message[1];
    int payloadLen = len - (tagLen + 3);
    uint8_t payloadType = message[2 + tagLen];

    char *tagStr = (char *)message + 2;
    message[2 + tagLen] = 0; // add null string. end of tagStr string.
    // If tagStr includes '@' charactor. it's uni-cast message.
    char *directMessageTopic = strchr(tagStr, '@');
    if (directMessageTopic)
    {
      if (messageCallback)
        messageCallback(directMessageTopic, payloadType, message + 3 + tagLen, payloadLen);
    }
    else
    {
      if (messageCallback)
        messageCallback(tagStr, payloadType, message + 3 + tagLen, payloadLen);
    }
    break;
  }

  case RemoteSignal::MsgType::CID_RES:
  {
    if (len >= 1 && len < MAX_CID_LEN)
    {
      memcpy(cid, message + 1, len - 1);
      cid[len] = 0;
      state = static_cast<int>(STATES::READY);
    }
    else
    {
      // Serial.println(F("-- ERR: CID_RES SIZE LIMIT"));
      break;
    }

    if (readyCallback)
      readyCallback();
    break;
  }

  case RemoteSignal::MsgType::SERVER_READY:
  {
    state = static_cast<int>(STATES::SERVER_READY);
    if (useAuth)
    {
      // Serial.println(F(">> SVR_READY!"));
      size_t size = auth_req(_buffer);
      send(_buffer, size);
    }
    else
    {
      _buffer[0] = RemoteSignal::MsgType::CID_REQ;
      send(_buffer, 1);
    }
    break;
  }

  case RemoteSignal::MsgType::SERVER_CLEAR_AUTH:
  {
    useAuth = false;
    clearAuth(); // Boho::clearAuth()
    if (tmpbuf != NULL) free(tmpbuf);
    cong.clear();
    close(RemoteSignal::MsgType::SERVER_REQ_CLOSE);
    return RemoteSignal::MsgType::SERVER_REQ_CLOSE;
  }

  case RemoteSignal::MsgType::SERVER_REDIRECT:
  {

    if( len == 7){  // 1 MsgType, ip4, port2 
      cong.clear();
      close(RemoteSignal::MsgType::SERVER_REDIRECT);
  
      char ipString[16];
      sprintf(ipString,  "%d.%d.%d.%d\0", message[1], message[2], message[3], message[4]);
      uint16_t port = (message[5] << 8 ) + message[6];
      
      // Serial.println("redirection");
      // boho_print_hex("ip hex:", message + 1, 4 );
      // Serial.print( "host port:");
      // Serial.print(ipString);
      // Serial.println( port);

      this->client->connect( ipString , port );
      if (tmpbuf != NULL) free(tmpbuf);
      return RemoteSignal::MsgType::SERVER_REDIRECT;

    }else{
      break;
    }
  }

  case Boho::MsgType::AUTH_NONCE:
  {
    uint8_t authPack[MetaSize_AUTH_HMAC];
    size_t auth_len = auth_hmac(authPack, message, len);
    send(authPack, auth_len);
    // Serial.println(F(">> AUTH_NONCE , sent authPack"));
    break;
  }

  case Boho::MsgType::AUTH_ACK:
  {
    state = static_cast<int>(STATES::AUTH_READY);
    if (check_auth_ack_hmac(message, len))
    {
      // Serial.println(F(">> AUTH_ACK"));
      _buffer[0] = RemoteSignal::MsgType::CID_REQ;
      send(_buffer, 1);
    }
    else
    {
      // Serial.println(F(">> WRONG SERVER AUTH_ACK"));
    }
    break;
  }

  case Boho::MsgType::AUTH_FAIL:
    state = static_cast<int>(STATES::AUTH_FAIL);
    // Serial.println(F(">> AUTH_FAIL"));
    break;

  default:
    // printHex("<unkonwn:", message , len );
    break;
  }

  if (tmpbuf != NULL) free(tmpbuf);
  cong.clear();
  return 0;
}




void RemoteSignal::setClient(Client *client)
{
  this->client = client;
  cong.init(client);
}



void RemoteSignal::setRxBuffer(size_t size)
{
  // Serial.println(F("-- setRxBuffer: "));

#ifdef USE_PSRAM
  uint8_t *buf = (uint8_t *)ps_malloc(size);
#else
  uint8_t *buf = (uint8_t *)malloc(size);
#endif

  if (buf == NULL)
  {
    // Serial.println(F("\n-- NO RX_BUFFER!"));
    return;
  }

  cong.setBufferSize(buf, size);
}





void RemoteSignal::write(const uint8_t *buffer, uint32_t size)
{
  this->client->write(buffer, size);
}

void RemoteSignal::send(const uint8_t *buffer, uint32_t size)
{
  cong.send(buffer, size);
  lastTxRxTime = getUnixTime();
}

void RemoteSignal::send_enc_mode(const uint8_t *buf, uint32_t bufSize)
{
  // useEncryption
  if (encMode == RemoteSignal::ENC_MODE::YES ||
      encMode == RemoteSignal::ENC_MODE::AUTO && isAuthorized)
  {
#ifdef USE_PSRAM
    uint8_t *enc_buf = (uint8_t *)ps_malloc(bufSize + 25);
#else
    uint8_t *enc_buf = (uint8_t *)malloc(bufSize + 25);
#endif
    if (enc_buf == NULL)
      return;

    int packSize = encrypt_488(enc_buf, buf, bufSize);
    send(enc_buf, packSize);
    free(enc_buf);
    // ('<< send_enc_mode [ENC_488]')
  }
  else
  {
    // ('<< send_enc_mode  [PLAIN]' )
    send(buf, bufSize);
  }
}

void RemoteSignal::ping()
{
  _buffer[0] = RemoteSignal::MsgType::PING;
  send(_buffer, 1);
}

void RemoteSignal::pong()
{
  _buffer[0] = RemoteSignal::MsgType::PONG;
  send(_buffer, 1);
}


void RemoteSignal::login(const char *auth_id, const char *auth_key)
{
  // instance manual login.
  set_id8(auth_id);
  set_key(auth_key);
  size_t size = auth_req(_buffer);
  send(_buffer, size);
}

void RemoteSignal::auth(const char *auth_id, const char *auth_key)
{
  // automatic
  // send auth_req when sever ready, re_auth when reconnect
  set_id8(auth_id);
  set_key(auth_key);
  useAuth = true;
}

void RemoteSignal::auth(const char *id_key)
{
  set_id_key( id_key );
  useAuth = true;
}

void RemoteSignal::set(const char *setString)
{
  int setLen = strlen(setString);
  if (setLen > 255)
    return;

  uint8_t *buf = NULL;

  bool useDefaultBuffer = (setLen + 2) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
#ifdef USE_PSRAM
    buf = (uint8_t *)ps_malloc(setLen + 2);
#else
    buf = (uint8_t *)malloc(setLen + 2);
#endif
    if (buf == NULL)
      return;
  }

  buf[0] = RemoteSignal::MsgType::SET;
  buf[1] = (uint8_t)setLen;
  strcpy((char *)buf + 2, setString);
  send_enc_mode(buf, 2 + setLen);

  if (!useDefaultBuffer)
    free(buf);
}

void RemoteSignal::subscribe(const char *tag)
{
  int tagLen = strlen(tag);
  if (tagLen > 255)
    return;

  uint8_t *buf = NULL;
  bool useDefaultBuffer = (2 + tagLen) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
#ifdef USE_PSRAM
    buf = (uint8_t *)ps_malloc(2 + tagLen);
#else
    buf = (uint8_t *)malloc(2 + tagLen);
#endif
    if (buf == NULL)
      return;
  }
  buf[0] = RemoteSignal::MsgType::SUBSCRIBE;
  buf[1] = tagLen;
  strcpy((char *)buf + 2, tag);
  send_enc_mode(buf, 2 + tagLen);

  if (!useDefaultBuffer)
    free(buf);
}

void RemoteSignal::signal2(const char *target, const char *topic, const char *data)
{
  int targetLen = strlen(target);
  int topicLen = strlen(topic);
  char tag[targetLen + topicLen + 1];
  memcpy(tag, target, targetLen);
  memcpy(tag + targetLen, topic, topicLen);
  tag[targetLen + topicLen] = 0;
  signal(tag, data);
}

void RemoteSignal::signal2(const char *target, const char *topic, const char *data1, const char *data2)
{
  int targetLen = strlen(target);
  int topicLen = strlen(topic);
  char tag[targetLen + topicLen + 1];
  memcpy(tag, target, targetLen);
  memcpy(tag + targetLen, topic, topicLen);
  tag[targetLen + topicLen] = 0;
  signal(tag, data1, data2);
}

// signal : no payload
void RemoteSignal::signal(const char *tag)
{
  int tagLen = strlen(tag);
  if (tagLen > 255)
    return;

  uint8_t *buf = NULL;

  bool useDefaultBuffer = (2 + tagLen) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
#ifdef USE_PSRAM
    buf = (uint8_t *)ps_malloc(2 + tagLen);
#else
    buf = (uint8_t *)malloc(2 + tagLen);
#endif

    if (buf == NULL)
      return;
  }

  buf[0] = RemoteSignal::MsgType::SIGNAL;
  buf[1] = tagLen;
  strcpy((char *)buf + 2, tag);
  buf[2 + tagLen] = RemoteSignal::PAYLOAD_TYPE::EMPTY;
  send_enc_mode(buf, 3 + tagLen);

  if (!useDefaultBuffer)
    free(buf);
}

// siganl : string payload.
void RemoteSignal::signal(const char *tag, const char *data)
{
  int tagLen = strlen(tag);
  if (tagLen > 255)
    return;

  uint32_t dataLen = strlen(data) + 1; // put null charactor
  uint8_t *buf = NULL;

  bool useDefaultBuffer = (3 + tagLen + dataLen) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
#ifdef USE_PSRAM
    buf = (uint8_t *)ps_malloc(3 + tagLen + dataLen);
#else
    buf = (uint8_t *)malloc(3 + tagLen + dataLen);
#endif

    if (buf == NULL)
      return;
  }

  buf[0] = RemoteSignal::MsgType::SIGNAL;
  buf[1] = tagLen;
  strcpy((char *)buf + 2, tag);
  buf[2 + tagLen] = RemoteSignal::PAYLOAD_TYPE::TEXT;
  memcpy(buf + 3 + tagLen, data, dataLen);
  send_enc_mode(buf, 3 + tagLen + dataLen);

  if (!useDefaultBuffer)
    free(buf);
}

// siganl : two string payload.  PAYLOAD_TYPE::MJSON (JSON array )
void RemoteSignal::signal(const char *tag, const char *data1, const char *data2)
{
  int tagLen = strlen(tag);
  if (tagLen > 255)
    return;

  int data1Len = strlen(data1);
  int data2Len = strlen(data2);
  uint32_t dataLen = data1Len + data2Len + 7; // ["data1","data2"]

  uint8_t *buf = NULL;

  bool useDefaultBuffer = (3 + tagLen + dataLen) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
#ifdef USE_PSRAM
    buf = (uint8_t *)ps_malloc(3 + tagLen + dataLen);
#else
    buf = (uint8_t *)malloc(3 + tagLen + dataLen);
#endif

    if (buf == NULL)
      return;
  }

  buf[0] = RemoteSignal::MsgType::SIGNAL;
  buf[1] = tagLen;
  strcpy((char *)buf + 2, tag);
  buf[2 + tagLen] = RemoteSignal::PAYLOAD_TYPE::MJSON;

  memcpy(buf + 3 + tagLen, "[\"", 2);
  memcpy(buf + 3 + tagLen + 2, data1, data1Len);
  memcpy(buf + 3 + tagLen + 2 + data1Len, "\",\"", 3);
  memcpy(buf + 3 + tagLen + 2 + data1Len + 3, data2, data2Len);
  memcpy(buf + 3 + tagLen + 2 + data1Len + 3 + data2Len, "\"]", 2);

  send_enc_mode(buf, 3 + tagLen + dataLen);

  if (!useDefaultBuffer)
    free(buf);
}

// siganl : binary payload.
void RemoteSignal::signal(const char *tag, const uint8_t *data, uint32_t dataLen)
{
  int tagLen = strlen(tag); // no count null char
  if (tagLen > 255)
    return;
  uint8_t *buf = NULL;

  bool useDefaultBuffer = (3 + tagLen + dataLen) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
#ifdef USE_PSRAM
    buf = (uint8_t *)ps_malloc(3 + tagLen + dataLen);
#else
    buf = (uint8_t *)malloc(3 + tagLen + dataLen);
#endif
    if (buf == NULL)
      return;
  }

  buf[0] = RemoteSignal::MsgType::SIGNAL;
  buf[1] = tagLen; //  ch str len
  strcpy((char *)buf + 2, tag);
  buf[2 + tagLen] = RemoteSignal::PAYLOAD_TYPE::BINARY;
  ;

  memcpy(buf + 3 + tagLen, data, dataLen);
  send_enc_mode(buf, (3 + tagLen + dataLen));

  if (!useDefaultBuffer)
    free(buf);
}

void RemoteSignal::signal2(const char *target, const char *topic, const uint8_t *data, uint32_t dataLen)
{
  int targetLen = strlen(target);
  int topicLen = strlen(topic);
  char tag[targetLen + topicLen + 1];
  memcpy(tag, target, targetLen);
  memcpy(tag + targetLen, topic, topicLen);
  tag[targetLen + topicLen] = 0;
  signal(tag, data, dataLen);
}

void RemoteSignal::signal_e2e(const char *tag, const uint8_t *data, uint32_t dataLen, const char *dataKey)
{

  int tagLen = strlen(tag);
  if (tagLen > 255)
    return;

  uint8_t *buf = NULL;
  int dataOffset = 0;

  if (encMode == RemoteSignal::ENC_MODE::YES ||
      encMode == RemoteSignal::ENC_MODE::AUTO && isAuthorized)
  {
    // enc_e2e(21) + sig_e2e(3+tagLen+ enc_pack_payload)

    // encMode on:
    // buf size: enc_e2e header(21) + RemoteSignalHeader( 3+tagLen) |  encpack(25) + dataLen
    dataOffset = 21 + 3 + tagLen;
#ifdef USE_PSRAM
    buf = (uint8_t *)ps_malloc(dataOffset + 25 + dataLen);
#else
    buf = (uint8_t *)malloc(dataOffset + 25 + dataLen);
#endif
    if (buf == NULL)
      return;

    uint8_t sigMsgHeader[3 + tagLen];
    sigMsgHeader[0] = RemoteSignal::MsgType::SIGNAL_E2E; // for end receiver.
    sigMsgHeader[1] = tagLen;
    strcpy((char *)sigMsgHeader + 2, tag);
    sigMsgHeader[2 + tagLen] = RemoteSignal::PAYLOAD_TYPE::BINARY;

    // encrypt sigHeader only. exclude payload(already encrypted)
    encrypt_488(buf, sigMsgHeader, 3 + tagLen);
    // now fills 21+3+tagLen.
    buf[0] = Boho::MsgType::ENC_E2E; // change type   .. for server
  }
  else
  {
    // encMode off:   RemoteSignalMsg Signal Header( 3+tagLen) | encpack(25) + dataLen
    dataOffset = 3 + tagLen;
#ifdef USE_PSRAM
    buf = (uint8_t *)ps_malloc(dataOffset + 25 + dataLen);
#else
    buf = (uint8_t *)malloc(dataOffset + 25 + dataLen);
#endif
    if (buf == NULL)
      return;

    // RemoteSignal Header
    buf[0] = RemoteSignal::MsgType::SIGNAL_E2E;
    buf[1] = tagLen; //  ch str len
    strcpy((char *)buf + 2, tag);
    buf[2 + tagLen] = RemoteSignal::PAYLOAD_TYPE::BINARY;
  }

  uint32_t e2ePackSize = encrypt_e2e(buf + dataOffset, data, dataLen, dataKey);
  send(buf, dataOffset + e2ePackSize);
  free(buf);
}

void RemoteSignal::signal2_e2e(const char *target, const char *topic, const uint8_t *data, uint32_t dataLen, const char *dataKey)
{
  int targetLen = strlen(target);
  int topicLen = strlen(topic);
  if ((targetLen + topicLen) > 255)
    return;

  char tag[targetLen + topicLen + 1];
  memcpy(tag, target, targetLen);
  memcpy(tag + targetLen, topic, topicLen);
  tag[targetLen + topicLen] = 0;
  signal_e2e(tag, data, dataLen, dataKey);
}

void RemoteSignal::onMessage(void (*messageCallback)(char *, uint8_t, uint8_t *, size_t))
{
  this->messageCallback = messageCallback;
}

void RemoteSignal::onReady(void (*readyCallback)(void))
{
  this->readyCallback = readyCallback;
}



void RemoteSignal::close(uint8_t reason)
{
  _buffer[0] = RemoteSignal::MsgType::CLOSE;
  _buffer[1] = reason;
  send(_buffer, 2);
  this->client->flush();
  clear();
}

void RemoteSignal::clear(void)
{
  cong.drop();
  cong.clear();
  isAuthorized = false;
}


RemoteSignal::~RemoteSignal()
{
}