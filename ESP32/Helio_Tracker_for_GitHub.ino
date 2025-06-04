#include<ESP32Servo.h> //Servo Header for ESP32
#include <LiquidCrystal_I2C.h> //LCD Header

//Wifi & MQTT Header
#include <WiFi.h>
#include <PubSubClient.h>

#define SIZE 30 //Array Size

Servo myservo;

LiquidCrystal_I2C lcd(0x27,16,2);  // LCD Address 0x27 for A 16 Chars and 2 Line Display

//函式宣告區
void Sensor_Initial_Data_Array(int *array, int pin);

int Median_Calculation(int* array, int size);

void Sensor_Data();

void setup_wifi();

void reconnect();

//變數宣告區
int Servo1=13;
 
int position = 90;    // 定義伺服馬達初始位置

int Photoresistor_Left=34;

int Photoresistor_Right=35;

int Initial_Sensor_Value_Left[SIZE] = {0}, Initial_Sensor_Value_Right[SIZE] = {0}, Median_Value_Left_Baseline = 0, Median_Value_Right_Baseline = 0, Value_Left, Value_Right, difference;

// Wifi & MQTT Setting

// Replace the next variables with your SSID/Password combination

const char* ssid = "Enter youself SSID";

const char* password = "Enter yourself Password";

// Add your MQTT Broker IP address

const char* MQTT_Server = "broker.mqttgo.io"; // You can replace the MQTT Broker which you like

int MQTTPort = 1883;// MQTT Port

WiFiClient WifiClient;

PubSubClient MQTTClient(WifiClient);

void setup() {
  
  Serial.begin(9600);

  myservo.attach(Servo1);

  myservo.write(position);

  lcd.init(); // Initialize the  LCD
 
  lcd.backlight();

  pinMode(4, OUTPUT); // Red Light

  pinMode(16, OUTPUT); // Green Light
  
  pinMode(17, OUTPUT); // White Light

  Sensor_Initial_Data_Array(Initial_Sensor_Value_Left,Photoresistor_Left); // 讀取左邊Sensor前30筆資料

  Median_Value_Left_Baseline = Median_Calculation(Initial_Sensor_Value_Left, SIZE); // 計算前30筆資料中位數

  Sensor_Initial_Data_Array(Initial_Sensor_Value_Right,Photoresistor_Right); // 讀取右邊Sensor前30筆資料

  Median_Value_Right_Baseline = Median_Calculation(Initial_Sensor_Value_Right, SIZE); // 計算前30筆資料中位數

  //Wifi

  setup_wifi();

  //MQTT

  MQTTClient.setServer(MQTT_Server, MQTTPort);

}

unsigned long lastReconnect = 0;

void loop() {

   if (!MQTTClient.connected()) {
    
    if (millis() - lastReconnect > 5000) {
      
      lastReconnect = millis();

      String clientId = "Helio_Turtle_Head"+ String(random(1000, 9999)); // You can replace the ID which you like, 後面加上亂數避免無聊人士取名一樣踢掉

      MQTTClient.connect(clientId.c_str());

    }
  } 
  
  else {
    
    MQTTClient.loop();
    
    Sensor_Data();  // 馬達更新邏輯

  }

}

// 在Array存入30筆Sensor資料

void Sensor_Initial_Data_Array(int *array, int pin){

   delay(50);

   int i;

   for(i=0; i<SIZE; i++){ 

      array[i] = analogRead(pin);

    }

}

// 利用中位數建立Sensor在該環境下的讀值基準

int Median_Calculation(int* array, int size) {

  // 複製陣列以免影響原始資料

  int temp[size];

  memcpy(temp, array, sizeof(int) * size);

  // 簡單氣泡排序

  for (int i = 0; i < size - 1; i++) {

    for (int j = i + 1; j < size; j++) {

      if (temp[i] > temp[j]) {

        int t = temp[i];

        temp[i] = temp[j];

        temp[j] = t;
        
      }

    }

  }

  // 回傳中位數
  if (size % 2 == 0){ // 偶數個數據

    return (temp[size / 2 - 1] + temp[size / 2]) / 2;

  }
    
  else{ // 奇數個數據

    return temp[size / 2];

  } 
    
}

unsigned long lastMove = 0;

const int moveInterval = 100;

String direction = "Freeze";

void Sensor_Data(){

  char angleBuf[4];  // 最多3位數 + '\0'

  sprintf(angleBuf, "%3d", position);  // 固定3格寬，空格補前面

  lcd.setCursor(0, 0);

  lcd.print("Angle : ");

  lcd.print(angleBuf);
  
  lcd.setCursor(0,1);

  lcd.print("Direction : ");
  
  if (millis() - lastMove >= moveInterval) {

      Value_Left = analogRead(Photoresistor_Left);

      Value_Right = analogRead(Photoresistor_Right);

      difference = Value_Left-Value_Right;

    // 設一個差值門檻，避免每次微小變化都動作

          if ((Value_Left > Median_Value_Left_Baseline+100) && position > 0 && difference >50) {

            position -= 1; // 左邊較亮 => 向左

            direction = "Left";

            lcd.print("<<< ");

            digitalWrite(4,HIGH);

            delay(50);

            digitalWrite(4,LOW);

          } 
          
          else if ((Value_Right > Median_Value_Right_Baseline+100) && position < 180 && difference < -50) {

            position += 1; // 右邊較亮 => 向右

            direction = "Right";

            lcd.print(">>> ");

            digitalWrite(16,HIGH);

            delay(50);

            digitalWrite(16,LOW);

          }

          else{
            
            direction = "Freeze";
            
            lcd.print("||  ");
            
            digitalWrite(17, HIGH);

            delay(50);

            digitalWrite(17, LOW);

          }
          
            myservo.write(position);
            
            lastMove = millis();
 
  }
 
  Serial.println("Difference between photoresistance is : " + String(difference));

  Serial.println("The position of servo motor is : " + String(position));


   //發送 JSON 到 MQTT

  String payload = "{\"left\":" + String(Value_Left) +
                  ",\"right\":" + String(Value_Right) +
                  ",\"difference\":" + String(difference) +
                  ",\"position\":" + String(position) +
                  ",\"direction\":\"" + direction + "\"}";
  
  MQTTClient.publish("helioTracker/data", payload.c_str()); // You can replace the Topic which you like

  delay(250);

}

void setup_wifi() {
  
  delay(10);
  
  Serial.print("Connecting to "+String(ssid));

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    
    Serial.print(".");
  
  }

  Serial.println("");
  
  Serial.println("WiFi connected to "+ WiFi.localIP().toString());

}

void reconnect() {

  while (!MQTTClient.connected()) {

    Serial.print("Attempting MQTT connection...");

    String clientId = "Helio_Turtle_Head"+ String(random(1000, 9999)); // You can replace the ID which you like, 後面加上亂數避免無聊人士取名一樣踢掉

    if (MQTTClient.connect(clientId.c_str())) {

      Serial.println("connected");

      Serial.print("Client ID: ");

      Serial.println(clientId);

    } 

    else {

      Serial.print("failed, rc=");
      
      Serial.print(MQTTClient.state());
      
      delay(5000);
    
    }
  
  }

}