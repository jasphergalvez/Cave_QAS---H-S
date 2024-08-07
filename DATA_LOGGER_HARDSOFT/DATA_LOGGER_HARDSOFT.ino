#include <BufferedPrint.h>
#include <FreeStack.h>
#include <MinimumSerial.h>
#include <RingBuf.h>
#include <SdFat.h>
#include <SdFatConfig.h>
#include <sdios.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h> //very specific to, DON'T UPDATE!!!
#include <LowPower.h> //rocketscream, easier to use for deep sleep
#include <avr/power.h> // lets us disable modules for max power conserve
#include <avr/sleep.h> //again provides more functionality
#include <DFRobot_DHT11.h>
#include <MQ135.h>
SdFat SD;
RTC_DS3231 RTC;
DFRobot_DHT11 DHT;
File myFile; // sdcard variable
DateTime time;
String dataString = "";
const char codebuild[] PROGMEM = __FILE__;
const char header[] PROGMEM = "Timestamp, RTC temp, Temperature, Humidity, RZero, AirRes, ppm";
#define SleepInterval  1 // THIS IS WHERE WE EDIT SLEEP INTERVAL TIME (IN MINUTES)
#define RTCpower 8 //pin power for RTC
volatile boolean clockInterrupt = false;
const int INTERRUPT_PIN = 2;
byte Alarmhour;
byte Alarmminute;
byte control;
float Battery_Voltage;
#define battery_pin A0
#define SDpower_pin 8
#define DHT11_pin 9
#define MQ135_powerpin 3
#define PIN_MQ135 A1
#define DS3231_power A2
MQ135 mq135_sensor(PIN_MQ135);
//DEFINE HERE POWER PINS (MOSFET PINS) FOR OTHER SENSORS
const int critical_voltage = 5.35;
const int chipSelect = 10;
const int MOSIPin = 11;
const int MISOPin = 12;
const int SCKPin = 13;
bool firstvoltage = true;

void setup() {
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  // Open serial communications and wait for port to open:
  pinMode(A0, INPUT);
  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW);
  HardwareOn();
  pinMode(MQ135_powerpin, OUTPUT); digitalWrite(MQ135_powerpin, LOW);
  Serial.begin(57600);
  detachInterrupt(0);
  // ================ initializing rtc ==========================
  pinMode(DS3231_power, OUTPUT); digitalWrite(DS3231_power, HIGH);
  RTC.begin();
  Wire.begin();
  // ============== clear RTC int trigger and prev alarm  ======================
  RTC.turnOffAlarm(2);
  RTC.turnOffAlarm(1);
  clockInterrupt = false;
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  if(!RTC.begin()){
    Serial.println(F("RTC cannot be found"));
  }
  if(!RTC.isrunning()){
    Serial.println(F("RTC ain't running"));
    RTC.adjust(DateTime(__DATE__, __TIME__));
  if(!RTC.isrunning()){
    Serial.println(F("Process halted"));
    while(1);
  }

  time = RTC.now(); //base time of setup completion
  }
  // check voltage to save sd integrity ======================
  HardwareOn();
  CheckVoltage();
  // ================ initializing sd card ====================
  Serial.print(F("INITIALIZING SD CARD..."));
  pinMode(10, OUTPUT); digitalWrite(10, HIGH);
  if (!SD.begin(10)) {
    Serial.println(F("INTIALIZATION FAILED! ---------- "));Serial.print(String(time.timestamp(DateTime::TIMESTAMP_FULL)));
    while (1);
  }
  Serial.println(F("INITIALIZATION DONE"));
  // for (int cycles = 1; cycles < 10; cycles++){
  //   for (int pin = 4; pin < 7; pin++){
  //     digitalWrite(pin, HIGH);
  //     delay(200);
  //     digitalWrite(pin, LOW);
  //   }
  // }
  delay(6);
  time = RTC.now();
  digitalWrite(DS3231_power, LOW);
  myFile = SD.open("datalog.txt", FILE_WRITE);
  if (myFile){
    myFile.println((__FlashStringHelper*)codebuild);myFile.print(F(" @: "));
    myFile.println(String(time.timestamp(DateTime::TIMESTAMP_FULL)));
    myFile.println((__FlashStringHelper*)header);
  }
  else{
    Serial.println(("datalog.txt can't be read"));
  }
  myFile.close();
}

