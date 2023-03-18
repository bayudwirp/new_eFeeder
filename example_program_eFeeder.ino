// ---=================================================---
// Dummy Project new eFeeder
// (c) bayudwirp, Maret 2023
// Ver.0.1 - 20230316 - Initial Version
// ---=================================================---
// Keterangan :
// Program ini akan membaca sensor-sensor terkait dengan kualitas air di laut atau tambak.
// Kemudian, hasil pembacaan sensor ini akan dikirimkan ke server menggunakan protokol MQTT
// Program dibuat untuk diimplementasikan pada board kontroler ESP32
// ---=================================================---

// Keperluan library yang dibutuhkan pada Aplikasi ini.
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DFRobot_PH.h" 
#include <NewPing.h>

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

void TaskReadSalinity(void *pvParameters);
void TaskReadpH(void *pvParameters);
void TaskReadTemp(void *pvParameters);
void TaskReadWaterLevel(void *pvParameters);
void TaskHandlingMQTT(void *pvParameters);

//Keperluan WiFi
const char* ssid = "eFishery";
const char* password =  "newFeeder";

//Keperluan MQTT
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* mqttUser = "user";
const char* mqttPassword = "password";
String data_periodic;

//Untuk keperluan pengiriman data melalui MQTT ke Server
WiFiClient espClient;
PubSubClient client_mqtt(espClient);

//Keperluan pembacaan pH meter
int sensorPin = 1;//pembacaan sensor ph menggunakan ADC
DFRobot_PH ph;
float tegangan,nilaiph;

//Keperluan membaca sensor water level menggunakan SRT04 (ultrasound)
#define trigPin 27
#define echoPin 26
#define LEDpin 2
#define MAX_DISTANCE 400
int len = 20;
float rata_rata_jarak = 0;
float temp = 0;
NewPing sonar = NewPing(trigPin, echoPin, MAX_DISTANCE);
 
//Untuk keperluan pembacaan sensor suhu
//Untuk pembacaan sensor Level Oil dan peripheral One Wire (Modul Temperatur, DS1208)
#define ONE_WIRE_BUS 32     //Modul 1 di Pin 32
float ADC_VALUE = 0.0;      //Untuk pembacaan nilai ADC
//Inisialisasi Library One Wire, untuk keperluan pembacaan sensor temperatur DS1820
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
//Deklarasi Variabel disini, digunakan untuk sensor Salinitas.
int analogPin = 0;
int dtime = 500;
int raw = 0;
int Vin = 5;
float Vout = 0;
float R1 = 1000;
float R2 = 0;
float buff = 0;
float avg = 0;
int samples = 5000;
String salinity_status;
float temperature1 = 0.0;
char publish_data[300];


void setup() {

  //Inisialisasi Serial
  Serial.begin(115200);

  //Inisialisasi MQTT
  mqtt_init();

  //Inisialisasi Pin untuk sensor salinitas
  pinMode(8,OUTPUT); // define ports 8 and 7 for AC 
  pinMode(7,OUTPUT);

  //Inisialisasi sensor ph
  ph.begin(); 
  
  xTaskCreatePinnedToCore(
    TaskReadSalinity
    ,  "TaskReadSalinity"  
    ,  1024  
    ,  NULL
    ,  2  
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskReadpH
    ,  "TaskReadpH"
    ,  1024 
    ,  NULL
    ,  2  
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskReadTemp
    ,  "TaskReadTemp"
    ,  1024 
    ,  NULL
    ,  2  
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);
  
  xTaskCreatePinnedToCore(
    TaskReadWaterLevel
    ,  "TaskReadWaterLevel"
    ,  1024 
    ,  NULL
    ,  2  
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);
  
  xTaskCreatePinnedToCore(
    TaskHandlingMQTT
    ,  "TaskHandlingMQTT"
    ,  1024 
    ,  NULL
    ,  3  
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);
}

void loop()
{
}

