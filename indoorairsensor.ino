/* rsimai 2022-03-20 indoor air sensor with bme280 and ccs811, 
  SSD1306 display, 2 capacitive sensor buttons and esp32. All
  connected through I2C */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BME280.h>
#include <U8x8lib.h>
#include <Adafruit_CCS811.h>
#include <Preferences.h>

Preferences preferences;

Adafruit_CCS811 ccs; // I2C
Adafruit_BME280 bme; // I2C

U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

String version="2022-03-20";
int baudrate = 115200;

boolean serialout = true;  // default: serial on
boolean ledon = true;      // default: LEDs on
int everysec = 2;          // default: read and update every 2 seconds
int ccsdrivemode = 1;      // default: 1 sec readings from the ccs
boolean powersave = false; // default: start with powersave off

int co2l1 = 1000; // green below
int co2l2 = 1400;
int co2l3 = 2000;
int co2l4 = 5000; // red above

int humidl1 = 35; // too low below
int humidl2 = 40;
int humidl3 = 60;
int humidl4 = 70; // too high above

int initcounter = 300 / everysec; //co2 sensor needs 5+ minutes to warm up

boolean key1;
boolean lastkey1;
int keyduration1;
boolean key2;
boolean lastkey2;
int keyduration2;
int longkey = 3;   //seconds to enter menu

int co2;           //actual measure
int avg_co2 = 400; //floating average
int tvoc;
int avg_tvoc;
float temp;
float avg_temp;
float press;
float avg_press;
float humid;
float avg_humid;
String text_humid; //warnings
String text_co2;   //warnings
char rtemp[7];     //output formatting
char rpress[7];
char rhumid[7];
char rco2[7];
char rtvoc[7];
int co2level = 0; //0-4 for warnings, 5 warmup

float ratio1 = 0.75; //floating average 1/4
float ratio2 = 0.25;

int ledred = 4;           // hardware pin
int ledyellow = 0;
int ledgreen = 2;
int ledredchannel = 0;    // PWM channel
int ledyellowchannel = 1;
int ledgreenchannel = 2;
int ledredbright = 240;   // default brightness
int ledyellowbright = 255;
int ledgreenbright = 35;
const int freq = 1000;    // PWM frequency
const int resolution = 8; // PWM resolution

int keypin1 = 34;
int keypin2 = 35;

String command; //for the serial interface

void setup() {
  pinMode(keypin1, INPUT); //down button
  pinMode(keypin2, INPUT); //up button
  pinMode(ledgreen, OUTPUT); 
  pinMode(ledyellow, OUTPUT);
  pinMode(ledred, OUTPUT);
  ledcSetup(ledredchannel, freq, resolution);
  ledcSetup(ledyellowchannel, freq, resolution);
  ledcSetup(ledgreenchannel, freq, resolution);
  ledcAttachPin(ledred, ledredchannel);
  ledcAttachPin(ledyellow, ledyellowchannel);
  ledcAttachPin(ledgreen, ledgreenchannel);
  
  ledplay();  
  
  //pinMode(32, OUTPUT); //beeper
  //digitalWrite(32, LOW); //beeper off

  preferences.begin("sensor", false);

  bool flash = preferences.getBool("flashavailable");

  u8x8.begin();
  u8x8.setPowerSave(0);
  splash();
  ledplay();
    
  Serial.begin(baudrate);
  Serial.setTimeout(2000);
  ledplay();

  Serial.print("bme280-ccs811-096oled ");
  Serial.println(version);

  if ( flash == true ) {
    Serial.println("flash available, reading values");
    loadvars();
  }
  else {
    Serial.println("no flash, using defaults");
  }

  if(!ccs.begin()){
    Serial.println("Failed to start ccs811. Halt.");
    while(1);
  }

  while(!ccs.available()); //give it a sec
  setccsdrivemode();

  Serial.println(F("ccs811 init done"));
  unsigned status;
  status = bme.begin(0x76); //AZ-Delivery's 280 is on 0x76, not 0x77
  if (!status) {
    Serial.println("Failed to start BME280. Halt.");
    while (1) delay(10);
  }

  bme.setSampling(Adafruit_BME280::MODE_NORMAL, 
                    Adafruit_BME280::SAMPLING_X1, // temperature 
                    Adafruit_BME280::SAMPLING_X1, // pressure 
                    Adafruit_BME280::SAMPLING_X1, // humidity 
                    Adafruit_BME280::FILTER_OFF   ); 
  
  Serial.println("BME280 init done");
  if ( serialout == true ) {
    Serial.print("start sending values, every ");
    Serial.print(everysec);
    Serial.println(" sec");
  }
  else {
    Serial.println("sending values disabled");
  }
  Serial.println("");

  firstrun(); //set floating averages to actuals
  u8x8.setPowerSave(powersave);
  prepdisplay();
}

