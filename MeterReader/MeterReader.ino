// Monitor the house electricity meter
// 400 rotations every kW - therefore one trigger = 2.5W consumed
// Use a Wemos D1 mini  Pin D5 (GPIO14) is used for the trigger count


#include <ESP8266WiFi.h>
#include <PubSubClient.h>


#define wifi_ssid ""
#define wifi_password ""

#define mqtt_server ""
#define mqtt_user ""
#define mqtt_password ""

#define meter_topic "sensor/power-meter-counter"
#define meter_power_topic "sensor/power-meter-counter/power"
#define meter_vss_topic "sensor/power-meter-counter/volts"
#define meter_kWhrs_topic "sensor/power-meter-counter/kWhrs"
#define client_name "ESP8266-power-meter"

#define PIN_COUNT_TRIGGER 14
#define UPDATE_INTERVAL_mS 60000     // update every 1 minutes

volatile unsigned int PulseCount = 0;   // don't go bigger without changing int2str function
volatile unsigned long PulseTime0 = 0;   // time between most recent pulses in uSec
volatile unsigned long PulseTime1 = 0;   // time between next most recent pulse in uSec

unsigned long LastTime = 0;
double MeterReading_kWhrs = 0.0;
unsigned int PreviousPulseCount = 0;
unsigned int DeltaPulseCount = 0;

WiFiClient espClient;
PubSubClient client(espClient);
ADC_MODE(ADC_VCC);

void setup() 
{
  attachInterrupt(digitalPinToInterrupt(PIN_COUNT_TRIGGER), TriggerCountISR, RISING);
  //Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}


ICACHE_RAM_ATTR void TriggerCountISR()
{
  PulseTime1 = PulseTime0;
  PulseTime0 = micros();
  PulseCount++;
}

void setup_wifi() 
{
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
  }
}

