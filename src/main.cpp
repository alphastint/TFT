/*******************************************************************************
 ******************************************************************************/
#include <Arduino_GFX_Library.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <regex>
#include <string>
#include <vector>
#include <sstream>

#define GFX_BL DF_GFX_BL // default backlight pin, you may replace DF_GFX_BL to actual backlight pin
#define TFT_BL 27
/* More dev device declaration: https://github.com/moononournation/Arduino_GFX/wiki/Dev-Device-Declaration */
#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else /* !defined(DISPLAY_DEV_KIT) */

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
//Arduino_DataBus *bus = create_default_Arduino_DataBus();

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
//Arduino_GFX *gfx = new Arduino_ILI9341(bus, DF_GFX_RST, 0 /* rotation */, false /* IPS */);

Arduino_DataBus *bus = new Arduino_ESP32SPI(2 /* DC */, 15 /* CS */, 14 /* SCK */, 13 /* MOSI */, GFX_NOT_DEFINED /* MISO */);
Arduino_GFX *gfx = new Arduino_ST7789(bus, -1 /* RST */, 1 /* rotation */, true /* IPS */);

#endif /* !defined(DISPLAY_DEV_KIT) */
/*******************************************************************************
 * End of Arduino_GFX setting
 ******************************************************************************/

#include "BmpClass.h"
static BmpClass bmpClass;

// pixel drawing callback
static void bmpDrawCallback(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h)
{
  // Serial.printf("Draw pos = %d, %d. size = %d x %d\n", x, y, w, h);
  gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
}

void ShowImage(String path) {
  unsigned long start = millis();

  // File bmpFile = SD.open("/octocatL.bmp");
  Serial.printf("Opening File: %s\n", path.c_str());
  File bmpFile = SD.open(path);
  if(!bmpFile){
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.printf("File Size: %d\n",bmpFile.size());


  // read BMP file header
  bmpClass.draw(
      &bmpFile, bmpDrawCallback, false /* useBigEndian */,
      0 /* x */, 0 /* y */, gfx->width() /* widthLimit */, gfx->height() /* heightLimit */);

  bmpFile.close();

  Serial.print("Time used: ");
  Serial.println(millis() - start);
}

void setup()
{
  Serial.begin(19200);
  while (!Serial);


  gfx->begin();
  gfx->fillScreen(WHITE);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  ledcSetup(0, 2000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 255); /* Screen brightness can be modified by adjusting this parameter. (0-255) */
#endif

  gfx->setCursor(10, 10);
  gfx->setTextColor(RED);
  gfx->println("Hello World!");

  Serial.println("BMP Image Viewer");
  delay(5000); // 5 seconds
  
  // Init Display
  gfx->begin();
  gfx->fillScreen(WHITE);

  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
#endif

   ShowImage("/Foxconn_logo.bmp");
}

std::vector<std::string> Split(std::string input) {
  std::vector<std::string> result;
  std::istringstream istream (input);
  std::string token;    
  while (getline(istream, token, ',')) {
      result.push_back(token);
  }

  return result;
}

int64_t to_int(std::string str) {
  std::istringstream istream(str);
  int64_t result;
  istream >> result;
  return result;
}

void ClearScreen() {
  gfx->fillScreen(BLACK);
}

void BackGroundOn() {
  Serial.println("BackGroundOn");
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  ledcSetup(0, 2000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 255); /* Screen brightness can be modified by adjusting this parameter. (0-255) */
}

void BackGroundOff() {
  Serial.println("BackGroundOff");
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, LOW);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  ledcSetup(0, 2000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 0); /* Screen brightness can be modified by adjusting this parameter. (0-255) */
}

void TextOut(String cmd) {
  Serial.println("TextOut");
  auto tokens = Split(cmd.c_str());
  int64_t xcoord = to_int(tokens.at(1));
  int64_t ycoord = to_int(tokens.at(2));
  std::string msg = tokens.at(3);

  gfx->setCursor(xcoord, ycoord);
  gfx->setTextColor(RED);
  gfx->setTextSize(3 /* x scale */, 3 /* y scale */, 0 /* pixel_margin */);
  gfx->println(msg.c_str());
}

void ImgOut(String cmd) {
  Serial.println("ImgOut");
  auto tokens = Split(cmd.c_str());

  String fileName = "/" + String(tokens.at(1).c_str());
  if (SD.exists(fileName)) {
    ShowImage(fileName);
  } else {
    Serial.printf("Error: No file Named `%s'\n", fileName.c_str());
  }
}

String EvaluateCommand(String cmd) {
  const std::regex ClearRGX{"<CLR>"};
  const std::regex BkgOnRGX{"<BKG,ON>"};
  const std::regex BkgOffRGX{"<BKG,OFF>"};
  const std::regex TextOutRGX{"<OUT,[0-9]+,[0-9]+,[^>,]*>"};
  const std::regex PicOutRGX{"<PIC,[^>,]+>"};

  String eval;
  String rest;
  auto end = cmd.indexOf('\n');
  if (end==-1) {
    return cmd;
  } 
  else 
  {
    eval = cmd.substring(0, end);
    // Serial.printf("eval='%s`\n", eval.c_str());
    rest = cmd.substring(end+1,cmd.length());
    // Serial.printf("rest='%s`\n", rest.c_str());
  }
  
  std::cmatch match;
  if (std::regex_match(eval.c_str(), match, ClearRGX)) {
    ClearScreen();
  } else if (std::regex_match(eval.c_str(), match, BkgOnRGX)) {
    BackGroundOn();
  } else if (std::regex_match(eval.c_str(), match, BkgOffRGX)) {
    BackGroundOff();
  } else if (std::regex_match(eval.c_str(), match, TextOutRGX)) {
    TextOut(eval.substring(1,eval.length()-1));
  } else if (std::regex_match(eval.c_str(), match, PicOutRGX)) {
    ImgOut(eval.substring(1,eval.length()-1));
  } else {
    Serial.printf("Unkown command `%s'. Ignoring\n", eval.c_str());
  }

  return rest;
}

void loop()
{
  static String candidate;
  
  while(Serial.available()) {

    String tmp = Serial.readString();// read the incoming data as string
    candidate.concat(tmp);
    candidate.replace("\r", "");

    // Serial.printf("command received: %s\n", candidate.c_str());

    candidate = EvaluateCommand(candidate);
  }
}
