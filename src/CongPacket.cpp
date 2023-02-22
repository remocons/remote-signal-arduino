
#include "Arduino.h"
#include "CongPacket.h"
#include <string.h>

CongPacket::CongPacket()
{
  _state = ReadingState::HEAD;
  _bufferIndex = 0;

}


void CongPacket::setBufferSize( uint8_t * buf, size_t size)
{
  _buffer = buf;

  if(!_buffer){
    // Serial.println("\nNo CongPacket RX buffer.");
  }else{
    // Serial.print("\nCongPacket RX buffer Size: " );
    // Serial.println( size );
    _bufferSize = size;
  }
  
}


void CongPacket::init( Stream *_client)
{
  client = _client;
  
}



void CongPacket::fill_head()
{
  if( client->available() > 0 ){
    uint8_t hd = client->read(); //one

    if( hd == MsgType::TYPE_LEN1 ){
      _state = ReadingState::LEN1;
      fill_len(1);
    }else if( hd == MsgType::TYPE_LEN2 ){
      _state = ReadingState::LEN2;
      fill_len(2);
    }else if( hd == MsgType::TYPE_LEN3 ){
      _state = ReadingState::LEN3;
      fill_len(3);
    }else if( hd == MsgType::TYPE_LEN4 ){
      _state = ReadingState::LEN4;
      fill_len(4);
    }else{
      _state = ReadingState::UNKNOWN_TYPE;
      
    }

  }

}



void CongPacket::fill_len( uint8_t lenBytes ) 
{
    if( client->available() >= lenBytes ){ 
      packetLength.u32 = 0;
      client->readBytes( packetLength.buf, lenBytes );
      _payloadLength = packetLength.u32;

     if(  _payloadLength > _bufferSize || _payloadLength == 0){
        _state = ReadingState::OVER_SIZE;
     }else{
        _state = ReadingState::BODY;
        fill_body();
     }
    }
}


void CongPacket::fill_body()
{
  
  size_t available = client->available();
  if(available ==  0) return;

  size_t need =_payloadLength - _bufferIndex ;
    if( need > available ){ 
      need = available;  
    }

   size_t readSize = client->readBytes( _buffer + _bufferIndex , need );
    _bufferIndex += readSize;

  // Serial.printlnv("###  available: %d  read: %d  _bufferIndex: %d", available, readSize,  _bufferIndex );

    if( _bufferIndex == _payloadLength ){
      _state = ReadingState::READY;
      _ready = true;
    }else if( _bufferIndex > _payloadLength ){
      // Serial.println("Error: congpack OVER_FLOW");
      _state = ReadingState::OVER_FLOW;
    }

}



bool CongPacket::ready()
{
  if( _ready ) return true;
    else return false;
}


void CongPacket::run()
{
  switch( _state ){
    case ReadingState::HEAD:
      fill_head();
      break;
    case ReadingState::LEN1:
      fill_len(1);
      break;
    case ReadingState::LEN2:
      fill_len(2);
      break;
    case ReadingState::LEN3:
      fill_len(3);
      break;
    case ReadingState::LEN4:
      fill_len(4);
      break;
    case ReadingState::BODY:
      fill_body();
      break;
    default:
      // Serial.println("error: unknown congpacket _state");
      break;
  }

}


void CongPacket::send(const uint8_t* buffer, uint32_t size ){

  uint8_t header[5];
  uint8_t lenBytes = 1; 

  if( size <= 2){
    // one or two bytes payload. 
    // ping, pong,, close, auth_reg, cid_req etc...
    header[0] = MsgType::TYPE_LEN1;
    header[1] = size;
    header[2] = buffer[0];
    if( size == 2 )header[3] = buffer[1];
    client->write( header, size + 2  );

  }else{
    packetLength.u32 = size;

    if( size < 256 ){
      header[0] = MsgType::TYPE_LEN1;
    }else if( size < 65536){
      header[0] = MsgType::TYPE_LEN2;
      lenBytes = 2;
    }else{
      header[0] = MsgType::TYPE_LEN4;
      lenBytes = 4;
    }
    //packet header
    memcpy( header + 1, packetLength.buf , lenBytes );
    client->write( header, lenBytes + 1 );
    //payload
    client->write( buffer, size );

  }



}



void CongPacket::drop(){
 while( client->available() ) client->read();
}


void CongPacket::clear(){
  _ready = false; 
  _state = ReadingState::HEAD; 
  _bufferIndex = 0;
}