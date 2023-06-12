//DANP (Daniel Amani Noordin Project, danielamani.com)
//Cat Feeder V3

#define BOARD 2 // <<<--- 1 = NodeMCU V3, 2 = Arduino UNO/NANO
#define BOTtoken "XXXXX:XXXXXXX" // <<<---CHANGE WITH YOUR BOT TOKEN
#define CHAT_ID "XXXXXXXX" // <<<---CHANGE WITH YOUR TELEGRAM CHAT ID
#define CALIBRATION_FACTOR -178 // <<<---CHANGE WITH CALIBRATION FOR LOAD CELL
#define SCREEN_TYPE 2 // 1 = OLED, 2 = LCD
#define OLED_ADDRESS 0x3C // <<<---CHANGE WITH i2C ADDRESS FOR OLED DISPLAY
#define LCD_ADDRESS 0x20// <<<---CHANGE WITH i2C ADDRESS FOR LCD DISPLAY  0x27 
#define DS1307_ADDRESS 0x68 // <<<---CHANGE WITH i2C ADDRESS FOR DS1307 RTC
#define PROTEUS 1 // <<<---CHANGE THIS TO 1 SIMULATE ON PROTEUS ( ARDUINO ONLY)

//Config for Proteus: BOARD 2, SCREEN_TYPE 2, LCD_ADDRESS 0x20, DS1307_ADDRESS 0x68, PROTEUS 1

//Scale Sensor
#include <HX711.h>
HX711 scale;
//Servo Setting
#include <Servo.h>
Servo servo_mot; 

//Display Setting
#if SCREEN_TYPE == 1
  #include <Adafruit_GFX.h> 
  #include <Adafruit_SSD1306.h>
  #define OLED_RESET -1
  #define SCREEN_WIDTH 128 // OLED display width, in pixels
  #define SCREEN_HEIGHT 64 // OLED display height, in pixels
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#elif SCREEN_TYPE == 2
  #include <LiquidCrystal_I2C.h>
  LiquidCrystal_I2C lcd(LCD_ADDRESS,16,2); 
#endif
//DHT Sensor
#include <DHT.h>
#if BOARD == 1
  #define DHTPIN 3
#elif BOARD == 2
  #define DHTPIN 6
#endif
#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE);

#if BOARD == 1 && PROTEUS == 0
  #include <ESP32Time.h>  //NTP Time Library

  #include <Arduino.h> 
  #include <WiFiManager.h> //Wifi AP system
  //#include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h> // Web Server
  ESP8266WebServer server(80);

  //Create NTP connection
  #include <NTPClient.h>
  #include <WiFiUdp.h>
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "pool.ntp.org");
  ESP32Time rtc(0);

  //Telegram Library and depedency
  #include <UniversalTelegramBot.h> 
  #include <ArduinoJson.h>
  #include <WiFiClientSecure.h>
  // Initialize Telegram BOT
  //Config Security Certificate
  #ifdef ESP8266
   X509List cert(TELEGRAM_CERTIFICATE_ROOT);
  #endif
  WiFiClientSecure client;
  UniversalTelegramBot bot(BOTtoken, client);
  // Checks for new messages every 15 second.
  int botRequestDelay = 15000;
  unsigned long lastTimeBotRan;

#elif BOARD == 2
  #include <uRTCLib.h>
  uRTCLib rtc(DS1307_ADDRESS);  //i2c Address for DS12 RTC
#endif
 
//Inputs-Outputs
#if BOARD == 1 && PROTEUS == 0
const int but1 = 14; // Time
const int but2 = 12; // Feeding
const int but3 = 13; // Potion
const int but4 = 15; // Weight
const int d_load = 16; // Load Digital HX711
const int s_load = 2; // Load Clock HX711
const int servo_pin = 0; // Servo PWM
#elif BOARD == 2
const int but1 = 8; // Time
const int but2 = 9; // Feeding
const int but3 = 10; // Potion
const int but4 = 11; // Weight
const int d_load = 2; // Load Digital HX711
const int s_load = 3; // Load Clock HX711
const int servo_pin = 12; // Servo PWM
const int poten_pin = A0;
#endif

