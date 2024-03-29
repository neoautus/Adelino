/*
             LUFA Library
     Copyright (C) Dean Camera, 2011.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2011  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/*
  Copyright 2019  Marcond Marchi (marcond [at] gmail [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that all copyright notices appear in
  all copies and that both that the copyright notices and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the names of the authors not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The authors disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the authors be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/*
 * This file was heavily changed from Caterina.c available at:
 * https://github.com/arduino/ArduinoCore-avr/tree/master/bootloaders/caterina
 */

/** \file
 *
 *  Main source file for the CDC class bootloader. This file contains the
 *  complete bootloader logic.
 */

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#include <LUFA/Drivers/USB/USB.h>

#include "Descriptors.h"
#include "Adelino.h"

#define MINIMAL_AVR109

// The very minimal needed for Avrdude to work
#ifdef MINIMAL_AVR109
  #ifndef NO_LOCK_BYTE_WRITE_SUPPORT
    #define NO_LOCK_BYTE_WRITE_SUPPORT
  #endif
  #ifndef NO_EEPROM_BYTE_SUPPORT
    #define NO_EEPROM_BYTE_SUPPORT
  #endif
  #ifndef NO_FLASH_BYTE_SUPPORT
    #define NO_FLASH_BYTE_SUPPORT
  #endif
#endif

/** Contains the current baud rate and other settings of the first virtual
 *  serial port. This must be retained as some operating systems will not open
 *  the port unless the settings can be set successfully.
 */
static CDC_LineEncoding_t LineEncoding =
{
    .BaudRateBPS = 0,
    .CharFormat  = CDC_LINEENCODING_OneStopBit,
    .ParityType  = CDC_PARITY_None,
    .DataBits    = 8
};

/** Current address counter. This stores the current address of the FLASH or
 *  EEPROM as set by the host, and is used when reading or writing to the AVRs
 *  memory (either FLASH or EEPROM depending on the issued command).
 */
static uint32_t CurrAddress;

// Calc how many 40ms ticks for the given millisecond interval
#define TICKS_MS(ms)              ((ms) / 50)

// Bootloader timeout timer
#define AVR_PROGRAMMER_TIMEOUT    TICKS_MS(3000)
#define ESP_PROGRAMMER_TIMEOUT    TICKS_MS(3000)
#define USB_COMM_TIMEOUT          TICKS_MS(250)

// uint16_t -> 3556, uint8_t -> 3504
static volatile uint8_t Timeout;
static volatile uint8_t rts_pulse;

// Bootloader actions
#define ACT_STAY_INTO_BOOTLOADER  TICKS_MS(1000)
#define ACT_BOOTLOADER_TIMEOUT    TICKS_MS(8000)
#define ACT_RESET_ESP8266         TICKS_MS(3000)
#define ACT_RECONFIGURE_ESP8266   TICKS_MS(5000)
#define ACT_FACTORY_DEFAULTS      TICKS_MS(5000)

// Bootloader status
static uint8_t boot_mode;
#define BM_NORMAL_BOOT            '0'
#define BM_KEEP_BOOTLOADER        '1'
#define BM_ESP_RESET              'R'
#define BM_ESP_RECONFIGURE        'E'
#define BM_ESP_FACTORY            'F'

// Serial baud divisor
#define SERIAL_2X_UBBRVAL(Baud) ((((F_CPU / 8) + (Baud / 2)) / (Baud)) - 1)

#ifndef BAUD_RATE
//#define BAUD_RATE   74880L        // ESP8266 Boot post
#define BAUD_RATE   115200L
#endif

uint16_t bootKey = 0x7777;
volatile uint16_t *const bootKeyPtr = (volatile uint16_t *)0x0800;

__attribute__((noreturn))
__attribute__((noinline))
__attribute__((naked))
static void start_sketch (void)
{
    cli ();

    // Disable USART1
    UCSR1B = 0;

    // Undo TIMER1 setup and clear the count before running the sketch
    TIMSK1 = 0;
    TCCR1B = 0;
    TCNT1H = 0;         // 16-bit write to TCNT1 requires high byte be written first
    TCNT1L = 0;

    // Relocate the interrupt vector table to the application section
    MCUCR = (1 << IVCE);
    MCUCR = 0;

    L_LED_OFF();
    ACT_LED_OFF();

    // jump to beginning of application space
    __asm__ volatile("jmp 0x0000");
}

