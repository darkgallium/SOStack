#include <M5Stack.h>
#include <TinyGPS++.h>

#define SERVERTELNB "+33606060606"

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

void loop() {
  if (millis() % 2000 == 0) {
    M5.Lcd.setCursor(0, 70);
    M5.Lcd.println("registered: "+_is_registered());

    M5.Lcd.print(F("gps avail: "));
    if (gps.satellites.value() > 0) {
      M5.Lcd.print(F("yes "));
      M5.Lcd.print(gps.satellites.value());
    }
    else {
      M5.Lcd.print(F("no"));
    }
    M5.Lcd.println();
    
    M5.Lcd.print(F("lat: "));
    printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
    M5.Lcd.println();
    M5.Lcd.print(F("lon: "));
    printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
  }
  
  if(M5.BtnA.wasPressed()) {
    M5.setWakeupButton(BUTTON_A_PIN);
    uint8_t deepSleepGps[] = {0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0x00, 0x00,0x08, 0x00, 0x16, 0x74}; // magic values from https://ukhas.org.uk/guides:ublox_psm
    sendUBX(deepSleepGps, sizeof(deepSleepGps)/sizeof(uint8_t));
    M5.Power.deepSleep();
  }

  if(M5.BtnB.wasPressed()) {
    while(!_is_registered());
    String ccid = _getccid(); //
    _sendSms(SERVERTELNB, "{\"id\":\""+ccid+"\",\"lat\":41.40338,\"lon\": 2.17403}");
  }

  M5.update();
}
