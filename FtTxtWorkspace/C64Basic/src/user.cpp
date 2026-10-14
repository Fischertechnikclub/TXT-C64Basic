//---------------------------------------------------------------------------
#include <iostream>
#include "expression.h"
#include "data.h"
#include "common.h"
#include "interface.h"
#include "user.h"
#include "display.h"
//---------------------------------------------------------------------------

uint8_t lowRam[LOWRAMLEN], highRam[HIGHRAMLEN], cartridge[CARTRAMLEN]; //2k @ 0x0000, 4k @0xC000, 8k @0x8000
static char specialChar = 0x5C; //wedge character '\\'

union lohi { uint16_t word; struct {uint8_t lo, hi;} bytes;};
//some special addresses
static lohi nmi = {DEFNMI}, isr = {DEFISR}, basic={BASICEXE}, mtop={MTOP}; //not really used, also in lowRam
static lohi usrAddress={0};
color_t VIC2Colors[5] = {DEFBORDERCOL, DEFBGNDCOL, DEFBGNDCOL, DEFBGNDCOL, DEFBGNDCOL};
uint8_t Cia1[CIA1LEN], Cia2[CIA2LEN];

                            //BLACK,     WHITE,      RED,       CYAN,         PURPLE,      GREEN,      BLUE,      YELLOW,
const char *fgndcolor[] = {"\x1b[30m", "\x1b[37m", "\x1b[31m", "\x1b[36m", "\x1b[35;2m", "\x1b[32m", "\x1b[34m", "\x1b[33m", //black,white,red,cyan,purple,green,blue,yellow
                           //ORANGE,     BROWN,       PINK,         dGREY,        mGREY,        ltGREEN,      ltBLUE,       ltGREY
                            "\x1b[35m", "\x1b[31;2m", "\x1b[31;1m", "\x1b[37;2m", "\x1b[37;2m", "\x1b[32;1m", "\x1b[34;1m", "\x1b[37;1m"};
const char *bgndcolor[] = {"\x1b[40m", "\x1b[47m", "\x1b[41m", "\x1b[46m", "\x1b[45;2m", "\x1b[42m", "\x1b[44m", "\x1b[43m",//black,white,red,cyan,purple,green,blue,yellow
                            "\x1b[45m", "\x1b[41;2m", "\x1b[41;1m", "\x1b[47;2m", "\x1b[47;2m", "\x1b[42;1m", "\x1b[44;1m", "\x1b[47;1m"};

static inline void setWord(uint16_t addr, uint16_t val)
{ if(addr>=LOWRAMSTART && addr < LOWRAMSTART+LOWRAMLEN-1)
	*(uint16_t*)(lowRam+addr) = val;
}

void initRam()
{ memset(lowRam, 0, sizeof(lowRam));
  memset(highRam, 0, sizeof(highRam));
  memset(cartridge, 0, sizeof(cartridge));
  lowRam[CURSORCOL] = DEFCURSORCOL; //text color
  setWord(MTOPADDR, MTOP);
  setWord(BASICADDR, BASICEXE);
  setWord(USRADDR, 0); //uninitialised
  setWord(ISRADDR, DEFISR);
  setWord(NMIADDR, DEFNMI);
  setWord(MTOPADDR, MTOP);
}