static uint16_t led_counter;
static uint8_t led_control = 0xff;
static uint8_t leds_enabled;

static void CDC_Task (void);

static void run_tasks (void)
{
    CDC_Task ();

    if (leds_enabled)
    {
        if (led_control == 0xff)
        {
            // Breathing animation on ACT/D13 LED indicates bootloader is running
            led_counter++;
            uint8_t p = led_counter >> 8;
            if (p > 127)
            {
                p = 254-p;
            }
            p += p;
            if (((uint8_t)led_counter) > p)
            {
                L_LED_OFF();
                ACT_LED_ON();
            }
            else
            {
                L_LED_ON();
                ACT_LED_OFF();
            }
        }
        else if (led_counter > led_control)
        {
            // Blinking animation on ACT/D13 indicates ESP8266 actions
            led_counter = 0;
            L_LED_TOGGLE();
            ACT_LED_TOGGLE();
        }
    }
}

__attribute__((noinline))
static void putch (char ch)
{
    while (!(UCSR1A & _BV(UDRE1)));
    UDR1 = ch;
}

static uint8_t buffer [256];        // This buffer MUST BE 256 bytes long !!!
static volatile uint8_t buffer_in;  // It is so because we use single bytes as
static uint8_t buffer_out; // indexes, with the natural range 0..255

ISR(USART1_RX_vect, ISR_BLOCK)
{
    uint8_t ch = UDR1;
    uint8_t next = buffer_in + 1;

    // A very simple circular buffer
    if (next != buffer_out)
    {
        buffer [buffer_in] = ch;
        buffer_in = next;
    }
}

__attribute__((noinline))
static uint8_t rstat (void)
{
    // Read the reset switch preserving the port configuration afterwards
    cli ();
    uint8_t save_PORTD = PORTD;
    PORTD |= _BV(5); // Pull up
    uint8_t save_DDRD = DDRD;
    DDRD &= ~_BV(5); // PD5 -> input
    __asm__ volatile ("rjmp .+0");      // Discard small diode capacitance
    uint8_t stat = !(PIND & _BV(5));
    PORTD = save_PORTD;
    DDRD = save_DDRD;
    sei ();
    return (stat);
}

static void reset_esp (uint8_t program_mode)
{
    // GPIO15 is also HSPI_CS, which is connected to SS on
    // AVR SPI; so we MUST ensure GPIO15 LOW for proper boot.
    PORTB &= ~_BV(0); // GPIO15 => LOW
    DDRB |= _BV(0); // PB0 (SS) => OUTPUT

    // D13 is also used to drive GPIO0, which is the main
    // boot mode selector. GPIO0 HIGH for normal boot, GPIO0
    // LOW to enter ROM bootloader.
    if (program_mode)
    {
        // Enter ROM bootloader
        PORTC &= ~_BV(7);
    }
    else
    {
        // Normal boot
        PORTC |= _BV(7);
    }
    DDRC |= _BV(7);

    // We set pin direction only after setting value
    // because if it's not configured yet (hi-z) it
    // will be pulled up. If we are setting it HIGH
    // and configure direction first, we may get a
    // unintended short LOW pulse. Therefore,
    // direction is set only after the intended level
    // is set.

    // Set CHIP_EN low
    PORTE &= ~_BV(2); // PE2 => LOW
    DDRE |= _BV(2); // PE2 => OUTPUT

    // Keep it low at least 1ms for a clean ESP8266 reset
    _delay_ms (1);

    // Set CHIP_EN high
    PORTE |= _BV(2); // PE2 => HIGH
}

__attribute__((noinline))
static void reset_esp_and_wait (uint8_t program_mode)
{
    leds_enabled = 0;
    ACT_LED_ON();
    L_LED_ON();
    reset_esp (program_mode);
    for (Timeout = TICKS_MS(350); Timeout; USB_USBTask ());
    leds_enabled = 1;

    // Ring buffer is now empty again
    buffer_in = buffer_out = 0;
}

