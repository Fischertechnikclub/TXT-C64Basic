//---------------------------------------------------------------------------

#include "expression.h"
#include "lexer.h"
#include "parser.h"
#include "user.h"
#include "files.h"

//#include "program.h"
//---------------------------------------------------------------------------
//#define PRIORITY  //give exponentiation (^) higher priority than unary + -

//extern std::string printline;   //for POS()
//unsigned getCursorPos();

#include <cstdint>
#include <ctime>
#include <stdexcept>

class Clock24 { //thanks Claude
public:
    // Total ticks in 24 hours at 1/60s resolution
    // 24 * 60 * 60 * 60 = 5,184,000 ticks
    static constexpr uint32_t TICKS_PER_DAY = 24UL * 60 * 60 * 60;

    Clock24() {
        offset_ticks_ = 0;
        base_ticks_   = getMonotonicTicks();
    }

    // Set the clock to a specific time expressed in ticks (0 .. TICKS_PER_DAY-1)
    void setTicks(uint32_t ticks) {
        base_ticks_   = getMonotonicTicks();
        offset_ticks_ = ticks % TICKS_PER_DAY;
    }

    // Set the clock using hours, minutes, seconds, sixtieths
    void setTime(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t sixtieths) {
        if (hours >= 24 || minutes >= 60 || seconds >= 60 || sixtieths >= 60)
            throw std::invalid_argument("Invalid time component");

        uint32_t ticks = ((uint32_t)hours   * 3600UL * 60)
                       + ((uint32_t)minutes *   60UL * 60)
                       + ((uint32_t)seconds *        60)
                       +  (uint32_t)sixtieths;
        setTicks(ticks);
    }

    // Get current time as raw ticks (0 .. TICKS_PER_DAY-1)
    uint32_t getTicks() const {
        uint32_t elapsed = getMonotonicTicks() - base_ticks_;   // wraps safely
        return (offset_ticks_ + elapsed) % TICKS_PER_DAY;
    }

    // Decompose into human-readable parts
    struct Time {
        uint8_t hours;       // 0-23
        uint8_t minutes;     // 0-59
        uint8_t seconds;     // 0-59
        uint8_t sixtieths;   // 0-59  (1/60 s each)
    };

    Time getTime() const {
        uint32_t t = getTicks();
        Time result;
        result.sixtieths = t % 60;  t /= 60;
        result.seconds   = t % 60;  t /= 60;
        result.minutes   = t % 60;  t /= 60;
        result.hours     = t % 24;
        return result;
    }

private:
    uint32_t offset_ticks_;  // value set by the user
    uint32_t base_ticks_;    // monotonic snapshot at last set

    // Returns monotonic ticks at 1/60 s resolution
    // Uses CLOCK_MONOTONIC — available on all Linux kernels >= 2.6
    static uint32_t getMonotonicTicks() {
        struct timespec ts;
        ::clock_gettime(CLOCK_MONOTONIC, &ts);
        // Convert to 1/60 s ticks
        // seconds * 60  +  nanoseconds / (1e9/60)
        //                = nanoseconds * 60 / 1e9
        //                = nanoseconds / 16666666
        return static_cast<uint32_t>(
            (uint64_t)ts.tv_sec  * 60ULL
          + (uint64_t)ts.tv_nsec / 16666667ULL   // 1e9/60 rounded
        );
    }
} clk;

void setTime(std::string& ts) //expect a 6 digit string representing
{ try
  { uint32_t v = std::stoul(ts);
    v %= 774021; //C64 behaviour, see the wiki https://www.c64-wiki.com/wiki/TIME$
    if(v%100 > 59) v+=40;
    if(v%10000 > 5959) v+=4000;
    while(v/10000 > 23) v-=240000;
    ts = std::to_string(v);
    clk.setTime(v/10000, (v/100)%100, v%100, 0);
  }
  catch(...)
  { ERRORMSG("error converting time string");
  }
}