void setccsdrivemode() {
  if ( ccsdrivemode == 1 ){
    ccs.setDriveMode(CCS811_DRIVE_MODE_1SEC);
  }
  else if ( ccsdrivemode == 10 ){
    ccs.setDriveMode(CCS811_DRIVE_MODE_10SEC);
  }
}

void ledplay() {
  ledcWrite(ledgreenchannel, ledgreenbright);
  delay(200);
  ledcWrite(ledyellowchannel, ledyellowbright);
  delay(200);
  ledcWrite(ledgreenchannel, 0);
  delay(200);
  ledcWrite(ledredchannel, ledredbright);
  delay(200);
  ledcWrite(ledyellowchannel, 0);
  delay(200);
  ledcWrite(ledredchannel, 0);
}

void firstrun() {
  readbme();
  readccs();
  avg_temp = temp;
  avg_press = press;
  avg_humid = humid;
  avg_co2 = 400;
  avg_tvoc = tvoc;
}

void fbig() {
  u8x8.setFont(u8x8_font_px437wyse700a_2x2_f); //big font
}

void fsmall() {
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f); //small font
}

void prepdisplay() { // names and units only, static
  fsmall();
  u8x8.clearDisplay();
  u8x8.setCursor(0,0);
  u8x8.drawUTF8(0,0, "Temp         °C ");
  u8x8.setCursor(0,1);
  u8x8.print("Press        hPa");
  u8x8.setCursor(0,2);
  u8x8.print("Humid        %  ");
  u8x8.setCursor(0,3);
  u8x8.print("CO2          ppm");
  u8x8.setCursor(0,4);
  u8x8.print("TVOC         ppb");
}

void valuesdisplay() { // values updates only, dynamic
  dtostrf(avg_temp, 6, 1, rtemp);
  dtostrf(avg_press, 6, 1, rpress);
  dtostrf(avg_humid, 6, 1, rhumid);
  dtostrf(avg_co2, 6, 0, rco2);
  dtostrf(avg_tvoc, 6, 0, rtvoc);
  fsmall();
  u8x8.setCursor(6,0);
  u8x8.print(rtemp); 
  u8x8.setCursor(6,1);
  u8x8.print(rpress);
  u8x8.setCursor(6,2);
  u8x8.print(rhumid); 
  u8x8.setCursor(4,3);
  if ( avg_co2 >400 ) {
    u8x8.print(rco2); 
  }
  else {
    u8x8.print("   min");
  }
  u8x8.setCursor(4,4);
  u8x8.print(rtvoc); 
  u8x8.setCursor(0,6);
  u8x8.print(text_humid);
  u8x8.setCursor(0,7);
  u8x8.print(text_co2);
}

void serial_out() {
  Serial.print(F("Temperature = "));
  Serial.print(temp);
  Serial.println(" °C");

  Serial.print(F("Pressure = "));
  Serial.print(press);
  Serial.println(" hPa ");
  
  Serial.print("Humidity = ");
  Serial.print(humid);
  Serial.println(" %");

  Serial.print("CO2 = ");
  Serial.print(co2);
  Serial.println(" ppm");

  Serial.print("TVOC = ");
  Serial.print(tvoc);
  Serial.println(" ppb");
}

void readbme() {
  temp = bme.readTemperature();
  press = bme.readPressure()/100;
  humid = bme.readHumidity();
}

void readccs() {
  ccs.setEnvironmentalData(humid, temp); //to compensate
  if(ccs.available()){
    if(!ccs.readData()){
      co2 = ccs.geteCO2(); //int
      tvoc = ccs.getTVOC(); //int
    }
    else{
      Serial.println("ccs ERROR!");
    }
  }
}

void floatingaverage() {
  avg_temp = (ratio1*avg_temp)+(ratio2*temp);
  avg_press = (ratio1*avg_press)+(ratio2*press);
  avg_humid = (ratio1*avg_humid)+(ratio2*humid);
  if ( co2 == 400 ) {
    avg_co2 = 400;
  }
  avg_co2 = (ratio1*avg_co2)+(ratio2*co2);
  avg_tvoc = (ratio1*avg_tvoc)+(ratio2*tvoc);
}
  
void  serialhelp() {
  Serial.println("available commands:");
  Serial.println("exit           - end the setup");
  Serial.println("serialon       - output actual values");
  Serial.println("serialoff      - don't output values");
  Serial.println("displayon      - start with oled on");
  Serial.println("displayoff     - start with oled off");
  Serial.println("ledon          - start with LEDs on");
  Serial.println("ledoff         - start with LEDs off");
  Serial.println("rate1          - 1  second  refresh");
  Serial.println("rate2          - 2  seconds refresh");
  Serial.println("rate5          - 5  seconds refresh");
  Serial.println("rate10         - 10 seconds refresh");
  Serial.println("ccsdrivemode1  - ccs reads every second");
  Serial.println("ccsdrivemode10 - ccs reads every 10 seconds");
  Serial.println("list           - list variables in use");
  Serial.println("save           - save variables to flash");
  Serial.println("load           - load variables from flash");
  Serial.println("clear          - clear flash");
  Serial.println("help           - this help");
}