void reconnect_mqtt() 
{
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    // Attempt to connect
    if (client.connect(client_name, mqtt_user, mqtt_password)) 
    {
      client.subscribe(meter_kWhrs_topic);  // Success connecting so subscribe to topic
    } 
    else 
    {
      // Failed - Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length)
{
  char SetkWhrs[20];

  if (length < 19)    // if payload is too long ignore it
  {
    for (int i = 0; i < length; i++) 
    {
      SetkWhrs[i]=(char)payload[i];
    }
    SetkWhrs[length] = NULL;
    MeterReading_kWhrs = atof(SetkWhrs);
  }
}


void loop() 
{
  int Vcc = 0;
  char TempStr[20];  // make sure it is big enough for size of number
  unsigned long LastPulseDuration = 0;
  unsigned long LastPulseTime = 0;
  float AveragePower = 0.0;
  unsigned int LastPulseCount = 0;
  
  if (!client.connected()) 
  {
    reconnect_mqtt();
  }
  client.loop();

  unsigned long now = millis();
  if (now - LastTime > UPDATE_INTERVAL_mS) 
  {
    LastTime = LastTime + UPDATE_INTERVAL_mS;   
    
    Vcc = ESP.getVcc();     // Dummy read to get rid of the first read value
    Vcc = ESP.getVcc();     // Now average out four readings
    Vcc = Vcc + ESP.getVcc();
    Vcc = Vcc + ESP.getVcc();
    Vcc = Vcc + ESP.getVcc();
    Vcc = Vcc / 4;

    cli(); //noInterrupts();
    LastPulseCount = PulseCount;
    LastPulseTime = PulseTime0;
    LastPulseDuration = PulseTime0 - PulseTime1;
    sei(); //interrupts();

    //******************************************************************
    //*  37.5W -> 4min per revolution
    //*  75W -> 2min
    //*  150W -> 1min
    //*  300W -> 30sec
    //*  600W -> 15sec
    //*  900W -> 10sec
    //*  2kW -> 4.5sec
    //*  5kW -> 1.8sec
    //*  1kW -> 9sec 
    //*  9kW -> 1sec
    //*  14.4kW -> 0.625sec (this is max speed as meter only rated to 60A
    //*
    //******************************************************************

    if ((micros() - LastPulseTime) > (4 * 60 * 1000 * 1000))     // Last pulse more than 4 minutes ago assume something wrong send error code
    {
      AveragePower = -1.0;   // Use this to signal invalid value - too low power reading     
    }
    else if (LastPulseDuration < 625000)    // Invalid as meter can only read to 60A - ie disc spinning faster than 0.625 seconds
    {
      AveragePower = -2.0;   // Use this to signal invalid value - too high power reading
    }
    else
    {
      AveragePower = 9000000000.0 / LastPulseDuration;       // equals (1 / 400) / (duration of pulse / (3600 * 1000 * 1000)) * (1000 Watts)
    }

    dtostrf(AveragePower, -8, 1, TempStr);   // The negative means left justify

    client.publish(meter_topic, int2str(LastPulseCount), false);    // false so the messages aren't retained
    client.publish(meter_power_topic, TempStr, false);    // false so the messages aren't retained
    client.publish(meter_vss_topic, int2str(Vcc), false);    // false so the messages aren't retained

    if (MeterReading_kWhrs > 0.0)    // only calculate kWhrs if we already have a starting value
    {
      DeltaPulseCount = LastPulseCount - PreviousPulseCount;
      MeterReading_kWhrs = MeterReading_kWhrs + (0.0025 * DeltaPulseCount);

      dtostrf(MeterReading_kWhrs, -12, 4, TempStr);   // The negative means left justify
      client.publish(meter_kWhrs_topic, TempStr, true);    // we want this retained so we update after reset/power out
    }
    PreviousPulseCount = LastPulseCount;
  }
}


char _int2str[7];
char* int2str( register int i ) {
  register unsigned char L = 1;
  register char c;
  register boolean m = false;
  register char b;  // lower-byte of i
  // negative
  if ( i < 0 ) {
    _int2str[ 0 ] = '-';
    i = -i;
  }
  else L = 0;
  // ten-thousands
  if( i > 9999 ) {
    c = i < 20000 ? 1
      : i < 30000 ? 2
      : 3;
    _int2str[ L++ ] = c + 48;
    i -= c * 10000;
    m = true;
  }
  // thousands
  if( i > 999 ) {
    c = i < 5000
      ? ( i < 3000
          ? ( i < 2000 ? 1 : 2 )
          :   i < 4000 ? 3 : 4
        )
      : i < 8000
        ? ( i < 6000
            ? 5
            : i < 7000 ? 6 : 7
          )
        : i < 9000 ? 8 : 9;
    _int2str[ L++ ] = c + 48;
    i -= c * 1000;
    m = true;
  }
  else if( m ) _int2str[ L++ ] = '0';
  // hundreds
  if( i > 99 ) {
    c = i < 500
      ? ( i < 300
          ? ( i < 200 ? 1 : 2 )
          :   i < 400 ? 3 : 4
        )
      : i < 800
        ? ( i < 600
            ? 5
            : i < 700 ? 6 : 7
          )
        : i < 900 ? 8 : 9;
    _int2str[ L++ ] = c + 48;
    i -= c * 100;
    m = true;
  }
  else if( m ) _int2str[ L++ ] = '0';
  // decades (check on lower byte to optimize code)
  b = char( i );
  if( b > 9 ) {
    c = b < 50
      ? ( b < 30
          ? ( b < 20 ? 1 : 2 )
          :   b < 40 ? 3 : 4
        )
      : b < 80
        ? ( i < 60
            ? 5
            : i < 70 ? 6 : 7
          )
        : i < 90 ? 8 : 9;
    _int2str[ L++ ] = c + 48;
    b -= c * 10;
    m = true;
  }
  else if( m ) _int2str[ L++ ] = '0';
  // last digit
  _int2str[ L++ ] = b + 48;
  // null terminator
  _int2str[ L ] = 0;  
  return _int2str;
}