//subscript: LPAR numexpression LIST RPAR
//int handleSubscripts(std::map<std::string, Array*>::iterator v)
int handleSubscripts(ArrayIter v)
{ unsigned result = 0;
  unsigned fact = 1;
  int n = 0;
  if(sym=='(')
  { nextsym();
    //Array *a = v->second;
  	Array *a = v->second.get();
	while(true)
	{ unsigned i = parseExpression();
	  if (i < 10 && a->dim(n)==0) a->addDimension(10);
	  if(i >= a->dim(n)) { i = 0; ERRORMSG("index out of bounds"); }
	  result += fact*i;
	  fact *= a->dim(n);
	  if(sym==COMMA) nextsym(); else break;
	  n++;
	}
	if(sym!=')') ERRORMSG("expected ')' after subscript(s)");
	nextsym();
	return result;
  }
  ERRORMSG("expected '(' after array identifier");
  return 0;
}

/*
unary	: [ PLUS | MINUS ]  '(' logexpression ')'
		| [ PLUS | MINUS ]  NUMBER
		| [ PLUS | MINUS ]  VARNAME
		| [ PLUS | MINUS ]  ARRAYNAME subscript
		| [ PLUS | MINUS ]  FN FUNCNAME  '(' logexpression ')'
		| [ PLUS | MINUS ]  FUNCIONNAME  '(' logexpression ')'
		| [ PLUS | MINUS ]  STRFUNCNAME  '(' stringexpression ')'
		:
*/
static double unary()
{ double result = 0;
  bool neg = false;
#ifndef PRIORITY
  if(sym==OPERATOR && (oper==PLUS || oper==MINUS)) //unary +/-
  { neg = oper==MINUS;
	nextsym();
  }
#endif
  if(sym==LPAR)  //parenthesised expression
  { nextsym();
	result = orExpr();  //result = parseExpression();
	if(sym!=RPAR) ERRORMSG("expected ')'");
	nextsym();
  }
  else if (sym==NUMBER) //literal number
  { result = value;
	nextsym();
  }
  else if(sym==INTVAR || sym==REALVAR) //simple variable
  {	//std::map<std::string, Var*>::iterator v = vars.find(identifier);
	VarIter v = vars.find(identifier);
	if(v==vars.end())
	{ if(identifier.substr(0, 2) == "TI")
	  { result = clk.getTicks();
	    //vars[identifier]->set(result); //read_only so no point in exposing it
	  }
	  else if(identifier.substr(0, 2) == "ST")
	  { result = C64File::getStatus();    //Commodore I/O status
	    //vars[identifier]->set(result);  //read_only so no point in exposing it
	  }
	  else throw "Panic unknown var";
	}
	else
	  result = vars[identifier]->get();
	nextsym();
  }
  else if(sym==INTARRAY || sym==REALARRAY) //subscripted variable
  {	//std::map<std::string, Array*>::iterator v = arrays.find(identifier);
	ArrayIter v = arrays.find(identifier);
 	if(v==arrays.end()) throw "Panic unknown array";
	nextsym();
	int idx = handleSubscripts(v);
	//result = ((Array*)(v->second))->get(idx);
	result = ((Array*)(v->second.get()))->get(idx);
  }
  else if(isRealFunction(sym, keyword))  //standard function returning a real
  { keyw_t fun = keyword;
	nextsym();
	if(sym==LPAR)
	{ nextsym();
	 try
	 {if (!takesString(FUNCTION, fun))   //... taking a numeric argument
	  { double arg = parseExpression();
		switch(fun)
		{ case ABS: result = fabs(arg); break;
		  case ATN: result = atan(arg); break;
		  case SIN: result = sin(arg); break;
		  case COS: result = cos(arg); break;
		  case EXP_: result = exp(arg); break;
		  case FRE: result = 0;/*progTokenised.free();*/ break;    //for now only free program memory
		  case INT_: result = floor(arg); break;
		  case LOG: if(arg<=0) ERRORMSG("LOG arg<=0"); else result = log(arg); break;
		  case PEEK: result = peek(arg); break;
		  case POS: result = getCursorPos(); break;
		  case RND: if (arg==0) std::srand(clock()); else if(arg<0) std::srand(-arg); result = (double)std::rand()/RAND_MAX; break;
		  case SGN: result = arg<0 ? -1 : (arg>0 ? 1 : 0); break;
		  case SQR: if(arg<0) ERRORMSG("SQR arg<0"); else result = sqrt(arg); break;
		  case TAN: result = tan(arg); break;
		  case USR: { unsigned retval = usr(arg);
					  result = (double)retval;
					}
					break;
#ifdef USE_TOKEN_VALUES
		  default: ERRORMSG("unknown function " + std::string(keywords[fun]));
#else
		  default: ERRORMSG("unknown function " + std::string(funs[fun-USR]));
#endif
		}
	  }
	  else  //... taking a string argument
	  { std::string arg = parseStringExpression();
		switch(fun)
		{ case ASC: if(arg.empty())ERRORMSG("argument is empty string");
					else
					{ unsigned char tmp = arg.front();
					  result = tmp;
					}
					break;
		  case LEN: result = arg.length(); break;
		  case VAL: //if(arg.empty())ERRORMSG("argument is empty string"); else
					result = strtod(arg.c_str(), nullptr);  //chose strtod i.o. std::stod because that one throws on illegal input i.o. returning 0
					break;
#ifdef USE_TOKEN_VALUES
		  default: ERRORMSG("unknown function " + std::string(keywords[fun]));
#else
		  default: ERRORMSG("unknown function " + std::string(funs[fun-USR]));
#endif
		}
	  }
	 }
	 catch(const std::string& msg)
	 { throw msg.c_str();
	 }
	 catch(...)
	 { ERRORMSG("function argument error");
	 }
	 if(sym!=RPAR) ERRORMSG("expected ')'");
	 nextsym();
	}
  }
  else if(sym==FUNCNAME) //user defined function , functions can be nested hence the parsing context must be saved
  { std::map<std::string, FunctionDef>::iterator f = functions.find(identifier);
	nextsym();
	if(sym==LPAR)
	{ nextsym();
	  double arg = orExpr(); //arg = parseExpression(); //compute actual parameter
	  if(f != functions.end())
	  { try
		{ NumVar sv = *(f->second.par); //formal parameter
		  f->second.par->set(arg);  //copy actual parameter to formal parameter
		  const std::string& def = program[f->second.tgt.nline].substr(f->second.tgt.pos);  //def is the actual expression
		  Scanner *main = saveAndStart(def); //save current scanner and create new one
		  result = parseExpression();
		  restore(main); //delete the current scanner and restore previous
		  //*(f->second.par) = sv;
		}
		catch(...)
		{ ERRORMSG("error in function evaluation");
        }
	  }
	  else ERRORMSG("undefined FN name");
	  if(sym!=RPAR) ERRORMSG("expected ')'");
	  nextsym();
	}
	else ERRORMSG("expected ')'");
  }
  else ERRORMSG("unexpected symbol " + symbol + " in line " + std::to_string(lineno));
  return neg ? -result : result;
}