/** Configures all hardware required for the bootloader. */
static void setup_hardware (void)
{
    // Disable watchdog if enabled by bootloader/fuses
    MCUSR &= ~(1 << WDRF);

    // Low-carbs wdt_disable()
    WDTCSR = _BV(WDE) | _BV(WDCE);
    WDTCSR = 0;

    // Low-carbs disable clock division
    CLKPR = _BV(CLKPCE);
    CLKPR = clock_div_1;

    // Relocate the interrupt vector table to the bootloader section
    MCUCR = (1 << IVCE);
    MCUCR = (1 << IVSEL);

    LED_SETUP();
    L_LED_OFF();
    ACT_LED_OFF();

    // Init USART1
    UBRR1  = SERIAL_2X_UBBRVAL(BAUD_RATE);
    UCSR1A = _BV(U2X1);                                 // Double speed mode
    UCSR1C = _BV(UCSZ10) | _BV(UCSZ11);                 // Async 8 N 1
    UCSR1B = _BV(RXEN1) | _BV(TXEN1) | _BV(RXCIE1);     // Enable tx and rx

    // Initialize TIMER1 to handle bootloader timeout and LED tasks.
    // The timer is set to 20Hz or once every 50ms, so we can use a single byte
    // for timeout counting (0..254 -> 254 x 50ms = 12700ms or 12.7s).
    // This interrupt is disabled selectively when doing memory reading, erasing,
    // or writing since SPM has tight timing requirements.
    OCR1A = 12499;
    TIMSK1 = _BV(OCIE1A);		                    // enable timer 1 output compare A match interrupt
    TCCR1B = _BV(CS11) | _BV(CS10) | _BV(WGM12);    // 1/64 prescaler on timer 1 input

    // Initialize USB Subsystem
    USB_Init ();

    // Enable global interrupts so that the USB stack can function
    sei ();
}

/** Check whether reset button is pressed during 'ticks' interval */
static uint8_t check_reset (uint8_t ticks)
{
    for (Timeout = ticks; Timeout && rstat (); run_tasks ());
    return (rstat ());
}

/** Main program entry point. This routine configures the hardware required by
 *  the bootloader, then continuously runs the bootloader processing routine
 *  until it times out or is instructed to exit.
 */
int main (void)
{
    // Save the value of the boot key memory before it is overwritten
    uint16_t bootKeyPtrVal = *bootKeyPtr;
    *bootKeyPtr = 0;

    // TODO: PUT THESE INITS ASIDE

    // Check the reason for the reset so we can act accordingly
    uint8_t  mcusr_state = MCUSR;       // store the initial state of the Status register
    MCUSR = 0;                          // clear all reset flags	

    // Watchdog may be configured with a 15 ms period so must disable it before going any further
    wdt_disable ();

    uint8_t sketch_available = pgm_read_word(0) != 0xFFFF;

    if (mcusr_state & (1<<EXTRF))
    {
        // External reset -  we should continue to self-programming mode.
    }
    else if ((mcusr_state & (1<<PORF)) && sketch_available)
    {
        // After a power-on reset skip the bootloader and jump straight to sketch 
        // if one exists.
        start_sketch ();
    } 
    else if ((mcusr_state & (1<<WDRF)) && (bootKeyPtrVal != bootKey) && sketch_available)
    {
        // If it looks like an "accidental" watchdog reset then start the sketch.
        start_sketch ();
    }

    // Setup hardware required for the bootloader
    setup_hardware ();

    // Should we stay inside the bootloader?
    if (check_reset (ACT_STAY_INTO_BOOTLOADER) || !sketch_available)
    {
        // Breathing led
        boot_mode = leds_enabled = BM_KEEP_BOOTLOADER;

        // Check for ESP8266 reset
        if (check_reset (ACT_RESET_ESP8266))
        {
            // Reset ESP8266
            boot_mode = BM_ESP_RESET;
            reset_esp_and_wait (false);

            // Check for ESP8266 reconfiguration (slow blinking)
            if (check_reset (ACT_RECONFIGURE_ESP8266))
            {
                // Reconfiguration is being requested
                boot_mode = BM_ESP_RECONFIGURE;
                led_control = TICKS_MS(200);

                // Check for ESP8266 factory defaults reset
                if (check_reset (ACT_FACTORY_DEFAULTS))
                {
                    // Factory reset is being requested
                    boot_mode = BM_ESP_FACTORY;

                    // Very fast blinking
                    led_control = 1;
                }
            }
        }
    }
    else
    {
        // The normal boot must have a normal bootloader timeout
        boot_mode = leds_enabled = BM_NORMAL_BOOT;
        Timeout = ACT_BOOTLOADER_TIMEOUT;
    }

    // Keep the bootloader running if:
    //
    // a) The boot mode is any other than normal;
    //    or
    // b) The boot mode is normal, but timeout is still running.
    //
    while (boot_mode != BM_NORMAL_BOOT || Timeout)
    {
        run_tasks ();
    }

    // Disconnect from the host - USB interface will be reset later along with the AVR
    USB_Detach();

    // Jump to beginning of application space to run the sketch - do not reset
    start_sketch ();
}

ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
    if (leds_enabled)
    {
        led_counter++;
    }

    if (Timeout)
    {
        Timeout--;
    }
}

