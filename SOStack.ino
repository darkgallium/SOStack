#include <M5Stack.h>
#include <TinyGPS++.h>

#define SERVERTELNB "+3306060606"
#define WATCHDOGINTV 10

TinyGPSPlus gps;
HardwareSerial sim(2);
HardwareSerial ss(0);
String _buffer;

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    while (ss.available()) {
      gps.encode(ss.read());
    }
  } while (millis() - start < ms);
}

String _readSerialSim(uint32_t timeout)
{
    uint64_t timeOld = millis();

    while (!sim.available() && !(millis() > timeOld + timeout))
    {
        delay(13);
    }

    String str;
    
    while(sim.available())
    {
        if (sim.available()>0)
        {
            str += (char) sim.read();
        }
    }
    
    return str;
}

bool _sendSms(String num, String msg)
{
    sim.print(F("AT+CMGF=1\r\n"));   //Text mode
    _buffer = _readSerialSim(5000);
    sim.print(F("AT+CMGS=\""));
    sim.print(num);
    sim.print(F("\"\r\n"));
    _buffer = _readSerialSim(5000);

    sim.print (msg);
    sim.print (char(26));
    delay(500);
    _buffer = _readSerialSim(5000);

    if ( (_buffer.indexOf("ER")) == -1)
    {
        return false;
    }
    else return true;
}


String _getccid() {
    sim.print (F("AT+CCID\r\n"));
    _buffer = _readSerialSim(1000);
    _buffer.trim();

    if (_buffer.endsWith("OK")) {
      _buffer.remove(0,8);
      _buffer.remove(22);
      return _buffer;
    }
    
    return "";
}

boolean _is_registered() {
    sim.print (F("AT+CREG?\r"));
    _buffer = _readSerialSim(5000);
    _buffer.trim();
    
    if (_buffer.endsWith("OK")) {
      _buffer.remove(0,18);
      _buffer.remove(3);
      return _buffer == "0,1";
    }
    
    return false;
}

String _get_network() {
    sim.print (F("AT+COPS?\r\n"));
    _buffer = _readSerialSim(2000);
    _buffer.trim();
    
    if (_buffer.endsWith("OK")) {
      _buffer.remove(0,23);
      return _buffer;
    }
    
    return "** no signal **";
}

String _get_signal_quality() {
    sim.print (F("AT+CSQ\r\n"));
    _buffer = _readSerialSim(2000);
    _buffer.trim();
    
    if (_buffer.endsWith("OK")) {
      _buffer.remove(0,15);
      _buffer.remove(7);
      int i = _buffer.toInt();
      return String(-113+i*2)+" dbm";
    }
    
    return "** no signal **";
}

void _siminfo() {
    sim.print (F("AT\r\n"));
    M5.Lcd.println(_readSerialSim(10000));
    sim.print (F("AT+CSQ\r"));
    M5.Lcd.println(_readSerialSim(10000));
    sim.print (F("AT+CCID\r"));
    M5.Lcd.println(_readSerialSim(10000));
    sim.print (F("AT+CREG?\r"));
    M5.Lcd.println(_readSerialSim(10000));
    sim.print (F("AT+COPS?\r"));
    M5.Lcd.println(_readSerialSim(10000));
    delay(500);
}

static void printInt(unsigned long val, bool valid, int len)
{
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  M5.Lcd.print(sz);
  smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t)
{
  if (!d.isValid())
  {
    M5.Lcd.print(F("********** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    M5.Lcd.print(sz);
  }
  
  if (!t.isValid())
  {
    M5.Lcd.print(F("******** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    M5.Lcd.print(sz);
  }

  printInt(d.age(), d.isValid(), 5);
  smartDelay(0);
}


static void printFloat(float val, bool valid, int len, int prec)
{
  if (!valid)
  {
    while (len-- > 1)
      M5.Lcd.print('*');
    M5.Lcd.print(' ');
  }
  else
  {
    M5.Lcd.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      M5.Lcd.print(' ');
  }
  smartDelay(0);
}

void sendUBX(uint8_t *MSG, uint8_t len) {
  for(int i=0; i<len; i++) {
    ss.write(MSG[i]);
  }
  //Serial.println();
}

void setup() {
  M5.begin();

  sim.begin(9600);
  delay(2000); // delay for SIM800L power on
  //_siminfo();
  M5.Lcd.println("init sim");

  ss.begin(9600);
  M5.Lcd.println("init serial");

  if (M5.Power.isResetbyDeepsleep()) {
    uint8_t wakeUpGps[] = {0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0xFF, 0x87, 0x00, 0x00, 0x94, 0xF5};
    sendUBX(wakeUpGps, sizeof(wakeUpGps)/sizeof(uint8_t));
  }

  _buffer.reserve(255);

  M5.Lcd.clear();
}

bool watchdogMode = false;

void loop() {
  if (millis() % 5000 == 0) {
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.println("gsm network: "+_get_network());
    M5.Lcd.println("gsm signal: "+_get_signal_quality());
  }

  if (millis() % 2000 == 0) {
    M5.Lcd.setCursor(0, 70);

    M5.Lcd.print(F("gps avail: "));
    if (gps.satellites.value() > 0) {
      M5.Lcd.print(F("yes ("));
      M5.Lcd.print(gps.satellites.value());
      M5.Lcd.print(F(") "));
    }
    else {
      M5.Lcd.print(F("** no **"));
    }
    M5.Lcd.println();
    
    M5.Lcd.print(F("lat: "));
    printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
    M5.Lcd.println();
    M5.Lcd.print(F("lon: "));
    printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
    M5.Lcd.println();
    M5.Lcd.print(F("date: "));
    printDateTime(gps.date, gps.time);

    smartDelay(1000);
  }
  
  if(M5.BtnA.wasPressed()) {
    M5.setWakeupButton(BUTTON_A_PIN);
    uint8_t deepSleepGps[] = {0xB5, 0x62, 0x06, 0x11, 0x02, 0x00, 0x08, 0x01, 0x22, 0x92}; // magic values from https://ukhas.org.uk/guides:ublox_psm
    sendUBX(deepSleepGps, sizeof(deepSleepGps)/sizeof(uint8_t));
    M5.Power.deepSleep();
  }

  if(M5.BtnB.wasPressed()) {
    while(!_is_registered());
    if (gps.location.isValid()) {
      while(!_sendSms(SERVERTELNB, "{\"id\":\"0\",\"type\":\"sos\",\"lat\":"+String(gps.location.lat(), 6)+",\"lon\": "+String(gps.location.lng(), 6)+"}"));
    }
   }

   if(M5.BtnC.wasPressed()) {
    watchdogMode = true;
   }

   if (watchdogMode && millis() % WATCHDOGINTV) {
    while(!_is_registered());
    if (gps.location.isValid()) {
      while(!_sendSms(SERVERTELNB, "{\"id\":\"0\",\"type\":\"watchdog\",\"lat\":"+String(gps.location.lat(), 6)+",\"lon\": "+String(gps.location.lng(), 6)+"}"));
    }
   }
   
  M5.update();
}
