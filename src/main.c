#ifndef F_CPU
#define F_CPU 1200000L
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

#define _IN(bit) (PINB & _BV(bit))
#define _SET(bit) (PORTB |= (1 << bit))
#define _CLEAR(bit) (PORTB &= ~(1 << bit))

#define KEY (!(_IN(PB0)))

#define OUT1 PB1
#define OUT2 PB2

// enable OUT3 with double click (need RSTDISBL)
//#define OUT3 PB5

#define SPK1 PB3
#define SPK2 PB4

#define SPK_IN (_IN(SPK1))

// timer resolution (4 ms)
#define TIMER_RESOLUTION 0.004f
// delays
#define DELAY_10TH    ((uint8_t) (0.1f/TIMER_RESOLUTION))
#define LONGKEY       ((uint8_t) (1.0f/TIMER_RESOLUTION))
#define DEBOUNCE      ((uint8_t) (0.06f/TIMER_RESOLUTION))

// delays in 0,1s resolution
#define STAGES_DELAY 200
#define SETTINGS_DELAY 120

#if !defined(OUT3)

#define MAX_STAGE 3

// bits
#define SWAP_OUTPUTS  7
#define OUT_MEMORY1  0
#define OUT_MEMORY2  1
#define OUT_SOUND    2

// bits shift
#define OUT1_SETTINGS_SHIFT   0
#define OUT2_SETTINGS_SHIFT   3

#define CONFIG_MEMORY_MASK ((1 << OUT_MEMORY1) | (1 << OUT_MEMORY2))
#define CONFIG_SOUND_MASK (1 << OUT_SOUND)

// stage 1 settings depending on beep's count
// stage settings array (mask, value)
const uint8_t settings_stage_1[][2] PROGMEM = {
  // 1 => key = OUT2, longkey = OUT1
  {(uint8_t) ~(1 << SWAP_OUTPUTS), (1 << SWAP_OUTPUTS)},
  // 2 => key = OUT1, longkey = OUT2
  {(uint8_t) ~(1 << SWAP_OUTPUTS), 0},
  // 3 => OUT1 memory enabled (delay 0)
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT1_SETTINGS_SHIFT),  1 << OUT1_SETTINGS_SHIFT},
  // 4 => OUT1 memory enabled (delay 1)
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT1_SETTINGS_SHIFT),  2 << OUT1_SETTINGS_SHIFT},
  // 5 => OUT1 memory enabled (delay 2)
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT1_SETTINGS_SHIFT),  3 << OUT1_SETTINGS_SHIFT},
  // 6 => OUT1 memory disabled
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT1_SETTINGS_SHIFT),  0},
  // 7 => OUT1 sound enabled
  {(uint8_t) ~(CONFIG_SOUND_MASK << OUT1_SETTINGS_SHIFT),   (1 << OUT_SOUND) << OUT1_SETTINGS_SHIFT},
  // 8 => OUT1 sound disabled
  {(uint8_t) ~(CONFIG_SOUND_MASK << OUT1_SETTINGS_SHIFT),   0},
  // 9 => OUT2 memory enabled (delay 0)
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT2_SETTINGS_SHIFT),  1 << OUT2_SETTINGS_SHIFT},
  // 10 => OUT2 memory enabled (delay 1)
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT2_SETTINGS_SHIFT),  2 << OUT2_SETTINGS_SHIFT},
  // 11 => OUT2 memory enabled (delay 2)
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT2_SETTINGS_SHIFT),  3 << OUT2_SETTINGS_SHIFT},
  // 12 => OUT2 memory disabled
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT2_SETTINGS_SHIFT),  0},
  // 13 => OUT2 sound enabled
  {(uint8_t) ~(CONFIG_SOUND_MASK << OUT2_SETTINGS_SHIFT),   (1 << OUT_SOUND) << OUT2_SETTINGS_SHIFT},
  // 14 => OUT2 sound disabled
  {(uint8_t) ~(CONFIG_SOUND_MASK << OUT2_SETTINGS_SHIFT),   0},
};