//-----------------------------------------------------------------------------
// USB FUNCTIONS
//-----------------------------------------------------------------------------

/** Event handler for the USB_ConfigurationChanged event. This configures the
 *  device's endpoints ready to relay data to and from the attached USB host.
 */
void EVENT_USB_Device_ConfigurationChanged (void)
{
    // Setup CDC Notification, Rx and Tx Endpoints
    Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPNUM, EP_TYPE_INTERRUPT,
                               ENDPOINT_DIR_IN, CDC_NOTIFICATION_EPSIZE,
                               ENDPOINT_BANK_SINGLE);

    Endpoint_ConfigureEndpoint(CDC_TX_EPNUM, EP_TYPE_BULK,
                               ENDPOINT_DIR_IN, CDC_TXRX_EPSIZE,
                               ENDPOINT_BANK_SINGLE);

    Endpoint_ConfigureEndpoint(CDC_RX_EPNUM, EP_TYPE_BULK,
                               ENDPOINT_DIR_OUT, CDC_TXRX_EPSIZE,
                               ENDPOINT_BANK_SINGLE);
}

/** Event handler for the USB_ControlRequest event. This is used to catch and
 *  process control requests sent to the device from the USB host before
 *  passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest (void)
{
    // Ignore any requests that aren't directed to the CDC interface
    if ((USB_ControlRequest.bmRequestType & (CONTROL_REQTYPE_TYPE | CONTROL_REQTYPE_RECIPIENT)) !=
        (REQTYPE_CLASS | REQREC_INTERFACE))
    {
        return;
    }

    // Process CDC specific control requests
    switch (USB_ControlRequest.bRequest)
    {
        case CDC_REQ_GetLineEncoding:
        {
            if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP ();

                // Write the line coding data to the control endpoint
                Endpoint_Write_Control_Stream_LE (&LineEncoding, sizeof(CDC_LineEncoding_t));
                Endpoint_ClearOUT ();
            }
            break;
        }
        case CDC_REQ_SetLineEncoding:
        {
            if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP ();

                // Read the line coding data in from the host into the global struct
                Endpoint_Read_Control_Stream_LE (&LineEncoding, sizeof(CDC_LineEncoding_t));
                Endpoint_ClearIN ();
            }
            break;
        }
        case CDC_REQ_SetControlLineState:
        {
            if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP ();
                Endpoint_ClearStatusStage ();

                // Let us know if we had a pulse on RTS
                rts_pulse |= USB_ControlRequest.wValue & CDC_CONTROL_LINE_OUT_RTS;
            }
            break;
        }
    }
}

static uint8_t usb_byte_available (void)
{
    // This is a strategic place to call the usb tasks, since checking
    // for ready bytes is perhaps the top task inside loops
    USB_USBTask ();

    // Select the OUT endpoint so that the next data byte can be read
    Endpoint_SelectEndpoint (CDC_RX_EPNUM);

    if (Endpoint_IsOUTReceived ())
    {
        if (Endpoint_BytesInEndpoint ())
        {
            return (true);
        }
        Endpoint_ClearOUT ();
    }
    return (false);
}

static uint8_t endpoint_detached (void)
{
    return (!Timeout || USB_DeviceState == DEVICE_STATE_Unattached);
}

/** Retrieves the next byte from the host in the CDC data OUT endpoint, and
 *  clears the endpoint bank if needed to allow reception of the next data
 *  packet from the host.
 *
 *  \return Next received byte from the host in the CDC data OUT endpoint
 */
static uint8_t usb_read_byte (void)
{
    while (!usb_byte_available ())
    {
        if (endpoint_detached ())
        {
            return (0);
        }
    }

    // Fetch the next byte from the OUT endpoint
    return (Endpoint_Read_8 ());
}

static void _usb_flush_endpoint (void)
{
    // Send the endpoint data to the host
    Endpoint_ClearIN ();

    // Wait until the data has been sent to the host
    while (!Endpoint_IsINReady ())
    {
        if (endpoint_detached ())
        {
            return;
        }
    }
}