//Variables
bool state_but1 = true;
bool state_but2 = true;
bool state_but3 = true;
bool state_but4 = true;
int rotate_angle = 0; //As for MG996 angle is for direction
int portions = 1; //Set default minimum portion
int interval = 8; //Set default interval
unsigned long pMillis_OLED = 0;
unsigned long pMillis_NTP = 0;
unsigned long pMillis_DHT = 0;
unsigned long interval_NTP = 900000; //every 15 min connect to NTP
unsigned long interval_DHT = 10000;  //every 10 sec read DHT data
int OLED_refresh_rate = 1000; // refresh OLED every 1 sec
int max_portions = 10; //set max portion 10
int min_portions = 1;  //set min portion 1
int Hour, Minute;
int last_feed_hour = 1;
int next_feed_hour = 1;
bool feed_active = true;
int portion_delay = 200;
int stat_wifi = 100;
float reading_weight;
float last_weight;
float feed_reading;
float temp = 0.0;
float humi = 0.0;
int state_button = 1;
int state_screen = 1;
String last_feed = "";
String next_feed = "";
String button_stat = "Disable Button";
String screen_stat = "Disable Screen";

#if BOARD == 1 && PROTEUS == 0
//Handle Telegram 
void handleNewMessages(int numNewMessages) {
  Serial.println(F("handleNewMessages"));
  Serial.println(String(numNewMessages));
  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    // Print the received message
    String text = bot.messages[i].text;
    String set_interval = bot.messages[i].text;
    String get_interval = bot.messages[i].text;
    String set_portion = bot.messages[i].text;
    String get_portion = bot.messages[i].text;
    set_interval.remove(13);
    get_interval.remove(0,14);
    set_portion.remove(12);
    get_portion.remove(0,13);
    Serial.println(text);

    String from_name = bot.messages[i].from_name;
    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following commands for the SMART-PET-FEEDER\n\n";
      welcome += "/feed_now to run feeder \n";
      welcome += "/add_interval to increase interval \n";
      welcome += "/set_interval_'value' to set interval by value \n";
      welcome += "/add_portion to increase portion \n";
      welcome += "/set_portion_'value' to set portion \n";
      welcome += "/weight to get current weight \n";
      welcome += "/tare to tare current weight \n";
      welcome += "/toggle_button to enable/disable button\n";
      welcome += "/toggle_screen to enable/disable screen\n";
      welcome += "/info to get system info \n";
      bot.sendMessage(chat_id, welcome, "");
    }
    if (text == "/feed_now") {
      handle_feeds();
      bot.sendMessage(chat_id, "Feeding Now...", "");
    } 
    if (text == "/add_interval") {
      handle_hours();
      String system = "Interval every " + String(interval) + " hours\n";
      bot.sendMessage(chat_id, system, "");
    }
    if (set_interval == "/set_interval") {
      interval = get_interval.toInt();
      interval--;
      handle_hours();
      String system = "Interval every " + String(interval) + " hours\n";
      bot.sendMessage(chat_id, system, "");
    }
    if (text == "/add_portion") {
      handle_portions();
      String system = "New Portion is " + String(portions) ;
      bot.sendMessage(chat_id, system, "");
    } 
    if (set_portion == "/set_portion") {
      portions = get_portion.toInt();
      portions--;
      handle_portions();
      String system = "New Portion is " + String(portions) ;
      bot.sendMessage(chat_id, system, "");
    } 
    if (text == "/weight") {
      handle_weight();
      String system = "Weight is " + String(reading_weight) + " g\n";

      bot.sendMessage(chat_id, system, "");
    }  
    if (text == "/tare") {
      handle_tare();
      String system = "Tare done...\nWeight is " + String(reading_weight) + " g\n";
      bot.sendMessage(chat_id, system, "");
    }
    if (text == "/toggle_button") {
      handle_button();
      button_stat.remove(7);
      String system = "Button is not " + button_stat ;
      bot.sendMessage(chat_id, system, "");
    }  
    if (text == "/toggle_screen") {
      handle_screen();
      screen_stat.remove(7);
      String system = "Screen is not " + screen_stat ;
      bot.sendMessage(chat_id, system, "");
    }  
    if (text == "/info") {
      String IPaddress =  WiFi.localIP().toString();
      String debug_info = "System time: " + String(Hour) + ":" +String(Minute) + "\n";
      debug_info += "Portion: " + String(portions) + "\n";
      debug_info += "Run every: " + String(interval) + " hours \n";
      debug_info += "Last feeding time: " + last_feed + "\n";
      debug_info += "Next feeding time: " + next_feed + "\n";
      debug_info += "Temperature: " +  String(temp) + " C\n";
      debug_info += "Humidity: " + String(humi) + " %\n";
      debug_info += "Weight: " + String(reading_weight) + " g \n";
      debug_info += "Web IP: " + IPaddress + " \n";
      debug_info += "Mem: " +String(ESP.getFreeHeap());
      bot.sendMessage(chat_id, debug_info, "");
    }
  }
}
#endif

