//---------------------------------------------------------------------------

#ifndef dataH
#define dataH
#include "lexer.h"
//#include "program.h"

#include <string>
#include <map>
#include <vector>
#include <stack>
#include <math.h>

#include "common.h"

#define ERRORMSG2(x) info(x)

#define C64REAL     Number
//#define C64REAL 	float
//#define C64STRING   c64String
#define C64STRING   std::string

//extern void info(const std::string&);  //print to diagnostic panel
//extern uint8_t redirect;

//extern const std::string err;
std::string numberToString(double num);

class Number   //5 byte C64 compatible float
{ int8_t exp;
  int32_t mant;
public:
  const Number& operator=(double d)
  { int e; mant = ldexp(frexp(d, &e), 31); exp=e+128;
	return *this;
  }
  operator double() const
  { double tmp = ldexp(ldexp((double)mant, -31), (int8_t)(exp-128));
	return tmp;
  }
  Number(double d)
  { int e; mant = ldexp(frexp(d, &e), 31); exp=e+128;
  }
};

struct c64String //intended for storage in external memory
{ uint8_t len;
  char *data; //contrary to C64 basic, the data is null terminated wasting 1 byte
  //const char* operator=(const std::string& s) { len = s.length()>255 ? 255: s.length(); data = strdup(s.substr(0, 255).c_str()); return data;}
  operator std::string() { return std::string(data);}
  c64String(const std::string& s) { len = s.length()>255 ? 255: s.length(); data = strdup(s.substr(0, 255).c_str());}
  //~c64String() { delete data;}
};

/*  line format: 'DATA' (<real>|string) CHAIN ',' <eoln>
	string: \"[^\"\n]*\" | [^ \r\n\t,:]+
 Strings can be written normally or in quotes, but characters like comma, space, colon, graphical ones or control characters has to be written inside double quotes like string constants.
*/
class Literals  //searches for DATA in mapped BASIC Program
{ const Program& prog;
  bool nomore;
  Program::const_iterator ln;//the current DATA line
  InputScanner scan; //scanner containing position on the DATA line
  void nextData()
  { while(ln != prog.end())
	{ const std::string& stmt = ln->second;
 //DATA needs not be the first statement on the line, scan the entire line for DATA (not in strings or after REM)
	  bool quoted = false;
	  for(unsigned p = 0; p < stmt.length(); p++)
	  { if(stmt[p]=='"') quoted = !quoted;
	    else if(stmt[p]==REM && !quoted) break;
	    else if(stmt[p]==DATA && !quoted)
		{ scan.set(stmt, p+1); //DATA token is 1 byte
		  goto end;//get out of nested loop
		}
	  }
	  ln++;
	}
  end:
	nomore = ln == prog.cend();
  }
public:
  void restore() { ln = prog.cbegin(); nextData();}
  //void reset(Program* prg) { prog = prg; ln = prog->begin(); nextData();}
  void reset() { ln = prog.cbegin(); nextData();}//same as restore(), there is only 1 program object, therefore could be const& but iterators can be invalidated or point to the wrong statement line
  Literals(const Program& prg): prog(prg),     //constructor is called statically with an empty program, so the only effect is that 'nomore' is set 'true'
								nomore(true),
								ln(prog.cbegin()) //effectively sets ln to cend()
                             /*, scan(InputScanner(ln->second))*/ //ln->second is undefined when ln==cend()
  { reset(); //as ln==cend() only sets 'nomore' to 'true' which it already is
  }
  ~Literals() { }
  std::string getString()
   { if(!nomore && scan.atEnd()) { ln++; nextData();}
	 if(!nomore)
	 { std::string tmp = scan.getString();
	   return tmp;
	 }
	 ERRORMSG("?OUT OF DATA (string)");
   }
   double getNumber()
   { if(!nomore && scan.atEnd()) {ln++; nextData();}
	 if(!nomore) return scan.getReal();
	 ERRORMSG("?OUT OF DATA (number)");
   }
};


//Var and derived classes need rethinking when moving to external memory because virtual functions might not be possible
class Var
{ char name[2] = {0,0};  //real 00, integer 11, string 01, function 10
public:
  enum type { integer, real, string, integerarray, realarray, stringarray};
protected:
  bool inited = false; //C64 does not have this, init to 0
  type t;              //C64 type in implied in the name or storage(arrays)
public:
  Var(type t):  inited(false), t(t) {  }
  Var(type t, const std::string& nm): inited(false), t(t)
  { name[0] = toupper(nm[0]);
	if(nm.length() > 1 && nm[1] != '%' && nm[1] != '$') name[1] = toupper(nm[1]);
	if(t!=real) name[1]|=0x80;
	if(t==integer)name[0]|=0x80;
  }
  virtual ~Var() {}
  virtual type Type() { return t;}
  virtual void set(double v, unsigned idx = 0) = 0;
  virtual void set(const std::string& v, unsigned idx = 0) = 0;
  virtual void set(const std::string_view v, unsigned idx = 0) = 0;
  virtual double get(unsigned idx = 0) const = 0;
  virtual const std::string& getString(unsigned idx = 0) const = 0;
  std::string Name() const
  { std::string n;
	n.push_back(name[0] & 0x7F);
	if(name[1] & 0x7F) n.push_back(name[1]&0x7F);
	if(name[1] & 0x80)
	{ if(name[0] & 0x80) n.push_back('%');
	  else n.push_back('$');
	}
	return n;
  }
  bool isInitialised() { return inited;}
};