static void usb_flush (void)
{
    // Select the IN endpoint
    Endpoint_SelectEndpoint (CDC_TX_EPNUM);

    // Do we have bytes to flush actually?
    if (!Endpoint_BytesInEndpoint ())
    {
        return;
    }

    // Is the endpoint full?
    if (!Endpoint_IsReadWriteAllowed ())
    {
        // If a full endpoint's worth of data was sent, we need to send
        // an empty packet afterwards to signal end of transfer
        _usb_flush_endpoint ();
    }

    // Send the endpoint data to the host and wait
    // until the data has been sent to the host
    _usb_flush_endpoint ();
}

/** Writes the next response byte to the CDC data IN endpoint, and sends the
 *  endpoint back if needed to free up the bank when full ready for the next
 *  byte in the packet to the host.
 *
 *  \param[in] Response  Next response byte to send to the host
 */
static void usb_write_byte (const uint8_t Response)
{
    USB_USBTask ();

    // Select the IN endpoint so that the next data byte can be written
    Endpoint_SelectEndpoint (CDC_TX_EPNUM);

    // If IN endpoint full, clear it and wait until ready
    // for the next packet to the host
    if (!Endpoint_IsReadWriteAllowed ())
    {
        _usb_flush_endpoint ();
    }

    // Write the next byte to the IN endpoint
    Endpoint_Write_8 (Response);
}

//-----------------------------------------------------------------------------
// AVR PROGRAMMING FUNCTIONS
//-----------------------------------------------------------------------------

#if !defined(NO_BLOCK_SUPPORT)
/** Reads or writes a block of EEPROM or FLASH memory to or from the
 *  appropriate CDC data endpoint, depending on the AVR910 protocol command
 *  issued.
 *
 *  \param[in] Command  Single character AVR910 protocol command indicating
 *                      what memory operation to perform
 */