//Setup 
void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(but1, INPUT_PULLUP);
  pinMode(but2, INPUT_PULLUP);
  pinMode(but3, INPUT_PULLUP); 
  pinMode(but4, INPUT_PULLUP); 

#if BOARD == 1 && PROTEUS == 0
  #if SCREEN_TYPE == 1
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);  // initialize with the I2C addr 0x3C (for the 128x32)
  delay(100);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.display();
  display.clearDisplay();
  display.setCursor(12,6);    
  display.print("First time setup...");
  display.setCursor(0,34);
  display.print("Wifi: CatFood by DANP"); 
  display.setCursor(0,44); 
  display.print("Pass: 12345678DANP"); 
  display.setCursor(14,56); 
  display.print("<<<Project EDNA>>>");   
  display.display(); 
  delay(5000);
  #elif SCREEN_TYPE ==2
  lcd.init();
  lcd.clear();         
  lcd.backlight();      // On background
  lcd.setCursor(0,0); 
  lcd.print("CatFood by DANP");
  lcd.setCursor(0,1); 
  lcd.print("12345678DANP");
  delay(5000);
  #endif

  WiFiManager wm;
      bool res;
    res = wm.autoConnect("CatFood by DANP","12345678DANP"); // password protected ap
    if(!res) {
        Serial.println("Failed to connect");
    } 
    else {
        //if you get here you have connected to the WiFi 
        display.clearDisplay();   
        Serial.println("connected...yeey :)");
    }
    timeClient.begin();
    timeClient.setTimeOffset(28800);// Offset MY

    server.on("/", handle_OnConnect);
    server.onNotFound(handle_NotFound);
    server.on("/portions_web", handle_portions);
    server.on("/hours_web", handle_hours);
    server.on("/feeds_web", handle_feeds);
    server.on("/sync_time_web", handle_sync_time);
    server.on("/weight_web", handle_weight);
    server.on("/tare_web", handle_tare);
    server.on("/button_web", handle_button);
    server.on("/screen_web", handle_screen);
    server.begin();
    //NTP and Security Cert for Telegram
  #ifdef ESP8266
    configTime(0, 0, "pool.ntp.org");      // get UTC time via NTP
    client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  #endif
    #ifdef ESP32
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  #endif
    rtc_ntp_sync();  //Sync local RTC with NTP time
    delay(100);

#elif BOARD == 2
  #if SCREEN_TYPE == 1
  display.clearDisplay();
  display.setCursor(12,34);    
  display.print("Loading system...");
  display.setCursor(14,56); 
  display.print("<<<Project EDNA>>>");   
  display.display(); 
  delay(5000);
  #elif SCREEN_TYPE == 2
  lcd.init();
  lcd.clear();         
  lcd.backlight();      // On background
  lcd.setCursor(0,0); 
  lcd.print("Project EDNA");
  delay(5000);
  #endif
  URTCLIB_WIRE.begin();
  rtc.set(0, 56, 12, 5, 13, 5, 23); //Change your Time Here for Arduino
  // rtc.set(second, minute, hour, dayOfWeek, dayOfMonth, month, year)
  // set day of week (1=Sunday, 7=Saturday)
#endif
    scale.begin(d_load, s_load);
    scale.set_scale(CALIBRATION_FACTOR);   // this value is obtained by calibrating the scale with known weights
    scale.tare();               // reset the scale to 0
}