//The plotter machine language part is poked at 0xCE00
//The trainingsrobotor ML (ROBOT.SYS) is poked at 0xCD80
//INTERFACE.64 ML is loaded at 0xCF00
//TU1.OBJ is loaded at 0x9000
//a picture loader is poked at 900 (0x384)
static void sys(unsigned addr, unsigned par)//this is for ft, sys only accepts the address of the machine language routine which can then process the rest of the line (in this case arg is processed by BASIC)
{ std::string txt;
  int arg = 0;
  switch(par)
  { case 0xFF: txt = "OFF "; arg = 0; break; //255 AUS
	case 0x55: txt = "CW  "; arg = 1; break; //85 RECHTS/EIN
	case 0xAA: txt = "CCW "; arg = 2; break; //170 LINKS
  }
  switch(addr)
  { case PLOTTER:  //INIT plotter
			        break;
	case INTERFAC:  info("interface.64: INIT"); //diagnose.bas/interface.64: INIT
	                initInterface();
	  		        break;
	case ROBOTSYS+2:reset();  //INIT trainingsroboter
			        break;
	case PLOTTER+4:
	case ROBOTSYS+0xE:
	case INTERFAC+4: //info("interface.64: M1 <- " + std::to_string(par)); //diagnose.bas/interface.64: M1
			 info("M1 " + txt);
			 setMotor(1, arg);
			 break;
	case ROBOTSYS+0x12:
	case PLOTTER+8:
	case INTERFAC+8: //info("interface.64: M2 <- " + std::to_string(par)); //diagnose.bas/interface.64: M2
			 info("M2 " + txt);
			 setMotor(2, arg);
			 break;
	case ROBOTSYS+0x18:
	case PLOTTER+12:
	case INTERFAC+12: //info("interface.64: M3 <- " + std::to_string(par)); //diagnose.bas/interface.64: M3
			 info("M3 " + txt);
			 setMotor(3, arg);
			 break;
	case PLOTTER+16:
	case ROBOTSYS+0x1A:
	case INTERFAC+16: //info("interface.64: M4 <- " + std::to_string(par)); //diagnose.bas/interface.64: M4
			 info("M4 " + txt);
			 setMotor(4, arg);
			 break;
	case 0x9000: //used in Computing Experimental
		//this routine replaces many original routines with dedicated versions
		//$308 (776,777) = $90C8 //this one is tested by the application to see if TU1.OBJ was loaded
		//$300=$C000 warm reset
		//$330=$9412 load
		//$332=$941B save
		//$328=$C024 stop
		//$318=$C027 nmi
		//$316=$C02A brk
		//$32E=$C02A unused
		//$326=$C02D CHROUT
		//$324=$C021 CHRIN
		//$238=$3837 ??? MTOP (end of basic)
		//$31A=$942A open
		//$31C=$9424 close
		//also copies $C400-$C800 from TU3.OBJ to $FC00
			 setWord(BASICADDR, TU1OBJ+0xC8);
			 basic.word = 0x90C8;//deprecated
			 break;
	case ROBOTPOS:     setPos(0, par);//P1
			 break;
	case ROBOTPOS+4:   setPos(1, par);//P2
			 break;
	case ROBOTPOS+8:   setPos(2, par);//P3
			 break;
	case ROBOTPOS+12:  setPos(3, par);//P4
			 break;
	case ROBOTBOT:     robot();//ROBOT
			 break;
	case INTERFAC+0xEF:info("interface.64: change STOP routine");//53231 diagnose.bas/interface.64: change STOP routine
		     break;
	default: info("SYS call to unknown address" + std::to_string(addr));
	         return;
  }
}

void restoreTerminalSettings();

void sys(unsigned addr)
{ if(sym==COMMA) nextsym();
  if(addr==900) //"FISCHER" SYS 900,"titel.pic", 1 string arrgument
  { const std::string& picfile = parseStringExpression();
    if(Display::Instance()==nullptr)
    { Display pic;
      pic.load(picfile.c_str());
    }
    else
    	Display::Instance()->load(picfile.c_str());
  }
  else if(   (addr >= INTERFAC+4   && addr <= INTERFAC+16   )//CF04-CF14 (INTERFACE.64) 0 or 1 numeric argument
		  || (addr >= PLOTTER+4    && addr <= PLOTTER+16    )//plotter/scanner
		  || (addr >= ROBOTSYS+0xE && addr <= ROBOTSYS+0x11E)//ROBOT.SYS
		  )
  { unsigned arg = parseExpression();
    sys(addr, arg);
  }
  else if(addr==0xFCE2)//used by diagnose.bas to exit the program
  { info("\x1b[0mSYS RESET");
    restoreTerminalSettings();
    exit(1);
  }
  else sys(addr, 0);//regular SYS without arguments (INIT, ROBOT)
}

double usr(double x)
{ double v = 0;
  if(    usrAddress.word==0xCE50  //plotter/scanner
	  || usrAddress.word==0xCE1A  //trainingsroboter
	  || usrAddress.word==0xCF50  //diagnose.bas/interface.64
  )
	v = ftinterface->usr(x); //handles all cases
  else info("USR call to undefined/unknown address " + std::to_string(usrAddress.word));
  return v;
}

