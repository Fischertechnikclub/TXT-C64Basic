#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <cstring>

#define strnicmp  strncasecmp
#define stricmp  strcasecmp
#define ERRORMSG(x) throw x

#define PROGCLASS  //extension of std::map with support for load/save
#define SHORTNAMES //some examples mix long and short names, with this setting they are matched on their short names
#define PARSERWEDGE //let the parser handle the wedge function instead of the lexer
#define DIRECTTEXT  0//when defined as 1 does not use screen/color buffer
#define TURTLEGRAPHICS


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


extern const char *keywords[];

void directory_file(const char *path, const char *name = "/.dir", bool list = false);
void initDir(const std::string& name, const std::string& id);


//commands typically run from the command line
bool Load(std::string fn = "", uint8_t dev = 8, uint8_t sec = 0);  //in commandMode implies Clr()
bool Save(std::string fn = "program.bas", uint8_t dev = 8, uint8_t sec = 0);  //sec is ignored
void List(uint16_t from = 0, uint16_t until = 0);   //requires special parsing <number> '-' <number>

void startProg(unsigned ln = 0);
void runLoop();

#endif //_COMMON_H_
