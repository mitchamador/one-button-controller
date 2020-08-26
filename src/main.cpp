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

#define SPK1 PB3
#define SPK2 PB4

#define SPK_IN (_IN(SPK1))

// delays in 1ms resolution
#define LONGKEY 1000
#define DEBOUNCE 50

// delays in 0,1s resolution
#define STAGES_DELAY 200
#define SETTINGS_DELAY 120

#define EXT_SETTINGS
//#define NAKED

#ifdef EXT_SETTINGS
#ifndef NAKED
#define NAKED
#endif
#endif

#define OUT_SETTINGS 0
#define OUT1_MEMORY 1
#define OUT2_MEMORY 2
#define OUT1_SOUND 3
#define OUT2_SOUND 4

EEMEM uint8_t eSettings = 0;
EEMEM uint8_t eTimer1 = 0;
EEMEM uint8_t eTimer2 = 0;
EEMEM uint8_t eState1 = 0;
EEMEM uint8_t eState2 = 0;

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

#define CONFIG_SOUND_TYPE 0
#define CONFIG_MEM_ENABLED OUT1_MEMORY
#define CONFIG_SOUND_ENABLED OUT1_SOUND

#define SOUND_TYPE(config) (config & (1 << CONFIG_SOUND_TYPE))
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
bool outSettings;


void BEEP_ON(uint8_t config)
{
  if (SOUND_ENABLED(config))
  {
    if (SOUND_TYPE(config))
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
  if ((config & ((1 << CONFIG_SOUND_ENABLED) | (1 << CONFIG_SOUND_TYPE))) == ((1 << CONFIG_SOUND_ENABLED) | (1 << CONFIG_SOUND_TYPE)))
  {
    BEEP_DOWN();
  }
}

void changeStateOut(outConfig* out, bool off)
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
    out->counter = 100;
  }

  if (MEM_ENABLED(out->config))
  {
    eeprom_write_byte(out->eState, (PINB & out->mask));
  }
}

void incrementTimer(outConfig *out) {
    if ((PINB & out->mask) && out->tTimer != 0)
    {
      if (out->counter < 100) {
        out->counter++;
        return;
      }
      out->counter = 0;
      if (out->timeroff < 0xFFFF)
        out->timeroff++;
    }
    else
      out->timeroff = 0;

    if (out->timeroff > out->tTimer)
    {
      changeStateOut(out, true);
    }

}

// Timer 0 overflow interrupt service routine
#ifdef NAKED
ISR(TIM0_OVF_vect, ISR_NAKED)
#else
ISR(TIM0_OVF_vect)
#endif
{
  static uint16_t counter = 0;

  // Reinitialize Timer 0 value
  TCNT0 = 0x6A;

  // Place your code here

  if (KEY) // если нажата кнопка
  {
    if (counter <= LONGKEY)
    {
      counter++;
    }
    if (counter == LONGKEY)
    {
      changeStateOut(outSettings ? &out1 : &out2, false);
    }
  }
  else // если кнопка отжата
  {
    if (counter > 1)
    {
      if (counter > DEBOUNCE && counter < LONGKEY)
      {
        changeStateOut(outSettings ? &out2 : &out1, false);
      }
      counter = 0;
    }

    incrementTimer(&out1);
    incrementTimer(&out2);

  }
#ifdef NAKED
  reti();
#endif
}


void setOutSettings(outConfig *out, uint8_t config) {
  out->config = config;
  out->tTimer = pgm_read_word(&time[eeprom_read_byte(out->eTimer)]);

  if (out->tTimer > 20)
  {
  // when timer setting is more than 2 sec, use BEEP_UP()/BEEP_DOWN() when out is switched on/off
  // otherwise use simple BEEP_OK() only when out is switched on
    out->config |= (1 << CONFIG_SOUND_TYPE);
  } else {
    // disable memory when timer setting is 2 sec or lower
    out->config &= ~(1 << CONFIG_MEM_ENABLED);
  }

  // switch on out when memory is enabled and saved state is on
  if (MEM_ENABLED(out->config) && eeprom_read_byte(out->eState))
  {
    PORTB |= out->mask;
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
  // Function: Bit5=In Bit4=Out Bit3=Out Bit2=Out Bit1=Out Bit0=In
  DDRB = (0 << DDB5) | (1 << DDB4) | (1 << DDB3) | (1 << DDB2) | (1 << DDB1) | (0 << DDB0);
  // State: Bit5=T Bit4=0 Bit3=0 Bit2=0 Bit1=0 Bit0=P
  PORTB = (0 << PORTB5) | (0 << PORTB4) | (0 << PORTB3) | (0 << PORTB2) | (0 << PORTB1) | (1 << PORTB0);

// Timer/Counter 0 initialization
// Clock source: System Clock
// Clock value: 150,000 kHz
// Mode: Normal top=0xFF
// OC0A output: Disconnected
// OC0B output: Disconnected
// Timer Period: 1 ms
TCCR0A=(0<<COM0A1) | (0<<COM0A0) | (0<<COM0B1) | (0<<COM0B0) | (0<<WGM01) | (0<<WGM00);
TCCR0B=(0<<WGM02) | (0<<CS02) | (1<<CS01) | (1<<CS00);
TCNT0=0x6A;
OCR0A=0x00;
OCR0B=0x00;

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
  if (KEY)
  {
    while (1)
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
          if (stage > 3)
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
        max_settings = stage == 1 ? 10 : TIME_ARRAY_SIZE;
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
          // save settings
          stage_setting--;
          switch (stage)
          {
          case 1:
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
            // stage 2 and stage3 (for OUT1 and OUT2 respectively)
            // timer settings depending on beep's count (index in time[] array)
            eeprom_write_byte(&eTimer1, stage_setting);
            break;
          case 3:
            eeprom_write_byte(&eTimer2, stage_setting);
            break;
          }
        }
        do
        {
        } while (!KEY);
      }
    }
  }
#else
  if (KEY)
  {
    while (1)
    {
      stage_setting = 0;
      keytime = SETTINGS_DELAY;
      while (KEY)
      {
        _delay_ms(10);
        if (--keytime == 0)
        {
          keytime = SETTINGS_DELAY;
          if (stage_setting <= 10 + TIME_ARRAY_SIZE + TIME_ARRAY_SIZE)
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
        if (stage_setting < 10)
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
        else if (stage_setting < 10 + TIME_ARRAY_SIZE)
        {
          eeprom_write_byte(&eTimer1, stage_setting - 10);
        }
        else
        {
          eeprom_write_byte(&eTimer2, stage_setting - 10 - TIME_ARRAY_SIZE);
        }
      }
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
  
  outSettings = tSettings & (1 << OUT_SETTINGS);

  setOutSettings(&out1, (tSettings >> 0) & ((1 << OUT1_SOUND) | (1 << OUT1_MEMORY)));

  setOutSettings(&out2, (tSettings >> 1) & ((1 << OUT1_SOUND) | (1 << OUT1_MEMORY)));

  // Global enable interrupts
  sei();

  for (;;)
  {
  }
}