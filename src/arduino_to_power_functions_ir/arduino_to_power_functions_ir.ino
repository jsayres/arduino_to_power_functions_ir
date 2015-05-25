#include <IRremote.h>

IRsend irsend;
unsigned int mark = 158;
unsigned int zero = 263;
unsigned int one = 553;
unsigned int bookend = 1026;
unsigned int space = 20000;
char receiveBuffer[4];
unsigned int sendBuffer[36];
unsigned int test[3] = {42, 42, 42};

void buildSendBuffer() {
    int i, j;
    int k = 1;
    char n;
    for (i = 0; i < 4; i++) {
      n = receiveBuffer[i];
      for (j = 0; j < 4; j++) {
        k += 2;
        sendBuffer[k] = bitRead(n, j) ? one : zero;
      }
    }
}

void sendIR() {
    irsend.sendRaw(sendBuffer, 36, 38);
}

void setup() {
    sendBuffer[1] = bookend;
    sendBuffer[35] = space;
    int i;
    for (i = 0; i < 36; i += 2) {
        sendBuffer[i] = mark;
    }
    Serial.begin(9600);
    pinMode(13, OUTPUT);
}

bool isPacket() {
  return (
    Serial.available() >= 3 &&
    Serial.read() == test[0] &&
    Serial.read() == test[1] &&
    Serial.read() == test[2]
  );
}

void loop() {
    if(isPacket()) {
        Serial.readBytes(receiveBuffer, 4);
        buildSendBuffer();
        sendIR();
    }
}