/* exponentiation: exponentiation EXP unary         NOTE: left recursive, associates right to left!
				| unary
				;
*/
double exponentiation() //some say: in C64 basic exponentiation has higher priority than prefix +/-
{ double result = unary();
  if(sym==OPERATOR && oper==EXP)
  {  nextsym();
	 double exp = exponentiation();
	 try
	 { if(result<=0 && exp!=(long)exp) ERRORMSG("complex result");
	   return pow(result, exp);
	 }
	 catch(...)
	 { ERRORMSG("exponentiation failed");
	 }
  }
  return result;
}

double prefix()
{
#ifdef PRIORITY                              //prefix: (PLUS | MINUS) exponentiation
  if(sym==OPERATOR && oper==PLUS)
  { nextsym();
	return exponentiation();
  }
  else if(sym==OPERATOR && oper==MINUS)
  { nextsym();
	return -exponentiation();
  }
  else
#endif
	return exponentiation();              //prefix: exponentiation
}

double factor()    //factor: prefix CHAIN (MULT | DIV)
{ double result = prefix();
  while(true)
  { if(sym!=OPERATOR) return result;
	switch(oper)
	{ case MULT: nextsym();
				result *= prefix();
				break;
	  case DIV: nextsym();
				{ double tmp = prefix();
				  if (tmp==0) ERRORMSG("division by 0");
                  else result /= tmp;
				}
				break;
	  default: return result;
	}
  }
}