#else

#define DOUBLE_CLICK_DELAY 100

#define MAX_STAGE 4

#define OUT_MEMORY 0
#define OUT_SOUND 3

#define OUT1_SETTINGS_SHIFT   0
#define OUT2_SETTINGS_SHIFT   1
#define OUT3_SETTINGS_SHIFT   2

#define CONFIG_MEMORY_MASK (1 << OUT_MEMORY)
#define CONFIG_SOUND_MASK (1 << OUT_SOUND)

// stage settings array (mask, value)
const uint8_t settings_stage_1[][2] PROGMEM = {
  // 1 => OUT1 memory enabled
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT1_SETTINGS_SHIFT),  (1 << OUT_MEMORY) << OUT1_SETTINGS_SHIFT},
  // 2 => OUT1 memory disabled
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT1_SETTINGS_SHIFT),  0},
  // 3 => OUT2 memory enabled
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT2_SETTINGS_SHIFT),  (1 << OUT_MEMORY) << OUT2_SETTINGS_SHIFT},
  // 4 => OUT2 memory disabled
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT2_SETTINGS_SHIFT),  0},
  // 5 => OUT3 memory enabled
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT3_SETTINGS_SHIFT),  (1 << OUT_MEMORY) << OUT3_SETTINGS_SHIFT},
  // 6 => OUT3 memory disabled
  {(uint8_t) ~(CONFIG_MEMORY_MASK << OUT3_SETTINGS_SHIFT),  0},
  // 7 => OUT1 sound enabled
  {(uint8_t) ~(CONFIG_SOUND_MASK << OUT1_SETTINGS_SHIFT),   (1 << OUT_SOUND) << OUT1_SETTINGS_SHIFT},
  // 8 => OUT1 sound disabled
  {(uint8_t) ~(CONFIG_SOUND_MASK << OUT1_SETTINGS_SHIFT),   0},
  // 9 => OUT2 sound enabled
  {(uint8_t) ~(CONFIG_SOUND_MASK << OUT2_SETTINGS_SHIFT),   (1 << OUT_SOUND) << OUT2_SETTINGS_SHIFT},
  // 10 => OUT2 sound disabled
  {(uint8_t) ~(CONFIG_SOUND_MASK << OUT2_SETTINGS_SHIFT),   0},
  // 11 => OUT3 sound enabled
  {(uint8_t) ~(CONFIG_SOUND_MASK << OUT3_SETTINGS_SHIFT),   (1 << OUT_SOUND) << OUT3_SETTINGS_SHIFT},
  // 12 => OUT3 sound disabled
  {(uint8_t) ~(CONFIG_SOUND_MASK << OUT3_SETTINGS_SHIFT),   0},
};

#endif

#define CONFIG_SETTINGS_MASK (CONFIG_MEMORY_MASK | CONFIG_SOUND_MASK)

#define MAX_SETTINGS_STAGE_1 (sizeof(settings_stage_1) / sizeof(settings_stage_1[0]))

// when timer setting is more than 2 sec, use BEEP_UP()/BEEP_DOWN() when out is switched on/off
// otherwise use simple BEEP_OK() only when out is switched on
#define SOUND_UPDOWN(out) (out->tTimer > 20)
#define MEM_ENABLED(out) (out->config & CONFIG_MEMORY_MASK)
#define SOUND_ENABLED(out) (out->config & CONFIG_SOUND_MASK)


typedef struct {
  uint8_t settings;
  uint8_t eTimer[3];
  uint8_t eState[3];
} tEepromConfig;

EEMEM tEepromConfig eConfig = {
  ((1 << OUT_SOUND) << OUT2_SETTINGS_SHIFT) | (((0 << OUT_MEMORY2) | (1 << OUT_MEMORY1)) << OUT2_SETTINGS_SHIFT),
  {1, 0, 0},
  {0, 0, 0}
};

