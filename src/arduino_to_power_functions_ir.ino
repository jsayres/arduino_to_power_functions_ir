#include <SoftwareSerial.h>
#include <IRremote.h>

IRsend irsend;
boolean nibble1[4] = {false, false, true, false};
boolean nibble2[4] = {false, false, false, false};
boolean nibble3[4] = {false, false, false, false};
boolean lrc[4];
unsigned int channel;
char color;
int power;
unsigned int sendBuffer[36];
unsigned int packetTime;
unsigned int mark = 158;
unsigned int lowSpace = 263;
unsigned int hiSpace = 553;
unsigned int startStopSpace = 1026;
/* unsigned int delays[4] = {128, 160, 192, 224}; */
byte messageBuffer[64];
unsigned int bytesRead;
unsigned int messageStart;
unsigned int messageLength;
String title;
String content;
/* byte ack[15] = {0x0d, 0x00, 0x01, 0x00, 0x81, 0x9e, 0x03, 0x6f, 0x6b, 0x00, 0x03, 0x00, 0x6f, 0x6b, 0x00}; */
/* unsigned int ackLength = 15; */
/* byte ackTrue[13] = {0x0b, 0x00, 0x01, 0x00, 0x81, 0x9e, 0x03, 0x6f, 0x6b, 0x00, 0x01, 0x00, 0x01}; */
/* byte ackFalse[13] = {0x0b, 0x00, 0x01, 0x00, 0x81, 0x9e, 0x03, 0x6f, 0x6b, 0x00, 0x01, 0x00, 0x00}; */
/* unsigned int ackLength = 13; */

// Power for blue, red respectively for each channel
/* int commandHistory[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}; */

SoftwareSerial bluetooth(4, 5);

/**
 * Commands are strings.
 *
 * The first character is the channel number, 1-4.
 * The second character is the output color, r or b.
 * The third character is + or - for motor direction.
 * The last character is the motor power 0-7, or 8 for a brake then float.
 *
 * Example: "2r-5" would be channel 2, red output, power 5 backwards.
 **/
boolean parseCmd() {
    if (content.length() != 4) {
        return false;
    }
    content.toLowerCase();
    channel = content[0] - '1';
    color = content[1];
    power = content.substring(2, 4).toInt();
    if (channel < 0 || channel > 3 || (color != 'r' && color != 'b') || power < -8 || power > 8) {
        return false;
    }
    setChannelNibble();
    setPowerNibble();
    setLRC();
    return true;
}

void setChannelNibble() {
    nibble1[0] = bitRead(channel, 0);
    nibble1[1] = bitRead(channel, 1);
}

void setPowerNibble() {
    /* Both 8 and -8 represent brake then float in our custom syntax,
     * but -8 is what the actual IR protocol expects */
    if (power == 8) {
        power = -8;
    }
    /* Need the 2's complement for the protocol */
    if (power < 0) {
        power += 16;
    }
    for (int i = 0; i < 4; i++) {
        if (color == 'r') {
            nibble2[i] = bitRead(power, i);
        }
        else {
            nibble3[i] = bitRead(power, i);
        }
    }
}

void setLRC() {
    for (int i = 0; i < 4; i ++) {
        lrc[i] = true ^ nibble1[i] ^ nibble2[i] ^ nibble3[i];
    }
}

void setupSendBuffer() {
    for (int i = 0; i < 36; i += 2) {
        sendBuffer[i] = mark;
    }
    sendBuffer[1] = startStopSpace;
    sendBuffer[35] = startStopSpace;
}

unsigned int getSpace(boolean val) {
    if (val) {
        return hiSpace;
    }
    else {
        return lowSpace;
    }
}

void buildSendBuffer() {
    packetTime = mark*18 + startStopSpace*2;
    int j;
    int k;
    for (int i = 0; i < 4; i++) {
        j = 3 - i;
        k = 3 + i*2;
        sendBuffer[k] = getSpace(nibble1[j]);
        packetTime += getSpace(nibble1[j]);
        sendBuffer[k + 8] = getSpace(nibble2[j]);
        packetTime += getSpace(nibble2[j]);
        sendBuffer[k + 16] = getSpace(nibble3[j]);
        packetTime += getSpace(nibble3[j]);
        sendBuffer[k + 24] = getSpace(lrc[j]);
        packetTime += getSpace(lrc[j]);
    }
}

void sendIR() {
    irsend.sendRaw(sendBuffer, 36, 38);
    /* delay(delays[channel] - packetTime/1000); */
}

boolean findIdCode() {
    // Check that we found the id code and also have the message length, which
    // comes 4 bytes before the id code
    for (int i = 5; i < bytesRead; i++) {
        if ((messageBuffer[i] == 0x9e) && (messageBuffer[i - 1] == 0x81)) {
            messageLength = word(messageBuffer[i - 4], messageBuffer[i - 5]) - 4;
            messageStart = i - 3;
            // Check that message is fully within bytes read
            if (messageStart + messageLength  - 1 < bytesRead) {
                return true;
            }
        }
    }
    return false;
}

void getMessageTitleAndContent() {
    // Title begins after 4 id bytes and 1 title length byte
    unsigned int titleStart = messageStart + 5;
    unsigned int titleLength = messageBuffer[titleStart - 1];
    title = "";
    if (titleLength < 10) {
        /* Ignore last character, which is a null */
        for (int i = titleStart; i < titleStart + titleLength - 1; i++) {
            title += (char)messageBuffer[i];
        }
    }
    // Content begins after title and 2 content length bytes
    unsigned int contentStart = titleStart + titleLength + 2;
    unsigned int contentLength = word(messageBuffer[contentStart - 1], messageBuffer[contentStart - 2]);
    content = "";
    if (contentLength < 10) {
        /* Ignore last character, which is a null */
        for (int i = contentStart; i < contentStart + contentLength - 1; i++) {
            content += (char)messageBuffer[i];
        }
    }
}

void readBuffer() {
    bytesRead = 0;
    while (bluetooth.available() > 0) {
        messageBuffer[bytesRead] = bluetooth.read();
        bytesRead++;
    }
}

void sendAck() {
    bluetooth.write(ackTrue, ackLength);
    bluetooth.write(ackFalse, ackLength);
}

void printMessageBuffer() {
    Serial.println(bytesRead);
    for (int i = 0; i < bytesRead; i++) {
        Serial.print(messageBuffer[i], HEX);
        Serial.print(" ");
    }
    Serial.println("");
}

void setup() {
    bluetooth.begin(115200);
    bluetooth.print("$");
    bluetooth.print("$");
    bluetooth.print("$");
    delay(100);
    bluetooth.println("U,9600,N");
    bluetooth.begin(9600);
    setupSendBuffer();
    pinMode(13, OUTPUT);
}

void loop() {
    if (bluetooth.available() > 0) {
        // Allow enough time for full command to make it into bluetooth buffer
        delay(15);
        readBuffer();
        if (findIdCode()) {
            getMessageTitleAndContent();
            if (parseCmd()) {
                buildSendBuffer();
                sendIR();
            }
        }
    }
    if (bluetooth.overflow()) {
        digitalWrite(13, HIGH);
        delay(100);
        digitalWrite(13, LOW);
        delay(100);
    }
}