void poke(unsigned ad, unsigned char byte)
{ //const char *bgndcolor[] = {"\x1b[40m","\x1b[47m","\x1b[41m","\x1b[46m","\x1b[45;2m","\x1b[42m","\x1b[44m","\x1b[43m"};//black,white,red,cyan,purple,green,blue,yellow
  //const char *fgndcolor[] = {"\x1b[30m","\x1b[37m","\x1b[31m","\x1b[36m","\x1b[35;2m","\x1b[32m","\x1b[34m","\x1b[33m"};//black,white,red,cyan,purple,green,blue,yellow
  switch(ad)
  { case MTOPADDR  : mtop.bytes.lo = byte; break;
	case MTOPADDR+1: mtop.bytes.hi = byte;
	                 info("set BASIC top address " + std::to_string(mtop.word));
	                 break;
	case CHRGETADDR: info("set CHRGET ");
	                 break;
	case CURSORCOL:  info("set current cursor color to " + std::to_string(byte));
                     if(Display::Instance()) Display::Instance()->setFG((color_t)(byte&0x0F));
	                 std::cout << fgndcolor[byte&0x0F];
	                 break;
	case USRADDR  :  usrAddress.bytes.lo = byte; break;
	case USRADDR+1:  usrAddress.bytes.hi = byte;
	                 info("set USR address " + std::to_string(usrAddress.word));
	                 break;
	case BASICADDR  :basic.bytes.lo = byte; break;
	case BASICADDR+1:basic.bytes.hi = byte;
	                 info("set BASIC character dispatch address " + std::to_string(basic.word));
	                 break;
	case ISRADDR  :  isr.bytes.lo = byte; info("set IRQ address " + std::to_string(isr.word));
	                 break;
	case ISRADDR+1:  isr.bytes.hi = byte; info("set IRQ address " + std::to_string(isr.word));
	                 break;
	case NMIADDR  :  nmi.bytes.lo = byte; info("set NMI address " + std::to_string(nmi.word));
	                 break;
	case NMIADDR+1:  nmi.bytes.hi = byte;
	                 info("set NMI address " + std::to_string(nmi.word));
	                 break;
	//TU1.OBJ area
	case WEDGECHAR:  specialChar = byte;
                     break;
	case WEDGECHAR+1: //background color, these colors are colors used in the picture
	case WEDGECHAR+2: //color 1 'fischer'
	case WEDGECHAR+3: //color 2 'technik Turtle"
	case WEDGECHAR+4: //color 3  grid (foreground color)
	case WEDGECHAR+5: //sprite color (arrow color)
	                 break;
//>=$D000 IO adresses
	case IOBORDER:   VIC2Colors[0] = (color_t)(byte&15);
                     info("set border colour to " + std::to_string(byte));  //0xD020
	                 return;
	case IOBGND:     VIC2Colors[1] = (color_t)(byte&15);
	                 std::cout << bgndcolor[byte&0x0F];
	                 if(Display::Instance()) Display::Instance()->setBG(VIC2Colors[1]);
	                 info("set background colour 0 to " + std::to_string(byte));//$D021
	                 return;
	case IOBGND+1:   VIC2Colors[2] = (color_t)(byte&15);
                     info("set background colour 1 to " + std::to_string(byte));
                     return;
	case IOBGND+2:   VIC2Colors[3] = (color_t)(byte&15);
                     info("set backrgound colour 2 to " + std::to_string(byte));
                     return;
	case IOBGND+3:   VIC2Colors[4] = (color_t)(byte&15);
                     info("set background colour 3 to " + std::to_string(byte));
	                 return;
	case CIA2PORTA:  info("CIA#2 PORT A " + std::to_string(byte)); //56576
	                 Cia2[ad-CIA2START] = byte;
	                 return;
	default:         break;
  }
  if(ad >= LOWRAMSTART && ad < LOWRAMSTART+LOWRAMLEN) lowRam[ad-LOWRAMSTART] = byte;
  else if (ad >= HIGHRAMSTART && ad < HIGHRAMSTART+HIGHRAMLEN) highRam[ad-HIGHRAMSTART] = byte;
  else if (ad >= CARTRAMSTART && ad < CARTRAMSTART+CARTRAMLEN) cartridge[ad-CARTRAMSTART] = byte;
  else if (ad >= COLORRAMSTART && ad < COLORRAMSTART+25*40)
  {
#if DIRECTTEXT==0
	  ad -= COLORRAMSTART;
	  if(TextDisplay::Instance()) TextDisplay::Instance()->writeColor(ad/40, ad%40, (color_t)(byte&15));
#endif
  }
  else
  { //throw "unallocated RAM " + std::to_string(ad);
	  info("unallocated RAM " + std::to_string(ad));
  }
}