#ifdef MINIMAL_AVR109
// Minimal version: Supports only Flash
static void ReadWriteMemoryBlock (const uint8_t Command)
{
    uint16_t BlockSize;
    char     MemoryType;

    bool     HighByte = false;
    uint8_t  LowByte  = 0;

    BlockSize  = (usb_read_byte () << 8);
    BlockSize |=  usb_read_byte ();

    MemoryType =  usb_read_byte ();

    if (MemoryType != 'F')
    {
        // Send error byte back to the host
        usb_write_byte ('?');
        return;
    }

    // Disable timer 1 interrupt - can't afford to process
    // nonessential interrupts while doing SPM tasks
    TIMSK1 = 0;

    /* Check if command is to read memory */
    if (Command == 'g')
    {
        // Re-enable RWW section
        boot_rww_enable ();

        while (BlockSize--)
        {
            // Read the next FLASH byte from the current FLASH page
#if (FLASHEND > 0xFFFF)
            usb_write_byte (pgm_read_byte_far(CurrAddress | HighByte));
#else
            usb_write_byte (pgm_read_byte(CurrAddress | HighByte));
#endif
            // If both bytes in current word have been read,
            // increment the address counter
            if (HighByte)
            {
                CurrAddress += 2;
            }

            HighByte = !HighByte;
        }
    }
    else // Write memory
    {
        uint32_t PageStartAddress = CurrAddress;

        boot_page_erase (PageStartAddress);
        boot_spm_busy_wait ();

        while (BlockSize--)
        {
            // If both bytes in current word have been written,
            // increment the address counter
            if (HighByte)
            {
                // Write the next FLASH word to the current FLASH page
                boot_page_fill (CurrAddress, ((usb_read_byte () << 8) | LowByte));

                // Increment the address counter after use
                CurrAddress += 2;
            }
            else
            {
                LowByte = usb_read_byte();
            }

            HighByte = !HighByte;
        }

        // Commit the flash page to memory
        boot_page_write (PageStartAddress);

        // Wait until write operation has completed
        boot_spm_busy_wait ();

        // Send response byte back to the host
        usb_write_byte ('\r');
    }

    // Re-enable timer 1 interrupt disabled earlier in this routine
    TIMSK1 = (1 << OCIE1A);
}
#else // MINIMAL_AVR109
// Full version: Supports Flash and EEPROM
static void ReadWriteMemoryBlock (const uint8_t Command)
{
    uint16_t BlockSize;
    char     MemoryType;

    bool     HighByte = false;
    uint8_t  LowByte  = 0;

    BlockSize  = (usb_read_byte () << 8);
    BlockSize |=  usb_read_byte ();

    MemoryType =  usb_read_byte ();

    if ((MemoryType != 'E') && (MemoryType != 'F'))
    {
        // Send error byte back to the host
        usb_write_byte ('?');

        return;
    }

    // Disable timer 1 interrupt - can't afford to process
    // nonessential interrupts while doing SPM tasks
    TIMSK1 = 0;

    /* Check if command is to read memory */
    if (Command == 'g')
    {
        /* Re-enable RWW section */
        boot_rww_enable();

        while (BlockSize--)
        {
            if (MemoryType == 'F')
            {
                // Read the next FLASH byte from the current FLASH page
#if (FLASHEND > 0xFFFF)
                usb_write_byte (pgm_read_byte_far (CurrAddress | HighByte));
#else
                usb_write_byte (pgm_read_byte (CurrAddress | HighByte));
#endif

                // If both bytes in current word have been read,
                // increment the address counter
                if (HighByte)
                {
                    CurrAddress += 2;
                }

                HighByte = !HighByte;
            }
            else
            {
                // Read the next EEPROM byte into the endpoint
                usb_write_byte (eeprom_read_byte ((uint8_t*)(intptr_t)(CurrAddress >> 1)));

                // Increment the address counter after use
                CurrAddress += 2;
            }
        }
    }
    else
    {
        uint32_t PageStartAddress = CurrAddress;

        if (MemoryType == 'F')
        {
            boot_page_erase(PageStartAddress);
            boot_spm_busy_wait();
        }

        while (BlockSize--)
        {
            if (MemoryType == 'F')
            {
                // If both bytes in current word have been written,
                // increment the address counter
                if (HighByte)
                {
                    // Write the next FLASH word to the current FLASH page
                    boot_page_fill (CurrAddress, ((usb_read_byte () << 8) | LowByte));

                    // Increment the address counter after use
                    CurrAddress += 2;
                }
                else
                {
                    LowByte = usb_read_byte();
                }

                HighByte = !HighByte;
            }
            else
            {
                // Write the next EEPROM byte from the endpoint
                eeprom_write_byte ((uint8_t*)((intptr_t)(CurrAddress >> 1)), usb_read_byte ());

                // Increment the address counter after use
                CurrAddress += 2;
            }
        }

        // If in FLASH programming mode, commit the page after writing
        if (MemoryType == 'F')
        {
            // Commit the flash page to memory
            boot_page_write (PageStartAddress);

            // Wait until write operation has completed
            boot_spm_busy_wait ();
        }

        // Send response byte back to the host
        usb_write_byte ('\r');
    }

    // Re-enable timer 1 interrupt disabled earlier in this routine
    TIMSK1 = (1 << OCIE1A);
}
#endif // MINIMAL_AVR109
#endif // NO_BLOCK_SUPPORT

//-----------------------------------------------------------------------------
// AVR-109 PROGRAMMER
//-----------------------------------------------------------------------------

/** Task to read in AVR910 commands from the CDC data OUT endpoint, process
 *  them, perform the required actions and send the appropriate response back
 *  to the host.
 */
