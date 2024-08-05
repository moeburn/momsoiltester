#include <GxEPD2_BW.h>
#include<Wire.h>

#include <WiFi.h>
#include "driver/periph_ctrl.h"
#include "nvs_flash.h"
#include <SimplePgSQL.h>
#include "time.h"
#include <ESP32Time.h>
ESP32Time rtc(0);  // offset in seconds, use 0 because NTP already offset
#include <Preferences.h>
Preferences prefs;
#include "bitmaps/Bitmaps200x200.h"

// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 1

#define sleeptimeSecs 1
#define maxArray 1500
#define controlpin 10
#define maximumReadings 60 // The maximum number of readings that can be stored in the available space
#define WIFI_TIMEOUT 12000


  float t, h;

 RTC_DATA_ATTR   int firstrun = 100;
RTC_DATA_ATTR float minVal = 3.9;
RTC_DATA_ATTR float maxVal = 4.2;
RTC_DATA_ATTR int readingCount = 0; // Counter for the number of readings

#include "bitmaps/Bitmaps128x250.h"
#include <Fonts/FreeSans12pt7b.h> 

// ESP32-C3 CS(SS)=7,SCL(SCK)=4,SDA(MOSI)=6,BUSY=3,RES(RST)=2,DC=1
//GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=5*/ SS, /*DC=*/ 1, /*RES=*/ 2, /*BUSY=*/ 3)); // DEPG0213BN 122x250, SSD1680
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=5*/ SS, /*DC=*/ 1, /*RES=*/ 2, /*BUSY=*/ 3)); // GDEH0154D67 200x200, SSD1681

