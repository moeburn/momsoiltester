#include <GxEPD2_BW.h>
#include<Wire.h>

#include <WiFi.h>
#include "driver/periph_ctrl.h"

#include "bitmaps/Bitmaps200x200.h"

// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 1

#define sleeptimeSecs 3600
#define maxArray 750
#define controlpin 10
#define DRY 3562 // dry = 3750, wet = 1550
#define WET 2000
#define switch1pin 8
#define switch2pin 2

int GxEPD_BLACK1   = 0;
int GxEPD_WHITE1   = 65535;
int newVal;

  float t, h;
RTC_DATA_ATTR float soil0[maxArray];
RTC_DATA_ATTR float volts0[maxArray];
 RTC_DATA_ATTR   int firstrun = 0;

RTC_DATA_ATTR int readingCount = 0; // Counter for the number of readings

#include "bitmaps/Bitmaps128x250.h"
#include <Fonts/FreeSans12pt7b.h> 
#include <Fonts/Roboto_Condensed_12.h>

// ESP32-C3 CS(SS)=7,SCL(SCK)=4,SDA(MOSI)=6,BUSY=3,RES(RST)=2,DC=1
//GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=5*/ SS, /*DC=*/ 1, /*RES=*/ 2, /*BUSY=*/ 3)); // DEPG0213BN 122x250, SSD1680
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=5*/ SS, /*DC=*/ 20, /*RES=*/ 21, /*BUSY=*/ 3)); // GDEH0154D67 200x200, SSD1681

#define every(interval) \
    static uint32_t __every__##interval = millis(); \
    if (millis() - __every__##interval >= interval && (__every__##interval = millis()))

void killwifi() {
            WiFi.disconnect(); 
}




float temp, volts, batwidth, soilPct;
bool switch1 = false;
bool switch2 = false;






void gotosleep(int secondsToSleep) {
      //WiFi.disconnect();
      display.hibernate();
      SPI.end();
      //Wire.end();
      pinMode(SS, INPUT_PULLUP );
      pinMode(6, INPUT_PULLUP );
      pinMode(4, INPUT_PULLUP );
      pinMode(8, INPUT_PULLUP );
      pinMode(9, INPUT_PULLUP );
      pinMode(1, INPUT_PULLUP );
      pinMode(2, INPUT_PULLUP );
      pinMode(3, INPUT_PULLUP );
      pinMode(controlpin, INPUT);

      //delay(10000);
      //rtc_gpio_isolate(gpio_num_t(SDA));
      //rtc_gpio_isolate(gpio_num_t(SCL));
      //periph_module_disable(PERIPH_I2C0_MODULE);  
      //digitalWrite(SDA, 0);
      //digitalWrite(SCL, 0);
      esp_deep_sleep_enable_gpio_wakeup(1 << 5, ESP_GPIO_WAKEUP_GPIO_LOW);
      esp_sleep_enable_timer_wakeup(secondsToSleep * 1000000ULL);
      delay(1);
      esp_deep_sleep_start();
      //esp_light_sleep_start();
      delay(1000);
}



void doDisplay() {
     //newVal = ads.computeVolts(ads.readADC_SingleEnded(0)) * 2.0;
    
    display.setPartialWindow(0, 0, display.width(), display.height());

    display.firstPage();
    do {
      display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);
    } while (display.nextPage());
    delay(10);
    display.firstPage();
    do {
      display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);
    } while (display.nextPage());
    
    
    display.firstPage();
    do {
        display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE1);


    // Draw the needle, with radius shrunk by 10% and twice as thick
  int centerX = 100;
  int centerY = 105;
  int radius = 63; // Shrunk radius by 10%
  int circleRadius = 9; // Radius of the circle in the middle
  int thickness = 3; // Thickness of the needle

  // Calculate the angle for the needle
  float needleAngle = mapf(soilPct, 0, 10, 240, -60);
  float needleRad = radians(needleAngle);

  // Calculate the end point of the needle
  float needleX = centerX + cos(needleRad) * radius;
  float needleY = centerY - sin(needleRad) * radius;

  // Calculate the start points on either side of the circle
  float startX1 = centerX + cos(needleRad + PI / 2) * circleRadius;
  float startY1 = centerY - sin(needleRad + PI / 2) * circleRadius;
  float startX2 = centerX + cos(needleRad - PI / 2) * circleRadius;
  float startY2 = centerY - sin(needleRad - PI / 2) * circleRadius;

  // Draw the thick needle using multiple parallel lines
  for (int i = -thickness; i <= thickness; i++) {
    int offsetX1 = i * cos(needleRad + PI / 2);
    int offsetY1 = i * sin(needleRad + PI / 2);
    int offsetX2 = i * cos(needleRad - PI / 2);
    int offsetY2 = i * sin(needleRad - PI / 2);
    display.drawLine(startX1 + offsetX1, startY1 + offsetY1, needleX + offsetX1, needleY + offsetY1, GxEPD_BLACK1);
    display.drawLine(startX2 + offsetX2, startY2 + offsetY2, needleX + offsetX2, needleY + offsetY2, GxEPD_BLACK1);
  }



  //display.setCursor(70, 170); // Adjusted for smaller size
  //display.print(newVal);
  //display.setCursor(70, 185); // Adjusted for smaller size
  //display.print(soilPct, 2);

  display.drawInvertedBitmap(0, 0, momsbackdropmom, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK1);   
  display.fillCircle(100, 104, 7, GxEPD_BLACK1);       
  display.fillRect(90, 190, batwidth, 7, GxEPD_BLACK1);
        
    } while (display.nextPage());

    display.setFullWindow();
}