void loop() {
  pinMode(DS3231_power, OUTPUT); digitalWrite(DS3231_power, HIGH);
  HardwareOn(); //turn everything on
  CheckVoltage();
  // for(int i=4; i<=6; i++){ //includes built in LED pin
  //   pinMode(i, OUTPUT); //14 A0 to 19 A5 disable din //for loops purposes
  // }
  // check for prev sleep RTC 
  if(clockInterrupt){
    if(RTC.checkIfAlarm(1)){
      RTC.turnOffAlarm(1);
    }
  }
  clockInterrupt = false;
  digitalWrite(DS3231_power, HIGH);
  time = RTC.now();
  digitalWrite(DS3231_power, LOW);
  // digitalWrite(RedPin, LOW);
  // digitalWrite(GreenPin,HIGH);
  // delay(1000);
  // Serial.println(F("Sensing right now"));

  // ========================== SENSOR SENSING GOES HERE (SAMPLING) ==============================
  float RTC_temp = RTC.getTemperature();
  DHT.read(DHT11_pin);
  pinMode(MQ135_powerpin, OUTPUT); digitalWrite(MQ135_powerpin, HIGH);
  float correctedRzero = mq135_sensor.getCorrectedRZero(DHT.temperature, DHT.humidity);
  float resistance = mq135_sensor.getResistance();
  float correctedPPM = mq135_sensor.getCorrectedPPM(DHT.temperature, DHT.humidity);
  pinMode(MQ135_powerpin, OUTPUT); digitalWrite(MQ135_powerpin, LOW);
  // ==================================================================================

  // ========================== CONCATENATION (FORMATTING) GOES HERE ============================
  dataString = ""; //this line simply erases the string ensures fresh write per cycle
  dataString = dataString + String(time.timestamp(DateTime::TIMESTAMP_FULL));;
  dataString = dataString + ", ";     //separate data with a comma so they can be imported to separate collumns in a spreadsheet
  dataString = dataString + String(RTC_temp) + "C";  //this only gives you two decimal places
  dataString = dataString + ", ";
  dataString = dataString + String(DHT.temperature) + "C"; // DHT TEMP
  dataString = dataString + ", ";
  dataString = dataString + String(DHT.humidity) + "%";
  dataString = dataString + ", "; 
  dataString = dataString + String(correctedRzero) + "RZ";
  dataString = dataString + ", ";
  dataString = dataString + String(resistance) + "AirRes";
  dataString = dataString + ", ";
  dataString = dataString + String(correctedPPM) + "ppm";
  //With floating point nubmers, if you want more digits after the decimal place replace the above line with these three:
  //char buff[10];
  //dtostrf(rtc_TEMP_degC, 4, 4, buff);  //4 is mininum width, second number is #digits after the decimal
  //dataString = dataString + buff;
  // dataString = dataString + String(valA0);// keep repeating until val A0 which is the last data you wanna enter
  // Serial.println(F("THE STRING TO BE PRINTED IS >"));Serial.println(dataString);  //---> FOR DEBUGGING
  // delay(5000); //debugging purposes, hugutin pag mali para iwas maling data.
  // =========================================================================================
  // delay(1000);
  // Serial.println(F("Writing rn"));
  // digitalWrite(BluePin, HIGH); digitalWrite(GreenPin,LOW);
  WriteToSD();
  delay(6);
  // ReadFromSD();
  // delay(6);
  Serial.println(F("CYCLE COMPLETED. WAIT FOR GREEN PIN MEANS SENSING | RED IS INTERVAL"));
  // digitalWrite(BluePin, LOW);
  // digitalWrite(RedPin, HIGH);
  SetAlarm();
  // digitalWrite(DS3231_power, LOW); pinMode(DS3231_power, INPUT);
  Serial.println("Going to sleep in 3 seconds");
  //delay(3000); //NOW THIS IS WHERE INTERVAL WOULD GO -- ALSO WHERE SLEEP MODE WOULD COME IN?                  // -- THIS IS JUST FOR DEBUGGING, OTHERWISE -
  // digitalWrite(RedPin, LOW);                                                                               // DI MAKIKITA CURRENT SA SOBRA BILIS NG SAMPLING PHASE ~271 mS
  HardwareOff(); //turn everything off before sleep
  Sleep();
}

// ====================================== MAIN LOOP ENDS HERE =======================================
// =========================================== WRITE FUNCTIONS BELOW ==========================================