static void AVR109_programmer (void)
{
    // Start setting programmer timeout
    Timeout = AVR_PROGRAMMER_TIMEOUT;

    while (Timeout)
    {
        // Check if endpoint has a command in it sent from the host
        if (!usb_byte_available ())
        {
            continue;
        }

        // Read in the bootloader command (first byte sent from host)
        uint8_t Command = usb_read_byte ();

        if (Command == 'E')
        {
            //  We nearly run out the bootloader timeout clock,
            // leaving just a few hundred milliseconds so the
            // bootloder has time to respond and service any
            // subsequent requests
            Timeout = TICKS_MS(500);

            // Re-enable RWW section - must be done here in case
            // user has disabled verification on upload.
            boot_rww_enable_safe ();

            // Send confirmation byte back to the host
            usb_write_byte ('\r');
        }
        else if (Command == 'T')
        {
            usb_read_byte ();

            // Send confirmation byte back to the host
            usb_write_byte ('\r');
        }
        else if ((Command == 'L') || (Command == 'P'))
        {
            // Send confirmation byte back to the host
            usb_write_byte ('\r');
        }
        else if (Command == 't')
        {
            // Return ATMEGA128 part code - this is only to allow AVRProg to
            // use the bootloader
            usb_write_byte (0x44);
            usb_write_byte (0x00);
        }
        else if (Command == 'a')
        {
            // Indicate auto-address increment is supported
            usb_write_byte ('Y');
        }
        else if (Command == 'A')
        {
            // Set the current address to that given by the host
            CurrAddress = (usb_read_byte () << 9);
            CurrAddress |= (usb_read_byte () << 1);

            // Send confirmation byte back to the host
            usb_write_byte ('\r');
        }
        else if (Command == 'p')
        {
            // Indicate serial programmer back to the host
            usb_write_byte ('S');
        }
        else if (Command == 'S')
        {
            // Write the 7-byte software identifier to the endpoint
            for (uint8_t CurrByte = 0; CurrByte < 7; CurrByte++)
            {
                usb_write_byte (SOFTWARE_IDENTIFIER [CurrByte]);
            }
        }
        else if (Command == 'V')
        {
            usb_write_byte ('0' + BOOTLOADER_VERSION_MAJOR);
            usb_write_byte ('0' + BOOTLOADER_VERSION_MINOR);
        }
        else if (Command == 's')
        {
            usb_write_byte (AVR_SIGNATURE_3);
            usb_write_byte (AVR_SIGNATURE_2);
            usb_write_byte (AVR_SIGNATURE_1);
        }
#if !defined(MINIMAL_AVR109)
        else if (Command == 'e')
        {
            // Clear the application section of flash
            for (uint32_t CurrFlashAddress = 0;
                 CurrFlashAddress < BOOT_START_ADDR;
                 CurrFlashAddress += SPM_PAGESIZE)
            {
                boot_page_erase (CurrFlashAddress);
                boot_spm_busy_wait ();
                boot_page_write (CurrFlashAddress);
                boot_spm_busy_wait ();
            }

            // Send confirmation byte back to the host
            usb_write_byte ('\r');
        }
#endif // MINIMAL_AVR109
#if !defined(NO_LOCK_BYTE_WRITE_SUPPORT)
        else if (Command == 'l')
        {
            // Set the lock bits to those given by the host
            boot_lock_bits_set (usb_read_byte ());

            // Send confirmation byte back to the host
            usb_write_byte ('\r');
        }
#endif // NO_LOCK_BYTE_WRITE_SUPPORT
#if !defined(MINIMAL_AVR109)
        else if (Command == 'r')
        {
            usb_write_byte (boot_lock_fuse_bits_get(GET_LOCK_BITS));
        }
        else if (Command == 'F')
        {
            usb_write_byte (boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS));
        }
        else if (Command == 'N')
        {
            usb_write_byte (boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS));
        }
        else if (Command == 'Q')
        {
            usb_write_byte (boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS));
        }
#endif // MINIMAL_AVR109
#if !defined(NO_BLOCK_SUPPORT)
        else if (Command == 'b')
        {
            usb_write_byte ('Y');

            // Send block size to the host
            usb_write_byte (SPM_PAGESIZE >> 8);
            usb_write_byte (SPM_PAGESIZE & 0xFF);
        }
        else if ((Command == 'B') || (Command == 'g'))
        {
            // Keep resetting the timeout counter if we're receiving
            // self-programming instructions
            Timeout = AVR_PROGRAMMER_TIMEOUT;
            boot_mode = BM_NORMAL_BOOT;

            // Delegate the block write/read to a separate function for clarity
            ReadWriteMemoryBlock (Command);
        }
#endif // NO_BLOCK_SUPPORT
#if !defined(NO_FLASH_BYTE_SUPPORT)
        else if (Command == 'C')
        {
            // Write the high byte to the current flash page
            boot_page_fill (CurrAddress, usb_read_byte ());

            // Send confirmation byte back to the host
            usb_write_byte('\r');
        }
        else if (Command == 'c')
        {
            // Write the low byte to the current flash page
            boot_page_fill (CurrAddress | 0x01, usb_read_byte ());

            // Increment the address
            CurrAddress += 2;

            // Send confirmation byte back to the host
            usb_write_byte ('\r');
        }
        else if (Command == 'm')
        {
            // Commit the flash page to memory
            boot_page_write (CurrAddress);

            // Wait until write operation has completed
            boot_spm_busy_wait ();

            // Send confirmation byte back to the host
            usb_write_byte ('\r');
        }
        else if (Command == 'R')
        {
#if (FLASHEND > 0xFFFF)
            uint16_t ProgramWord = pgm_read_word_far (CurrAddress);
#else
            uint16_t ProgramWord = pgm_read_word (CurrAddress);
#endif
            usb_write_byte (ProgramWord >> 8);
            usb_write_byte (ProgramWord & 0xFF);
        }