void doChart() {
       //newVal = ads.computeVolts(ads.readADC_SingleEnded(0)) * 2.0;
    
    // Shift the previous data points


    // Recalculate min and max values
    float minVal = soil0[maxArray - readingCount];
    float maxVal = soil0[maxArray - readingCount];

    for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
        if ((soil0[i] < minVal) && (soil0[i] > 0)) {
            minVal = soil0[i];
        }
        if (soil0[i] > maxVal) {
            maxVal = soil0[i];
        }
    }

    // Calculate scaling factors
    float yScale = 199.0 / (maxVal - minVal);
    float xStep = 199.0 / (readingCount - 1);

    // Draw the line chart
   // display.firstPage();
  //  do {
   // display.fillScreen(GxEPD_WHITE);
  //  } while (display.nextPage());
    display.setPartialWindow(0, 0, display.width(), display.height());

    display.firstPage();
    do {
      display.fillRect(0,0,display.width(),display.height(),GxEPD_BLACK);
    } while (display.nextPage());
    delay(10);
    display.firstPage();
    do {
      display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);
    } while (display.nextPage());
    
    
    display.firstPage();
    do {
        display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE1);
        display.setCursor(0, 9);
        display.print(maxVal, 3);
        display.setCursor(0, 190);
        display.print(minVal, 3);
        display.setCursor(100, 190);
        display.print(">");
        display.print(soilPct, 2);
        display.print("<");
        display.setCursor(100, 9);
        display.print("#");
        display.print(readingCount);
        display.print(",");
        display.print(newVal);
        
        for (int i = maxArray - (readingCount); i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 199 - ((soil0[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 199 - ((soil0[i + 1] - minVal) * yScale);
            //if (soil0[i] > 0) {
                display.drawLine(x0, y0, x1, y1, GxEPD_BLACK1);
            //}
        }
    } while (display.nextPage());

    display.setFullWindow();
}

void doBatChart() {
       //newVal = ads.computeVolts(ads.readADC_SingleEnded(0)) * 2.0;
    
    // Shift the previous data points


    // Recalculate min and max values
    float minVal = volts0[maxArray - readingCount];
    float maxVal = volts0[maxArray - readingCount];

    for (int i = maxArray - readingCount + 1; i < maxArray; i++) {
        if ((volts0[i] < minVal) && (volts0[i] > 0)) {
            minVal = volts0[i];
        }
        if (volts0[i] > maxVal) {
            maxVal = volts0[i];
        }
    }

    // Calculate scaling factors
    float yScale = 199.0 / (maxVal - minVal);
    float xStep = 199.0 / (readingCount - 1);

    // Draw the line chart
   // display.firstPage();
  //  do {
   // display.fillScreen(GxEPD_WHITE);
  //  } while (display.nextPage());
    display.setPartialWindow(0, 0, display.width(), display.height());

    display.firstPage();
    do {
      display.fillRect(0,0,display.width(),display.height(),GxEPD_BLACK);
    } while (display.nextPage());
    delay(10);
    display.firstPage();
    do {
      display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);
    } while (display.nextPage());
    
    
    display.firstPage();
    do {
        display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE1);
        display.setCursor(0, 9);
        display.print(maxVal, 3);
        display.setCursor(0, 190);
        display.print(minVal, 3);
        display.setCursor(100, 190);
        display.print(">");
        display.print(volts, 2);
        display.print("v");
        display.setCursor(100, 9);
        display.print("#");
        display.print(readingCount);
        
        for (int i = maxArray - (readingCount); i < (maxArray - 1); i++) {
            int x0 = (i - (maxArray - readingCount)) * xStep;
            int y0 = 199 - ((volts0[i] - minVal) * yScale);
            int x1 = (i + 1 - (maxArray - readingCount)) * xStep;
            int y1 = 199 - ((volts0[i + 1] - minVal) * yScale);
            //if (soil0[i] > 0) {
                display.drawLine(x0, y0, x1, y1, GxEPD_BLACK1);
            //}
        }
    } while (display.nextPage());

    display.setFullWindow();
}


double mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setup()
{
  temp = temperatureRead();
  volts = analogReadMilliVolts(1) / 500.0;
  
  
  pinMode(switch1pin, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(switch2pin, INPUT_PULLUP);
  /*if (digitalRead(switch2pin) == LOW){switch1 = true;}
  if (digitalRead(switch1pin) == LOW){
    switch2 = true;
    GxEPD_BLACK1  =   65535;
    GxEPD_WHITE1  =   0000;
  }*/
  
  batwidth = mapf(volts,3.6,4.0,1,18);
  if (batwidth > 18) {batwidth = 18;}
  if (batwidth < 0) {batwidth = 0;}
  pinMode(controlpin, OUTPUT);
  digitalWrite(controlpin, HIGH);
  delay(100);
  newVal = analogRead(0);
  //Wire.begin();  
   // dry = 3750, wet = 1550
  soilPct = mapf(newVal, DRY, WET, 0, 10);
  if (soilPct < 0) {soilPct = 0;}
  if (soilPct > 10) {soilPct = 10;}

   if (firstrun > 1) {
    for (int i = 0; i < (maxArray - 1); i++) { //add to array for chart drawing
        soil0[i] = soil0[i + 1];
        volts0[i] = volts0[i + 1];
    }
    volts0[(maxArray - 1)] = (volts);
    soil0[(maxArray - 1)] = (soilPct);
   }

  // Increase the reading count up to maxArray
  if (readingCount < maxArray) {
      readingCount++;
  }



  delay(10);
  display.init(115200, false, 10, false); // void init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration = 10, bool pulldown_rst_mode = false)
  display.setRotation(0);
  display.setFont(&Roboto_Condensed_12);
  display.setTextColor(GxEPD_BLACK1);
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
    display.clearScreen();
    doChart();
    esp_sleep_enable_timer_wakeup(10000000); //10 seconds
    esp_sleep_enable_gpio_wakeup();
    esp_light_sleep_start();
    doBatChart();
    gotosleep(10);
  }
            
  if (firstrun == 2) {
    display.clearScreen();
  }
  firstrun++;
  if (firstrun > 99) {firstrun = 2;}
  doDisplay();
  gotosleep(sleeptimeSecs);

}

void loop()
{
    //newVal = analogRead(0);

    doDisplay();
    gotosleep(sleeptimeSecs);
    // Add a delay to sample data at intervals (e.g., every minute)
    //delay(1000); // 1 minute delay, adjust as needed
}
