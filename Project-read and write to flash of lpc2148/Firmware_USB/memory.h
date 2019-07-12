/*----------------------------------------------------------------------------
 *      Name:    MEMORY.H
 *      Purpose: USB Memory Storage Demo Definitions
 *      Version: V1.10
 *----------------------------------------------------------------------------
 *      This software is supplied "AS IS" without any warranties, express,
 *      implied or statutory, including but not limited to the implied
 *      warranties of fitness for purpose, satisfactory quality and
 *      noninfringement. Keil extends you a royalty-free right to reproduce
 *      and distribute executable files created using this software for use
 *      on Philips LPC2xxx microcontroller devices only. Nothing else gives
 *      you the right to use this software.
 *
 *      Copyright (c) 2005-2006 Keil Software.
 *---------------------------------------------------------------------------*/

#define CCLK            60000000    /* CPU Clock */

/* LED Definitions */
#define LED_MSK         0x00FF0000  /* P1.16..23 */
#define LED_RD          0x00010000  /* P1.16 */
#define LED_WR          0x00020000  /* P1.17 */
#define LED_CFG         0x00400000  /* P1.22 */
#define LED_SUSP        0x00800000  /* P1.23 */

/* MSC Disk Image Definitions */
#define MSC_ImageSize   0x00001000
#define MSC_ImageStart  0x00010000

extern const unsigned char DiskImage[MSC_ImageSize];   /* Disk Image */
