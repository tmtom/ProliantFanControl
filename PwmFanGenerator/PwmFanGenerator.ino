/*
 * Simple test tool for PWM controlled FANs (especially for the "HpFanControl" using Arduino Nano
 * 
 * Rotary encoder on pins A2, A3 (no external pull-ups or caps)
 * HD44780 LCD 16x2 on RS=D12, EN=D11, DB4-7 = D5, D4, D3, D2
 * PWM output on D9
 * 
 * Required libraries:
 *  - TimerOne
 *  - RotaryEncoder
 *  - HD44780
 * 
 * Settings via defines:
 *    STARTPOS   - starting duty, default si 10%
 *    INVERT_PWM - whether to invert PWM or not. Default is to invert as this is what Gen8 Proliant is using
 *    _SERDBG_   - debug (also print set PWM) via serial, 9600
 */

#include <RotaryEncoder.h>

#include <hd44780.h>
#include <hd44780ioClass/hd44780_pinIO.h>

#include <TimerOne.h>



#define _SERDBG_
#undef _SERDBG_

#define STARTPOS 10

// Setup a RoraryEncoder for pins A2 and A3:
RotaryEncoder encoder(A2, A3);

// using D6 for FAN control
const int fanPin = TIMER1_A_PIN; // aka 9


// Setup LCD
//                RS  EN   DB4 DB5 DB6 DB7
hd44780_pinIO lcd(12, 11, 5, 4, 3, 2);

const int LCD_COLS = 16;
const int LCD_ROWS = 2;

#define INVERT_PWM
//#undef INVERT_PWM

#ifdef INVERT_PWM
#define SetFan(duty)   Timer1.pwm(fanPin, 1023L - (((duty) * 1023L) / 100L))
#else
#define SetFan(duty)   Timer1.pwm(fanPin, ((duty) * 1023L) / 100L)
#endif

void PrintPwm(int pos)
{
    lcd.setCursor(0,1);
    if(pos < 10)
    {
#ifdef _SERDBG_
      Serial.print("  ");
#endif
      lcd.print("  ");
    }
    else
    {
      if(pos < 100)
      {
#ifdef _SERDBG_
        Serial.print(" ");
#endif
        lcd.print(" ");
      }
    }
    lcd.print(String(pos));
    lcd.print("% ");
#ifdef _SERDBG_
    Serial.print(String(pos));
    Serial.println("%");
#endif
}

void setup()
{
  Timer1.initialize(40);  // 40 us = 25 kHz
  
#ifdef _SERDBG_
  Serial.begin(9600);
  Serial.println("PWM FAN control");
#endif

  // You may have to modify the next 2 lines if using other pins than A2 and A3
  PCICR |= (1 << PCIE1);    // This enables Pin Change Interrupt 1 that covers the Analog input pins or Port C.
  PCMSK1 |= (1 << PCINT10) | (1 << PCINT11);  // This enables the interrupt for pin 2 and 3 of Port C.

  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.print("PWM FAN control");
  PrintPwm(STARTPOS);
  encoder.setPosition(STARTPOS);
  SetFan(STARTPOS);
} // setup()



// The Interrupt Service Routine for Pin Change Interrupt 1
// This routine will only be called on any signal change on A2 and A3: exactly where we need to check.
ISR(PCINT1_vect) {
  encoder.tick(); // just call tick() to check the state.
}


// Read the current position of the encoder and print out when changed.
void loop()
{
  static int pos = STARTPOS;

  int newPos = encoder.getPosition();
  if (pos != newPos)
  {
#ifdef _SERDBG_
    Serial.print(newPos);
    Serial.println();
#endif    
    if (newPos < 0)
    {
      newPos = 0;
      encoder.setPosition(newPos);
    }
    else
    {
      if (newPos > 100)
      {
        newPos = 100;
        encoder.setPosition(newPos);
      }
      else
      {
        PrintPwm(newPos);
        SetFan(newPos);
      }
    }
    pos = newPos;
  }
} // loop ()