void sack() {
  Serial.println("OK");
}

void snack() {
  Serial.println("unknown command");
}

void serialconfig() {
  //beep();
  u8x8.clearDisplay();
  u8x8.setPowerSave(0);
  u8x8.setCursor(0,0);
  u8x8.print("Serial port open");
  u8x8.setCursor(0,1);
  u8x8.print("waiting for conf");
  u8x8.setCursor(0,2);
  u8x8.print("baudrate ");
  u8x8.setCursor(9,2);
  u8x8.print(baudrate);

  while ( true ) {
    command = Serial.readStringUntil('\n');
    command.trim();
    u8x8.setCursor(0,4);
    u8x8.clearLine(4);
    u8x8.print(command);
    if ( command != "" ) {
      Serial.println(command);
    }
    if ( command == "serialon" ) {
      serialout = true;
      sack();
    }
    else if ( command == "serialoff" ) {
      serialout = false;
      sack();
    }
    else if ( command == "displayon" ) {
      powersave = 0;
      sack();
    }
    else if ( command == "displayoff" ) {
      powersave = 1;
      sack();
    }
    else if ( command == "ledon" ) {
      ledon = true;
      sack();
    }
    else if ( command == "ledoff" ) {
      ledon = false;
      sack();
    }
    else if ( command == "rate1" ) {
      everysec = 1;
      sack();
    }
    else if ( command == "rate2" ) {
      everysec = 2;
      sack();
    }
    else if ( command == "rate5" ) {
      everysec = 5;
      sack();
    }
    else if ( command == "rate10" ) {
      everysec = 10;
      sack();
    }
    else if ( command == "ccsdrivemode1" ) {
      ccsdrivemode = 1;
      setccsdrivemode();
      sack();
    }
    else if ( command == "ccsdrivemode10" ) {
      ccsdrivemode = 10;
      setccsdrivemode();
      sack();
    }
    else if ( command == "list" ) {
      listvars();
      sack();
    }
    else if ( command == "save" ) {
      savevars();
      sack();
    }
    else if ( command == "load" ) {
      loadvars();
      sack();
    }
    else if ( command == "clear" ) {
      preferences.clear();
      sack();
    }
    else if ( command == "help" ) {
      serialhelp();
      sack();
    }
    else if (( command == "exit" ) or ( digitalRead(34) == true)) {
      Serial.println("exit config");
      //beep();
      u8x8.setCursor(0,4);
      u8x8.clearLine(4);
      u8x8.print("exiting ...");
      delay(2000);
      prepdisplay();
      return;
    }
    else if ( command != "" ) {
      snack();
    }
  }

  delay(1000);
  u8x8.clearDisplay();
  u8x8.setPowerSave(powersave);
  //powersave = 0;
  prepdisplay();
}

void listvars() {
  Serial.print("serial ");
  Serial.println(serialout);
  Serial.print("oledoff ");
  Serial.println(powersave);
  Serial.print("LED on ");
  Serial.println(ledon);
  Serial.print("rate ");
  Serial.println(everysec);
  Serial.print("ccsdrivemode ");
  Serial.println(ccsdrivemode);
}

void savevars() {
  preferences.putBool("serial", serialout);
  preferences.putBool("powersave", powersave);
  preferences.putBool("ledon", ledon);
  preferences.putInt("rate", everysec);
  preferences.putInt("ccsdrivemode", ccsdrivemode);
  preferences.putBool("flashavailable", true); // check if read from flash possible
}

void loadvars() {
  serialout = preferences.getBool("serial");
  ledon = preferences.getBool("ledon");
  everysec = preferences.getInt("rate");
  ccsdrivemode = preferences.getInt("ccsdrivemode");
  powersave = preferences.getBool("powersave");  
}

void splash() {
  u8x8.clearDisplay();
  fsmall();
  u8x8.setCursor(0,0);
  u8x8.print("Initialize ...");
  u8x8.setCursor(0,1);
  u8x8.print("ver ");
  u8x8.setCursor(4,1);
  u8x8.print(version);
  u8x8.setCursor(0,3);
  u8x8.print("BME280");
  u8x8.setCursor(0,4);
  u8x8.print("  temp, pressure");
  u8x8.setCursor(0,5);
  u8x8.print("  humidity");
  u8x8.setCursor(0,6);
  u8x8.print("CCS811");
  u8x8.setCursor(0,7);
  u8x8.print("  CO2, TVOC");
}

