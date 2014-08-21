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
unsigned int delays[4] = {128, 160, 192, 224};
/* unsigned int delays[4][5] = */
/*     { */
/*         {48, 80, 80, 128, 128}, */
/*         {32, 80, 80, 160, 160}, */
/*         {16, 80, 80, 192, 192}, */
/*         {0, 80, 80, 224, 224} */
/*     }; */
byte messageBuffer[64];
unsigned int messageLength;
String title;
String content;

// Power for blue, red respectively for each channel
int commandHistory[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};

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

void printNibble(boolean nibble[]) {
    Serial.print(nibble[3]);
    Serial.print("|");
    Serial.print(nibble[2]);
    Serial.print("|");
    Serial.print(nibble[1]);
    Serial.print("|");
    Serial.println(nibble[0]);
}

void printSendBuffer() {
    for (int i = 0; i < 36; i++) {
        Serial.println(sendBuffer[i]);
    }
}

void sendIR() {
    irsend.sendRaw(sendBuffer, 36, 38);
    delay(80);
    irsend.sendRaw(sendBuffer, 36, 38);
    delay(delays[channel] - packetTime/1000);
    /* for (int i = 0; i < 5; i++) { */
    /*     delay(delays[channel][i]); */
    /*     irsend.sendRaw(sendBuffer, 36, 38); */
    /* } */
    /* delay(delays[channel][4]); */
}

boolean findIdCode() {
    for (int i = 0; i < 64; i++) {
        messageBuffer[i] = bluetooth.read();
        /* Serial.print(messageBuffer[i]); */
        /* Serial.print(" "); */
        // Check that we found the id code and also have the message length, which
        // comes 4 bytes before the id code
        if ((i > 4) && (messageBuffer[i] == 0x9e) && (messageBuffer[i - 1] == 0x81)) {
            messageLength = word(messageBuffer[i - 4], messageBuffer[i - 5]) - 4;
            if (messageLength < 65) {
                return true;
            }
        }
        else if (messageBuffer[i] == -1) {
            return false;
        }
    }
    return false;
}

void readMessage() {
    for (int i = 0; i < messageLength; i++) {
        messageBuffer[i] = bluetooth.read();
    }
}

void getMessageTitle() {
    title = "";
    if (messageBuffer[0] < 10) {
        for (int i = 0; i < messageBuffer[0] - 1; i++) {
            title += (char)messageBuffer[1 + i];
        }
    }
}

void getMessageContent() {
    unsigned int lengthIndex = 1 + messageBuffer[0];
    word contentLength = word(messageBuffer[lengthIndex + 1], messageBuffer[lengthIndex]);
    content = "";
    if (contentLength == 5) {
        /* Ignore last character, which is a null */
        for (int i = 0; i < contentLength - 1; i++) {
            content += (char)messageBuffer[lengthIndex + 2 + i];
        }
    }
}

void setup() {
    /* Serial.begin(9600); */
    bluetooth.begin(115200);
    bluetooth.print("$");
    bluetooth.print("$");
    bluetooth.print("$");
    delay(100);
    bluetooth.println("U,9600,N");
    bluetooth.begin(9600);
    /* Serial.println("test"); */
    setupSendBuffer();
    /* parseCmd("2r+7"); */
    /* buildSendBuffer(); */
    /* printNibble(nibble1); */
    /* printNibble(nibble2); */
    /* printNibble(nibble3); */
    /* printNibble(lrc); */
    /* printSendBuffer(); */
    /* bluetooth.println("hello there"); */
    pinMode(13, OUTPUT);
}

void loop() {
    delay(100);
    if (bluetooth.available()) {
        if (findIdCode()) {
            readMessage();
            /* getMessageTitle(); */
            getMessageContent();
            /* Serial.print("Command: "); */
            /* Serial.println(content); */
            if (parseCmd()) {
                buildSendBuffer();
                sendIR();
                /* Serial.print("Send cmd: "); */
                /* Serial.println(content); */
            }
        }
    }
    if (bluetooth.overflow()) {
        digitalWrite(13, HIGH);
        delay(100);
        digitalWrite(13, LOW);
        delay(100);
    }
    /* if (Serial.available()) { */
        /* bluetooth.print((char)Serial.read()); */
    /* } */
}