#endif // NO_FLASH_BYTE_SUPPORT
#if !defined(NO_EEPROM_BYTE_SUPPORT)
        else if (Command == 'D')
        {
            // Read the byte from the endpoint and write it to the EEPROM
            eeprom_write_byte ((uint8_t*)((intptr_t)(CurrAddress >> 1)), usb_read_byte());

            // Increment the address after use
            CurrAddress += 2;

            // Send confirmation byte back to the host
            usb_write_byte ('\r');
        }
        else if (Command == 'd')
        {
            // Read the EEPROM byte and write it to the endpoint
            usb_write_byte (eeprom_read_byte((uint8_t*)((intptr_t)(CurrAddress >> 1))));

            // Increment the address after use
            CurrAddress += 2;
        }
#endif // NO_EEPROM_BYTE_SUPPORT
        else if (Command != 27)
        {
            // Unknown (non-sync) command, return fail code
            usb_write_byte ('?');
        }

        usb_flush ();
    }
}

//-----------------------------------------------------------------------------
// ESP8266 PROGRAMMER
//-----------------------------------------------------------------------------

/** This is essentially an USB<->Serial loop, which flushes all data when it
 *  sees SLIP packet END markers (character C0h), and automatically restarts
 *  ESP8266 into programming mode upon start.
 */

static void ESP8266_programmer (void)
{
    // Start with UART buffer empty
    buffer_out = 0;

    // Reset ESP8266 enabling programming mode
    reset_esp (true);

    // Wait until ESP8266 finishes loading ROM Bootloader
    for (Timeout = ESP_PROGRAMMER_TIMEOUT; Timeout; )
    {
        if (buffer_in)
        {
            // Keep emptying the UART receive buffer until
            // ESP8266 finishes its post messages
            buffer_in = 0;

            // Silence is about 250ms
            Timeout = USB_COMM_TIMEOUT;
        }
    }

    // We use RTS pulse to exit the programmer
    for (rts_pulse = 0; !rts_pulse; )
    {
        Timeout = USB_COMM_TIMEOUT;
        uint8_t ch = 0;

        // ESP8266 ----> USB (via AVR)
        if (buffer_in != buffer_out)
        {
            ch = buffer [buffer_out++];
            usb_write_byte (ch);
        }

        // Flush USB every SLIP start/end
        // not the smartest way, but it works(TM)
        if (ch == 0xc0)
        {
            usb_flush ();
        }

        // USB (via AVR) -----> ESP8266
        if ((UCSR1A & _BV(UDRE1)) && usb_byte_available ())
        {
            UDR1 = Endpoint_Read_8 ();
        }
    }

    // Reboot ESP8266 into normal mode
    reset_esp_and_wait (false);
}

//-----------------------------------------------------------------------------
// CDC USB-SERIAL TASK
//-----------------------------------------------------------------------------

/** This is essentially an USB<->Serial loop, which looks for special chars
 *  comming from the USB side:
 *
 *      1Bh (ESC) -> Enter AVR-109 programmer;
 *      C0h (END) -> Enter ESP8266 programmer
 *
 *  All other characters are just echoed. Characters 1Bh and C0h are echoed
 *  when sent from ESP8266 towards USB.
 */
static void CDC_Task (void)
{
    uint8_t ch = 0;
    
    // ESP8266 ----> USB (via AVR)
    while (buffer_in != buffer_out)
    {
        ch = buffer [buffer_out++];
        usb_write_byte (ch);
    }

    usb_flush ();

    // USB (via AVR) -----> ESP8266
    if (!usb_byte_available ())
    {
        return;
    }

    ch = usb_read_byte ();

    if (ch == 0xc0)
    {
        // This probably is the SLIP header for ESP8266 programming
        ESP8266_programmer ();
    }
    else if (ch == 27)
    {
        // TODO: USE ESC+'S' SEQUENCE TO ENTER AVR PROGRAMMER
        // Sync command, let's enter AVR Programmer
        AVR109_programmer ();
    }
    else if (ch == 4)
    {
        usb_write_byte ('\\');
        usb_write_byte ('D');
        usb_write_byte (boot_mode);
    }
    else // We are in a USB<->Serial loop actually, so just echo
    {
        putch (ch);
    }

    led_control = 0xff;
}

// EOF