// timer array in 0,1 s resolution
const uint16_t time[] PROGMEM = {
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

// delayed start timer array
const uint16_t start_time[] PROGMEM = {
  0,                                          // disable
  ((uint16_t) (0.0f/TIMER_RESOLUTION)) + 1,   // 0 sec
  ((uint16_t) (10.0f/TIMER_RESOLUTION)) + 1,  // 10 sec
  ((uint16_t) (30.0f/TIMER_RESOLUTION)) + 1,  // 30 sec
};

#define TIME_ARRAY_SIZE sizeof(time) / sizeof(time[0])

typedef enum {
  INDEX_OUT1=0,
  INDEX_OUT2,
#if defined(OUT3)
  INDEX_OUT3
#endif  
} t_out_index;

typedef struct
{
  uint16_t tTimer;
  uint16_t timeroff;
  uint8_t counter;
  uint8_t config;
  uint8_t mask;
  uint8_t *eState;
#if !defined(OUT3)  
  uint16_t sTimer;
#endif 
} outConfig;

outConfig out1;
outConfig out2;
#if defined(OUT3)
outConfig out3;
#endif

#if !defined(OUT3)
uint8_t swapOutputs;
#endif

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

void BEEP_ON(outConfig *out)
{
  if (SOUND_ENABLED(out))
  {
    if (SOUND_UPDOWN(out))
    {
      BEEP_UP();
    }
    else
    {
      BEEP_OK();
    }
  }
}

void BEEP_OFF(outConfig *out)
{
  if (SOUND_ENABLED(out) && SOUND_UPDOWN(out))
  {
    BEEP_DOWN();
  }
}

typedef enum {
  OFF = 0,
  ON,
  ON_STARTUP
} tState;

void setOutputState(outConfig *out, tState state)
{
#if !defined(OUT3)
  out->sTimer = 0;
#endif

  if (state == OFF || (PINB & (out->mask)))
  {
    // off
    PORTB &= ~out->mask;
    BEEP_OFF(out);
  }
  else
  {
    // on
    PORTB |= out->mask;
    if (state != ON_STARTUP)
    {
      BEEP_ON(out);
    }
    out->counter = DELAY_10TH;
    out->timeroff = out->tTimer;
  }

  // enable state saving only for unlimited timer setting
  if (MEM_ENABLED(out) && out->tTimer == 0xFFFF && state != ON_STARTUP)
  {
    eeprom_write_byte(out->eState, (PINB & out->mask));
  }
}

void processOutputTimers(outConfig *out)
{
#if !defined(OUT3)
  if (out->sTimer != 0) {
    if (--out->sTimer == 0) {
      setOutputState(out, ON_STARTUP);
    }
  } else
#endif
  if ((PINB & out->mask) && out->tTimer != 0xFFFF)
  {
    if (--out->counter == 0)
    {
      out->counter = DELAY_10TH;
      if (--out->timeroff == 0) {
        setOutputState(out, OFF);
      }
    }
  }
}

// Timer 0 overflow interrupt service routine
ISR(TIM0_OVF_vect, ISR_NAKED)
{
  static uint8_t key_counter = 0;
#if defined(OUT3)
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
#if defined(OUT3)
      setOutputState(&out1, ON);
      double_click_counter = 0;
#else
      setOutputState(swapOutputs ? &out1 : &out2, ON);
#endif
    }
  }
  else // key released
  {
#if defined(OUT3)
    if (double_click_counter > 0)
    {
      if (--double_click_counter == 0)
      {
        // single click
        setOutputState(&out2, ON);
      }
    }
#endif
    if (key_counter > DEBOUNCE && key_counter < LONGKEY)
    {
#if defined(OUT3)
      if (double_click_counter > 0)
      {
        double_click_counter = 0;
        // double click
        setOutputState(&out3, ON);
      }
      else
      {
        double_click_counter = DOUBLE_CLICK_DELAY;
      }
#else
      // key press
      setOutputState(swapOutputs ? &out2 : &out1, ON);
#endif
    }
    key_counter = 0;
  }
  processOutputTimers(&out1);
  processOutputTimers(&out2);
