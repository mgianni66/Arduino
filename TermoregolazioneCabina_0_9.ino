/*
 * ---------------------------------------------------------------------------
 * Project:        CONTROLLO DI TEMPERATURA E UMIDITÀ PER CABINA ARMADIO
 * Author:         GIOVANNI MANTI
 * Version:        0.9 beta
 * First version:  01/11/2018
 * Last version:   27/12/2018
 * License:        Public Domain
 * ---------------------------------------------------------------------------
 *
 */

/* -- LIBRARIES -- */

#include <Wire.h>                // Library for communication with I2C/TWI devices 
#include <DS3231.h>              // Library for Real Time Clock DS3231
#include <LiquidCrystal_I2C.h>   // Library for LCD with I2C 
#include <DHT.h>                 // Library for DHT11/DHT22 Temperature/Humidity sensor 
#include <avr/wdt.h>             // Library for AVR Watchdog

/* -- DEFINITIONS -- */

#define HEATERPIN 2     // pin a cui è collegato il riscaldatore
#define FANPIN 4        // pin a cui è collegato il ventilatore
#define DHTPIN 8        // pin a cui è collegato il sensore DHT22
#define DHTTYPE DHT22   // definisco il sensore di temperatura/umidità DHT22 (AM2302)
// DHT22 Connection: Pin1(power)--> +5V, Pin2(data)--> DigPin8, Pin3 --> NC, Pin4(ground) --> GND, Resistor 4K7 bewtteen Pin1-Pin2

/* -- OBJECT CREATION -- */

DHT dht(DHTPIN, DHTTYPE);             // Creazione oggetto DHT
LiquidCrystal_I2C lcd(0x27, 20, 4);   // Creazione oggetto LCD, indirizzo 0x27, display 20 caratteri 4 linee
DS3231 Clock;                         // Creazione oggetto RTC

/* -- GLOBAL VARIABLES -- */

// debug
bool debugMode = true;  // debug mode

// rtc
bool Century = false;
bool h12;
bool PM;
byte ADay, AHour, AMinute, ASecond, ABits;
bool ADy, A12h, Apm;
byte seconds;         // secondi
byte minutes;         // minuti
byte hours;           // ore
byte dayOfWeek;       // giorno della settimana (1 dom. - 7 sab.)
byte day;             // giorno (0-31)
byte month;           // mese (1-12)
int year;             // anno
char* dayOfWeekChar;  // array di caratteri contenente il giorno della settimana

// sensor's data
float hum;                  // value of humidity from the sensor
float tem;                  // value of temperature from the sensor
char* valueReadVer;         // reading data sensor verification

// state variables
bool tempState = false;     // true= superamento soglia di temperatura
bool humState = false;      // true= superamento soglia di umidità
bool workingDay = false;    // true= lavorativo, false= festivo
bool heatWdOnTime = false;  // fascia oraria lavorativa di accensione del riscaldatore
bool heatHdOnTime = false;  // fascia oraria festiva di accensione del riscaldatore
bool fanWdOnTime = false;   // fascia oraria lavorativa di accensione del ventilatore
bool fanHdOnTime = false;   // fascia oraria festtiva di accensione del ventilatore
bool heater = false;        // stato del riscaldatore (true=acceso)
bool fan = false;           // stato del ventilatore (true=acceso)

// operating mode
byte operatingMode = 0;     // modalità operativa (0=auto, 1=manual, 2=stop)

// workingday time slots
byte heatWdOn = 3;      // heater working-day switch-on (default value= 3)
byte heatWdOff = 7;     // heater working-day switch-off (default value= 7)
byte fanWdOn = 8;       // ventilation working-day switch-on (default value= 8)
byte fanWdOff = 18;     // ventilation working-day switch-off (default value= 18)

// holiday time slots
byte heatHdOn = 15;     // heater holiday switch-on (default value= 15)
byte heatHdOff = 19;    // heater holiday switch-off (default value= 19)
byte fanHdOn = 11;      // ventilation holiday switch-on (default value= 11)
byte fanHdOff = 20;     // ventilation holiday switch-off(default value= 20)

// time scheduling
unsigned long baseTime = 60000;   // tempo base di schedulazione eventi (default 60sec.)
unsigned long currMill;           // tempo attuale

// thresholds setting
float setTemp = 21;           // temperature threshold (default value= 21)
float setHum = 55;            // humidity threshold (default value= 55)
float hysteresisTemp = 0.5;   // temperature hysteresis (default value= 0.5)
float hysteresisHum = 5;      // humidity hysteresis (default value= 5)