void loop() {  
  unsigned long millis_now = millis();

#if BOARD == 1 && PROTEUS == 0
    Hour = rtc.getHour();
    Minute = rtc.getMinute();
if (millis_now  > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while(numNewMessages) {
      Serial.println(F("got response"));
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis_now ;
  }

if( millis_now  - pMillis_NTP >= interval_NTP) {
      //update time to ntp server every 15 minute
      if(WiFi.status() != WL_CONNECTED){
        Serial.println(F("Wifi not available"));      
      }
      else{      
        Serial.println(F("Resync to NTP (15 min)"));
        rtc_ntp_sync();
      }
      pMillis_NTP = millis_now ;
    } 
#elif BOARD == 2
  rtc.refresh();
  Hour = rtc.hour();
  Minute = rtc.minute();
#endif 

if( millis_now - pMillis_DHT >= interval_DHT){
   pMillis_DHT = millis_now ;
   float newT = dht.readTemperature();
    if (isnan(newT)) {
      Serial.println(F("Failed to read from DHT sensor!"));
    }
    else {
      temp = newT;
      Serial.println(temp);
    }
   float newH = dht.readHumidity();
     if (isnan(newH)) {
      Serial.println(F("Failed to read from DHT sensor!"));
    }
    else {
      humi = newH;
      Serial.println(humi);
    }
}
                            
  if( millis_now - pMillis_OLED >= OLED_refresh_rate){     //Refresh rate of the OLED
    pMillis_OLED =  millis_now ;                            //Save "before value" for next loop
    
#if BOARD == 1 && PROTEUS == 0
    server.handleClient(); 
    #if SCREEN_TYPE == 1
    display.clearDisplay();
    #endif
#endif

    if(state_screen == 1){
      #if SCREEN_TYPE == 1
      display.setCursor(14,0); 
      display.print("System Time: "); 
      display.print(Hour); 
      display.print(":"); 
      display.print(Minute); 
   
      display.setCursor(14,10);
      display.print("Portions: ");   
      display.print(portions); 
   
      display.setCursor(14,18);
      display.print("Each "); 
      display.print(interval); 
      display.print(" hours"); 

      display.setCursor(14,26);
      display.print("Weight:"); 
      display.print(reading_weight); 
      display.print(" g"); 

      display.setCursor(14,34);
      display.print("Temperature:"); 
      display.print(temp);
      display.print("C"); 

      display.setCursor(14,42);
      display.print("Humidity:"); 
      display.print(humi); 
      display.print(" %");

      #if BOARD == 1 && PROTEUS == 0
        display.setCursor(12,56);
        display.print("IP:"); 
        display.print(WiFi.localIP());
      #endif  

    #elif SCREEN_TYPE == 2
      lcd.clear(); 
      lcd.setCursor(0,0); 
      lcd.print("T");
      lcd.print(Hour);
      lcd.print(Minute);
      lcd.print(" P");
      lcd.print(portions);
      lcd.print("I");
      lcd.print(interval); 
      lcd.print(" H");
      int humi_i=int(round(humi));
      lcd.print(humi_i);

      lcd.setCursor(0,1); 
      lcd.print("C:");
      int temp_i=int(round(temp));
      lcd.print(temp_i);
      lcd.print("W:");
      lcd.print(reading_weight);
      lcd.print("g");
    #endif
    } 
    #if SCREEN_TYPE == 1
    display.display();
    #endif
  }

  if(Hour == next_feed_hour){                 //If the time is the "next_feed_hour",we activate feeding
    feed_active = true;
    last_feed_hour = Hour;
    next_feed_hour = Hour + interval;         //Increase next feeding time by the interval
    if(next_feed_hour >= 23){                 //if we pass 23 hours, we start back at 00 hours
      next_feed_hour = next_feed_hour - 24;   //That's why we substract 24 hours which is maximum
    }
  }
 
  if(feed_active){                            //If this is active we rotate the motor
    int i = 0;
      #if SCREEN_TYPE == 1
      display.clearDisplay();   
      display.setCursor(48,28);      
      display.print(F("FEEDING"));    
      display.display();
      #elif SCREEN_TYPE == 2
      lcd.clear(); 
      lcd.setCursor(0,0); 
      lcd.print("FEEDING");
      #endif
      delay(1000);
    while(i<portions){
      servo_mot.attach(servo_pin);
      #if PROTEUS == 0   
      servo_mot.write(rotate_angle);                 
      #elif PROTEUS == 1
      rotate_angle = rotate_angle + 10;
      servo_mot.write(rotate_angle); 
      Serial.println(rotate_angle);
      if(rotate_angle>350){
        rotate_angle = 0;
      }
      #endif
      servo_mot.detach();
      i++;  //Rotate the motor according to the portions value
      delay(portion_delay); //Delay for each portion is milliseconds

    }
    feed_active = false;                      //Set "feed_active" back to false
    #if BOARD == 1 && PROTEUS == 0
    last_feed_time();
    #endif
    feeding_weight();
  }
  if(state_button==1){
  /////////////////////////////////
  //Button1 (increase portions)   
  if(!digitalRead(but1) && state_but1){
    add_portions();
    state_but1 = false;
  }
  else if(digitalRead(but1) && !state_but1){
    state_but1 = true;
  }
  
  /////////////////////////////////
  //Button2 (Manual feed) 
  if(!digitalRead(but2) && state_but2){     
    manual_feed();
    state_but2 = false;
  }
  else if(digitalRead(but2) && !state_but2){
    state_but2 = true;
  }

  /////////////////////////////////
  //Button3 (increase time inteval 0h.23h)
  if(!digitalRead(but3) && state_but3){
    add_hours();
    #if BOARD == 1 && PROTEUS == 0
    new_feed_time();
    #endif
    
    state_but3 = false;
  }
  else if(digitalRead(but3) && !state_but3){
    state_but3 = true;
  }  

  /////////////////////////////////
  //Button4 (weight and tare)
  if(!digitalRead(but4) && state_but4){
    weight_check();
    delay(2000);
    if(digitalRead(but4)==HIGH){
      scale.tare();     
    }
    state_but4 = false;
  }
  else if(digitalRead(but4) && !state_but4){
    state_but4 = true;
  }
  delay(50);

 }  
}

void add_portions(){
      portions++;
    if (portions < 1){
      portions = 1;
    }
    if(portions > max_portions){          //Where "max_portions" is set above
      portions = min_portions;
    }
}

void add_hours(){
      interval++;
    if(interval < 1){
      interval = 1;
    }
    if(interval > 23){
      interval = 1;
    }
}

void manual_feed(){
    next_feed_hour = Hour + interval;
     if(next_feed_hour >= 23){
        next_feed_hour = next_feed_hour - 24;
      }  
    #if BOARD == 1 && PROTEUS == 0
    new_feed_time();
    #endif
    feed_active = true;
}

void weight_check(){

  delay(200);
  #if SCREEN_TYPE == 1
  display.clearDisplay();   
  display.setCursor(24,28);      
  display.print(F("WEIGHT CHECK"));    
  display.display();
  #elif SCREEN_TYPE == 2
  lcd.clear(); 
  lcd.setCursor(0,0); 
  lcd.print("WEIGHT CHECK");
  #endif
  delay(1000);
  reading_weight = scale.get_units();
  #if PROTEUS == 1
  reading_weight = analogRead(poten_pin);
  #endif
  Serial.println(reading_weight);

}

void feeding_weight(){
  last_weight = reading_weight;
  weight_check();
  feed_reading=last_weight - reading_weight;
}

#if BOARD == 1 && PROTEUS == 0
void rtc_ntp_sync(){//Handle RTC-NTP sync (Internet)
  if(!timeClient.update()){
    Serial.println(F("NPT Fail to connect"));
    return;
  }
  timeClient.getHours();
  timeClient.getMinutes();
  timeClient.getSeconds();
  rtc.setTime(timeClient.getEpochTime());
}

void new_feed_time(){//Handle neext feed time (Internet)
    next_feed.remove(0);
    next_feed += next_feed_hour;
    next_feed += ":";
    next_feed += rtc.getMinute();
    next_feed += ":";
    next_feed += rtc.getSecond();
}

void last_feed_time(){//Handle last feed (Internet)
  last_feed.remove(0);
  last_feed = rtc.getTime("%A, %B %d %Y %H:%M:%S");
}

void handle_NotFound(){//Handle not found link (Internet)
  server.send(404, "text/plain", "Not found");
}

void handle_OnConnect() {//Handle webpage display value
  server.send(200, "text/html", SendHTML(Hour,Minute,portions,interval,last_feed,next_feed,feed_reading,last_weight,reading_weight,button_stat,screen_stat,temp,humi)); 
}

void handle_portions() {//Handle portion (Internet)
  add_portions();
  handle_OnConnect();
  Serial.println(F("WEB: Add portion"));
}

void handle_hours() {//Handle interval (Internet)
  add_hours();
  handle_OnConnect();
  Serial.println(F("WEB: Add time"));
}

void handle_feeds(){//Handle manual feed (Internet)
  manual_feed();
  last_feed_time();
  handle_OnConnect();
  Serial.println(F("WEB: Feed now"));
}
void handle_sync_time(){//Handle sync timee (Internet)
  rtc_ntp_sync();
  handle_OnConnect();
  Serial.println(F("WEB: Sync time NTP"));
}

void handle_weight(){//Handle weight (Internet)
  weight_check();
  handle_OnConnect();
  Serial.println(F("WEB: Check weight"));
}

void handle_tare(){//Handle tare (Internet)
  scale.tare();
  delay(100);
  weight_check();
  handle_OnConnect();
  Serial.println(F("WEB: Tare weight"));
}

void handle_button(){//Handle enable/disable hardware push button
  if(state_button == 0){
    state_button = 1;
    button_stat.remove(0);
    button_stat = "Disable Button";
    Serial.println(F("WEB: Button Disable"));
  }
  else if(state_button == 1){    
    state_button = 0;
    button_stat.remove(0);
    button_stat = "Enable Button";
    Serial.println(F("WEB: Button Enable"));
    }
    handle_OnConnect();
}

void handle_screen(){//Handle enable/disable OLED Display
  if(state_screen == 0){
    state_screen = 1;
    screen_stat.remove(0);
    screen_stat = "Disable Screen";
    Serial.println(F("WEB: Screen Disable"));
  }
  else if(state_screen == 1){    
    state_screen = 0;
    screen_stat.remove(0);
    screen_stat = "Enable Screen";
    Serial.println(F("WEB: Screen Enable"));
    }
    handle_OnConnect();
}
// Web page render
String SendHTML(int Current_hour,int Current_minute,int Portions_data,int Interval_data,String last_feed,String next_feed,float feed_reading,float last_weight,float reading_weight, String button_stat, String screen_stat, float temp, float humi){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<script>\n";
  ptr +="setInterval(loadDoc,200);\n";
  ptr +="function loadDoc() {\n";
  ptr +="var xhttp = new XMLHttpRequest();\n";
  ptr +="xhttp.onreadystatechange = function() {\n";
  ptr +="if (this.readyState == 4 && this.status == 200) {\n";
  ptr +="document.getElementById(\"webpage\").innerHTML =this.responseText}\n";
  ptr +="};\n";
  ptr +="xhttp.open(\"GET\", \"/\", true);\n";
  ptr +="xhttp.send();\n";
  ptr +="}\n";
  ptr +="</script>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>Cat Food Data</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr +=".buttons {width: 200px; margin: 0 auto; display: inline;}\n";
  ptr +=".button {display: block;width: 320px;background-color: #1abc9c;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-w {display: block;width: 100px;background-color: #1abc9c;border: none;color: white;padding: 10px 10px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-press {background-color: #1abc9c;}\n";
  ptr +=".button-press:active {background-color: #16a085;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<div id=\"webpage\">\n";
  ptr +="<h1>ESP8266: Cat Food Control</h1>\n";
  ptr +="<p>System Time: ";
  ptr +=(int)Current_hour;
  ptr +=":";
  ptr +=(int)Current_minute;
  ptr +="<a class=\"button button-press\" href=\"/sync_time_web\">Sync Time</a>\n";
  ptr +="<p>Temperature: ";
  ptr +=(float)temp;
  ptr +=" C | Humidity: ";
  ptr +=(float)humi;
  ptr +=" %";
  ptr +="<p>Portion: ";
  ptr +=(int)Portions_data;
  ptr +="<a class=\"button button-press\" href=\"/portions_web\">Add Portion</a>\n";
  ptr +="<p>Run Every: ";
  ptr +=(int)Interval_data;
  ptr +=" hours";
  ptr +="<a class=\"button button-press\" href=\"/hours_web\">Add Interval</a>\n";
  ptr +="<p>Last Feeding: ";
  ptr +=last_feed; 
  ptr +="<br>Next Feeding: ";
  ptr +=next_feed;
  ptr +="<a class=\"button button-press\" href=\"/feeds_web\">Feed Now</a>\n";
  ptr +="<p>Last Feeding Weight: ";
  ptr += (float)last_weight;
  ptr += " g";
  ptr +="<br>Last Portion Weight: ";
  ptr += (float)feed_reading;
  ptr += " g";
  ptr +="<br>Weight: ";
  ptr += (float) reading_weight;
  ptr += " g";
  ptr += "<div class=\"buttons\">\n";
  ptr +="<br><a class=\"button-w button-press\" href=\"/tare_web\">Tare</a><a class=\"button-w button-press\" href=\"/weight_web\">Weight</a>\n";
  ptr +="</div>\n";
  ptr +="<br><a class=\"button button-press\" href=\"/button_web\">";
  ptr +=button_stat;
  ptr +="</a>\n";
  ptr +="<a class=\"button button-press\" href=\"/screen_web\">";
  ptr +=screen_stat;
  ptr +="</a>\n";
  ptr +="</div>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}
#endif
