#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>

#define strnicmp  strncasecmp
#define stricmp  strcasecmp
#define ERRORMSG(x) throw x

#define PROGCLASS  //extension of std::map with support for load/save
#define SHORTNAMES //some examples mix long and short names, with this setting they are matched on their short names
#define PARSERWEDGE //let the parser handle the wedge function instead of the lexer
#define DIRECTTEXT  0//when defined as 1 does not use screen/color buffer
#define TURTLEGRAPHICS

#ifndef PROGCLASS
typedef std::map<uint16_t, std::string> Program;
#else
class Program: public std::map<uint16_t, std::string>
{  uint16_t startAddress;
   std::string progName;
   uint8_t device;
   int status;
public:
      using base = std::map<uint16_t, std::string>;          // shorthand alias
      using base::map; //constructors
      using typename base::iterator;              // begin(), end()
      using typename base::const_iterator;        // cbegin(), cend()
      /*using typename base::reverse_iterator;      // rbegin(), rend()
      using typename base::const_reverse_iterator;// crbegin(), crend()
      // Other useful map types
      using typename base::key_type;              // Key
      using typename base::mapped_type;           // Value
      using typename base::value_type;            // std::pair<const Key, Value>
      using typename base::size_type;             // std::size_t
      using typename base::difference_type;       // std::ptrdiff_t
      using typename base::reference;             // value_type&
      using typename base::const_reference;       // const value_type&
      using typename base::pointer;               // value_type*
      using typename base::const_pointer;         // const value_type*
      */
  void setStartAddress(uint16_t ad) { startAddress = ad;}
  uint16_t getStartAddress() { return startAddress;}
  Program(): base(), startAddress(0x0801), progName(""), device(8), status(0) {}
  bool Save(bool bas);
  bool Load(bool bas, bool abs);
  int Status() { return status;}
  void set(const std::string& fn, uint8_t dev) { progName = fn; device = dev;}
};
#endif

typedef Program::iterator prog_iter;

enum keyw_t {     NOKEYW, END=0x80,FOR,NEXT,DATA,INPUTH,INPUT_,DIM,READ,LET,GOTO,RUN,IF,RESTORE,GOSUB,RETURN,REM,STOP,ON,WAIT,LOAD,SAVE,VERIFY,DEF,POKE,
						  PRINTH,PRINT,CONT,LIST,CLR,CMD,SYS,OPEN,CLOSE,GET,NEW,
						  TABt,TO,FN,SPCt,THEN, NOTt, STEP, PLUSt,MINUSt,MULTt,DIVt,EXPt, ANDt,ORt, GTt,EQt,LTt,
						  SGN,INT_,ABS,USR,FRE,POS,SQR,RND,LOG,EXP_,COS,SIN,TAN,ATN,PEEK,LEN,STRS,VAL,ASC,CHRS,LEFTS,RIGHTS, MIDS,GO,
#ifdef SIMONS
						  CIRCLE,HIRES,PLOT,LINE,
#endif
#ifdef PARSERWEDGE
						  WEDGE,
#endif
						  PI=0xFF};
#define TAB NOKEYW   //the parser still recognises TAB and SPC but the scanner will not generate them anymore since the program is stored tokenised where SPC( and TAB( are single tokens
#define SPC NOKEYW

class Var;
class Array;

#ifdef SHORTNAMES    //the names are still stored with all characters
struct identCompare
{ bool operator()(const std::string& lhs, const std::string& rhs) const
  { char lh[4]={0,0,0,0}, rh[4]={0,0,0,0};
	strncpy(lh, lhs.c_str(), 2);
	strncpy(rh, rhs.c_str(), 2);
	if(lhs.back()=='$' || lhs.back()=='%') lh[strlen(lh)] = lhs.back();//compare only up to 3 characters including the optional type character
	if(rhs.back()=='$' || rhs.back()=='%') rh[strlen(rh)] = rhs.back();
	return stricmp(lh, rh)<0;
  }
};
#else
typedef std::less<std::string> identCompare;
#endif

typedef std::map<std::string, std::unique_ptr<Var>, identCompare> Variables;
typedef std::map<std::string, std::unique_ptr<Array>, identCompare> Arrays;
typedef std::map<std::string, std::unique_ptr<Var>, identCompare>::iterator VarIter;
typedef std::map<std::string, std::unique_ptr<Array>, identCompare>::iterator ArrayIter;
//typedef std::map<std::string, Var*, identCompare> Variables;
//typedef std::map<std::string, Array*, identCompare> Arrays;

enum state_t { idle, running, waiting, stopped, ended, cmdmode};
extern state_t state;
extern Program program;
extern prog_iter pc;
extern uint16_t lineno;
extern const char *keywords[];
//extern size_t Nkeywords;
extern uint8_t redirect;

//void directory_file(const char *path, const char *name = "/.dir", bool list = false);
void initDir(const std::string& name, const std::string& id, const char *full_path = "./.dir");
void list_directory(const char *dfn = ".dir", const char *dn = ".", bool list = true);

//commands typically run from the command line
void Run(uint16_t lineno = 0);
void Stop();  //continue with Cont() or a GOTO
void Cont();
bool Load(std::string fn = "", uint8_t dev = 8, uint8_t sec = 0);  //in commandMode implies Clr()
bool Save(std::string fn = "program.bas", uint8_t dev = 8, uint8_t sec = 0);  //sec is ignored
void Verify(std::string fn = "", uint8_t dev = 1);  //does nothing
void List(uint16_t from = 0, uint16_t until = 0);   //requires special parsing <number> '-' <number>
void Clr();
void Cmd(uint8_t file);  //implies a PRINT statement for additional parameters
void New();   //clears program and data

void startProg(unsigned ln = 0);
void runLoop();

//UI functions (main)
unsigned getCursorPos(); //het current print position on current line
void info(const std::string&);  //print to diagnostic panel
void processInputKey();
bool processInput();
void print(const std::string&, bool nl = true);
void pauseContinue(uint8_t outputs);

#endif //_COMMON_H_