uint8_t peek(uint16_t ad)
{ switch(ad)
  { case BASICADDR  : //return basic.bytes.lo; //not used anymore, now returned from lowRam and set by initRam()
    case BASICADDR+1: //return basic.bytes.hi; //return  90C8 as basic character dispatch address 144<<8 = 0x9000, 200 = 0xC8
    case CURSORCOL:   break;
//IO addresses
    case CIA1PORTB: info("reading CIA#1 PORT B"); return Cia1[ad-CIA1START]; //56321 CIA#1 PORT B, bits 0-4 indicate that 1 of the joystick buttons was pressed (up/down/left/right/fire)
    case CIA2PORTA: info("reading CIA#2 PORT A"); return Cia2[ad-CIA2START]; //56576 bits 1-0 VIC memory bank, bits 7-2 serial bus, program FISCHER pokes this to set VIC II memory to $C000 (upper 16k)
    case IOBORDER: return VIC2Colors[0];     //$D020 Border color
    case IOBGND:   return VIC2Colors[1]; //$D021 Background color
//KERNAL addresses
    case RESETVEC  : return RESET&0xFF; //reset vector FCE2
    case RESETVEC+1: return RESET>>8;
    case 58504     : return '6'; //taken from the welcome string, identifies machine as a C64
    case 58505     : return '4';
    default: info("peek " + std::to_string(ad)); break;
  }
  if(ad >= LOWRAMSTART && ad < LOWRAMSTART+LOWRAMLEN) return lowRam[ad-LOWRAMSTART];      //just read back what was poked
  else if (ad >= HIGHRAMSTART && ad < HIGHRAMSTART+HIGHRAMLEN) return highRam[ad-HIGHRAMSTART];
  else if (ad >= CARTRAMSTART && ad < CARTRAMSTART+CARTRAMLEN) return cartridge[ad-CARTRAMSTART];
  else return 0; //throw "unallocated RAM";
}

void addInputVar(const char *identifier) //ft experimental machine language routines generate BASIC variables
{ if (vars.find(identifier) == vars.end())
	//vars[identifier] = new NumVar(Var::real, identifier);
	vars[identifier] = std::make_unique<NumVar>(Var::real, identifier);
}

void assignInputs()
{	uint8_t in = getInputs();
	char name[3] = {'E', '1', '\0'};
	for(int i = 0; i < 8; i++)
	{ name[1] = '1' + i;
	  Variables::iterator v = vars.find(name);
	  if(v!=vars.end())
	     v->second->set(in & (1<<i) ? 1 : 0);
	}
}

//Turtle
#ifdef ENGLISH
#define BRAKE	VAL
#define BACK	'B'
#else //German
#define BACK	'Z' //Zuruck
#define BRAKE	'B' //Bremse
#endif

