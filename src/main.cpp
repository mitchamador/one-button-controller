#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

// Declare your global variables here

#define _IN(bit) (PINB & _BV(bit))
#define _SET(bit) (PORTB |= (1 << bit))
#define _CLEAR(bit) (PORTB &= ~(1 << bit))

#define KEY (!(_IN(PB0)))

#define OUT1 PB1
#define OUT2 PB2
#define OUT3 PB5

#define SPK1 PB3
#define SPK2 PB4

#define SPK_IN (_IN(SPK1))

// double click support
//#define OUT3_SUPPORT

// extended settings
#define EXT_SETTINGS

// delays in 4ms resolution
#define DELAY_10TH 25
#define LONGKEY 250
#define DEBOUNCE 10

#ifdef OUT3_SUPPORT
#define DOUBLE_CLICK_DELAY 100
#endif

// delays in 0,1s resolution
#define STAGES_DELAY 200
#define SETTINGS_DELAY 120

#ifndef OUT3_SUPPORT
#define MAX_STAGE 3
#define MAX_SETTINGS_STAGE_1 10
#else
#define MAX_STAGE 4
#define MAX_SETTINGS_STAGE_1 12
#endif

#ifndef OUT3_SUPPORT
#define OUT_SETTINGS 0
#define OUT1_MEMORY 1
#define OUT2_MEMORY 2
#define OUT1_SOUND 3
#define OUT2_SOUND 4
#else
#define OUT1_MEMORY 0
#define OUT2_MEMORY 1
#define OUT3_MEMORY 2
#define OUT1_SOUND 3
#define OUT2_SOUND 4
#define OUT3_SOUND 5
#endif

EEMEM uint8_t eSettings = 0;
EEMEM uint8_t eTimer1 = 0;
EEMEM uint8_t eTimer2 = 0;
#ifdef OUT3_SUPPORT
EEMEM uint8_t eTimer3 = 0;
#endif
EEMEM uint8_t eState1 = 0;
EEMEM uint8_t eState2 = 0;
#ifdef OUT3_SUPPORT
EEMEM uint8_t eState3 = 0;
#endif

// timer array in 0,1 s resolution
const unsigned int time[] PROGMEM = {
    0xFFFF, // unlimited
    2,      // 0,2s
    5,      // 0,5s
    10,     // 1s
    20,     // 2s
    300,    // 30s
    600,    // 1m
    1200,   // 2m
    1800,   // 3m
    3000,   // 5m
    6000,   // 10m
    9000,   // 15m
    12000,  // 20m
    18000,  // 30m
    27000,  // 45m
    36000,  // 60m
    54000,  // 90m
};

#define TIME_ARRAY_SIZE sizeof(time) / sizeof(time[0])

void BEEP(uint8_t tone, uint8_t length)
{
  uint8_t i, r;
  for (i = 0; i < length; i++)
  {
    if (SPK_IN)
    {
      _CLEAR(SPK1);
      _SET(SPK2);
    }
    else
    {
      _SET(SPK1);
      _CLEAR(SPK2);
    }
    for (r = 0; r < tone; r++)
    {
      _delay_us(15);
    }
  }
  _CLEAR(SPK1);
  _CLEAR(SPK2);
}

void BEEP_UP(void)
{
  BEEP(25, 150);
  BEEP(15, 250);
}

void BEEP_DOWN(void)
{
  BEEP(15, 250);
  BEEP(25, 150);
}

void BEEP_OK(void)
{
  BEEP(15, 255);
}

#define CONFIG_SOUND_UPDOWN 0
#define CONFIG_MEM_ENABLED OUT1_MEMORY
#define CONFIG_SOUND_ENABLED OUT1_SOUND

#define SOUND_UPDOWN(config) (config & (1 << CONFIG_SOUND_UPDOWN))
#define MEM_ENABLED(config) (config & (1 << CONFIG_MEM_ENABLED))
#define SOUND_ENABLED(config) (config & (1 << CONFIG_SOUND_ENABLED))

