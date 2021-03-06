//*****************************************************************************
//
// simpliciti_hub_dev.c - End Device application for the "Access Point as Data
//                        Hub" SimpliciTI LPRF example.
//
// Copyright (c) 2010-2012 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This file has been adapted from DK-LM3S9D96-EM2-CC2500-SIMPLICITI Firmware Package
// to Stellaris Launchpad
//
//*****************************************************************************

#include <stddef.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "driverlib/debug.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/interrupt.h"
#include "driverlib/flash.h"
#include "utils/ustdlib.h"

//
// SimpliciTI Headers
//
#include "simplicitilib.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>End Device for ``Access Point as Data Hub'' example
//! (simpliciti_hub_dev)</h1>
//!
//! This application offers the end device functionality of the generic
//! SimpliciTI ``Access Point as Data Hub'' example.  Pressing buttons on the
//! display will toggle the corresponding LEDs on the access point board to
//! which this end device is linked.
//!
//! The application can communicate with another SimpliciTI-enabled device
//! equipped with a compatible radio and running its own version of the
//! access point from the ''Access Point as Data Hub'' example or with other
//! development boards running the simpliciti_hub_ap example.
//!
//! To run this binary correctly, the development board must be equipped with
//! an EM2 expansion board with a CC2520EM module installed in the ``MOD1''
//! position (the connectors nearest the oscillator on the EM2).  Hardware
//! platforms supporting SimpliciTI 1.1.1 with which this application may
//! communicate are the following:
//!
//! <ul>
//! <li>eZ430 + RF2500</li>
//! <li>EXP430FG4618 + CC2500 + USB Debug Interface</li>
//! <li>SmartRF04EB + CC2510EM</li>
//! <li>CC2511EM USB Dongle</li>
//! <li>Stellaris Development Board + EM2 expansion board + CC2500EM</li>
//! </ul>
//!
//! Start the board running the access point example first then start the
//! end devices.  The LEDs on the end device will flash once to indicate that
//! they have joined the network.  After this point, pressing one of the
//! buttons on the display will send a message to the access point causing it
//! to toggle either LED1 or LED2 depending upon which button was pressed.
//!
//! For additional information on running this example and an explanation of
//! the communication between the two devices and access point, see section 3.4
//! of the ``SimpliciTI Sample Application User's Guide'' which can be found
//! under C:/StellarisWare/SimpliciTI-1.1.1/Documents assuming that
//! StellarisWare is installed in its default directory.
//
//*****************************************************************************

//*****************************************************************************
//
// This application sets the SysTick to fire every 100mS.
//
//*****************************************************************************
#define TICKS_PER_SECOND 10

//*****************************************************************************
//
// A couple of macros used to introduce delays during monitoring.
//
//*****************************************************************************
#define SPIN_ABOUT_A_QUARTER_SECOND ApplicationDelay(250)
#define SPIN_ABOUT_A_SECOND         ApplicationDelay(1000)

//*****************************************************************************
//
// The number of times we try a transmit and miss an acknowledge before doing
// a channel scan.
//
//*****************************************************************************
#define MISSES_IN_A_ROW  2

//*****************************************************************************
//
// A global system tick counter.
//
//*****************************************************************************
static volatile unsigned long g_ulSysTickCount = 0;

//*****************************************************************************
//
// The states of the 2 "LEDs" on the display.
//
//*****************************************************************************
static tBoolean g_bLEDStates[2];

//*****************************************************************************
//
// Flag used to indicate which button has been pressed.
//
//*****************************************************************************
static unsigned long g_ulButtonPressed;

static uint8_t  g_ucTid = 0;
static linkID_t g_sLinkID = 0;

//*****************************************************************************
//
// The colors of each LED in the OFF and ON states.
//
//*****************************************************************************
#define DARK_GREEN      0x00002000
#define DARK_RED        0x00200000
#define BRIGHT_GREEN    0x0000FF00
#define BRIGHT_RED      0x00FF0000

