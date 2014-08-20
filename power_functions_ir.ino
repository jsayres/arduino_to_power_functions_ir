#include <IRremote.h>

IRsend irsend;


unsigned int parseMotorPower(String powerStr) {
  int power = powerStr.toInt();
  if (power > 7) {
    power = 7;
  }
  if (power < -8) {
    power = -8;
  }
  if (power < 0) {   
    power += 16;
  }
  return power;
}


unsigned int makePacket(int channel, int redPower, int bluePower) {
  
}

void setup() {
  Serial.begin(9600);
  Serial.println("hi");
  Serial.println(parseMotorPower("-6"));
}

void loop() {
  
}
