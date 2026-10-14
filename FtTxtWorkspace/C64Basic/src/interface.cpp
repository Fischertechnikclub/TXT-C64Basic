//---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>

#include "interface.h"
#include "common.h"
//#include "serialib.h"


//---------------------------------------------------------------------------
// Common debugging stuff for RoboProLib
unsigned int DebugFlags;
FILE *DebugFile;

Interface *ftinterface = nullptr;

uint8_t getInputs() { if (ftinterface) return ftinterface->getInputs(); return -1;}
uint16_t getEX()    { if (ftinterface) return ftinterface->getEX(); return -1;}
uint16_t getEY()    { if (ftinterface) return ftinterface->getEY(); return -1;}
void setOutput(uint8_t O, bool on)                  { if (ftinterface) ftinterface->setOutput(O, on);}
void setMotor(uint8_t M, uint8_t c)                 { if (ftinterface) ftinterface->setMotor(M, c);}
void stepMotor(uint8_t M, uint8_t c, uint8_t E) { if (ftinterface) ftinterface->stepMotor(M, c, E);}
//void initInterface(const char* comport);
uint16_t setTurtle(uint8_t c, uint16_t s) { if (ftinterface) return ftinterface->setTurtle(c, s); return -1;}
void reset() { if (ftinterface) ftinterface->reset();}
void robot() { if (ftinterface) ftinterface->robot();}
void setPos(uint8_t p, int16_t pos) { if (ftinterface) ftinterface->setPos(p, pos);}
void stopOutputs() { if (ftinterface) ftinterface->stopOutputs();}

#if 0 //SERIAL INTERFACE
void EnumerateCOMPorts()
{   HKEY hKey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					 L"HARDWARE\\DEVICEMAP\\SERIALCOMM",
					 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{   wchar_t valueName[256];
		wchar_t portName[256];
		DWORD index = 0;
		DWORD valueNameSize, portNameSize, type;
		while (true)
		{   valueNameSize = 256;
			portNameSize = 256;
			LONG result = RegEnumValue(hKey, index++, valueName, &valueNameSize,
									   NULL, &type, (LPBYTE)portName, &portNameSize);
			if (result != ERROR_SUCCESS) break;
			if (type == REG_SZ) //std::wcout << L"Found port: " << portName << std::endl;
				SerialInterface->ComboBox1->Items->Add(portName) ;
		}
		RegCloseKey(hKey);
    }
}

void initInterface(const char* comport)
{ if(comport==nullptr)
	SerialInterface->Show();
  else
  {//serial.openDevice(AnsiString(comport, 9600);
	SerialInterface->CheckBox1->Checked = true; //not sure this works on hidden form
  }
  delete ftinterface;
  ftinterface = 0;//new localTxtInterface;
}
#endif

void initInterface()
{ delete ftinterface;
  ftinterface = new localTxtInterface;
}

//USR() for trainingsroboter, 0xCE1A
double Interface::usr(double x)
{ uint16_t arg = x, res = 0;
  switch(arg)
  { case 0x00A2:res = getEX(); break;      //EX
	case 0x0092:res = getEY(); break;      //EY
	case 0xCEC2: res = ropos[0]; break;                        //ROBO POSITION
	case 0xCEC6: res = ropos[1]; break;                        //ROBO POSITION
	case 0xCECA: res = ropos[2]; break;                        //ROBO POSITION
	case 0xCECE: res = ropos[3]; break;                        //ROBO POSITION
	default: if((arg&0xFF00)==0)
				res = (bool)(arg & 0xFF & getInputs());   //E1..E8
			 break;
  }
  return res;
}

