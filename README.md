# Arduino Client for Remote Signal

This library provides a client for doing signaling(messaging) with a server that supports [`RemoteSignal`](https://github.com/congtrol/remote-signal).

## Examples

The library comes with a number of example sketches. See File > Examples > RemoteSignal
within the Arduino application.

## Features

### Signaling
- pub/sub style multicast: by channel name.
- uni-cast: one to one messaging by CID.
- CID subscribing: subscribe one peer using CID.
- CID: Communication Id.
- HomeChannel: group by IP address.

### Built-in Security
- Authentication
- Encryption
- E2EE
- thanks to the `Boho` [ [github](https://github.com/congtrol/boho-arduino) ]


## Compatible Hardware

The library uses Stream for interacting with the
underlying network hardware. This means it Just Works with a growing number of
boards and shields, including:

### tested
 - Arduino Ethernet Shield
 - ESP8266
 - ESP32
 - Arduino UNO R4 WiFi

## Compatibility
 - Support Remote Signal Sever only.
 - No MQTT protocol support.

## Remote Signal repositories.
- Javascript: `remote-signal` [ [github](https://github.com/congtrol/remote-signal) | [npm](https://www.npmjs.com/package/remote-signal) ]
  - Node.js server
  - Node.js client ( WebSocket, CongSocket)
  - Web Browser client( WebSocket)
- Arduino client: 
  - `remote-signal-arduino` [ [github](https://github.com/congtrol/remote-signal-arduino) ]
  - or use Arduino Library Manager: `RemoteSignal`
- CLI program 
  - `remocon` [ [github](https://github.com/congtrol/remocon) | [npm](https://www.npmjs.com/package/remocon) ]
  - install: `npm i -g remocon`
  - support mac, linux and windows.
  - server and client

## License

This code is released under the MIT License.