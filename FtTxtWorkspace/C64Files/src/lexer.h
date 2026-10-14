//---------------------------------------------------------------------------

#ifndef lexerH
#define lexerH

#include <string>
#include "common.h"

#define USE_TOKEN_VALUES            //the coding of keywords corresponds to C64 tokens
#define USE_TOKENIZED_STRINGS      //the scanner expects C64 tokens on input in addition to regular ascii
//#define SIMONS

enum sym_t {NONE,NUMBER,STRING,INTVAR,REALVAR,STRVAR,FUNCNAME,COMMAND,FUNCTION, OPERATOR, EOLN='\n', HASH='#', LPAR='(', RPAR=')', COMMA=',', COLON=':', SEMICOLON=';', INTARRAY, REALARRAY, STRARRAY};
enum oper_t { NOPER=0, AND=1, OR=2, NOT=3, LE, GE, NE, PLUS='+', MINUS='-', MULT='*',DIV='/',EXP='^',LT='<',GT='>',EQ='='};  //sym==OPERATOR


extern keyw_t keyword;
extern sym_t sym;
extern oper_t oper;  //last scanned OPERATOR
extern double value; //last scanned NUMBER
extern std::string identifier; //last scanned text (strings, variables, keywords)
extern std::string symbol; //debugging

extern const char* funs[], *keywords[];   //literals for (FUNCTION) keywords
extern const size_t Nkeywords;
//---------------------------------------------------------------------------
class Scanner
{  //the scanner scans one program line at a time, the line itself is constant so a copy should not be necessary, I've seen cases though where a reference got lost. Therefor I may later decide to make a copy of the active line
protected:
  const char *s; //reset at every invocation of nextsym, could be made local
  unsigned i;    //the only real 'state' of the scanner
  double value;  //the other properties relate to the most recent symbol
  std::string identifier, symbol;
  sym_t sym;
  keyw_t keyword;
  oper_t oper;
public:
  Scanner(): s(""), i(0), sym(NONE) {}
  Scanner(const std::string& ln):  s(ln.c_str()), i(0), sym(NONE) {}
  sym_t nextsym(/*const std::string& ln*/);
  //pos() and start() are used to resume scanning from a saved location on the line
  void start(const std::string& ln, int p = 0) { s = ln.c_str(); i = p; sym = NONE;} //NOTE that 'start' requires calling nextsym() or previous sym, etc. must be restored
  unsigned pos() { return i;} //indicates the position of the next character to read on the line
  //getters of recent symbol properties
  double Value() { return value;}
  const std::string& Identifier() { return identifier;}
  sym_t Sym() { return sym;}
  keyw_t Keyword() { return keyword;}
  oper_t Operator() { return oper;}
  const std::string& Symbol() { return symbol;}
};


class InputScanner   //specialised reader for read DATA or INPUT ',' separated lists of numbers and (possibly unquoted) strings
{ std::string str;
  std::size_t p;
  const char *sep;
  void skipws() {while(p < str.length() && isspace(str[p])) p++;}
  void skipsep()
  {	if(p < str.length())
	{ if(strchr(sep, str[p])) p++;  //skip the separator
	  else throw "separator expected";
	}
  }
public:
  InputScanner(const std::string& s, size_t p = 0, const char* sep=",;:\r\n"):
														str(const_cast<std::string&>(s)),
														p(p),
														sep(sep)
  { skipws();
  }
  InputScanner(const char* sep=",;:\r\n"): str(""), p(0), sep(sep) {}
  void set(const std::string& line, int pos)
  { str = line;
	p = pos;
  }
  short getShort()
  { short v = 0;
	try { size_t pp;v = std::stoi(str.substr(p), &pp); p+=pp;}
	catch(std::invalid_argument& e){ throw "literal type error, integer value expected";}
	catch(std::out_of_range& e){ throw "literal value error, integer out of range";}
	skipws(); skipsep();
	return v;
  }
  double getReal()
  { double v = 0;
	try { size_t pp; v = std::stod(str.substr(p), &pp); p+=pp;}
	catch(std::invalid_argument& e){ throw "literal type error, float value expected";}
	skipws(); skipsep();
	return v;
  }
  std::string getString()
  { std::string result = "";
	skipws(); //skip ws
	if(str[p]=='"') //quoted string
	{ p++;
	  while(p < str.length() && str[p]!='"')
		 result.push_back(str[p++]);
	  if(str[p]!='"') throw "closing quote missing";
	  p++; //skip the quote
	  skipws(); //skip trailing ws
	}
	else
	{ while(p < str.length() && strchr(sep, str[p])==0)
		result.push_back(str[p++]);   //copy chars upto the separator or the end
#ifdef TRIMTRAILINGSPACES
	  std::size_t q = p;
	  while(q>0 && isspace(s[q-1]))q--;
	  result = result.substr(0, q); //trim trailing whitespace
#endif
	}
	skipsep();
	return result;
  }
  bool atEnd() const { return p >= str.length();}
  std::size_t Pos() { return p;}
};

sym_t nextsym();
sym_t nextsym(const std::string& ln);
Scanner* saveAndStart(const std::string& line);
void restore(Scanner *prev);

extern Scanner *scan;

//convenience functions
inline bool returnsString(sym_t s, keyw_t k) { return s==FUNCTION && (k==STRS || k==CHRS || k==LEFTS || k==RIGHTS  || k==MIDS);}         //functions with string result
inline bool isFormatFunction(sym_t s, keyw_t k) { return s==FUNCTION && (k == TAB || k == SPC || k == TABt || k == SPCt);}               //SPC and TAB
inline bool isVariable(sym_t s) { return s==INTVAR || s==REALVAR || s==STRVAR;}                                                          //scalars
inline bool isArray(sym_t s) { return s==INTARRAY || s==REALARRAY || s==STRARRAY;}                                                       //arrays
inline bool isStringVariable(sym_t s) { return s==STRVAR || s==STRARRAY;}                                                                //strings and string arrays
inline bool takesString(sym_t s, keyw_t k) { return s==FUNCTION && (k==LEN || k==VAL || k==ASC || k==LEFTS || k==RIGHTS  || k==MIDS);}   //functions taking at least one string as argument
inline bool isRealFunction(sym_t s, keyw_t k) { return s==FUNCTION && !returnsString(s, k);}                                             //functions with real result
inline bool isRelOp(sym_t s, oper_t op) { return s==OPERATOR && (op==EQ || op==NE || op==GT || op==LT || op==GE || op==LE);}             //relational operators

#endif //lexerH

//-----------------------------------------------------------------------------------