double parseExpression()   //Expression: factor CHAIN (PLUS | MINUS)
{ double result = factor();
  while(true)
  { if(sym!=OPERATOR) return result;
	switch(oper)
	{ case PLUS: nextsym();
				 result += factor(); break;
	  case MINUS: nextsym();
				  result -= factor(); break;
	  default: return result;
	}
  }
}

/*
relExpr	: [ NOT ] stringExpression RELOP stringexpression
		| [ NOT ] numExpression [ RELOP numexpression ]
		;
*/
double relExpr() //returns logical result but as a special case the numeric result of a numeric expression (relational operator and rhs omittted)
{ bool result = false;
  bool neg = sym==OPERATOR && oper==NOT;
  oper_t op;
  if (neg) nextsym();
  if(isStringVariable(sym) || sym==STRING || returnsString(sym, keyword) )  //string expression lookahead
  { std::string lhs = parseStringExpression();
	if(isRelOp(sym, oper)) op = oper;
	else ERRORMSG("RELOP expected");
	nextsym();
	std::string rhs = parseStringExpression();
	switch(op)
	{ case EQ: result = lhs==rhs; break;
	  case NE: result = lhs!=rhs; break;
	  case LT: result = lhs<rhs; break;
	  case GT: result = lhs>rhs; break;
	  case LE: result = lhs<=rhs; break;
	  case GE: result = lhs>=rhs; break;
	  default: break;
	}
	//printf("'%s' %d '%s' => %d\n", lhs.c_str(), (int)op, rhs.c_str(), result);
  }
  else //if (sym==INTVAR || sym==REALVAR || ||sym==INTARRAY || sym==REALARRAY || sym==NUMBER || (sym==FUNCTION && keyword < MIDS) || (sym==OPERATOR && (oper==PLUS || oper==MINUS)) || sym==LPAR)  //expression look ahead
  { double lhs = parseExpression();
	if(isRelOp(sym, oper))
	{ op = oper;
	  nextsym();
	  double rhs = parseExpression();
	  switch(op)
	  { case EQ: result = lhs==rhs; break;
		case NE: result = lhs!=rhs; break;
		case LT: result = lhs<rhs; break;
		case GT: result = lhs>rhs; break;
		case LE: result = lhs<=rhs; break;
		case GE: result = lhs>=rhs; break;
		default: break;
	  }
		//printf("'%f' %d '%f' => %d\n", lhs, (int)op, rhs, result);
	}
	else return neg ? ~(short)lhs : lhs; //return lhs as double when no rhs of relop, unless inverted (convert to inverted short)
  }
  //else  ERRORMSG(AnsiString(symbol.c_str()) + " cannot start a relational expression");
  if (neg==result) return 0;
  else return -1;
}

double andExpr()  //andExpr: relExpr CHAIN AND
{ double result = relExpr();
  while(sym==OPERATOR && oper==AND)
  { nextsym();
	result = (short)result & (short)relExpr();
  }
  //printf(" AND result=%f, (%d)\n", result, result==0);
  return result;
}