void setup() {

  currMill = millis();        // actual time acquisition

  /* -- WATCHDOG -- */        // valid parameters: 15MS,30MS,60MS,120MS,250MS,500MS,1S,2S,4S,8S
  wdt_enable(WDTO_8S);        // watchdog enable with 8sec. reset time

  /* -- OUTPUT RESET -- */
  pinMode(HEATERPIN, OUTPUT);      // riscaldatore
  digitalWrite(HEATERPIN, HIGH);   // spengo il riscaldatore
  pinMode(FANPIN, OUTPUT);         // ventilatore
  digitalWrite(FANPIN, HIGH);      // spengo il ventilatore

  /* -- SERIAL MONITOR -- */
  if (debugMode == true)
    Serial.begin(9600);      // serial monitor enable

  /* -- SENSORS -- */
  dht.begin();               // inizializza il sensore di temperatura/umidità

  /* -- DISPLAY -- */
  lcd.init();                // inizializza il display
  lcd.clear();
  delay(500);
  lcd.backlight();           // accende la retroilluminazione
  delay(1000);
  displayWelcomeMsg(3000);   // display messaggio di benvenuto per un certo tempo (3sec.)
  delay(1000);

  // first RTC and DHT22 reading
  delay(250);
  getRtcData(0);          // get data from RTC immediately
  delay(250);
  getSensorData(0);       // get data from DHT sensor immediately

  // verifica se giorno lavorativo/festivo
  workingDay = wdCheck(0, dayOfWeek);         // working day verification (true/false)

  // first display update
  delay(250);
  displayFixInfos();                   // display delle informazioni fisse
  delay(100);
  displaySensorData(0);                // update sensor data
  displayTimeData(0);                  // update time data
  displayHeatStatus(0, heater);        // update heater status
  displayFanStatus(0, fan);            // update fan status
  displayProgStatus(0, workingDay);    // update program mode status
  displayModStatus(0, operatingMode);  // update operating mode status

}

void loop() {

  currMill = millis();      // actual time acquisition

  /* -- INPUT -- */
  // impostazione modalità di funzionamento

  // lettura RTC e DHT22
  getRtcData(baseTime);     // get data from RTC every baseTime (default 60sec.)
  getSensorData(baseTime);  // get data from sensor every baseTime (defaul 60sec.)

  /* -- PROCESSING -- */

  // verifica superamento della soglia di temperatura
  if (tempState == false && tem >= setTemp)
    tempState = true;
  if (tempState == true && tem <= (setTemp - hysteresisTemp))
    tempState = false;

  // verifica superamento della soglia di umidità
  if (humState == false && hum >= setHum)
    humState = true;
  if (humState == true && hum <= (setHum - hysteresisHum))
    humState = false;

  // verifica se giorno lavorativo/festivo
  workingDay = wdCheck(0, dayOfWeek);         // working day verification (true/false)

  // verifica se accendere il riscaldatore
  if (operatingMode == 0 && tempState == false)
    heater = timeSlotCheck(0, workingDay, 0); // parametri: intervallo, workingDay, 0=Tem
  else
    heater = false;

  // verifica se accendere il ventilatore
  if (operatingMode == 0 && humState == true)
    fan = timeSlotCheck(0, workingDay, 1);    // parametri: intervallo, workingDay, 1=Hum
  else
    fan = false;

  /* -- OUTPUT -- */
  // blinking of time points every 1sec.
  displayTimePointsBlink(1000);               // time's points blinking

  // display update informations every baseTime (60sec.)
  // displayFixInfos();                       // display delle informazioni fisse
  displaySensorData(baseTime);                // update sensor data
  displayTimeData(baseTime);                  // update time data
  displayHeatStatus(baseTime, heater);        // update heater status
  displayFanStatus(baseTime, fan);            // update fan status
  displayProgStatus(baseTime, workingDay);    // update program moded status
  displayModStatus(baseTime, operatingMode);  // update operating mode status

  // heater on/off
  if (heater == true)
    digitalWrite(HEATERPIN, LOW);   // heater on
  else
    digitalWrite(HEATERPIN, HIGH);  // heater off

  // fan on/off
  if (fan == true)
    digitalWrite(FANPIN, LOW);      // fan on
  else
    digitalWrite(FANPIN, HIGH);     // fan off

  /* -- DEBUG -- */
  if (debugMode == true)
    serialPrintDebugInfos(10000);   // serial print debug informations every 10sec.

  /* -- END -- */
  wdt_reset();          // watchdog restart

}