static unsigned long g_ulLEDColors[2][2] =
{
  {DARK_GREEN, BRIGHT_GREEN},
  {DARK_RED, BRIGHT_RED}
};

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)
{
}
#endif

//*****************************************************************************
//
// This is the handler for this SysTick interrupt.  All it does is increment
// a tick counter.
//
//*****************************************************************************
void
SysTickHandler(void)
{
    //
    // Update our tick counter.
    //
    g_ulSysTickCount++;
}

//*****************************************************************************
//
// A simple delay function which will wait for a particular number of
// milliseconds before returning.  During this time, the application message
// queue is serviced.  The delay granularity here is the system tick period.
//
//*****************************************************************************
void
ApplicationDelay(unsigned long ulDelaymS)
{
    unsigned long ulTarget;

    //
    // What will the system tick counter be when we are finished this delay?
    //
    ulTarget = g_ulSysTickCount + ((ulDelaymS * TICKS_PER_SECOND) / 1000);

    //
    // Hang around waiting until this time.  This doesn't take into account the
    // system tick counter wrapping but, since this takes about 13 and a half
    // years, it's probably not too much of a problem.
    //
    while(g_ulSysTickCount < ulTarget)
    {
    }
}

//*****************************************************************************
//
// Map a SimpliciTI API return value into a human-readable string.
//
//*****************************************************************************
char *
MapSMPLStatus(smplStatus_t eVal)
{
    switch(eVal)
    {
        case SMPL_SUCCESS:          return("SUCCESS");
        case SMPL_TIMEOUT:          return("TIMEOUT");
        case SMPL_BAD_PARAM:        return("BAD_PARAM");
        case SMPL_NO_FRAME:         return("NO_FRAME");
        case SMPL_NO_LINK:          return("NO_LINK");
        case SMPL_NO_JOIN:          return("NO_JOIN");
        case SMPL_NO_CHANNEL:       return("NO_CHANNEL");
        case SMPL_NO_PEER_UNLINK:   return("NO_PEER_UNLINK");
        case SMPL_NO_PAYLOAD:       return("NO_PAYLOAD");
        case SMPL_NOMEM:            return("NOMEM");
        case SMPL_NO_AP_ADDRESS:    return("NO_AP_ADDRESS");
        case SMPL_NO_ACK:           return("NO_ACK");
        case SMPL_TX_CCA_FAIL:      return("TX_CCA_FAIL");
        default:                    return ("Unknown");
    }
}