class NumVar: public Var
{ C64REAL num;
public:
  virtual void set(double v, unsigned idx = 0)
  { if(t==integer)
	  num = (short)v;
	else
	  num = v;
	inited = true;
  }
  virtual void set(const std::string& v, unsigned idx = 0) { ERRORMSG("assigning string to number");}
  virtual void set(const std::string_view v, unsigned idx = 0) { ERRORMSG("assigning string to number");}
  virtual const std::string& getString(unsigned idx = 0) const
  {
#if 1
	ERRORMSG("getting string from number"); //return "";
#else
	return numberToString(num);
#endif
  }
  double get(unsigned idx = 0) const
  { if(!inited && (state==running || state==cmdmode))
	  ERRORMSG2(Name() + " not initialised");
	return num;
  }
  NumVar(type t): Var(t), num(0) {}
  NumVar(type t, const std::string& nm): Var(t, nm), num(0) {}
};

class StrVar: public Var
{ C64STRING str;
public:
  virtual void set(double v, unsigned idx = 0) { ERRORMSG("assigning number to string");}
  double get(unsigned idx = 0) const { ERRORMSG("getting number from string");}
  virtual void set(const std::string_view v, unsigned idx = 0) { str = v; inited = true;}
  virtual void set(const std::string& v, unsigned idx = 0) { str = v; inited = true;}
  const std::string& getString(unsigned idx = 0) const { if(!inited && state==running) ERRORMSG2(Name() + " not initialised"); return str;}
  StrVar(): Var(string), str("") {}
  StrVar(const std::string& nm): Var(string, nm), str("") {}
  ~StrVar() {}
};

class Array: public Var   //arrays are 0 or "" initialised
{
protected:
  unsigned size = 1;
  std::vector<unsigned> dims;
  virtual void alloc() = 0;
public:
  virtual type Type() const =0; //{ return t==integer ? integerarray : realarray;}
  virtual void set(double v, unsigned idx)=0;
  virtual void set(const std::string& v, unsigned idx = 0)=0;
  virtual double get(unsigned idx) const = 0;
  virtual const std::string& getString(unsigned idx = 0) const { ERRORMSG("Array is numeric");}
  void addDimension(unsigned n) { dims.push_back(n); size *= n; alloc();}
  virtual unsigned dimensions() const { return dims.size();}
  virtual unsigned dim(unsigned n) const { if (n < dimensions()) return dims[n]; return 0;}
  Array(type t): Var(t) {}
  Array(type t, const std::string& nm): Var(t, nm) {}
  ~Array() {}
};

class StrArray: public Array
{ std::vector<C64STRING> svals;
protected:
  virtual void alloc() { svals.clear(); svals.reserve(size); for(unsigned i = 0; i < size; i++) svals.push_back("");}
public:
  virtual type Type() const { return stringarray;}
  virtual void set(double v, unsigned) { ERRORMSG("assigning number to string");}
  virtual void set(const std::string& v, unsigned idx) { if(idx>=svals.size())  ERRORMSG("Index out of bounds"); else svals[idx] = v; }
  virtual void set(const std::string_view v, unsigned idx) { if(idx>=svals.size())  ERRORMSG("Index out of bounds"); else svals[idx] = v; }
  const std::string& getString(unsigned idx) const { if (idx<svals.size()) return svals[idx]; ERRORMSG("Index out of bounds");}
  double get(unsigned idx) const { ERRORMSG("getting number from string");}
  StrArray():Array(string) {}
  StrArray(const std::string& nm):Array(string, nm) {}
};

class IntArray: public Array
{  std::vector<short> vals;
protected:
  virtual void alloc() { vals.clear(); vals.reserve(size); for(unsigned i = 0; i < size; i++)  vals.push_back(0);}
public:
  virtual type Type() const { return integerarray;}
  virtual void set(double v, unsigned idx) { set((short)v, idx);}
  virtual void set(const std::string& v, unsigned) { ERRORMSG("assigning string to integer");}
  virtual void set(const std::string_view v, unsigned) { ERRORMSG("assigning string to integer");}
  virtual void set(short v, unsigned idx) { if(idx>=vals.size()) ERRORMSG("Index out of bounds"); else vals[idx] = v; }
  double get(unsigned idx) const { if (idx<vals.size()) return vals[idx]; ERRORMSG("Index out of bounds");}
  IntArray():Array(integer) {}
  IntArray(const std::string& nm):Array(integer, nm) {}
};

class RealArray: public Array
{  std::vector<C64REAL> vals;
protected:
  virtual void alloc() { vals.clear(); vals.reserve(size); for(unsigned i = 0; i < size; i++) vals.push_back(0);}
public:
  virtual type Type() const { return realarray;}
  virtual void set(short v, unsigned idx) { set((double)v, idx);}
  virtual void set(const std::string_view v, unsigned) { ERRORMSG("assigning string to real");}
  virtual void set(const std::string& v, unsigned) { ERRORMSG("assigning string to real");}
  virtual void set(double v, unsigned idx) { if(idx>=vals.size()) ERRORMSG("Index out of bounds"); else vals[idx] = v; }
  double get(unsigned idx) const { if (idx<vals.size()) return vals[idx]; ERRORMSG("Index out of bounds");}
  RealArray():Array(real) {}
  RealArray(const std::string& nm):Array(real, nm) {}
};


extern Variables vars;
extern Arrays arrays;
extern Literals literals;

//---------------------------------------------------------------------------
#undef ERRORMSG
#endif