void warnings() {
  // warning text
  if ( avg_humid < humidl1 ) {
    text_humid = "Humidity too low";
  }
  else if ( avg_humid < humidl2 ) {
    text_humid = "Humidity low    ";
  }
  else if ( avg_humid < humidl3 ) {
    text_humid = "                ";
  }
  else if ( avg_humid < humidl4 ) {
    text_humid = "Humidity high   ";
  }
  else if ( avg_humid >= humidl4 ) {
    text_humid = "Humidity extreme";
  }
  else {
    text_humid = "Humidity undef  ";
  }

  if ( initcounter > 0 ) {
    text_co2 = "CO2 warmup ";
    text_co2.concat(initcounter);
    text_co2 = text_co2 + "  ";
    initcounter --;
    co2level = 5;
  }
  else if ( avg_co2 < co2l1 ) {
    text_co2 = "                ";
    co2level = 0;
  }
  else if ( avg_co2 < co2l2 ) {
    text_co2 = "CO2 high        ";
    co2level = 1;
  }
  else if ( avg_co2 < co2l3 ) {
    text_co2 = "CO2 higher      ";
    co2level = 2;
  }
  else if ( avg_co2 < co2l4 ) {
    text_co2 = "CO2 too high    ";
    co2level = 3;
  }
  else if ( avg_co2 >= co2l4 ) {
    text_co2 = "CO2 extreme     ";
    co2level = 4;
  }
  else {
    text_co2 = "CO2 undefined   ";
  }
}

void ledout() {
  if ( ledon == false ) {
    ledcWrite(ledgreenchannel, 0);
    ledcWrite(ledyellowchannel, 0);
    ledcWrite(ledredchannel, 0);
    return;
  }
  if ( co2level == 0 ) {
    ledcWrite(ledgreenchannel, ledgreenbright);
    ledcWrite(ledyellowchannel, 0);
    ledcWrite(ledredchannel, 0);
  }
  else if ( co2level == 1 ) {
    ledcWrite(ledgreenchannel, ledgreenbright/2);
    ledcWrite(ledyellowchannel, ledyellowbright/2);
    ledcWrite(ledredchannel, 0);
  }
  else if ( co2level == 2 ) {
    ledcWrite(ledgreenchannel, 0);
    ledcWrite(ledyellowchannel, ledyellowbright);
    ledcWrite(ledredchannel, 0);
  }
  else if ( co2level == 3 ) {
    ledcWrite(ledgreenchannel, 0);
    ledcWrite(ledyellowchannel, ledyellowbright/2);
    ledcWrite(ledredchannel, ledredbright/2);
  }
  else if ( co2level == 4 ) {
    ledcWrite(ledgreenchannel, 0);
    ledcWrite(ledyellowchannel, 0);
    ledcWrite(ledredchannel, ledredbright);
  }
  else {
    ledcWrite(ledgreenchannel, ledgreenbright/30);
    ledcWrite(ledyellowchannel, ledyellowbright/30);
    ledcWrite(ledredchannel, ledredbright/30);
  }
}

void checkkey() {
  for (int count = 0; count < (everysec * 100 - 5); count++) { //overall timing
  key1 = digitalRead(keypin1);
  key2 = digitalRead(keypin2);
  if ( key1 == true) {
    keyduration1++;
    if ( keyduration1 > ( 100 * longkey )) {
      Serial.println("enter config, try help");
      powersave = !powersave;
      serialconfig();
      lastkey1 = 0;
      key1 = 0;
    }
  }
  else {
    keyduration1 = 0;
  }
  if (key1 != lastkey1) {    //key change?
    if ( key1 > lastkey1 ) { //toggle only when key down
      powersave = !powersave;
      //beep();
    }
    lastkey1 = key1;
    u8x8.setPowerSave(powersave);
  }
  
  if ( key2 == true) {
    keyduration2++;
    if ( keyduration2 > ( 100 * longkey )) {
      Serial.println("key2 long press, not implemented");
      ledon = !ledon;
      lastkey2 = 0;
      key2 = 0;
    }
  }
  else {
    keyduration2 = 0;
  }
  if (key2 != lastkey2) {    //key change?
    if ( key2 > lastkey2 ) { //toggle only when key down
      ledon = !ledon;
      ledout();
      //beep();
    }
    lastkey2 = key2;
  }
  
  delay(10);
  }
}

//void beep() {
//  digitalWrite(32, HIGH); //beep
//  delay(5);
//  digitalWrite(32, LOW);
//}

void loop() {

  readbme();
  readccs();
  floatingaverage();
  warnings();

  if ( serialout == true ) {
    serial_out();
  }

  ledout();
  valuesdisplay();
  checkkey();          
}
