//---------------------------------------------------------------------------

#ifndef interfaceH
#define interfaceH
//---------------------------------------------------------------------------
#include <sys/time.h>
#include "KeLibTxtDl.h"     // TXT Lib
#include "FtShmem.h"        // TXT Transfer Area


/*!  \class     timeOut
     \brief     This class can manage a timer which is used as a timeout.
   */
// Class timeOut
class timeOut
{
public:
    timeOut();// Constructor
    void initTimer(); // Init the timer
    unsigned long int   elapsedTime_ms();// Return the elapsed time since initialization
private:
#if defined (NO_POSIX_TIME)
    LONGLONG       counterFrequency;
    LONGLONG       previousTime; // Used to store the previous time (for computing timeout)
#else
    struct timeval      previousTime; // Used to store the previous time (for computing timeout)
#endif
};


//---------------------------------------------------------------------------
class Interface
{
protected:	// User declarations
	volatile uint8_t inputs, outputs, ticks;
	uint16_t ex, ey;
	virtual void setOutputs() { getInputs();} //often in- and outputs are set in 1 transaction (serial (remote) interfaces, local interface may update based on timer)
	bool stepTurtle(uint8_t cmd);
	int16_t ropos[4], rosol[4];  //ist, soll
	uint8_t nl[4];   //nachlauf
public:		// User declarations
	Interface();
	virtual ~Interface() {}
	virtual uint8_t getOutputs() { return outputs;}
	virtual uint8_t getInputs()  { return inputs;}
	virtual uint16_t getEX()     { return ex;}
	virtual uint16_t getEY()     { return ey;}
	virtual void setOutput(uint8_t O, bool on);
	virtual void setMotor(uint8_t M, uint8_t c);
	virtual void stepMotor(uint8_t M, uint8_t c, uint8_t E = 0);
	virtual uint16_t setTurtle(uint8_t cmd, uint16_t steps);
	virtual double usr(double);
	virtual void reset() { for (int i = 0; i < 4; i++) { ropos[i] = rosol[i] = 0; nl[i] = 0;} outputs = 0; getInputs();} //0xCDB2 INIT
	virtual void setPos(uint8_t p, int16_t pos) { if (p<4) rosol[p] = pos;} //0xCEC2, 0xCEC6, 0xCECA, 0xCECE
	virtual void robot(); // 0xCEE7
	virtual void stopOutputs() = 0;
};

/*
 * The TXT has no dedicated EX, EY (or RoboInterfacelike AX, AY, A1, A2)
 * Therefore A1, A2 are simulated by I3, I4, EX(AX), EY(AY) are simulated by I1, I2
 * Inputs E1-E4 are now at C1-C4 and E5-E8 are at I4-I8
 */
class localTxtInterface: public Interface
{  FISH_X1_TRANSFER    *pTArea;
protected:
    void setOutputs();
public:
	localTxtInterface();
	~localTxtInterface();
	uint8_t getInputs();
	uint16_t getEX();
	uint16_t getEY();
	void stopOutputs();
};

class intelligentInterface: public Interface
{
protected:
    //void setOutputs();
public:
	intelligentInterface();
	~intelligentInterface();
	uint8_t getInputs();
	uint16_t getEX();
	uint16_t getEY();
};
//---------------------------------------------------------------------------
extern  Interface *ftinterface;
//---------------------------------------------------------------------------

uint8_t getInputs();
uint16_t getEX();
uint16_t getEY();
//void setOutput(uint8_t O, bool on)                  { SerialInterface->setOutput(O, on);}
void setMotor(uint8_t M, uint8_t c);
void stepMotor(uint8_t M, uint8_t c, uint8_t E = 0);
//void initInterface(const char* comport);
void initInterface();
uint16_t setTurtle(uint8_t c, uint16_t s) ;
void reset();
void robot();
void setPos(uint8_t p, int16_t pos);
void stopOutputs();
#endif
