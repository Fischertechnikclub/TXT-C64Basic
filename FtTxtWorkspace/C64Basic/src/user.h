/*
 * user.h
 *
 *  Created on: 9 Apr 2026
 *      Author: ad
 */

#ifndef USER_H_
#define USER_H_

#include "display.h" //for color_t

#define LOWRAMSTART 	0x0000
#define LOWRAMLEN	0x800
#define CARTRAMSTART	0x8000
#define CARTRAMLEN	0x2000
#define HIGHRAMSTART	0xC000
#define HIGHRAMLEN	0x1000
#define VIC2START		0xD000
#define VIC2LEN		47
#define SIDSTART		0xD400
#define SIDLEN		16
#define COLORRAMSTART	0xD800
#define COLORRAMLEN	0x400
#define CIA1START		0xDC00
#define CIA1LEN		16
#define CIA2START		0xDD00
#define CIA2LEN		16

//fixed addresses, mostly vectors to other routines that may be hooked
#define MTOPADDR	55 //$37
#define CHRGETADDR	115 //$73 CHRGET
#define CURSORCOL	646 //$286
#define BASICADDR	776 //$308
#define USRADDR		785 //$311
#define ISRADDR		788 //$314
#define NMIADDR		792 //$318
#define RESETVEC	0xFFFC

//default values
#define MTOP		0xA000
#define DEFCURSORCOL	ltBLUE
#define DEFBORDERCOL	ltBLUE
#define DEFBGNDCOL		BLUE
#define BASICEXE	0xA7E4 //BASIC ROM
#define DEFISR		0xEA34 //KERNAL ROM
#define DEFNMI		0xFE47
#define RESET		0xFCE2

//IO region
//VIC II
#define IOBORDER	VIC2START+0x20 //53280
#define IOBGND		VIC2START+0x21 //53281
//CIA1
#define CIA1PORTA	CIA1START
#define CIA1PORTB	CIA1START+1
#define CIA1DDRA	CIA1START+2
#define CIA1DDRB	CIA1START+3
//CIA2
#define CIA2PORTA	CIA2START
#define CIA2PORTB	CIA2START+1
#define CIA2DDRA	CIA2START+2
#define CIA2DDRB	CIA2START+3

//ML adresses
#define TITLEPIC	0x2000 //not used, pictures are loaded directly to framebuffer
#define PICLOADER	900    //poked into lowRam
#define TU1DRU		0x8C00 //printer drivers
#define TUXDRU		0x8E00
#define TU1OBJ		0x9000 //fischertechnik ML
#define WEDGECHAR	0x901A
#define TU2OBJ		0xC800
#define TU3OBJ		0xC000
#define INTERFAC	0xCF00
#define PICTURE		0xA000 //not used, pictures are loaded directly to framebuffer
#define PLOTTER		0xCE00 //fischertechnik ML (poked)
#define ROBOTSYS	0xCDB0 //see ROBOT.SYS
#define ROBOTPOS	0xCEC2 //see ROBOT.SYS
#define ROBOTBOT	0xCEE7 //see ROBOT.SYS

//internal C64 RAM simulation
extern uint8_t lowRam[LOWRAMLEN], highRam[HIGHRAMLEN], cartridge[CARTRAMLEN]; //2k @ 0x0000, 4k @0xC000, 8k @0x8000
void initRam();

extern color_t VIC2Colors[5];//some of the VIC II registers

//extern USER functions, see expression.cpp and parser.cpp
void sys(unsigned);
void poke(unsigned, unsigned char);
double usr(double);
uint8_t peek(uint16_t);
unsigned wedge(const char* line);

void addInputVar(const char *identifier);//some ML introduce new BASIC variables

#endif /* USER_H_ */
