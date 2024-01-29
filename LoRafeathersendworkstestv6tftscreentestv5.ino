/*
   Open Source
   Jan 15 2024
   Lora communication device with touchscreen TFT
   Adafruit m0 /w Lora built in 915Mhz
*/

#include <Arduino.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>              // include libraries
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Wire.h>

// pin definition for the m0 tft
#define tft_cs   19
#define tft_dc   18
#define tft_rst  17
#define ts_cs 16

const int LoraCsPin = 8;          // LoRa radio chip select
const int LoraResetPin = 4;       // LoRa radio reset
const int LoraIrqPin = 7;         // change for your board; must be a hardware interrupt pin
const int powerPin = 13;
#define VBATPIN 9

String outgoing = "";   // outgoing message
String incomingRec[5] = {"","","","",""};
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 1;     // address of this device
byte destination = 0xFF;      // destination to send to
long lastSendTime = 0;        // last send time
int interval = 2000;          // interval between sends
byte headerextra;   //   extraheaderbytes

// create an instance of the library
//TFT TFTscreen = TFT(cs, dc, rst);
// char array to print to the screen
//char sensorPrintout[4];
Adafruit_ILI9341 tft = Adafruit_ILI9341(tft_cs, tft_dc, tft_rst);
XPT2046_Touchscreen ts(ts_cs);
int midPage = 0; //what page the middle of the screen is on.  page one is keyboard. page 2 is destination select. 3islog/power/sf/options
int SpreadingFactor = 12;
float xPix;
float yPix;
char keyboardKeys[] = "qwertyuiopasdfghjkl zxcvbnm.?!0123456789";
float measuredvbat;

const unsigned char epd_bitmap_Bitmap [] PROGMEM = {
  // 'output-onlinepngtools, 24x24px
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xf8, 0x00, 0xff, 0xf8, 0x00, 
  0xc0, 0x18, 0x00, 0xc0, 0x18, 0x00, 0xc0, 0x18, 0x00, 0xcc, 0x18, 0x00, 0x0e, 0x18, 0x00, 0x07, 
  0x18, 0x1f, 0xff, 0x98, 0x1f, 0xff, 0x98, 0x00, 0x07, 0x18, 0x00, 0x0e, 0x18, 0x00, 0xcc, 0x18, 
  0x00, 0xc0, 0x18, 0x00, 0xc0, 0x18, 0x00, 0xc0, 0x18, 0x00, 0xff, 0xf8, 0x00, 0xff, 0xf8, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  // 'batt, 24x24px
  // 'batt, 24x24px
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0xc7, 0x00, 0x01, 
  0x01, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x10, 0x80, 0x01, 0x10, 
  0x80, 0x01, 0x30, 0x80, 0x01, 0x3c, 0x80, 0x01, 0x3c, 0x80, 0x01, 0x18, 0x80, 0x01, 0x10, 0x80, 
  0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x01, 0x80, 0x00, 
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  // 'wireless.bmp 24x24px
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x03, 0xff, 0x80, 0x07, 
 0xff, 0xe0, 0x0f, 0xff, 0xf0, 0x1f, 0x81, 0xf8, 0x3e, 0x00, 0x7c, 0x7c, 0x00, 0x3c, 0x78, 0x7c, 
  0x1e, 0xf0, 0xff, 0x1e, 0xf1, 0xc3, 0x8f, 0x03, 0x01, 0x8c, 0x03, 0x18, 0xc0, 0x00, 0x3c, 0xc0, 
  0x00, 0x7c, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  
};
const GFXglyph epd_bitmap_Glyphs [] PROGMEM = {
  { 0, 24, 24, 0, 0, 0 },// 'output-onlinepngtools'
  { 72, 24, 24, 0, 0, 0 },  // 'batt, 24x24px
  { 144, 24, 24, 0, 0, 0 }
};

const GFXfont epd_bitmap_Font PROGMEM = {
  (uint8_t  *)epd_bitmap_Bitmap,
  (GFXglyph *)epd_bitmap_Glyphs,
  0, 1, 0 };