//turtle kinematics: 2 pulses/revolution of worm axle, worm axle to wheel axle is 1:20, wheel is 60mm diameter
//1 pulse (step) is 9 degrees wheel rotation which amounts to approx. 5mm (1.5pi)
//the wheels are approx. 105mm apart, hence a 360deg rotation is approx. 330mm (105pi) which is 70 steps for each motor, 1 step is approx. 5 degrees
void turtle(char cmd, int arg)
{ static int x, y, k, s; //note that x, y and k(urs) are computed rather than measured and are only updated after the move
  //printf("turtle T%c,%d", cmd, arg);
  assignInputs(); //turtle programs do not use \DE to request inputs E1-E8, so we do it here, in step mode it should be frequent enough
  switch(cmd)
  {  //case 'B': //Bremse
     case BRAKE:
			   break; //arg = 0-255, set duration of reverse motor current to stop the motor fast, N/I
	 case 'I': //Init
			   x = y = k = s = 0;
			   addInputVar("TX");
			   addInputVar("TY");
			   addInputVar("TK");
			   addInputVar("TS");
			   vars["TX"]->set(0);
			   vars["TY"]->set(0);
			   vars["TK"]->set(0);
			   vars["TS"]->set(0);
               //initInterface(); //should have been done with \IN before
			   break;
	 case 'K': //Kurs
	 case 'H': //Heading
			   vars["TK"]->set(k);
			   break;
	 case 'L': //Links
	 case 'R': //Rechts
			   arg %= 360;
			   if(arg<0) arg += 360;
			   s = arg/5;
			   //vars["TS"]->set(s);
			   setTurtle(cmd=='L'? 1 : 2, s);
			   if(cmd=='L') k -= 5*s; else k += 5*s; //5 degrees per step
			   k = (k+360)%360;
			   //vars["TS"]->set(s);
			   break;
	 case 'V': //Vorwarts
	 case 'F': //Forward
			   if(arg >= 0 && arg < 32768)
			   { s = setTurtle(0, arg); //in steps of 5mm
				 x += s*5*cos(k*M_PI/180.0); //in mm
				 y += s*5*sin(k*M_PI/180.0);
				 vars["TS"]->set(s);
			   }
			   break;
	 case 'X': //X-Pos
			   vars["TX"]->set(x);
			   break;
	 case 'Y': //Y-Pos
			   vars["TY"]->set(y);
			   break;
	 //case 'Z': //Zuruck
	 case BACK:
			   if(arg >= 0 && arg < 32768)
			   { setTurtle(3, arg); //in steps of 5mm
				 x -= arg*5*cos(k*M_PI/180.0); //in mm
				 y -= arg*5*sin(k*M_PI/180.0);
			   }
			   break;
  }
}

//With hindsight it was better to have the lexer return a special symbol to the parser and let the parser handle the wedge
/*
 * the assumption was that the parameters to wedge commands were literal stings or numbers, it turns out that
 * some examples use variables, so instead of an inputScanner we should use the regular scanner/parser.
 */