#define every(interval) \
    static uint32_t __every__##interval = millis(); \
    if (millis() - __every__##interval >= interval && (__every__##interval = millis()))

void killwifi() {
            WiFi.disconnect(); 
}

typedef struct {
  float temp;
  float soil;
  unsigned long   time;
  float volts;
} sensorReadings;


float temp;





RTC_DATA_ATTR sensorReadings Readings[maximumReadings];

bool sent = false;
IPAddress PGIP(216,110,224,105);


int WiFiStatus;
WiFiClient client;


char buffer[1024];
PGconnection conn(&client, 0, 1024, buffer);

char tosend[192];
String tosendstr;


const char ssid[] = "mikesnet";      //  your network SSID (name)
const char pass[] = "springchicken";      // your network password

const char user[] = "wanburst";       // your database user
const char password[] = "elec&9";   // your database password
const char dbname[] = "blynk_reporting";         // your database name

int newVal;
float soilPct;


#ifndef USE_ARDUINO_ETHERNET
void checkConnection()
{
    int status = WiFi.status();
    if (status != WL_CONNECTED) {
        if (WiFiStatus == WL_CONNECTED) {
            Serial.println("Connection lost");
            WiFiStatus = status;
        }
    }
    else {
        if (WiFiStatus != WL_CONNECTED) {
            Serial.println("Connected");
            WiFiStatus = status;
        }
    }
}

#endif

static PROGMEM const char query_rel[] = "\
SELECT a.attname \"Column\",\
  pg_catalog.format_type(a.atttypid, a.atttypmod) \"Type\",\
  case when a.attnotnull then 'not null ' else 'null' end as \"null\",\
  (SELECT substring(pg_catalog.pg_get_expr(d.adbin, d.adrelid) for 128)\
   FROM pg_catalog.pg_attrdef d\
   WHERE d.adrelid = a.attrelid AND d.adnum = a.attnum AND a.atthasdef) \"Extras\"\
 FROM pg_catalog.pg_attribute a, pg_catalog.pg_class c\
 WHERE a.attrelid = c.oid AND c.relkind = 'r' AND\
 c.relname = %s AND\
 pg_catalog.pg_table_is_visible(c.oid)\
 AND a.attnum > 0 AND NOT a.attisdropped\
    ORDER BY a.attnum";

static PROGMEM const char query_tables[] = "\
SELECT n.nspname as \"Schema\",\
  c.relname as \"Name\",\
  CASE c.relkind WHEN 'r' THEN 'table' WHEN 'v' THEN 'view' WHEN 'm' THEN 'materialized view' WHEN 'i' THEN 'index' WHEN 'S' THEN 'sequence' WHEN 's' THEN 'special' WHEN 'f' THEN 'foreign table' END as \"Type\",\
  pg_catalog.pg_get_userbyid(c.relowner) as \"Owner\"\
 FROM pg_catalog.pg_class c\
     LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace\
 WHERE c.relkind IN ('r','v','m','S','f','')\
      AND n.nspname <> 'pg_catalog'\
      AND n.nspname <> 'information_schema'\
      AND n.nspname !~ '^pg_toast'\
  AND pg_catalog.pg_table_is_visible(c.oid)\
 ORDER BY 1,2";

int pg_status = 0;

int i;

void doPg(void)
{
    char *msg;
    int rc;
    if (!pg_status) {
        conn.setDbLogin(PGIP,
            user,
            password,
            dbname,
            "utf8");
        pg_status = 1;
        return;
    }

    if (pg_status == 1) {
        rc = conn.status();
        if (rc == CONNECTION_BAD || rc == CONNECTION_NEEDED) {
            char *c=conn.getMessage();
            if (c) Serial.println(c);
            pg_status = -1;
        }
        else if (rc == CONNECTION_OK) {
            pg_status = 2;
            Serial.println("Enter query");
        }
        return;
    }
    if (pg_status == 2) {
        if (!Serial.available()) return;
        char inbuf[192];
        int n = Serial.readBytesUntil('\n',inbuf,191);
        while (n > 0) {
            if (isspace(inbuf[n-1])) n--;
            else break;
        }
        inbuf[n] = 0;

        if (!strcmp(inbuf,"\\d")) {
            if (conn.execute(query_tables, true)) goto error;
            Serial.println("Working...");
            pg_status = 3;
            return;
        }
        if (!strncmp(inbuf,"\\d",2) && isspace(inbuf[2])) {
            char *c=inbuf+3;
            while (*c && isspace(*c)) c++;
            if (!*c) {
                if (conn.execute(query_tables, true)) goto error;
                Serial.println("Working...");
                pg_status = 3;
                return;
            }
            if (conn.executeFormat(true, query_rel, c)) goto error;
            Serial.println("Working...");
            pg_status = 3;
            return;
        }

        if (!strncmp(inbuf,"exit",4)) {
            conn.close();
            Serial.println("Thank you");
            pg_status = -1;
            return;
        }
        if (conn.execute(inbuf)) goto error;
        Serial.println("Working...");
        pg_status = 3;
    }
    if (pg_status == 3) {
        rc=conn.getData();
        if (rc < 0) goto error;
        if (!rc) return;
        if (rc & PG_RSTAT_HAVE_COLUMNS) {
            for (i=0; i < conn.nfields(); i++) {
                if (i) Serial.print(" | ");
                Serial.print(conn.getColumn(i));
            }
            Serial.println("\n==========");
        }
        else if (rc & PG_RSTAT_HAVE_ROW) {
            for (i=0; i < conn.nfields(); i++) {
                if (i) Serial.print(" | ");
                msg = conn.getValue(i);
                if (!msg) msg=(char *)"NULL";
                Serial.print(msg);
            }
            Serial.println();
        }
        else if (rc & PG_RSTAT_HAVE_SUMMARY) {
            Serial.print("Rows affected: ");
            Serial.println(conn.ntuples());
        }
        else if (rc & PG_RSTAT_HAVE_MESSAGE) {
            msg = conn.getMessage();
            if (msg) Serial.println(msg);
        }
        if (rc & PG_RSTAT_READY) {
            pg_status = 2;
            Serial.println("Enter query");
        }
    }
    return;
error:
    msg = conn.getMessage();
    if (msg) Serial.println(msg);
    else Serial.println("UNKNOWN ERROR");
    if (conn.status() == CONNECTION_BAD) {
        Serial.println("Connection is bad");
        pg_status = -1;
    }
}


void transmitReadings() {
  i=0;
          while (i<maximumReadings) {
            //if (WiFi.status() == WL_CONNECTED) {
            doPg();


            if ((pg_status == 2) && (i<maximumReadings)){
              tosendstr = "insert into burst values (51,1," + String(Readings[i].time) + "," + String(Readings[i].temp,1) + "), (51,2," + String(Readings[i].time) + "," + String(Readings[i].volts,3) + "), (51,3," + String(Readings[i].time) + "," + String(Readings[i].soil,3) + ")";
              conn.execute(tosendstr.c_str());
              pg_status = 3;
              delay(10);
              i++;
            }
            delay(10);
            
          }
          
}

void gotosleep() {
      //WiFi.disconnect();
      display.hibernate();
      SPI.end();
      Wire.end();
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
      esp_sleep_enable_timer_wakeup(sleeptimeSecs * 1000000);
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
      display.fillRect(0,0,display.width(),display.height(),GxEPD_BLACK);
    } while (display.nextPage());
    delay(10);
    display.firstPage();
    do {
      display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);
    } while (display.nextPage());
    
    
    display.firstPage();
    do {
        display.fillRect(0,0,display.width(),display.height(),GxEPD_WHITE);
  // Draw the needle, with radius shrunk by 10% and twice as thick
// Draw the needle, with radius shrunk by 10% and twice as thick
int needleAngle = map(soilPct, 0, 100, 240, -60); // Adjusted the angle mapping
float needleRad = radians(needleAngle);
int needleX = 100 + cos(needleRad) * 63; // 70 * 0.9 = 63
int needleY = 105 - sin(needleRad) * 63;

// Draw the thick needle using a filled rectangle to cover both vertical and horizontal cases
int thickness = 3; // Thickness of the needle

for (int i = -thickness; i <= thickness; i++) {
  int offsetX = i * sin(needleRad);
  int offsetY = i * cos(needleRad);
  display.drawLine(100 + offsetX, 105 - offsetY, needleX + offsetX, needleY - offsetY, GxEPD_BLACK);
}

  display.fillCircle(100, 104, 7, GxEPD_WHITE);
  // Display the soilPct value
  display.setCursor(90, 190); // Adjusted for smaller size
  display.print(soilPct, 0);
  display.drawInvertedBitmap(0, 0, momsbackdropmom, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);          
        
    } while (display.nextPage());

    display.setFullWindow();
}

long mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setup()
{
  temp = temperatureRead();
  pinMode(controlpin, OUTPUT);
  digitalWrite(controlpin, HIGH);
  delay(10);
  Wire.begin();  
  newVal = analogRead(0); // dry = 3750, wet = 1550
  soilPct = map(newVal, 1550, 3750, 100, 0);
  if (soilPct < 0) {soilPct = 0;}
  if (soilPct > 100) {soilPct = 100;}

  delay(10);
  display.init(115200, false, 10, false); // void init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration = 10, bool pulldown_rst_mode = false)
        display.setRotation(1);
            display.setFont(&FreeSans12pt7b);
            //if (firstrun == 0) {display.clearScreen();
            //firstrun++;
            //if (firstrun > 99) {firstrun = 0;}
           // }
  display.setTextColor(GxEPD_BLACK);
  
  doDisplay();
  gotosleep();

}

void loop()
{
    //newVal = analogRead(0);

    doDisplay();
    gotosleep();
    // Add a delay to sample data at intervals (e.g., every minute)
    //delay(1000); // 1 minute delay, adjust as needed
}