int recipient=0;        // recipient address
byte sender = 0;
byte incomingMsgId=0;   // incoming msg ID
byte incomingLength=0;   // incoming msg length
String incoming = "";
String incoming2 = "";

String incomingRec5 = "";
String incomingRec4 = "";
String incomingRec3 = "";
String incomingRec2 = "";
String incomingRec1 = "";
String incomingRec0 = "";

void setup() {
  //deselect all SPI devices
  pinMode(LoraCsPin, OUTPUT);  //set pin high so lora is disabled
  pinMode(tft_cs, OUTPUT);
  pinMode(ts_cs, OUTPUT);
  digitalWrite(LoraCsPin, HIGH);
  digitalWrite(tft_cs, HIGH);
  digitalWrite(ts_cs, HIGH);

  Serial.begin(9600);
  Serial.println("LoRa Duplex");
  tft.begin();
  ts.begin();
  ts.setRotation(1);
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);

  LoRa.setPins(LoraCsPin, LoraResetPin, LoraIrqPin);// set CS, reset, IRQ pin
  if (!LoRa.begin(915E6)) {             // initialize ratio at 915 MHz
    Serial.println("LoRa init failed. Check your connections.");
    tft.print("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }
  Serial.println("LoRa init succeeded.");

  //tft.println("LoRa init succeeded.");

  LoRa.setSpreadingFactor(SpreadingFactor);
  LoRa.dumpRegisters(Serial);

  drawTopHalf();
  drawMidHalf(midPage);
  drawBottomHalf("Recieving...");
  drawMsg("");
}

void loop() {

  onReceive(LoRa.parsePacket());
  delay(50);

  if (ts.touched()) {
    delay(50);

    TS_Point p = ts.getPoint();

    Serial.print("Pressure = ");
    Serial.print(p.z);
    Serial.print(", x = ");
    Serial.print(p.x);
    Serial.print(", y = ");
    Serial.print(p.y);
    Serial.println();

    xPix = (p.x - 360) / 11;
    Serial.print(xPix);
    Serial.println();

    yPix = (p.y - 270) / 14.8;
    Serial.println(yPix);
    Serial.println();

    //      y                           x
    // ((tft.height()*1) / 10)   ((tft.width()*1) / 10)

    if ((yPix < (tft.height() * 1) / 10) && (xPix > (tft.width() * 1) / 6) && (xPix < (tft.width() * 5) / 12)) {
      //if point is on "To:"
      midPage = 2;
      drawMidHalf(midPage);
    }

    if ((yPix < (tft.height() * 1) / 10) && (xPix < (tft.width() * 1) / 6)) {
      //ifpoint is on "Send:"
      midPage = 1;
      drawMidHalf(midPage);
    }
      //ifpointer is on log
     if ((yPix < (tft.height() * 1) / 10) && (xPix > (tft.width() * 11) / 12+12)) {
      
      midPage = 3;
      drawMidHalf(midPage);
    }
          //ifpointer is on batt sf
     if ((yPix < (tft.height() * 1) / 10) && (xPix > (tft.width() * 9) / 12+12) && (xPix < (tft.width() * 11) / 12+12)) {
      
      midPage = 4;
      drawMidHalf(midPage);
    }
//__________________________
    if ((yPix > (tft.height() * 1) / 10) && (yPix < (tft.height() * 5) / 10) ) {
      //keyboard select
      if (midPage == 1) {
        int Kx = 0;
        int Ky = 0;
        float xPix2 = xPix;
        float yPix2 = yPix;

        while (xPix2 > 0) {
          xPix2 -= ((tft.width() * 1) / 10);
          if (xPix2 >= 0) {
            Kx++;
          }
        }

        yPix2 -= ((tft.height() * 1) / 10);
        while (yPix2 > 0) {
          yPix2 -= ((tft.height() * 1) / 10);
          if (yPix2 >= 0) {
            Ky++;
          }
        }

        outgoing += (keyboardKeys[(Ky * 10 + Kx)]);
     
        drawMsg(outgoing);
      }
    }
       //device select screen
      if ((yPix > (tft.height() * 2) / 10) && ((yPix < (tft.height() * 3) / 10)) ) {
        if (midPage == 2) {
          int Kx = 0;
          float xPix2 = xPix;
          while (xPix2 > 0) {
            xPix2 -= ((tft.width() * 1) / 10);
            if (xPix2 >= 0) {
              Kx++;
            }
          }
          destination = (Kx);
        }
      }
    
    //dest 255
    if ((yPix > (tft.height() * 3) / 10) && ((yPix < (tft.height() * 4) / 10))  && ((xPix < (tft.width() * 2) / 10))) {
      if (midPage == 2) {
        destination = 255;
      }
    }

    //BackSpace button
    if ((yPix > (tft.height() * 5) / 10) && ((yPix < (tft.height() * 6) / 10))  && ((xPix < (tft.width() * 2) / 10))) {
      if (midPage == 1) {
        if (outgoing.length() > 0) {
          outgoing.remove(outgoing.length() - 1);
        }
        //int length = strlen(&outgoing);
        //outgoing[length] = "zsed";
        drawMsg(outgoing);
        Serial.print(outgoing);
      }
    }
      //send message
    if ((yPix > (tft.height() * 7) / 10) && ((yPix < (tft.height() * 8) / 10))  && ((xPix > (tft.width() * 8) / 10))){
        sendMessage(outgoing);
       // tft.setCursor((tft.width() * 0) / 10 + 1, (tft.height() * 7) / 10 + 5);
        tft.setTextColor(0xAEDC);
        // tft.print("Msg: ");
        tft.fillRect(int((tft.width() * 2) / 10), int((tft.height() * 7) / 10 )-1,int((tft.width() * 8) / 10),((tft.height() * 1) / 10), 0x0000);
        tft.setCursor(((tft.width() * 2) / 10 )-3, (tft.height() * 7) / 10 + 5);
        tft.print(outgoing);           
        tft.setCursor(((tft.width() * 2) / 10 )-3, (tft.height() * 7) / 10 + 5);
        //move msg off screen
        for (int i = 0; i < 180; i=i+2){
        tft.setCursor(((tft.width() * 2) / 10 )-3 + i, (tft.height() * 7) / 10 + 5);
        tft.setTextColor(0xAEDC);  
        tft.print(outgoing); 
        delay(7);
          tft.setCursor(((tft.width() * 2) / 10 )-3 + i, (tft.height() * 7) / 10 + 5);
          tft.setTextColor(0x0000);
           tft.print(outgoing);
        }
        outgoing ="";
        drawMsg(outgoing);
      }
      
    drawTopHalf();

  }//end iftouched()
}
//__________________________________________________________________

void drawTopHalf() {
  tft.setTextColor(0xffff);
  tft.fillRect(0, 0, tft.width(), tft.height() / 10, 0x0000);
    //send
  tft.drawRect((tft.width() * 0) / 6, (tft.height() * 0) / 3, tft.width()*1 / 6, tft.height() / 10, 0xffff);
  tft.setCursor((tft.width() * 0) / 6 + 4, (tft.height()) * 0 / 3 + 5);
  tft.setTextSize(2);
  tft.print("Send");
//to
  tft.drawRect((tft.width() * 1) / 6, (tft.height() * 0) / 3, tft.width()*3 / 12, tft.height() / 10, 0xffff);
  tft.setCursor((tft.width() * 1) / 6 + 4, (tft.height()) * 0 / 3 + 5);
  tft.setTextSize(2);
  tft.print("To:");
  tft.print(destination);
//Id:
  tft.drawRect((tft.width() * 5) / 12, (tft.height() * 0) / 3, tft.width()*3 / 12, tft.height() / 10, 0xffff);
  tft.setCursor((tft.width() * 5) / 12 + 4, (tft.height()) * 0 / 3 + 5);
  tft.setTextSize(2);
  tft.print("ID:");
  tft.print(localAddress);
//log/sf/batt:
 tft.setFont(&epd_bitmap_Font);
   tft.drawChar((tft.width() * 11) / 12 +2, 1, 0, ILI9341_PINK, ILI9341_BLACK, 1); //log icon
 tft.drawChar((tft.width() * 10) / 12 +10, 0, 1, ILI9341_GREEN, ILI9341_BLACK, 1); // power icon
 tft.drawChar((tft.width() * 9) / 12 +12, 0, 2, ILI9341_YELLOW, ILI9341_BLACK, 1); //spreadingfactor icon
  tft.setFont();  //reset font to default
}

void drawMidHalf(int page) {
  tft.setTextColor(0xffff);
  tft.fillRect((tft.width() * 0) / 10, (tft.height() * 1) / 10, tft.width(), ((tft.height() * 6) / 10) - 1, 0x0000);
  tft.setTextSize(2);

  if (midPage == 0) { //default loading screen
    tft.setCursor((tft.width() * 0) / 10 + 5, (tft.height()) * 3 / 10 + 5);
    tft.setTextColor(0xF81F);
    tft.setTextSize(3);
    tft.print("LoRa Communicator");
  }
  if (midPage == 1) {  //keyboard screen
    //THIS CAN BE SHORTENED
    int j = 0;
    for (int i = 0; i < 10; i++) { //topkeyboard
      tft.drawRect((tft.width()*i) / 10, (tft.height() * 1) / 10, tft.width() / 10, tft.height() / 10, 0xffff);
      tft.setCursor((tft.width()*i) / 10 + 10, (tft.height()) * 1 / 10 + 5);
      tft.print(keyboardKeys[j]);
      j++;
    }

    for (int i = 0; i < 10; i++) {  //second line of keyboard
      tft.drawRect((tft.width()*i) / 10, (tft.height() * 2) / 10, tft.width() / 10, tft.height() / 10, 0xffff);
      tft.setCursor((tft.width()*i) / 10 + 10, (tft.height() * 2) / 10 + 5);
      tft.print(keyboardKeys[j]);
      j++;
    }

    for (int i = 0; i < 10; i++) {  //third line of keyboard
      tft.drawRect((tft.width()*i) / 10, (tft.height() * 3) / 10, tft.width() / 10, tft.height() / 10, 0xffff);
      tft.setCursor((tft.width()*i) / 10 + 10, (tft.height() * 3) / 10 + 5);
      tft.print(keyboardKeys[j]);
      j++;
    }

    for (int i = 0; i < 10; i++) {  //numbers of keyboard
      tft.drawRect((tft.width()*i) / 10, (tft.height() * 4) / 10, tft.width() / 10, tft.height() / 10, 0xffff);
      tft.setCursor((tft.width()*i) / 10 + 10, (tft.height() * 4) / 10 + 5);
      tft.print(keyboardKeys[j]);
      j++;
    }
    tft.drawRect((tft.width() * 0) / 10, (tft.height() * 5) / 10, tft.width() * 2 / 10, tft.height() / 10, 0xffff);
    tft.setCursor((tft.width() * 0) / 10 + 10, (tft.height() * 5) / 10 + 5);
    tft.print("<<<");

  }
  if (midPage == 2) {//  TO: device selector screen
    for (int i = 0; i < 10; i++) {
      tft.setCursor((tft.width() * 0) / 10 + 10, (tft.height()) * 1 / 10 + 5);
      tft.print("Sending To:...");
      tft.drawRect((tft.width()*i) / 10, (tft.height() * 2) / 10, tft.width() / 10, tft.height() / 10, 0xffff);
      tft.setCursor((tft.width()*i) / 10 + 10, (tft.height()) * 1 / 5 + 5);
      tft.print(i);
    }
    //all button
    tft.drawRect((tft.width() * 0) / 10, (tft.height() * 3) / 10, tft.width() * 2 / 10, tft.height() / 10, 0xffff);
    tft.setCursor((tft.width() * 0) / 10 + 10, (tft.height()) * 3 / 10 + 5);
    tft.print("ALL");
  }
  if (midPage == 3) {
//print log
  tft.setCursor(((tft.width() * 0) / 10 )+1, (tft.height() * 1) / 10 + 5);
           tft.setTextColor(0xffff);
           tft.println("LOG:");
           tft.println(incomingRec5);
           tft.println(incomingRec4);
           tft.println(incomingRec3);
           tft.println(incomingRec2);
           tft.println(incomingRec1);
   }
   if (midPage == 4) {
       //show battery and SF
       measuredvbat = analogRead(VBATPIN);
        measuredvbat *= 2;
        // we divided by 2, so multiply back
        measuredvbat *= 3.3; // Multiply by 3.3V, our reference voltage
        measuredvbat /= 1024; // convert to voltage
        tft.setCursor(((tft.width() * 0) / 10 )+1, (tft.height() * 1) / 10 + 5);
        tft.setTextColor(0xffff);
        tft.print("VBat: " + String(measuredvbat) + "V ");
        measuredvbat = measuredvbat - 3.7;
        measuredvbat = measuredvbat * 200; 
        tft.println("("+String(measuredvbat)+ "%)");
        tft.println("Spreading Factor = " + String(SpreadingFactor));
   }
}

void drawMsg(String msg) {
  tft.fillRect((tft.width() * 0) / 10, (tft.height() * 7) / 10 -1, tft.width(), (((tft.height() * 1) / 10)) , 0x0000);
  tft.setCursor((tft.width() * 0) / 10 + 1, (tft.height() * 7) / 10 + 5);
  tft.setTextColor(0xAEDC);
  tft.print("Msg: ");
  tft.print(msg);
  tft.setTextColor(0xF81F);  //magenta
  tft.setCursor((tft.width() * 9) / 10 - 5, (tft.height() * 7) / 10 );
  tft.print("-->");
}

void drawBottomHalf(String incoming2) {
  tft.fillRect((tft.width() * 0) / 10, (tft.height() * 8) / 10, tft.width(), ((tft.height() * 2) / 10), 0x0000);
  tft.setTextSize(2);
  tft.setTextColor(0x07E0, ILI9341_BLACK);
  tft.drawFastHLine(0, (tft.height()) * 4 / 5 - 1, tft.width(), 0xffff);
  tft.setCursor(0, (tft.height()) * 4 / 5);
  tft.print(incoming2 + " " + String(LoRa.packetRssi()) + " " + String(LoRa.packetSnr()));
  Serial.println(incoming2 + " " + String(LoRa.packetRssi()) + " " + String(LoRa.packetSnr()));
 
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return
  // read packet header bytes:
   recipient = LoRa.read();          // recipient address
    sender = LoRa.read();
   incomingMsgId = LoRa.read();     // incoming msg ID
   incomingLength = LoRa.read();    // incoming msg length
   incoming2 = "";
  while (LoRa.available()) {
    incoming2 += (char)LoRa.read();
  }
  Serial.println(recipient);
  Serial.println(sender);
  Serial.println(incomingMsgId);
  Serial.println(incomingLength);

    if(recipient == localAddress || recipient == 255){
   // incoming = incoming2;
    incoming2=incoming2 + " ~" + sender;
    incomingRec5=incomingRec4;
    incomingRec4=incomingRec3;
    incomingRec3=incomingRec2;
    incomingRec2=incomingRec1;
    incomingRec1=incomingRec0;
    incomingRec0=incoming2;
    drawBottomHalf(incoming2);
  }
 
}

void sendMessage(String outgoing) {
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID
}

//if (incomingLength != incoming.length()) {   // check length for error
//  Serial.println("error: message length does not match length");
//  return;                             // skip rest of function
//}