void Interface::robot() //Normally implemented in ROBOT.SYS 0xCEE7
{ outputs = 0;
  for (int i = 0; i < 4; i++)
  { if(rosol[i] < ropos[i]) outputs |= 1<<(2*i);
	else if(rosol[i] > ropos[i]) outputs |= 2<<(2*i);
  }
  getInputs();    //inputs = SCRATCH
  bool stop = false;
  do //while motors are running or coasting...
  { uint8_t flanks = inputs ^ getInputs();   //flanks = SCRATCL
	for(int i = 0; i < 4; i++)
	{ if(!(inputs & (1<<(2*i)))) //end switch pressed
	  {	nl[i] = 0;   //no nachlauf
		outputs &= ~(3<<(2*i)); //motor off
	  }
	  if(flanks & (2<<(2*i))) //pulse detected from sector wheel
	  { if(outputs & (1<<(2*i)))  //CW /CCW
		   ropos[i]--;
		else
		   ropos[i]++;
		if(ropos[i]==rosol[i]) //target reached
		{  if(outputs & (3<<(i+2))) //first time
		   { nl[i] = 255;
			 outputs &= ~(3<<(2*i)); //motor off
		   }
		}
      }
	  if(nl[i]) nl[i]--;
	}
	getInputs(); //set outputs and get new inputs
	if(outputs==0) //all motors off
	{ stop = true;
	  for(int i = 0; i < 4; i++)
		 if(nl[i])
		  { stop = false; //still coasting
			break;
		  }
	}
  } while(!stop);
}
//---------------------------------------------------------------------------
Interface::Interface()
{ ticks = 0;
  inputs = outputs = 0;
  ex = ey = 0;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

void Interface::setOutput(uint8_t O, bool on)
{ --O;
  if (O < 8)
  { if(on) outputs|=1<<O;
	else   outputs&=~(1<<O);
	setOutputs();
  }
}

void Interface::setMotor(uint8_t M, uint8_t c)
{ --M; c&=3;
  if(M<4)
  { M<<=1;
	outputs&=~(3<<M);
	outputs|=c<<M;
	setOutputs();
  }
}

void Interface::stepMotor(uint8_t M, uint8_t c, uint8_t E /*= 0*/)
{ uint8_t m = M - 1;
  c&=3;
  if(m<4 && E<=8)
  { m<<=1;           //m==2(M-1)
	outputs&=~(3<<m); outputs|=c<<m; //compute the output byte
	if(E==0) E = m+1; else --E;      //select the pulse input, by default E == 2M
	setOutputs();
	timeOut t;
	t.initTimer();
	while( t.elapsedTime_ms() < 5000 && (inputs & (1<<E))) { getInputs();}  //wait for release
	while( t.elapsedTime_ms() < 5000 && !(inputs & (1<<E))) { getInputs();}  //wait for press
	setMotor(M, 0);  //stop the motor
  }
}

/*                        NEXT STATES
in E2 	E4  out M1 	M2  11  10  01  00
A  	1   1       1   1   A   B   C   E
B   1   0       1   1   D   B   G   E
C   0   1       1   1   F   H   C   E
D   1   1       1   0   D   -   G   -
E   0   0       1   1   I   H   G   E
F   1   1       0   1   F   H   -   -
G   0   1       1   0   I   -   G   -
H   1   0       0   1   I   H   -   -
I   1   1       0   0   I   -   -   -
*/

bool Interface::stepTurtle(uint8_t cmd)
{ uint8_t m1, m2;
  switch(cmd)
  { case 0: m1=1; m2=1<<2; break;   //both fwd 0101
	case 1: m1=1; m2=2<<2; break;   //         1001
	case 2: m1=2; m2=1<<2; break;   //         0110
	case 3: m1=2; m2=2<<2; break;   //both rev 1010
  }
  timeOut t;
  t.initTimer();
  enum states {A, BD, CF, EGHI} state = A; //there are in fact 9 states A-I with unique outputs but DFGH have only 1 successor and I is the final state
  outputs = m1|m2; //motor commands, start both
  setOutputs();
  while(outputs)
  {   getInputs(); //read inputs and set motors
	  switch(state)
	  { case A: if     ((inputs&0x0A)==8) state = BD;  //to B
				else if((inputs&0x0A)==2) state = CF; //to C
				else if((inputs&0x0A)==0) state = EGHI; //to E
				break;   //wait until at least one switch is released
		case BD:if((inputs&0x08)==0) state = EGHI;  //to EG
				if((inputs&0x02)==2) outputs = m1;  // D
				break;
		case CF:if((inputs&0x02)==0) state = EGHI;  //to EH
				if((inputs&0x08)==8) outputs = m2;  // F
				break;
		case EGHI: switch(inputs&0x0A) //wait until both switches are pressed
				   {   case 0x0: outputs = m1|m2; break;   //E
					   case 0x2: outputs = m2;    break;   //H
					   case 0x8: outputs = m1;    break;   //G
					   case 0xA: outputs = 0;     break;   //I, exit the loop
				   }
				   break;
	  }
	  if(!(inputs&0x10) && cmd==0) outputs = 0; //bumper, active low
	  if(t.elapsedTime_ms() > 5000) { outputs = 0; printf("turtle step timeout\n");}//timeout
	  setOutputs();
  }
  return !(inputs & 0x10);   //stop motors and return bumper
}

/* M1/E2 right motor, M2/E4 left motor, E5 front bumper
   cmd: 0 fwd, 1 left, 2 right, 3 rev
   return the number of fwd steps until bumper
*/
uint16_t Interface::setTurtle(uint8_t cmd, uint16_t steps)
{ int n = 0;
  //printf("turtle: %d, %d steps\n", cmd, steps);
  while(n < steps)
  { n++;
	if(stepTurtle(cmd) && cmd==0) break;//break out of loop on bumper AND forward move (bumper is on front side only)
	if(state==stopped || state==ended) break;
	pauseContinue(outputs);
  }
  return n;
}


//-------------------------------- -----------------------------------------------
localTxtInterface::localTxtInterface(): Interface()
{ if (StartTxtDownloadProg() == KELIB_ERROR_NONE)
  {	pTArea = GetKeLibTransferAreaMainAddress();
	if (pTArea==nullptr) throw "Could not get TransferArea";
	for(int i = 0; i < IZ_UNI_INPUT; i++)
	{ //I1,I2 resistance, I3,I4 voltage, I5-I8 pullup
	  pTArea->ftX1config.uni[i].mode = i==2||i==3 ? MODE_U : MODE_R; //pTArea->ftX1config.uni[0].mode = MODE_U;voltage
	  //I1-I4 analog, I5-I8 digital
	  pTArea->ftX1config.uni[i].digital = i>=IZ_COUNTER;       //pTArea->ftX1config.uni[0].digital = 0;analog
	}
	for(int i = 0; i < IZ_COUNTER; i++)//C1-C4
	{ pTArea->ftX1config.cnt[i].mode = MODE_U;
	}
	for(int i = 0; i < IZ_MOTOR; i++)
	{ pTArea->ftX1config.motor[i] = true;
	}
	pTArea->ftX1state.config_id ++; //pTArea->ftX1state.config_id ++;
  }
  else throw "Could not start TxtDownloadProg";
}

localTxtInterface::~localTxtInterface()
{ StopTxtDownloadProg();
  pTArea = nullptr;
}

void localTxtInterface::setOutputs()
{	if(pTArea==nullptr) throw "No TransferArea, interface not open";
	for(int i = 0; i < IZ_PWM_CHAN; i++)
	{ pTArea->ftX1out.duty[i] = outputs&(1<<i) ? 512 : 0;
	}
	//fprintf(stderr, "\rout:%02X ", outputs);
}

void localTxtInterface::stopOutputs()
{	if(pTArea==nullptr) throw "No TransferArea, interface not open";
	for(int i = 0; i < IZ_PWM_CHAN; i++)
	  pTArea->ftX1out.duty[i] = 0;
}

uint8_t localTxtInterface::getInputs()
{	inputs = 0;
    if(pTArea==nullptr) throw "No TransferArea, interface not open";
	for(int i = 0; i < IZ_COUNTER; i++)
	{ if (pTArea->ftX1in.cnt_in[i])  inputs |= 1<<i;
	}
    for(int i = IZ_COUNTER; i < IZ_UNI_INPUT; i++)
	{ if (pTArea->ftX1in.uni[i]) inputs |= 1<<i;
	}
	//fprintf(stderr, "\rin:%02X ", inputs);
	return inputs;
}

uint16_t localTxtInterface::getEX() { return pTArea->ftX1in.uni[0] / 20;} //Conversion from TXT reported resistance value (0...5k) to parallel interface analog counter value (0..255)
uint16_t localTxtInterface::getEY() { return pTArea->ftX1in.uni[1] / 20;}


// ******************************************
//  Class timeOut
// ******************************************


/*!
    \brief      Constructor of the class timeOut.
*/
// Constructor
timeOut::timeOut()
{}


/*!
    \brief      Initialise the timer. It writes the current time of the day in the structure PreviousTime.
*/
//Initialize the timer
void timeOut::initTimer()
{
#if defined (NO_POSIX_TIME)
    LARGE_INTEGER tmp;
    QueryPerformanceFrequency(&tmp);
    counterFrequency = tmp.QuadPart;
    // Used to store the previous time (for computing timeout)
    QueryPerformanceCounter(&tmp);
    previousTime = tmp.QuadPart;
#else
    gettimeofday(&previousTime, NULL);
#endif
}

/*!
    \brief      Returns the time elapsed since initialization.  It write the current time of the day in the structure CurrentTime.
                Then it returns the difference between CurrentTime and PreviousTime.
    \return     The number of microseconds elapsed since the functions InitTimer was called.
  */
//Return the elapsed time since initialization
unsigned long int timeOut::elapsedTime_ms()
{
#if defined (NO_POSIX_TIME)
    // Current time
    LARGE_INTEGER CurrentTime;
    // Number of ticks since last call
    int sec;

    // Get current time
    QueryPerformanceCounter(&CurrentTime);

    // Compute the number of ticks elapsed since last call
    sec=CurrentTime.QuadPart-previousTime;

    // Return the elapsed time in milliseconds
    return sec/(counterFrequency/1000);
#else
    // Current time
    struct timeval CurrentTime;
    // Number of seconds and microseconds since last call
    int sec,usec;

    // Get current time
    gettimeofday(&CurrentTime, NULL);

    // Compute the number of seconds and microseconds elapsed since last call
    sec=CurrentTime.tv_sec-previousTime.tv_sec;
    usec=CurrentTime.tv_usec-previousTime.tv_usec;

    // If the previous usec is higher than the current one
    if (usec<0)
    {
        // Recompute the microseonds and substract one second
        usec=1000000-previousTime.tv_usec+CurrentTime.tv_usec;
        sec--;
    }

    // Return the elapsed time in milliseconds
    return sec*1000+usec/1000;
#endif
}