double orExpr()   //orExpr: andExpr CHAIN OR
{ double result = andExpr();
  while(sym==OPERATOR && oper==OR)
  { nextsym();
	result = (short)result | (short)andExpr();
  }
  //printf(" OR result=%f, (%d)\n", result, result==0);
  return result;
}
/*
stringExpression : string CHAIN '+'
string : STRVAR | STRARRAY subscript | stringFunction
stringFunction	: (STR$ | CHR$) '(' numExpression ')'
				| (LEFT$ | RIGHT$) '(' stringExpression ',' numExpression ')'
				| MID$ '(' stringExpression ',' numExpression [ ',' numExpression ] ')'
*/
std::string parseStringExpression()
{ std::string result = "";
  keyw_t fun;
  while(true)
  { if(sym==STRING)
	{ result = result + identifier;
	  nextsym();
	}
	else if (sym==STRVAR)
	{ //std::map<std::string, Var*>::iterator v = vars.find(identifier);
	  VarIter v = vars.find(identifier);
	  if(identifier.substr(0, 2) == "TI")
	  { char buf[30];
		//time_t t = time(NULL);
		//strftime(buf, 8, "%H%M%S", localtime(&t));    // %T with : :
	    //uint32_t t = updateSystemTime()/60;
	    Clock24::Time t = clk.getTime();
	    //snprintf(buf, sizeof(buf), "%02u%02u%02u", (t/3600)%24, (t/60)%60, t%60);
	    snprintf(buf, sizeof(buf), "%02u%02u%02u", t.hours, t.minutes, t.seconds);
        vars[identifier]->set(std::string(buf));
		result += std::string(buf);
	  }
	  else
	  { if(v == vars.end())
		  ERRORMSG("Error: variable not found ");
		else
		{  //StrVar* sv = (StrVar*)(v->second);
		   StrVar* sv = (StrVar*)(v->second.get());
		   result += sv->getString();
		}
	  }
      nextsym();
	}
	else if (sym==STRARRAY)
	{ //std::map<std::string, Array*>::iterator v = arrays.find(identifier);
	  ArrayIter v = arrays.find(identifier);
	  nextsym();
	  if(v == arrays.end())
		ERRORMSG("Error: variable not found ");
	  else
	  { int idx = handleSubscripts(v);
		//result += ((StrArray*)(v->second))->getString(idx);
		result += ((StrArray*)(v->second.get()))->getString(idx);
	  }
	}
	else if (returnsString(sym, keyword))
	{ fun = keyword;
	  std::string temp;
	  unsigned par = 0, par2 = 0;
	  nextsym();
	  if(sym==LPAR)
	  { nextsym();
		switch(fun)
		{ case STRS:
		  case CHRS: { double arg = orExpr();
					   if(fun==STRS)
					   {
#if 1
						 temp = numberToString(arg);
#else
						 double intpart, frac = modf(arg, &intpart);
						 if(fabs(frac)<1e-5)
							temp = std::to_string((long)arg); //snprintf(buf, sizeof(buf), "%ld", (long)arg);
						 else
							temp = std::to_string(arg); //snprintf(buf, sizeof(buf), "%f", arg);
#endif
					   }
					   else  //CHR$
					   { char buf[5] = {(char)arg, '\0'};
						 //snprintf(buf, sizeof(buf), "#%02X", (unsigned)arg & 0xFF);
						 temp = buf;
					   }
					 }
					 break;
		  case MIDS:  //returns par2 characters starting at par (1 based!)
		  case LEFTS:  //returns the first par characters
		  case RIGHTS: //returns the last par characters
					   temp = parseStringExpression();
					   if(sym==COMMA) nextsym();
					   else ERRORMSG("Error: ',' expected: substring functions have at least 2 parameters");
					   par = parseExpression(); //Length of LEFT$/RIGHT$ or start of MID$
					   switch(fun)
					   { case MIDS: if(par>0) par--; //index is 1 based
									if(sym==COMMA)
									{ nextsym();
									  par2 = parseExpression();  //length of MID$
									  if(par>=temp.length()) temp = ""; else temp = temp.substr(par, par2);
									}
									else if(par>=temp.length()) temp = ""; else temp = temp.substr(par);
									break;
						 case LEFTS: temp = temp.substr(0, par); break;
						 case RIGHTS: if(par<temp.length()) temp = temp.substr(temp.length()-par); break;
						 default: break;
					   }
					   break;
		  default: ERRORMSG("Error: illegal string function in this context");
		}
		if(sym!=RPAR)
			ERRORMSG("Error: ')' expected after function parameters");
		nextsym();
	  }
	  else ERRORMSG("Error: '(' expected after function name");
	  result = result + temp;
	}
	else ERRORMSG("Error: malformed string expression");
	if(sym==OPERATOR && oper==PLUS)  nextsym();
	else break;
  }
  //Form1->Memo2->Lines->Add(result.c_str());
  return result;
}