//computing experimental  uses custom basic commands of the form \CC [, par1[, par]] ,
//this differs from the plotter and the trainingsroboter which use SYS and USR()
//the machine code for these commands is obtained from TU1.OBJ, TU2.OBJ and TU3.OBJ.
//these load machine code in sections $9000-$9FEA, $B340-$BAE8 and $C440-$D11A resp.
//the code below is not based on the machine language but on the description in the manual
unsigned wedge(const char* line) //return the number of consumed characters if the statement line is consumed or 0 for unrecognised trigger
{ if(*line != specialChar) return 0;   //ft defined: british pound sign in C64 basic (PETSCII) but shows as backslash in ASCII
  if(strlen(line) < 3) return 0;//not a valid command
  char first = line[1];  //usually 2 character command, this is the first
  char sec= line[2];    //second command char
  //now we have the opcode we continue with the regular parser for parameter parsing
#ifndef PARSERWEDGE
  scan->start(line, 3); //start the scanner after the \XX. This effectively replaces the current scanline with a shorter version of itself
  nextsym();//expect COMMA, COLON or EOLN. NOTE that this is a recursive call because wedge was called from nextsym()
#endif
  //fprintf(stderr,"Wedge %s: op=%c%c sym=%s\n", line, first, sec, symbol.c_str());
  switch(first)  //i==3 + number of spaces
  { case 'I': if(sec=='N')     // \IN
			  { addInputVar("EX");
				addInputVar("EY");
				addInputVar("E1");
				addInputVar("E2");
				addInputVar("E3");
				addInputVar("E4");
				addInputVar("E5");
				addInputVar("E6");
				addInputVar("E7");
				addInputVar("E8");
                initInterface();
			  }
			  break;
	case '1': case '2': case '3': case '4': //motor outputs
			  { uint8_t m = first-'0';
				switch(sec)
				{ case 'A': //anhalten //X in english
				  case 'X': setMotor(m, 0);
				            break;
				  case 'L': setMotor(m, 1); //links, left
				            break;
				  case 'R': setMotor(m, 2); //rechts, right
				            break;
				  //step commands run until rising edge on E(2m)
				  case 'V': //F in english
				  case 'F': stepMotor(m, 1);//forward 1 step
				            break;
				  case 'Z': // B in english
				  case 'B': stepMotor(m, 2);//back 1 step
				            break;
				  default:  break;
				}
			  }
			  break;
	case 'D': if(sec=='E')   // \DE digital eingange
			  {/* get digital inputs and set vars E1-E8 */  //DI in english
		        assignInputs();
			  }
			  break;
	case 'E': if(sec=='X')      // \EX
			  {/* get analog input EX set var EX */
		        int v = getEX();
				if(v==0) v = 1;
				vars["EX"]->set(v);
			  }
			  else if(sec=='Y')  // \EY
			  { /* get analog input EY set var EY */
			        int v = getEY();
				if(v==0) v=1; //set to one because example code crashes on zero (lOG(0))
				vars["EY"]->set(v);
			  }
			  break;
#ifdef TURTLEGRAPHICS
	case 'G': /* Graphic commands (on screen plot commands, many have comma separated parameters, in parenthesis the english command
			     GE(GON), GA(GEND), G0, G1, GC, GPRINT,
				 GK(GH), GX, GY, GF(GT)
				 GSAVE,"name", GLOAD,"name",
				 GV,n(GF) GZ,n(GB) GL,n GR,n GH(GS),n GS(GP),n[,n]
				 Deutsch/English clash on K/H, V/F and H/S
			  */
	      {  Turtle *gfx = Turtle::Instance();
		      if (sec=='E' || sec==ON)
		      {  if (gfx==nullptr)
		    	 { //gfx = new Turtle;
		    	   gfx = new Mixed;
		    	   if(gfx==nullptr) throw("NEW TURTLE FAILED!");
		    	 }
		      }
		      else if(sec==LOAD || sec==SAVE)
		      { if(sym==COMMA) nextsym(); else ERRORMSG("COMMA <string expression> expected");
		    	std::string fn = parseStringExpression() + ".PIC";
		        if(Display::Instance()==nullptr)
		        { Display pic; //C64 does not require \GE prior to \GLOAD, \GSAVE, so we use a temporary Display object
		          if (sec == SAVE) pic.save(fn.c_str(), VIC2Colors[1]);
		          else             pic.load(fn.c_str(), (color_t)peek(CURSORCOL), VIC2Colors[1]);
		        }
		        else //use the existing instance
		        { if (sec == SAVE) Display::Instance()->save(fn.c_str(), VIC2Colors[1]);
		          else             Display::Instance()->load(fn.c_str(), (color_t)peek(CURSORCOL), VIC2Colors[1]);
		        }
		        break;
		      }
		      if(gfx==nullptr)
		      { if(!(sec == 'A' || sec==END)) fprintf(stderr,"Turtle Graphics not initialised, Display = '%s'", Display::Instance()?Display::Instance()->Name():"nullptr");
		      }
			  else
			  { double arg1 = 0; int arg2 = 0;
			    if(sym==COMMA)
				{ nextsym();
				  arg1 = parseExpression();
				  if(sym==COMMA)
				  { nextsym();
				    arg2 = parseExpression();   //second arg only for GStift colour code
				  }
				}
				gfx->graph(sec, arg1, arg2);
			  }
		      if(sec == 'A' || sec==END)//destroy the turtle
		      {  delete gfx;
		         TextDisplay *txt = new TextDisplay();
		         txt->clear();
		      }
	      }
#else
	          info("TURTLEGRAPHICS not available");
#endif //TURTLEGRAPHICS
		      break;
	case 'T': /* Turtle commands, similar to G commands but controlling hardware 'turtle'
			     TI, TX, TY, TK(TH),
				 TV(TF),n TZ(TB),n TR,n TL,n
				 TB(TVAL),n
			  */
			  { double arg1 = 0;
				if(sym==COMMA)
				{ nextsym();
				  arg1 = parseExpression();
				}
				turtle(sec, arg1);
			  }
			  break;
	default:  fprintf(stderr, "unknown wedge %s", line);
  }
#ifndef PARSERWEDGE
  //return scan->pos();//now this is the question, the calling nextsym is messed up by scan->start
  return 0; //return 0 as not to advance the char index in the scanner because the nested calls have already moved it
#else
  return 1;
#endif
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