typedef struct
{
  uint16_t tTimer;
  uint16_t timeroff;
  uint8_t counter;
  uint8_t config;
  uint8_t mask;
  uint8_t *eTimer;
  uint8_t *eState;
} outConfig;

outConfig out1;
outConfig out2;
#ifdef OUT3_SUPPORT
outConfig out3;
#else
bool outSettings;
#endif

void BEEP_ON(uint8_t config)
{
  if (SOUND_ENABLED(config))
  {
    if (SOUND_UPDOWN(config))
    {
      BEEP_UP();
    }
    else
    {
      BEEP_OK();
    }
  }
}

void BEEP_OFF(uint8_t config)
{
  if (SOUND_ENABLED(config) && SOUND_UPDOWN(config))
  {
    BEEP_DOWN();
  }
}

void changeStateOut(outConfig *out, bool off)
{
  if (off || (PINB & (out->mask)))
  {
    // off
    PORTB &= ~out->mask;
    BEEP_OFF(out->config);
  }
  else
  {
    // on
    PORTB |= out->mask;
    BEEP_ON(out->config);
    out->counter = 0;
  }

  if (MEM_ENABLED(out->config))
  {
    eeprom_write_byte(out->eState, (PINB & out->mask));
  }
}

void incrementTimer(outConfig *out)
{
  if ((PINB & out->mask) && out->tTimer != 0)
  {
    if (out->counter++ >= DELAY_10TH)
    {
      out->counter = 0;
      if (out->timeroff < 0xFFFF)
        out->timeroff++;
    }
  }
  else
    out->timeroff = 0;

  if (out->timeroff > out->tTimer)
  {
    changeStateOut(out, true);
  }
}

// Timer 0 overflow interrupt service routine
ISR(TIM0_OVF_vect, ISR_NAKED)
{
  static uint8_t key_counter = 0;
#ifdef OUT3_SUPPORT
  static uint8_t double_click_counter = 0;
#endif

  // Reinitialize Timer 0 value
  TCNT0 = 0xB5;

  if (KEY) // key pressed
  {
    if (key_counter <= LONGKEY)
    {
      key_counter++;
    }
    if (key_counter == LONGKEY)
    {
      // long keypress
#ifdef OUT3_SUPPORT
      changeStateOut(&out1, false);
      double_click_counter = 0;
#else
      changeStateOut(outSettings ? &out1 : &out2, false);
#endif
    }
  }
  else // key released
  {
#ifdef OUT3_SUPPORT
    if (double_click_counter > 0)
    {
      if (--double_click_counter == 0)
      {
        // single click
        changeStateOut(&out2, false);
      }
    }
#endif
    if (key_counter > DEBOUNCE && key_counter < LONGKEY)
    {
#ifdef OUT3_SUPPORT
      if (double_click_counter > 0)
      {
        double_click_counter = 0;
        // double click
        changeStateOut(&out3, false);
        //BEEP_DOWN();
        //BEEP_DOWN();
        //BEEP_DOWN();
      }
      else
      {
        double_click_counter = DOUBLE_CLICK_DELAY;
      }
#else
      // key press
      changeStateOut(outSettings ? &out2 : &out1, false);
#endif
    }
    key_counter = 0;
  }
  incrementTimer(&out1);
  incrementTimer(&out2);
#ifdef OUT3_SUPPORT
  incrementTimer(&out3);
#endif

  reti();
}

