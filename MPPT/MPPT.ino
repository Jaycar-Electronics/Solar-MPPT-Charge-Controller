
#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>

LiquidCrystal_PCF8574 lcd(0x27); // set the LCD address to 0x27 for a 16 chars and 2 line display

#define RELAY_SIGNAL A6
#define ENC_A A2
#define ENC_B A3
#define ENC_BUTTON A7
#define VOLTAGE_SENSE A0
#define CURRENT_SENSE A1
#define GATE 3

short menu = 0;
#define MAIN_SCREEN 0

String MPPTstatus = "xx";
String sanitisedVoltage = "#.##v";
String sanitisedPower = "#.#W";
String sanitisedPWM = "--%";

//comment out if not debugging
#define DEBUG 1

// scaling factors
#define VOLTAGE_FACTOR 5  //5 millivolts per reading
#define CURRENT_FACTOR 77 //77 milliamps per reading

uint16_t previousPower = 0;
uint16_t previousVoltage = 0;

unsigned short dutyCycle = 125;

bool button_trig = 0;
short enc_value = 0;

// 0123456789ABCDEF
// 0.00v  0.0W  --%

void setup()
{
    Serial.begin(115200);
    Serial.println("LCD...");

    // wait for serial to start
    while (!Serial)
        ;

    checkLCD();

    lcd.setBacklight(255);

    pinMode(RELAY_SIGNAL, OUTPUT);
    pinMode(VOLTAGE_SENSE, INPUT);
    pinMode(CURRENT_SENSE, INPUT);
    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
    pinMode(ENC_BUTTON, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(ENC_B), isr_enc, CHANGE); //change to ENC_A for opposite direction
    attachInterrupt(digitalPinToInterrupt(ENC_BUTTON), isr_button, CHANGE);

    //by default, make sure relay is on, we can turn the load off later
    digitalWrite(RELAY_SIGNAL, HIGH);
}

void loop()
{
    uint16_t voltageReading = analogRead(A0);
    uint16_t currentReading = analogRead(A1);

    //calc power
    uint16_t powerRating = getPower(voltageReading, currentReading);

    if (powerRating > previousPower)
    {
        //power is increasing? then increase voltage
        if (voltageReading > previousVoltage)
        {
            //ramp it up
            increasePWM();
        }
        else
        {
            //our voltage is lowering, so lets slow down.
            decreasePWM();
        }
    }
    else
    {
        if (voltageReading > previousVoltage)
        {
            //our power is decreasing, but voltage is higher, so slow down.
            decreasePWM();
        }
        else
        {
            //our voltage is lower as well, speed up the PWM
            increasePWM();
        }
    }
    analogWrite(GATE, dutyCycle);

    recordStatus(voltageReading, powerRating);

    switch (menu)
    {
    case MAIN_SCREEN:
        lcd.clear();
        lcd.home();
        //         0123456789ABCDEF
        lcd.print("Solar MPPT  (");
        lcd.print(MPPTstatus); //2 char status
        lcd.print(")");

        lcd.setCursor(0, 1);
        lcd.print(sanitisedVoltage);
        lcd.print(" ");
        lcd.print(sanitisedPower);
        lcd.print(" ");
        lcd.print(sanitisedPWM);
        break;
    }
    delay(250);
}

uint16_t getPower(uint16_t voltage, uint16_t current)
{

    //adjust for factor, producing milli-X; 
    voltage *= VOLTAGE_FACTOR;
    current *= CURRENT_FACTOR;

    float v = voltage / 1000;
    float c = current / 1000;
    float wattage = c * v;

#ifdef DEBUG
    Serial.print("voltage: ");
    Serial.print(voltage);
    Serial.print(" Current: ");
    Serial.println(current);
#endif

    //give in terms of milliwatts; let them scale later.
    return wattage * 1000;
}
void recordStatus(uint16_t voltage, uint16_t power)
{

    //if we wanted to record to SD card, we would do so here, writing the voltage and power information for this timestamp.

    previousVoltage = voltage;
    previousPower = power;
}
void increasePWM()
{
    //record on screen
    MPPTstatus = "^^";
    if (dutyCycle < 255)
        dutyCycle += 1;
}
void decreasePWM()
{
    // record on screen
    MPPTstatus = "vv";

    if (dutyCycle > 1)
        dutyCycle -= 1;
}

void checkLCD(void)
{

    Serial.print("checking LCD..");
    Wire.begin();
    Wire.beginTransmission(0x27);
    short error = Wire.endTransmission();
    if (error != 0)
    {
        // we do not have an LCD, do you want to continue?
        Serial.println("None Found!");
        Serial.println("Do you want to continue without an LCD? modify code to continue");

        for (;;)
            ; //halt
    }
    Serial.println("OK");

    lcd.begin(16, 2); // initialize the lcd
}

/***********************
	encoder interrupts
*/
void isr_enc()
{
    if (digitalRead(ENC_A) == digitalRead(ENC_B))
        enc_value++;
    else
        enc_value--;
}

void isr_button()
{                          //called every change
    delayMicroseconds(10); //debounce,
    if (!button_trig && digitalRead(ENC_BUTTON))
    {
        //we test if the button change went HIGH, which means button release
        button_trig = true;
    }
}

short read_encoder()
{ // consume encoder value
    short ret = 0;
    if (enc_value)
        ret = enc_value;
    enc_value = 0;
    return ret;
}