void TaskReadSalinity(void *pvParameters) 
{
  (void) pvParameters;
  for (;;)
  {
      float tot = 0;
      for (int i =0; i<samples; i++) 
      {
        digitalWrite(7,HIGH);
        digitalWrite(8,LOW);
        delayMicroseconds(dtime);
        digitalWrite(7,LOW);
        digitalWrite(8,HIGH);
        delayMicroseconds(dtime);
        raw = analogRead(analogPin);
        if(raw){
          buff = raw * Vin;
          Vout = (buff)/1024.0;
          buff = (Vin/Vout) - 1;
          R2= R1 * buff;
          tot = tot + R2;
        }
      }

      //Hasil perhitungan pembacaan sensor salinitas
      avg = tot/samples;
      if (avg > 2000) 
      {
        salinity_status = "Air Demineralisasi";
      }
      else if (avg > 1000) 
      {
        salinity_status = "Air Tawar";
      }
      else if (avg > 190)
      {
        salinity_status = "Air Payau";
      }
      else if (avg < 190) 
      {
        salinity_status = "Air Laut";
      }
      vTaskDelay(10);  // one tick delay (15ms) in between reads for stability
  }
}

void TaskReadpH(void *pvParameters)  
{
  (void) pvParameters;
  for (;;)
  {
    tegangan = analogRead(sensorPin)/1024.0*5000;   //mengubah tegangan analog menjadi digital dan menjadi tegangan
    nilaiph = ph.readPH(tegangan,temperature1);     //konversi tegangan menjadi ph meter dengan kompensasi suhu
    delay(990);
    vTaskDelay(10);  // one tick delay (15ms) in between reads for stability
  }
}

void TaskReadTemp(void *pvParameters)  
{
  (void) pvParameters;
  for (;;)
  {
    sensors.requestTemperatures();  // Send the command to get temperatures
    temperature1 = sensors.getTempCByIndex(0);
    delay(900);
    vTaskDelay(10);  // one tick delay (15ms) in between reads for stability
  }
}

void TaskReadWaterLevel(void *pvParameters) 
{
  (void) pvParameters;
  int ketinggian_kolam = 500;   //Asumsikan nilai ketinggian kolam tersebut 500 cm (5meter
  for (;;)
  {
    for(int i=0;i<len;i++)
    {
      delay(25); 
      temp += sonar.ping_cm();
    }    
    rata_rata_jarak = ketinggian - (temp / len); //Jarak = Total ketinggian - jumlah nilai yang terbaca
    temp = 0;
    vTaskDelay(10);  // one tick delay (15ms) in between reads for stability
  }
}

void TaskHandlingMQTT(void *pvParameters) 
{
  (void) pvParameters;
  for (;;)
  {
     //Kumpulkan data untuk dijadikan satu paket data yang dikirim perdetik
     data_periodic="EFEEDER,";        //Menandakan HEADER Data
     data_periodic+=temperature1;     //Data nilai temperature
     data_periodic+=",";
     data_periodic+=avg;              //Data nilai salinitas
     data_periodic+=",";
     data_periodic+=salinity_status;  //Status Nilai Salinitas
     data_periodic+=",";
     data_periodic+=rata_rata_jarak;  //Nilai ketinggian air
     data_periodic+=",";
     data_periodic+=nilaiph;          //Data Nilai Status pH
     data_periodic+=",REDEEFE";       //Menandakan TAIL data
     data_periodic.toCharArray(publish_data,300);  //Update 08 Desember 2020 : Tambahan 

     //Kirim data melalui MQTT
     if (client_mqtt.connect("ESP32Client", mqttUser, mqttPassword )) 
     {
        client_mqtt.publish("/efishery/new_efeeder", publish_data); //Publish data dengan topic /efishery/new_efeeder
     } 
    delay(990);
    vTaskDelay(10);  // one tick delay (15ms) in between reads for stability
  }
}

void mqtt_init()
{
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.println("Menghubungkan ke WiFi");
  }
  Serial.println("Terhubung ke jaruingan WiFi");
  client_mqtt.setServer(mqttServer, mqttPort);
  while (!client_mqtt.connected()) 
  {
    Serial.println("Menghubungkan ke server MQTT");
    if (client_mqtt.connect("ESP32Client", mqttUser, mqttPassword )) 
    {
      Serial.println("Terhubung");
    } 
    else 
    {
      Serial.print("failed with state ");
      Serial.print(client_mqtt.state());
      delay(2000);
    }
  }
}


//--- Sumber Referensi Source Code ---
// - Example project arduino
// - https://www.instructables.com/Water-Salinity-meter/
// - https://www.nyebarilmu.com/tutorial-mengakses-module-ph-meter-sensor-menggunakan-arduino/#google_vignette
//--------------------------------------
//--- END OF FILE --