void setOutSettings(outConfig *out, uint8_t config)
{
  out->config = config;
  out->tTimer = pgm_read_word(&time[eeprom_read_byte(out->eTimer)]);

  if (out->tTimer > 20)
  {
    // when timer setting is more than 2 sec, use BEEP_UP()/BEEP_DOWN() when out is switched on/off
    // otherwise use simple BEEP_OK() only when out is switched on
    out->config |= (1 << CONFIG_SOUND_UPDOWN);
  }
  if (out->tTimer != 0xFFFF)
  {
    // disable memory when timer setting is not unlimited
    out->config &= ~(1 << CONFIG_MEM_ENABLED);
  }

  // switch on out when memory is enabled and saved state is on
  if (MEM_ENABLED(out->config) && eeprom_read_byte(out->eState))
  {
    PORTB |= out->mask;
    out->counter = 0;
  }
}

int main(void)
{

  // Declare your local variables here
  uint8_t keytime;
  uint8_t stage_setting;
  uint8_t tSettings;
#ifdef EXT_SETTINGS
  uint8_t i;
  uint8_t max_settings;
  uint8_t stage;
#endif

  // Crystal Oscillator division factor: 1
  //CLKPR = (1 << CLKPCE);
  //CLKPR = (0 << CLKPCE) | (0 << CLKPS3) | (0 << CLKPS2) | (0 << CLKPS1) | (0 << CLKPS0);

  // Input/Output Ports initialization
  // Port B initialization
#ifdef OUT3_SUPPORT
  // Function: Bit5=Out Bit4=Out Bit3=Out Bit2=Out Bit1=Out Bit0=In
  DDRB = (1 << DDB5) | (1 << DDB4) | (1 << DDB3) | (1 << DDB2) | (1 << DDB1) | (0 << DDB0);
  // State: Bit5=0 Bit4=0 Bit3=0 Bit2=0 Bit1=0 Bit0=P
  PORTB = (0 << PORTB5) | (0 << PORTB4) | (0 << PORTB3) | (0 << PORTB2) | (0 << PORTB1) | (1 << PORTB0);
#else
  // Function: Bit5=In Bit4=Out Bit3=Out Bit2=Out Bit1=Out Bit0=In
  DDRB = (0 << DDB5) | (1 << DDB4) | (1 << DDB3) | (1 << DDB2) | (1 << DDB1) | (0 << DDB0);
  // State: Bit5=T Bit4=0 Bit3=0 Bit2=0 Bit1=0 Bit0=P
  PORTB = (0 << PORTB5) | (0 << PORTB4) | (0 << PORTB3) | (0 << PORTB2) | (0 << PORTB1) | (1 << PORTB0);
#endif

  // Timer/Counter 0 initialization
  // Clock source: System Clock
  // Clock value: 18,750 kHz
  // Mode: Normal top=0xFF
  // OC0A output: Disconnected
  // OC0B output: Disconnected
  // Timer Period: 4 ms
  TCCR0A = (0 << COM0A1) | (0 << COM0A0) | (0 << COM0B1) | (0 << COM0B0) | (0 << WGM01) | (0 << WGM00);
  TCCR0B = (0 << WGM02) | (0 << CS02) | (1 << CS01) | (1 << CS00);
  TCNT0 = 0xB5;
  OCR0A = 0x00;
  OCR0B = 0x00;

  // Timer/Counter 0 Interrupt(s) initialization
  TIMSK0 = (0 << OCIE0B) | (0 << OCIE0A) | (1 << TOIE0);

  // External Interrupt(s) initialization
  // INT0: Off
  // Interrupt on any change on pins PCINT0-5: Off
  GIMSK = (0 << INT0) | (0 << PCIE);
  MCUCR = (0 << ISC01) | (0 << ISC00);

  // Analog Comparator initialization
  // Analog Comparator: Off
  // The Analog Comparator's positive input is
  // connected to the AIN0 pin
  // The Analog Comparator's negative input is
  // connected to the AIN1 pin
  ACSR = (1 << ACD) | (0 << ACBG) | (0 << ACO) | (0 << ACI) | (0 << ACIE) | (0 << ACIS1) | (0 << ACIS0);
  ADCSRB = (0 << ACME);
  // Digital input buffer on AIN0: On
  // Digital input buffer on AIN1: On
  DIDR0 = (0 << AIN0D) | (0 << AIN1D);

  // ADC initialization
  // ADC disabled
  ADCSRA = (0 << ADEN) | (0 << ADSC) | (0 << ADATE) | (0 << ADIF) | (0 << ADIE) | (0 << ADPS2) | (0 << ADPS1) | (0 << ADPS0);

#ifdef EXT_SETTINGS
  while (KEY)
  {
    stage = 0;
    keytime = STAGES_DELAY;
    while (KEY)
    {
      _delay_ms(10);
      if (--keytime == 0)
      {
        keytime = STAGES_DELAY;
        stage++;
        if (stage > MAX_STAGE)
          stage = 0;
        for (i = 0; i < stage; i++)
        {
          BEEP_UP();
          _delay_ms(150);
        }
      }
    }
    if (stage > 0)
    {
      _delay_ms(10 * SETTINGS_DELAY);
      max_settings = stage == 1 ? MAX_SETTINGS_STAGE_1 : TIME_ARRAY_SIZE;
      stage_setting = 0;
      keytime = SETTINGS_DELAY;
      while (!KEY)
      {
        _delay_ms(10);
        if (--keytime == 0)
        {
          keytime = SETTINGS_DELAY;
          if (stage_setting < max_settings)
          {
            BEEP_OK();
            stage_setting++;
          }
          else
          {
            do
            {
            } while (!KEY);
            stage_setting = 0;
          }
        }
      }
      if (KEY && stage_setting > 0)
      {
        BEEP_UP();
        BEEP_UP();
        BEEP_UP();
        
        do
        {
        } while (KEY);

        // save settings
        stage_setting--;
        switch (stage)
        {
        case 1:
#ifndef OUT3_SUPPORT
          // stage 1 settings depending on beep's count
          // 1 => key = OUT2, longkey = OUT1      (set OUT_SETTINGS)
          // 2 => key = OUT1, longkey = OUT2      (clear OUT_SETTINGS)
          // 3 => OUT1 memory enabled             (set OUT1_MEMORY)
          // 4 => OUT1 memory disabled            (clear OUT1_MEMORY)
          // 5 => OUT2 memory enabled             (set OUT2_MEMORY)
          // 6 => OUT2 memory disabled            (clear OUT2_MEMORY)
          // 7 => OUT1 sound enabled              (set OUT1_SOUND)
          // 8 => OUT1 sound disabled             (clear OUT1_SOUND)
          // 9 => OUT2 sound enabled              (set OUT2_SOUND)
          // 10 => OUT2 sound disabled            (clear OUT2_SOUND)
#else
          // stage 1 settings depending on beep's count
          // 1 => OUT1 memory enabled             (set OUT1_MEMORY)
          // 2 => OUT1 memory disabled            (clear OUT1_MEMORY)
          // 3 => OUT2 memory enabled             (set OUT2_MEMORY)
          // 4 => OUT2 memory disabled            (clear OUT2_MEMORY)
          // 5 => OUT3 memory enabled             (set OUT3_MEMORY)
          // 6 => OUT3 memory disabled            (clear OUT3_MEMORY)
          // 7 => OUT1 sound enabled              (set OUT1_SOUND)
          // 8 => OUT1 sound disabled             (clear OUT1_SOUND)
          // 9 => OUT2 sound enabled              (set OUT2_SOUND)
          // 10 => OUT2 sound disabled            (clear OUT2_SOUND)
          // 11 => OUT3 sound enabled             (set OUT3_SOUND)
          // 12 => OUT3 sound disabled            (clear OUT3_SOUND)
#endif
          tSettings = eeprom_read_byte(&eSettings);
          if ((stage_setting & 1) == 0)
          {
            tSettings |= (1 << (stage_setting >> 1));
          }
          else
          {
            tSettings &= ~(1 << (stage_setting >> 1));
          }
          eeprom_write_byte(&eSettings, tSettings);
          break;
        case 2:
          // stage 2 and stage 3 (for OUT1 and OUT2 respectively)
          // timer settings depending on beep's count (index in time[] array)
          eeprom_write_byte(&eTimer1, stage_setting);
          break;
        case 3:
          eeprom_write_byte(&eTimer2, stage_setting);
          break;
#ifdef OUT3_SUPPORT
        case 4:
          eeprom_write_byte(&eTimer3, stage_setting);
          break;
#endif
        }
        do
        {
        } while (!KEY);
      }
    }
  }
#else
  while (KEY)
  {
    stage_setting = 0;
    keytime = SETTINGS_DELAY;
    while (KEY)
    {
      _delay_ms(10);
      if (--keytime == 0)
      {
        keytime = SETTINGS_DELAY;
        if (stage_setting <= MAX_SETTINGS_STAGE_1 + (TIME_ARRAY_SIZE * (MAX_STAGE - 1)))
        {
          BEEP_OK();
          stage_setting++;
        }
        else
        {
          do
          {
          } while (KEY);
          stage_setting = 0;
        }
      }
    }
    if (!KEY && stage_setting > 0)
    {
      BEEP_UP();
      BEEP_UP();
      BEEP_UP();

      // save settings
      stage_setting--;
      if (stage_setting < MAX_SETTINGS_STAGE_1)
      {
        tSettings = eeprom_read_byte(&eSettings);
        if ((stage_setting & 1) == 0)
        {
          tSettings |= (1 << (stage_setting >> 1));
        }
        else
        {
          tSettings &= ~(1 << (stage_setting >> 1));
        }
        eeprom_write_byte(&eSettings, tSettings);
      }
      else if (stage_setting < MAX_SETTINGS_STAGE_1 + TIME_ARRAY_SIZE * 1)
      {
        eeprom_write_byte(&eTimer1, stage_setting - MAX_SETTINGS_STAGE_1 - TIME_ARRAY_SIZE * 0);
      }
#ifdef OUT3_SUPPORT
      else if (stage_setting < MAX_SETTINGS_STAGE_1 + TIME_ARRAY_SIZE * 2)
#else
      else
#endif
      {
        eeprom_write_byte(&eTimer2, stage_setting - MAX_SETTINGS_STAGE_1 - TIME_ARRAY_SIZE * 1);
      }
#ifdef OUT3_SUPPORT
      else
      {
        eeprom_write_byte(&eTimer3, stage_setting - MAX_SETTINGS_STAGE_1 - TIME_ARRAY_SIZE * 2);
      }
#endif
      do
      {
      } while (!KEY);
    }
  }
#endif

  tSettings = eeprom_read_byte(&eSettings);

  out1.mask = 1 << OUT1;
  out1.eTimer = &eTimer1;
  out1.eState = &eState1;

  out2.mask = 1 << OUT2;
  out2.eTimer = &eTimer2;
  out2.eState = &eState2;

#ifdef OUT3_SUPPORT
  out3.mask = 1 << OUT3;
  out3.eTimer = &eTimer2;
  out3.eState = &eState2;
#endif

#ifndef OUT3_SUPPORT
  outSettings = tSettings & (1 << OUT_SETTINGS);
#endif

  setOutSettings(&out1, (tSettings >> 0) & ((1 << OUT1_SOUND) | (1 << OUT1_MEMORY)));

  setOutSettings(&out2, (tSettings >> 1) & ((1 << OUT1_SOUND) | (1 << OUT1_MEMORY)));

#ifdef OUT3_SUPPORT
  setOutSettings(&out3, (tSettings >> 2) & ((1 << OUT1_SOUND) | (1 << OUT1_MEMORY)));
#endif

  // Global enable interrupts
  sei();

  for (;;)
  {
  }
}