//*****************************************************************************
//
// Link to the access point and continue processing local button requests
// forever.  This function is called following initialization in main() and
// does not return.
//
//*****************************************************************************
static void
LinkTo(void)
{
    uint8_t     pucMsg[2];
    uint8_t     ucMisses, ucDone;
    uint8_t     ucNoAck;
    smplStatus_t eRetcode;
    unsigned long ulButton;


    //
    // Keep trying to link.  Flash our "LEDs" while these attempts continue.
    //
    while (SMPL_SUCCESS != SMPL_Link(&g_sLinkID))
    {
//        ToggleLED(1);
//        ToggleLED(2);
        SPIN_ABOUT_A_SECOND;
    }

    //
    // Turn off both LEDs now that we are linked.
    //
//    SetLED(1, false);
//    SetLED(2, false);

    //
    // Tell the user all is well.
    //

    //
    // Put the radio to sleep until a button is pressed.
    //
    SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SLEEP, 0);

    //
    // Start of the main button processing loop.
    //
    while (1)
    {

        //
        // Grab the button press flag and clear it.  We do this since the flag
        // could be changed during the loop whenever function
        // WidgetMessageQueueProcess() is called and we don't want to miss any
        // button presses.
        //
        ulButton = g_ulButtonPressed;

        //
        // Has either button been pressed?
        //
        if (ulButton)
        {
            //
            // Clear the main button press flag.
            //
            g_ulButtonPressed = 0;

            //
            // Wake the radio up since we are about to need it.
            //
            SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_AWAKE, 0);

            /* Set TID and designate which LED to toggle */
            pucMsg[1] = ++g_ucTid;
            pucMsg[0] = (ulButton == 1) ? 1 : 2;
            ucDone = 0;
            while (!ucDone)
            {

                //
                // Remember that we have yet to receive an ack from the AP.
                //
                ucNoAck = 0;

                //
                // Try sending the message MISSES_IN_A_ROW times looking for an
                // ack after each transmission.
                //
                for (ucMisses = 0; ucMisses < MISSES_IN_A_ROW; ucMisses++)
                {
                    //
                    // Send the message and request acknowledgement.
                    //
                    eRetcode=SMPL_SendOpt(g_sLinkID, pucMsg, sizeof(pucMsg),
                                          SMPL_TXOPTION_ACKREQ);

                    //
                    // Did we get the ack?
                    //
                    if (eRetcode == SMPL_SUCCESS)
                    {
                        //
                        // Yes - Message acked.  We're done.  Toggle LED 1 to
                        // indicate ack received. */
//                        ToggleLED(1);
                        break;
                    }

                    //
                    // Did we send the message but fail to get an ack back?
                    //
                    if (SMPL_NO_ACK == eRetcode)
                    {
                        //
                        // Count ack failures.  Could also fail because of CCA
                        // and we don't want to scan in this case.
                        //
                        ucNoAck++;
                    }
                }

                //
                // Did we fail to get an ack after every transmission?
                //
                if (MISSES_IN_A_ROW == ucNoAck)
                {

                    //
                    // Message not acked.  Toggle LED 2.
                    //
//                    ToggleLED(2);

#ifdef FREQUENCY_AGILITY
                    //
                    // At this point, we assume we're on the wrong channel so
                    // look for the correct channel by using Ping to initiate a
                    // scan when it gets no reply.  With a successful ping try
                    // sending the message again.  Otherwise, for any error we
                    // get we will wait until the next button press to try
                    // again.
                    //
                    if (SMPL_SUCCESS != SMPL_Ping(g_sLinkID))
                    {
                        ucDone = 1;
                    }
#else
                    ucDone = 1;
#endif  /* FREQUENCY_AGILITY */
                }
                else
                {
                    //
                    // We got the ack so drop out of the transmit loop.
                    //
                    ucDone = 1;

                }
            }

            //
            // Now that we are finished with it, put the radio back to sleep.
            //
            SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SLEEP, 0);
        }

    }
}


//*****************************************************************************
//
// Main application entry function.
//
//*****************************************************************************
int
main(void)
{
    tBoolean bRetcode;

    //
    // Set the system clock to run at 50MHz from the PLL
    //
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                       SYSCTL_XTAL_16MHZ);

    //
    // Configure SysTick for a 10Hz interrupt.
    //
    ROM_SysTickPeriodSet(ROM_SysCtlClockGet() / TICKS_PER_SECOND);
    ROM_SysTickEnable();
    ROM_SysTickIntEnable();


    //
    // Initialize the SimpliciTI BSP.
    //
    BSP_Init();

    BSP_TURN_ON_LED1();
    SPIN_ABOUT_A_SECOND;
    BSP_TURN_ON_LED2();
    SPIN_ABOUT_A_SECOND;

    BSP_TURN_ON_LED3();
    SPIN_ABOUT_A_SECOND;

    //
    // Turn both "LEDs" off.
    //
//    SetLED(1, false);
//    SetLED(2, false);

    //
    // Keep trying to join (a side effect of successful initialization) until
    // successful.  Toggle LEDS to indicate that joining has not occurred.
    //

    while(SMPL_SUCCESS != SMPL_Init(0))
    {
     BSP_TOGGLE_LED1();
      SPIN_ABOUT_A_SECOND;
    }

    //
    // We have joined the network so turn on both "LEDs" to indicate this.
    //
      BSP_TURN_ON_LED3();
//    SetLED(1, true);
//    SetLED(2, true);
//    UpdateStatus(true, "Joined network");

    //
    // Link to the access point which is now listening for us and continue
    // processing.  This function does not return.
    //
    LinkTo();
}
