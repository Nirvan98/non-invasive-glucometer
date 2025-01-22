#include <MAX30105.h>
#include <spo2_algorithm.h>
#include <U8glib.h>

/*
   Hardware Connections (Breakoutboard to Arduino):
  -5V = 5V (3.3V is allowed)
  -GND = GND
  -SDA = A4 (or SDA)
  -SCL = A5 (or SCL)
  -INT = Not connected
*/
#include <ESP8266WiFi.h> //wifi library for esp
#include <WiFiClientSecure.h> // Secure Wifi Client
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <U8g2lib.h>
#include <U8x8lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0,/*clock=*/14,/*data=*/12,U8X8_PIN_NONE);
MAX30105 particleSensor;


//----------Blood Glucose Variable----------
float A=0,beer=0,diff;
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
uint32_t un_ir_mean; //Mean infrared value
#define FreqS 25    //sampling frequency
#define BUFFER_SIZE (FreqS * 4) 
int32_t n_th1, n_npks,k=0,samp=0;
float final_res=0.0;   
int32_t an_ir_valley_locs[15] ;
int32_t n_peak_interval_sum;
float irmax=0.0,irmin=9999999.0;
//-----------------------------------------
//------------Button Variables-----------------
int btn=D7;
int B=0;

//----Host & httpsPort------------
const char* host = "script.google.com";
const int httpsPort = 443;
//----------------------------------------
WiFiClientSecure client; //Create a WiFiClientSecure object.

String GAS_ID = "AKfycbz_xrRSZwyxTIbXKWGAJZ68x2BfWskt8GZ7EiXwdSFUEPwjL_6cvYUh3eNZvRu0Vor5"; //Spreadsheet script ID

void setup()
{
  Serial.begin(115200);
  Serial.println("Initializing...");
  WiFi.begin("Nirvan", "nirvan7606003");//wifi conncetion 
  
  //oled display
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_smart_patrol_nbp_tf);
   u8g2.setCursor(0,10);
  u8g2.print("Booting....");
  u8g2.sendBuffer();
  pinMode(btn, INPUT);
  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(30); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED

}
void wifistat()
{
  if(WiFi.status()== WL_CONNECTED)
  {
     u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
      u8g2.drawGlyph(0,65,0x00f8);
     
  }
  else
  {
      u8g2.setFont(u8g2_font_streamline_all_t);
      u8g2.drawGlyph(0,64,0x0206);
  }
   client.setInsecure();
}

void loop()
{
  long irValue = particleSensor.getIR();
  long redV= particleSensor.getRed();
  B=digitalRead(btn);
  
  if(B==1 && samp>=10){
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_smart_patrol_nbp_tf);
    u8g2.setCursor(30,10);
    u8g2.print("Saving data");
    Serial.println("Button Pressed!");
    u8g2.sendBuffer();
    sendData(final_res);
}
  if (k>100) k==0;
//**********algo***********************

 if (irValue < 50000)
  {
      Serial.print(" No finger?");
      un_ir_mean=0.0;irmax=0.0;irmin=999999.0,samp=0,k=0,A=0.0,irBuffer[100]=0;
      u8g2.clearBuffer();
      wifistat();
      u8g2.setFont(u8g2_font_smart_patrol_nbp_tf);
      u8g2.setCursor(30,10);
      u8g2.print("No Finger ");
      u8g2.setFont(u8g2_font_unifont_t_symbols);
      u8g2.drawGlyph(60,35,0x2718);
      u8g2.sendBuffer();
  }
  else
  {
    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(",RED=");
    Serial.print(redV);
    Serial.print(", A =");
    Serial.print(A);
    Serial.print(", BGM =");
    Serial.print(beer);
    Serial.print(", IR Mean =");
    Serial.print(un_ir_mean);
    Serial.print(", IR Max =");
    Serial.print(irmax);
    Serial.print(", IR Min =");
    Serial.print(irmin);
  }
    Serial.println();

   
if (samp>10){
    final_res=beer;
    u8g2.clearBuffer();
    wifistat();
    u8g2.setFont(u8g2_font_6x12_te);
    u8g2.setCursor(0,10);
    u8g2.print("Keep finger on sensor and");
    u8g2.setCursor(0,30);
    u8g2.print("Press Button to save");
    u8g2.setCursor(20,50);
    u8g2.print("Result:");
    u8g2.setCursor(60,50);
    u8g2.print(beer);
    u8g2.sendBuffer();
  } 
  else  if (irValue > 50000)
  {
    u8g2.clearBuffer();
    wifistat();
    u8g2.setFont(u8g2_font_smart_patrol_nbp_tf);
    u8g2.setCursor(0,10);
    u8g2.print("Sampling..");
    u8g2.setCursor(80,10);
    u8g2.print(samp);
    u8g2.setCursor(30,60);
    u8g2.print("Please Wait");
    u8g2.setCursor(40,30);
    u8g2.print(beer);
    u8g2.sendBuffer();
   int bufferlenght=100;
      for (byte i=0;i< bufferlenght;i++){
      redBuffer[i]=particleSensor.getRed();
      irBuffer[i]=particleSensor.getIR();
    //particleSensor.nextSample();
  }
  un_ir_mean=0;
 
    un_ir_mean+= irBuffer[k];
  
  un_ir_mean= un_ir_mean/100;
  if(un_ir_mean >irmax){
    irmax=un_ir_mean;
  }
   if(un_ir_mean<irmin){
    irmin=un_ir_mean;
  }
      diff=(irmax-irmin);
      A=log(un_ir_mean/diff); //Beer Lambert's law
      beer= (A/(2.303*0.0555)); //Beer lambert's law
  }
  k++;
  samp++;
}

// Subroutine for sending data to Google Sheets
void sendData(float res) {
  Serial.println("==========");
  Serial.print("connecting to ");
  Serial.println(host);
  
  //-----Connect to Google host--------------
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }
 

  //--------Processing data and sending data------------------
  String string_res =  String(res);
  String url = "https://script.google.com/macros/s/" + GAS_ID + "/exec?res=" + string_res;
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");

  Serial.println("request sent");
  //----------------------------------------

  //----Checking whether the data was sent successfully or not---
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Data Sent");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.print("reply was : ");
  Serial.println(line);
  Serial.println("closing connection");
  Serial.println("==========");
  Serial.println();
  //----------------------------------------
} 
//==============================================================================
