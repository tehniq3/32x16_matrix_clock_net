/*
 * (c)20016-18 Pawel A. Hernik
 * 
  ESP-01 pinout from top:

  GND    GP2 GP0 RX/GP3
  TX/GP1 CH  RST VCC

  MAX7219 5 wires to ESP-1 from pin side:
  Re Br Or Ye
  Gr -- -- --
  capacitor between VCC and GND
  resistor 1k between CH and VCC

  USB to Serial programming
  ESP-1 from pin side:
  FF(GP0) to GND, RR(CH_PD) to GND before upload
  Gr FF -- Bl
  Wh -- RR Vi

  ------------------------
  NodeMCU 1.0/D1 mini pinout:

  D8 - DataIn
  D7 - LOAD/CS
  D6 - CLK

*/

#include <ESP8266WiFi.h>

#define NUM_MAX 8
#define LINE_WIDTH 32
#define ROTATE  90

// for ESP-01 module
#define DIN_PIN 2 // D4
#define CS_PIN  3 // D9/RX
#define CLK_PIN 0 // D3

// for NodeMCU 1.0/D1 mini
//#define DIN_PIN 15  // D8
//#define CS_PIN  13  // D7
//#define CLK_PIN 12  // D6

#define DEBUG(x)
//#define DEBUG(x) x

#include "max7219.h"
#include "fonts.h"

// =======================================================================
// Your config below!
// =======================================================================
const char* ssid     = "yourssid";     // SSID of local network
const char* password = "yourpasswd";   // Password on network
long utcOffset = 1;                    // UTC for Warsaw,Poland
// =======================================================================

int h, m, s, day, month, year, dayOfWeek;
int summerTime = 0; // calculated in code from date
long localEpoc = 0;
long localMillisAtUpdate = 0;
String date;
String buf="";

int xPos=0, yPos=0;
int clockOnly = 0;

void setup() 
{
  buf.reserve(500);
  Serial.begin(115200);
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN, 1);
  sendCmdAll(CMD_INTENSITY, 0);
  DEBUG(Serial.print("Connecting to WiFi ");)
  WiFi.begin(ssid, password);
  clr();
  xPos=0;
  printString("CONNECT..", font3x7);
  refreshAll();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); DEBUG(Serial.print("."));
  }
  clr();
  xPos=0;
  DEBUG(Serial.println(""); Serial.print("MyIP: "); Serial.println(WiFi.localIP());)
  printString((WiFi.localIP().toString()).c_str(), font3x7);
  refreshAll();
  getTime();
}

// =======================================================================

unsigned int curTime,updTime=0;
int dots,mode;

void loop()
{
  curTime = millis();
  if(curTime-updTime>600000) {
    updTime = curTime;
    getTime();  // update time every 600s=10m
  }
  dots = (curTime % 1000)<500;     // blink 2 times/sec
  mode = (curTime % 60000)/20000;  // change mode every 20s
  updateTime();
  if(mode==0) drawTime0(); else
  if(mode==1) drawTime1(); else drawTime2();
  refreshAll();
  delay(100);
}

// =======================================================================