void WriteToSD(){
  myFile = SD.open("datalog.txt", FILE_WRITE);
  // if the file opened okay, write to it:
  if (myFile) {
    Serial.println(F("Writing to datalog..."));
    myFile.println(dataString);
    // close the file:
    myFile.close();
    Serial.println(F("done."));
  } else {
    // if the file didn't open, print an error:
    Serial.println(F("error opening datalog.txt"));
  }
}

// void ReadFromSD(){
//   // re-open the file for reading:
//   myFile = SD.open("datalog.txt");
//   if (myFile) {
//     Serial.println(F("Reading from datalog.txt:"));
//     // read from the file until there's nothing else in it:
//     while (myFile.available()) {
//       Serial.write(myFile.read());
//     }
//     // close the file:
//     myFile.close();
//   } else {
//     // if the file didn't open, print an error:
//     Serial.println(F("error opening datalog.txt"));
//   }
// }

void Sleep(){
  ADCSRA = 0;
  noInterrupts();
  attachInterrupt(0,RTC_ISR,LOW);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(0);
  Serial.println("Wakey wakey. It's GO TIME!");
}

void SetAlarm(){
  time = RTC.now();
  Alarmhour = time.hour();
  Alarmminute = time.minute() + 1;
  if(Alarmminute > 59){
    Alarmminute = 0;
    Alarmhour = Alarmhour + 1;
      if(Alarmhour > 23){
      Alarmhour = 0;
      Alarmminute = 0;
    }
  }
  RTC.setAlarm1Simple(Alarmhour, Alarmminute);
  RTC.turnOnAlarm(1);
  if (RTC.checkAlarmEnabled(1)){
    Serial.println("Alarm set for: ");Serial.print(SleepInterval);Serial.println(" minutes.");
  }
  else{
    Serial.println("Alarm couldn't be set");
  }
}

void RTC_ISR(){
  clockInterrupt = true;
}

void CheckVoltage(){
  int sensorValue = analogRead(battery_pin);
    Battery_Voltage = sensorValue * (4.65 / 1023.0);
  // if(firstvoltage){
  //   Battery_Voltage = Battery_Voltage + 0.20;
  //   firstvoltage = false;
  // }
  // print out the value you read:
  Serial.println("Battery voltage supply is at: ");Serial.print(Battery_Voltage * 2);Serial.println(" V");
  if((Battery_Voltage * 2) <= critical_voltage){
    Serial.println("========BATTERY CRITICAL======");
    Serial.println("APPROACHING 5.35Vmin");
    Serial.println("SHUTTING DOWN IN 3 SECONDS");
    delay(3000);
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  }
}

void SdOff(){
  delay(6);
  pinMode(SDpower_pin, OUTPUT);
  digitalWrite(SDpower_pin, LOW);
  pinMode(11, INPUT);
  pinMode(12, INPUT);
  pinMode(13, INPUT);
  digitalWrite(10, LOW); pinMode(10, INPUT);
  delay(6);
}

void SdOn(){
  digitalWrite(10, HIGH); pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(SDpower_pin, OUTPUT);
  digitalWrite(SDpower_pin, HIGH);
  delay(6);
  SD.begin();
  delay(6); // just a flag for initiation // may need to extend this delay with some cards
}

void HardwareOff(){
  pinMode(DS3231_power, INPUT); digitalWrite(DS3231_power, LOW);
  digitalWrite(LED_BUILTIN, LOW);
  SdOff();
  pinMode(MQ135_powerpin, OUTPUT); digitalWrite(MQ135_powerpin, LOW);
  // this is sleep mode (pseudo-hardware)
  power_spi_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_twi_disable();
  power_adc_disable();
  //
  // ADD HERE MORE HANDLERS FOR SENSORS AND STUFF THIS IS FOR MOSFET TRIGGERING.
}

void HardwareOn(){
  power_spi_enable();
  power_timer1_enable();
  power_timer2_enable();
  power_twi_enable();
  power_adc_enable();
  SdOn();
  pinMode(MQ135_powerpin, OUTPUT); digitalWrite(MQ135_powerpin, HIGH);
  digitalWrite(DS3231_power, HIGH); pinMode(DS3231_power, OUTPUT);
  //add here more handlers
}

// =============================== I2C REGISTRY FUNCTIONS LIFTED FROM EDMALLON =========================================
