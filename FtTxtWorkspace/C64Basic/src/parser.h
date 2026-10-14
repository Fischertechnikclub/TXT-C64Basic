//---------------------------------------------------------------------------

#ifndef parserH
#define parserH

#include <map>
#include <vector>
#include "common.h"
#include "lexer.h"  //for class Scanner
#include "data.h"

struct VarRef
{ Var *v;
  int i;
  VarRef(): v(0),i(0){}
};

struct Target
{ unsigned nline, pos;
  sym_t sym;
};

struct ForCtrl
{ Var* v;   //or a VarRef when allowing indexed variables
  Target tgt;
  double end, step=1;
  bool cond(double);
  bool iterate();   //iterate one step and return TRUE on final step
};

struct FunctionDef
{ char name[2] = "\0";
  Target tgt;  //the position of the expression, alternatively it could be the position of the definition (e.g. the formal parameter)
  NumVar *par; //the formal parameter, this might also be obtained from the definition during execution
  bool exec;
  FunctionDef(): par(0), exec(false) { }
  FunctionDef(const std::string& nm): par(0), exec(false) { name[0] = toupper(nm[0])|0x80; if(nm.length()>1) name[1] = toupper(nm[1]);}
  std::string Name() { std::string n; n.push_back(name[0]&0x7F); n.push_back(name[1]&0x7F); return n;}
};

#ifndef dataH
//enum state_t { idle, running, waiting, stopped, ended, cmdmode};
#endif

extern bool inputGet;

extern std::vector<Target> callStack;  //for debug.cpp
extern std::vector<VarRef> varList;
extern std::vector<ForCtrl> loopStack; //for debug.cpp
extern std::map<std::string, FunctionDef> functions;

int parseLine();
//---------------------------------------------------------------------------
#endif