//char* monthNames[] = {"STY","LUT","MAR","KWI","MAJ","CZE","LIP","SIE","WRZ","PAZ","LIS","GRU"};
char* monthNames[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
char txt[30];

void drawTime0()
{
  clr();
  yPos = 0;
  xPos = (h>9) ? 0 : 2;
  sprintf(txt,"%d",h);
  printString(txt, digits7x16);
  if(dots) printCharX(':', digits5x16rn, xPos);
  xPos+=(h>=22 || h==20)?1:2;
  sprintf(txt,"%02d",m);
  printString(txt, digits7x16);
}

void drawTime1()
{
  clr();
  yPos = 0;
  xPos = (h>9) ? 0 : 3;
  sprintf(txt,"%d",h);
  printString(txt, digits5x16rn);
  if(dots) printCharX(':', digits5x16rn, xPos);
  xPos+=(h>=22 || h==20)?1:2;
  sprintf(txt,"%02d",m);
  printString(txt, digits5x16rn);
  sprintf(txt,"%02d",s);
  printString(txt, font3x7);
}

void drawTime2()
{
  clr();
  yPos = 0;
  xPos = (h>9) ? 0 : 2;
  sprintf(txt,"%d",h);
  printString(txt, digits5x8rn);
  if(dots) printCharX(':', digits5x8rn, xPos);
  xPos+=(h>=22 || h==20)?1:2;
  sprintf(txt,"%02d",m);
  printString(txt, digits5x8rn);
  sprintf(txt,"%02d",s);
  printString(txt, digits3x5);
  yPos = 1;
  xPos = 1;
  sprintf(txt,"%d&%s&%d",day,monthNames[month-1],year-2000);
  printString(txt, font3x7);
  for(int i=0;i<LINE_WIDTH;i++) scr[LINE_WIDTH+i]<<=1;
}

// =======================================================================

int charWidth(char c, const uint8_t *font)
{
  int fwd = pgm_read_byte(font);
  int fht = pgm_read_byte(font+1);
  int offs = pgm_read_byte(font+2);
  int last = pgm_read_byte(font+3);
  if(c<offs || c>last) return 0;
  c -= offs;
  int len = pgm_read_byte(font+4);
  return pgm_read_byte(font + 5 + c * len);
}

// =======================================================================

int stringWidth(const char *s, const uint8_t *font)
{
  int wd=0;
  while(*s) wd += 1+charWidth(*s++, font);
  return wd-1;
}

// =======================================================================

int stringWidth(String str, const uint8_t *font)
{
  return stringWidth(str.c_str(), font);
}

// =======================================================================

int printCharX(char ch, const uint8_t *font, int x)
{
  int fwd = pgm_read_byte(font);
  int fht = pgm_read_byte(font+1);
  int offs = pgm_read_byte(font+2);
  int last = pgm_read_byte(font+3);
  if(ch<offs || ch>last) return 0;
  ch -= offs;
  int fht8 = (fht+7)/8;
  font+=4+ch*(fht8*fwd+1);
  int j,i,w = pgm_read_byte(font);
  for(j = 0; j < fht8; j++) {
    for(i = 0; i < w; i++) scr[x+LINE_WIDTH*(j+yPos)+i] = pgm_read_byte(font+1+fht8*i+j);
    if(x+i<LINE_WIDTH) scr[x+LINE_WIDTH*(j+yPos)+i]=0;
  }
  return w;
}

// =======================================================================

void printChar(unsigned char c, const uint8_t *font)
{
  if(xPos>NUM_MAX*8) return;
  int w = printCharX(c, font, xPos);
  xPos+=w+1;
}

// =======================================================================

void printString(const char *s, const uint8_t *font)
{
  while(*s) printChar(*s++, font);
  //refreshAll();
}

void printString(String str, const uint8_t *font)
{
  printString(str.c_str(), font);
}

// =======================================================================

void getTime()
{
  WiFiClient client;
  DEBUG(Serial.print("connecting to www.google.com ...");)
  if(!client.connect("www.google.com", 80)) {
    DEBUG(Serial.println("connection failed");)
    return;
  }
  client.print(String("GET / HTTP/1.1\r\n") +
               String("Host: www.google.com\r\n") +
               String("Connection: close\r\n\r\n"));

  int repeatCounter = 10;
  while (!client.available() && repeatCounter--) {
    delay(200); DEBUG(Serial.println("y."));
  }

  String line;
  client.setNoDelay(false);
  int dateFound = 0;
  while(client.connected() && client.available() && !dateFound) {
    line = client.readStringUntil('\n');
    line.toUpperCase();
    // Date: Thu, 19 Nov 2015 20:25:40 GMT
    if(line.startsWith("DATE: ")) {
      localMillisAtUpdate = millis();
      dateFound = 1;
      date = line.substring(6, 22);
      date.toUpperCase();
      decodeDate(date);
      //Serial.println(line);
      h = line.substring(23, 25).toInt();
      m = line.substring(26, 28).toInt();
      s = line.substring(29, 31).toInt();
      summerTime = checkSummerTime();
      if(h+utcOffset+summerTime>23) {
        if(++day>31) { day=1; month++; };  // needs better patch
        if(++dayOfWeek>7) dayOfWeek=1; 
      }
      DEBUG(Serial.println(String(h) + ":" + String(m) + ":" + String(s)+"   Date: "+day+"."+month+"."+year+" ["+dayOfWeek+"] "+(utcOffset+summerTime)+"h");)
      localEpoc = h * 60 * 60 + m * 60 + s;
    }
  }
  client.stop();
}

// =======================================================================
// MON, TUE, WED, THU, FRI, SAT, SUN
// JAN FEB MAR APR MAY JUN JUL AUG SEP OCT NOV DEC
// Thu, 19 Nov 2015
// decodes: day, month(1..12), dayOfWeek(1-Mon,7-Sun), year
void decodeDate(String date)
{
  switch(date.charAt(0)) {
    case 'M': dayOfWeek=1; break;
    case 'T': dayOfWeek=(date.charAt(1)=='U')?2:4; break;
    case 'W': dayOfWeek=3; break;
    case 'F': dayOfWeek=5; break;
    case 'S': dayOfWeek=(date.charAt(1)=='A')?6:7; break;
  }
  int midx = 6;
  if(isdigit(date.charAt(midx))) midx++;
  midx++;
  switch(date.charAt(midx)) {
    case 'F': month = 2; break;
    case 'M': month = (date.charAt(midx+2)=='R') ? 3 : 5; break;
    case 'A': month = (date.charAt(midx+1)=='P') ? 4 : 8; break;
    case 'J': month = (date.charAt(midx+1)=='A') ? 1 : ((date.charAt(midx+2)=='N') ? 6 : 7); break;
    case 'S': month = 9; break;
    case 'O': month = 10; break;
    case 'N': month = 11; break;
    case 'D': month = 12; break;
  }
  day = date.substring(5, midx-1).toInt();
  year = date.substring(midx+4, midx+9).toInt();
  return;
}

// =======================================================================
// https://en.wikipedia.org/wiki/Summer_Time_in_Europe
// from
// Sunday (31 − ((((5 × y) ÷ 4) + 4) mod 7)) March at 01:00 UTC
// to
// Sunday (31 − ((((5 × y) ÷ 4) + 1) mod 7)) October at 01:00 UTC

int checkSummerTime()
{
  if(month>3 && month<10) return 1;
  if(month==3 && day>=31-(((5*year/4)+4)%7) ) return 1;
  if(month==10 && day<31-(((5*year/4)+1)%7) ) return 1;
  return 0;
}
// =======================================================================

void updateTime()
{
  long curEpoch = localEpoc + ((millis() - localMillisAtUpdate) / 1000);
  long epoch = round(curEpoch + 3600 * (utcOffset+summerTime) + 86400L) % 86400L;
  h = ((epoch  % 86400L) / 3600) % 24;
  m = (epoch % 3600) / 60;
  s = epoch % 60;
}

// =======================================================================