#if defined(OUT3)
  processOutputTimers(&out3);
#endif

  reti();
}

void setOutputSettings(outConfig *out, uint8_t config, uint8_t mask, uint8_t index)
{
  out->mask = mask;
  out->eState = &eConfig.eState[index];

  out->config = config;
  out->tTimer = pgm_read_word(&time[eeprom_read_byte(&eConfig.eTimer[index])]);

  // switch on out when memory is enabled (and saved state is on for unlimited timer)
#if !defined(OUT3)
  if (/*MEM_ENABLED(out) && */(out->tTimer != 0xFFFF || eeprom_read_byte(&eConfig.eState[index]))) {
    out->sTimer = pgm_read_word(&start_time[config & CONFIG_MEMORY_MASK]);
  }
#else
  if (MEM_ENABLED(out) && out->tTimer != 0xFFFF && eeprom_read_byte(eState)) {
    PORTB |= out->mask;
  }
#endif
}

int main(void)
{

  // Declare your local variables here
  uint8_t keytime;
  uint8_t stage_setting;
  uint8_t tSettings;
  uint8_t i;
  uint8_t max_settings;
  uint8_t stage;

  // Crystal Oscillator division factor: 1
  //CLKPR = (1 << CLKPCE);
  //CLKPR = (0 << CLKPCE) | (0 << CLKPS3) | (0 << CLKPS2) | (0 << CLKPS1) | (0 << CLKPS0);

  // Input/Output Ports initialization
  // Port B initialization
#if defined(OUT3)
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
#if 1
        uint8_t *eeAddr;
        if (stage == 1) {
          tSettings = eeprom_read_byte(&eConfig.settings);
          tSettings &= settings_stage_1[stage_setting][0];
          tSettings |= settings_stage_1[stage_setting][1];
          eeAddr = &eConfig.settings;
        } else {
          tSettings = stage_setting;
          eeAddr = &eConfig.eTimer[stage - 2];
        }
        eeprom_write_byte(eeAddr, tSettings);
#else
        switch (stage)
        {
        case 1:
          tSettings = eeprom_read_byte(&eConfig.settings);
          tSettings &= settings_stage_1[stage_setting][0];
          tSettings |= settings_stage_1[stage_setting][1];
          eeprom_write_byte(&eConfig.settings, tSettings);
          break;
        case 2:
          // stage 2 and stage 3 (for OUT1 and OUT2 respectively)
          // timer settings depending on beep's count (index in time[] array)
          eeprom_write_byte(&eConfig.eTimer[OUT1], stage_setting);
          break;
        case 3:
          eeprom_write_byte(&eConfig.eTimer[OUT2], stage_setting);
          break;
#if defined(OUT3)
        case 4:
          eeprom_write_byte(&eConfig.eTimer[OUT3], stage_setting);
#endif
        }
#endif

        do
        {
        } while (!KEY);
      }
    }
  }

  tSettings = eeprom_read_byte(&eConfig.settings);

#if !defined(OUT3)
  swapOutputs = tSettings & (1 << SWAP_OUTPUTS);
#endif

  setOutputSettings(&out1, (tSettings >> OUT1_SETTINGS_SHIFT) & CONFIG_SETTINGS_MASK, 1 << OUT1, INDEX_OUT1);

  setOutputSettings(&out2, (tSettings >> OUT2_SETTINGS_SHIFT) & CONFIG_SETTINGS_MASK, 1 << OUT2, INDEX_OUT2);

#if defined(OUT3)
  setOutputSettings(&out3, (tSettings >> OUT3_SETTINGS_SHIFT) & CONFIG_SETTINGS_MASK, 1 << OUT3, INDEX_OUT3);
#endif

  // Global enable interrupts
  sei();

  for (;;)
  {
  }
}