//---------------------------------------------------------------------------


#include <stack>
#include <vector>
#include <string>
#include "parser.h"
#include "expression.h"
#include "common.h"
#include "user.h"

#include "files.h"
//#include "program.h"
//---------------------------------------------------------------------------

/* The parser is the bread and butter of the BASIC engine, it's entrypoint is ParseLine(std::string) which normally executes
   one full line of statements unless a jump (GOSUB/THEN/INPUT/GET/STOP/END) leaves the line or a RETURN or resume from INPUT/GET/CONT reenters it.
   INPUT/GET/PRINT handling is in main.cpp because of the GUI dependency
   expressions are handles in expressions.cpp which is also part of the BASIC engine
   variables, arrays en DATA are handled in data.cpp  which is also part of the BASIC engine
   OPEN/CLOSE/PRINT#/GET# are handled in files.cpp which is more like a stub
   graphical functions from simons basic are handled in plotter.cpp which is optional
   the parser needs the lexer from lexer.cpp which has several options, see lexer.h
*/

#define DIAGMESSAGE(x) info(x)

/* meta symbols used (where 'symbol' can be any terminal or nom-tyerminal symbol) :
nonterminal:symbol OPTION 			a.k.a. [ symbol ] means:
nonterminal	: symbol
			| // nothing
			;
nonterminal:symbol CLOSURE  		a.k.a. [ symbol ... ]	means:
nonterminal	: symbol nonterminal
			| // nothing
			;
nonterminal:symbol SEQUENCE  		a.k.a. symbol [ symbol ... ]	means:
nonterminal	: symbol nonterminal
			| symbol
			;
nonterminal:symbol1 CHAIN symbol2  a.k.a. symbol1 [ symbol2 symbol1 ] ... 	means:
nonterminal	: symbol1 symbol2 nonterminal
			| symbol1
			;
nonterminal:symbol LIST  			a.k.a. symbol [ ',' symbol ] ... 	means:
nonterminal	: symbol ',' nonterminal
			| symbol
			;
natural implementations are (where sym is the current terminal symbol and LA is the look-ahead set):
OPTION: if(sym in LA) symbol() ;
CLOSURE: while(sym in LA) symbol();
SEQUENCE: do{ symbol(); while (sym in LA);
CHAIN: do { symbol1(); if(sym NOT in LA) break; symbol2();} while (sym in LA);
or, e.g. for LIST
LIST: symbol(); while (sym == ',') symbol();
*/

//public variables
bool inputGet = false;  //set to true when expecting an input to GET (as opposed to INPUT)


std::vector<Target> callStack;
std::vector<ForCtrl> loopStack;
std::vector<VarRef> varList;
std::map<std::string, FunctionDef> functions;


//------------------ ForCtrl -------------------------
  bool ForCtrl::cond(double val)   //iteration condition, TRUE on terminate, FALSE on continue
  { return (step < 0 && val < end) || (step > 0 && val > end) /*|| step==0*/; //step==0 is to prevent infinite loops, alternatively warn on init or just allow it
  }

  bool ForCtrl::iterate()   //iterate one step and return TRUE on final step
  { double val = v->get();
	val += step;
	v->set(val);
	return cond(val);
  }
//---------------------------------------------------------------------
// '(' <expression> LIST ')'
//static void dimension(std::map<std::string, Array*>::iterator v)   //handle DIM statement
static void dimension(ArrayIter v)   //handle DIM statement
{ //Array *a = v->second;
  Array *a = v->second.get();
  if(a->dimensions() > 0)
  { ERRORMSG("cannot reDIM array");
	return;
  }
  if(sym==LPAR)
  { nextsym();
	while(true)
	{ unsigned i = parseExpression();
	  a->addDimension(i+1);
	  if(sym==COMMA) nextsym(); else break;
	}
	if(sym!=RPAR) ERRORMSG("expected ')' after DIM list");
	nextsym();
  }
  else
	ERRORMSG("DIM without parameters");
}

/* printargs: printarg CLOSURE (EOLN | COLON)
   printarg	: stringarg
			| TABt numexpression RPAR
			| SPCt numexpression RPAR
			| TAB LPAR numexpression RPAR
			| SPC LPAR numexpression RPAR
			| logexpression
			| COMMA
			| SEMICOLON
			;
*/

static void printLength(const std::string& s, unsigned &pos)
{ for(unsigned i = 0; i < s.length(); i++)
  switch(s[i])
  { case '\r': case 19: case 147: pos = 0; break;
    case 29: pos++; break;
    case 157: if(pos>0) pos--; break;
    default: if((s[i]&0x7F) >= 32) pos++; break;
  }
}

static std::string parsePrintArgs(bool&nl)  //handle arguments of PRINT and PRINT#
{ nl = true;
  std::string printline;
  unsigned pos = getCursorPos();//the horizontal number of printable characters already printed
  while (!(sym == EOLN || sym == COLON))
  { if(isStringVariable(sym) || sym==STRING || returnsString(sym, keyword))
	  printline += parseStringExpression();
	else if (isFormatFunction(sym, keyword))
	{ keyw_t fun = keyword;
	  nextsym();
	  if(fun == TAB || fun == SPC)
	  { if(sym==LPAR) nextsym();
		else ERRORMSG("Error: '(' expected after TAB or SPC function");
	  }
	  unsigned arg = parseExpression();
	  if(sym==RPAR) nextsym();
	  else  ERRORMSG("Error: ')' expected after function parameter");
	  if (fun==SPC || fun==SPCt) printline = printline + std::string(arg, ' ');
	  else //TAB
	  { unsigned n = pos; printLength(printline, n);//update n according to characters in buffer (MAY RESET THE COUNTER)
		if (n<=arg) printline = printline + std::string(arg-n, '\x1d'); //cursor right
//		if (n<=arg) printline = printline + std::string(arg-n, ' ');
		//else { print(printline.c_str()); printline = std::string(arg, ' ');}		 //just ignore tab when already past position
	  }
	}
	else
	{ double x = orExpr();
	  if(x>=0) printline += " "; //leading space for positive numbers
	  printline += numberToString(x) + " ";
	}
	if(sym==COMMA) //use tabulation
	{ nextsym();
	  unsigned n = pos; printLength(printline, n);
      printline = printline + std::string(10*(n/10+1)-n, ' ');
	}
	else if(sym==SEMICOLON)
	{ nextsym();
	  nl = false;
	}
	else
		nl = true;
  } //while
  return printline;
}

static std::vector<double> parList;
// parlist: (logexpression LIST) OPTION
static void parseParList()  //only used for Simons BASIC extensions, no actions
{ parList.clear();
  while(true)
  { parList.push_back(orExpr());
	if (sym != COMMA) break;
	nextsym();
  }
}

/*  on NEXT without variable: use the top of the stack, pop when exhausted
	on NEXT with variables: if variable is on top treat as above,  otherwise proceed to next variable in list
	NEXT with multiple variables e.g. NEXT  A, B: treat as NEXT A: NEXT B
*/
//nextlist: (VARNAME LIST) OPTION
//ToDo: this will not work in direct mode because there in no line 0
static unsigned parseNextList()  //used for 'NEXT' [<varlist>]
{ if(loopStack.size()==0)
  { print("NEXT WITHOUT FOR");
	return 0;   //error
  }
  ForCtrl fc = loopStack.back();
  if(sym==REALVAR)  //can it be an array variable?
  { //at least one named variable
	do  //iterate over the variables in the list
	{ //std::map<std::string, Var*>::iterator v = vars.find(identifier);
	  VarIter v = vars.find(identifier);
	  nextsym();
	  if(v == vars.end()) return 0;  //panic! control variable not found!
	  //Var *iter = v->second;
	  Var *iter = v->second.get();
	  if(iter != fc.v) //the current variable is not on top of the stack
	  { DIAGMESSAGE("loop preempted"); //the program has stepped over an inner NEXT that wasn't completed
		do
		{ loopStack.pop_back(); //pop the stack until a matching variable
		  if(loopStack.size()==0)
		  { ERRORMSG("NEXT without matching FOR");
			return 0;   //error
		  }
		  fc = loopStack.back();
		} while (iter != fc.v); //when a match is found continue
		//now the current variable is on top of the stack
	  }
	  //the current variable in the list is on the top of the stack //proceed as case without variable
	  { if(fc.iterate())
		  loopStack.pop_back(); //the loop is complete, move to next variable if any
		else
		{ //return fc.line;   //return to the begining of the FOR loop
		  if(state==running)
		  { scan->start(program[fc.tgt.nline], fc.tgt.pos);//the new position is just after the COLON or EOLN after the FOR statement
		    pc = program.find(fc.tgt.nline);
		  }
		  else
			scan->set(fc.tgt.pos, fc.tgt.sym);
          sym = fc.tgt.sym;
		  return 0;//fc.line;   //return to the beginning of the FOR loop
		}
	  }
	  if (sym != COMMA) break;   //no more variables, NEXT statement ends
	  nextsym();
	  //move to next variable
	  if(loopStack.size()>0)
		fc = loopStack.back();
	  else
	  { ERRORMSG("more NEXT than FOR");
		return 0;  //error
	  }
	} while(sym==REALVAR); //until the list is exhausted
	return 0;  //end of NEXT, continue with next statement
  }
  else //no named variable
  { if(fc.iterate())
	{ loopStack.pop_back();
	  return 0;     //end of loop continue
	}
    if(state==running)
	{ scan->start(program[fc.tgt.nline], fc.tgt.pos);//the new position is just after the COLON or EOLN after the FOR statement
	  pc = program.find(fc.tgt.nline);
	}
	else
	  scan->set(fc.tgt.pos, fc.tgt.sym);
    sym = fc.tgt.sym;
	return 0;//fc.line;   //return to the beginning of the FOR loop
  }
}

//varlist (VARNAME LIST) OPTION , NOTE: the varList is used in main.cpp for input handling
static void parseVarList()  //used for 'INPUT' [<prompt>;] <varlist>  and 'READ' <varlist>
{ varList.clear();
  while(isVariable(sym)||isArray(sym))
  {	VarRef ref;
	if (isArray(sym))
	{ //std::map<std::string, Array*>::iterator v = arrays.find(identifier);
	  ArrayIter v = arrays.find(identifier);
	  nextsym();
	  ref.i = handleSubscripts(v);
	  //ref.v = v->second;
	  ref.v = v->second.get();
	}
	else
	{ //std::map<std::string, Var*>::iterator v = vars.find(identifier);
	  VarIter v = vars.find(identifier);
	  nextsym();
	  //ref.v = v->second;
	  ref.v = v->second.get();
	}
	varList.push_back(ref);
	if (sym != COMMA) break;
	nextsym();
  }
}

/* Statement sequencing:
unconditional:
	GOTO <line> : goto the indicated line, no need to scan the rest of the line                                              (1)
conditional:
	ON <int> GOTO <line>... : goto the computed line, no need to scan the rest of the line, if none apply goto the next line (2)
	IF <true> THEN <line> : same as unconditional                                                                            (3)
	IF <true> THEN <statement>... : treat ALL statements following THEN as usual                                             (4)
	IF <false> : proceed to next line, no need to parse the rest of the line but sym MUST be EOLN                            (5)
loop:
	FOR <control var assignment> TO <end expression> [STEP <step expression>] : mark the end of the statement (on the loop stack, sym[COLON, EOLN]), proceed to next STATEMENT (6)
	NEXT : <control var> += <step>; if <control var> < <end> goto marked position else proceed to next STATEMENT                                                               (7)
call:
	GOSUB <line> : mark the end of the statement (on the call stack, sym[COLON, EOLN]), goto the indicated line                                        (8)
	ON <int> GOSUB <line>... : mark the end of the statement (on the call stack), parse ALL linenumbers , sym[COLON, EOLN], goto the computed line     (9)
	RETURN : goto the position (STATEMENT) on the call stack                                                                                           (10)
function:
	DEF <fname> ( <arg> ) = <expression> : mark the beginning of the expression in the function list (along with ref to the arg variable)              (11)
	execution (in expression) :  save and restore the scanner context                                                                                  (12)
*/


//file handling functions  , files.h/.cpp
 static void parseOpenArgs()
 { uint8_t unit = parseExpression();
   uint8_t dev = 1; //datasette
   uint8_t sec = 0; //read
   std::string rest;
   if(sym==COMMA)
   { nextsym();
	 dev = parseExpression();
	 if(sym==COMMA)
	 { nextsym();
	   sec = parseExpression();
	   if(sym==COMMA)
	   { nextsym();
		 rest = parseStringExpression();
	   }
	 }
   }
   if(files.find(unit) != files.end())
	   ERRORMSG("? FILE ALREADY OPEN");
   else
   { C64File& f = files[unit];
     if(files.find(unit) == files.end()) ERRORMSG("? FILE DESCR COULD NOT BE CREATED");
     else f.open(dev, sec, rest);
   }
 }

 static void parseFileSpecification(bool load)
 { if((sym==EOLN || sym==COLON) && !load)
   {  Save();
	  return;
   }
   std::string fn = parseStringExpression();
   //uint8_t dev = 1, sec = 0; //datasette, basic prog
   uint8_t dev = 8, sec = 0; //first disk (.prg file), basic prog
   if(sym==COMMA)
   { nextsym();
	 if(load && fn.empty()) ERRORMSG("LOAD needs a filename");
	 dev = parseExpression();
	 if(dev==0 || dev>15)  ERRORMSG("invalid device number");
	 if(sym==COMMA)
	 { nextsym();
	   sec = parseExpression();
	   if(sec<0 || sec>2)  ERRORMSG("invalid secondary number");
	 }
   }
   if(sym==EOLN)
   { if(load) Load(fn, dev, sec);
	 else Save(fn, dev, sec);
   }
   else ERRORMSG("unexpected symbol in LOAD/SAVE statement");
 }

 /* processInputH works similar to processInput but does not consider the case of too few data.
	Also it does not depend on line separators in the file, so the data is not read from a string but directly fom the file.
	This means that the varList is not necessary, the data could be read while parsing. FIX THIS
 */
 static bool processInputH(uint8_t unit)
{ char str[81];
  if(files.find(unit)==files.end())
  { ERRORMSG("logical channel is not open");
	return false;
  }
  files[unit].Restart();
  try
  {	for(size_t idx = 0; idx < varList.size(); idx++)
	{ VarRef ref = varList[idx];
	  switch(ref.v->Type())
	  { case Var::type::integer:
		case Var::type::real:     ref.v->set(files[unit].readNum());
								  break;
		case Var::type::string:   files[unit].readStr(str, sizeof(str)-1);
								  ref.v->set(std::string(str));
								  break;
		case Var::type::integerarray:
		case Var::type::realarray:    ref.v->set(files[unit].readNum(), ref.i);
									  break;
		case Var::type::stringarray:  files[unit].readStr(str, sizeof(str)-1);
									  ref.v->set(std::string(str), ref.i);
									  break;
	  }
	}
  }
  catch(const char* msg)
  {  fprintf(stderr,"INPUT# reports: %s\n", msg);
	  return false;
  }
  catch(...)   //type mismatch/conversion error
  {  ERRORMSG("error processing INPUT#");
	 return false;
  }
  return true;
}

 static bool processGetH(uint8_t unit)
 { char ch[] = {0, 0};
   files[unit].Restart();
   try
   { for(size_t idx = 0; idx < varList.size(); idx++)
 	 { VarRef ref = varList[idx];
 	   switch(ref.v->Type())
 	   { case Var::type::integer:
		 case Var::type::integerarray:ref.v->set(files[unit].readInteger(), ref.i);
 	                                  break;
 		 case Var::type::real:
 		 case Var::type::realarray:ref.v->set(files[unit].readNum(), ref.i);
 		 						   break;
  		 case Var::type::stringarray:
 		 case Var::type::string:   ch[0] = files[unit].read();
 		                           ref.v->set(std::string(ch), ref.i);
 								   break;
 	   }
 	 }
   }
   catch(...)   //type mismatch/conversion error
   { ERRORMSG("error processing GET#");
 	 return false;
   }
   return true;
 }
//end of file handling functions

/* parses/executes a single statement
  returns: the linenumber of the next statement following [ON] GOTO/GOSUB
		   0: for execution of the next Statement //possibly on the next line
		   -1:
*/
static int parseStatement()
{  int dest = 0;
   if(sym==EOLN || sym==COLON) { nextsym(); info("Warning: empty statement"); return 0;} //empty statement
   if (sym==COMMAND && keyword != LET)
   { //parse command
	 keyw_t cmd = keyword;
	 std::string keywordString = identifier;//only for the wedge, alternative is to make WEDGE a symbol instead of a command
	 uint8_t unit;
	 nextsym(); //read ahead for simplicity, unfortunately messes up keyword and identifier so these need saving
	 switch(cmd)
	 { case SYS:   sys((unsigned)parseExpression());
				   break;
	   case WEDGE: if(wedge(keywordString.c_str())==0)
		              ERRORMSG("?SYNTAX ERROR (WEDGE FAILED)");
	               break;
 	   case GET:   if(sym==HASH)
				   { nextsym();
					 unit = parseExpression();
					 if(sym==COMMA)
					 { nextsym();
#if 1
					   parseVarList();
					   processGetH(unit);
#else
					   char ch = files[unit].read();
					   if(sym==STRVAR)
					   { vars[identifier]->set(std::string(1, ch));
						 nextsym();
					   }
					   else if(sym==STRARRAY)
					   { std::map<std::string, Array*>::iterator v = arrays.find(identifier);
						 if(v==arrays.end()) throw "Panic unknown string array";
						 nextsym();
						 int idx = handleSubscripts(v);
						 ((Array*)(v->second))->set(std::string(1, ch), idx);
					   }
#endif
					 }
					 else ERRORMSG("Warning: comma expected");
				   }
				   else //no hash, regular get from keyboard
				   { print("", false); //does nothing here, acts as flush in GUI app
					 inputGet = true;
					 parseVarList();
					 if(state==running)
					 { //state = waiting;
					   processInputKey();
					 }
					 else ERRORMSG("?ILLEGAL DIRECT ERROR");
				   }
				   redirect = 0; //GET terminates redirection
				   break;
	   case PRINTH:unit = parseExpression();
	               if(sym==COMMA)
				   { nextsym();
				     if(!files[unit].istty())
					 { bool nl = true;
					   std::string line = parsePrintArgs(nl);
					   files[unit].write(line.c_str());
					   if(nl)files[unit].write(unit>128 ? "\r\n" : "\r");
					   break;
					 }
				   }
	               else
				   { if(unit==redirect)
					   redirect = 0;
				     break;
				   }
				   //fallthrough for the case where we were redirected to the tty
	   case PRINT: { bool nl = true;   //redirection (CMD) not supported ???
					 std::string tmp = parsePrintArgs(nl);
					 print(tmp, nl);
				   }
				   break;
	   case INPUTH:unit = parseExpression();
	               if(sym==COMMA) nextsym();
	               else ERRORMSG("Warning: comma expected");
				   parseVarList();
				   processInputH(unit);
				   break;
	   case INPUT_:{  if (sym==STRING)
					  { print(identifier, false);
						nextsym();
					    if (sym==SEMICOLON) nextsym();
					    else ERRORMSG("expected ';' ");
					  }
					  //print(" ? ", false); //done by processInput
					  //print("", false); //flush
				   }
				   inputGet = false;
				   parseVarList();
				   if(state==running)
				   { //state = waiting;
					 while(!processInput()) print("?REDO FROM START");
				   }
				   break;
	   case IF:     { double cond = fabs(orExpr()); //0==false, otherwise true
					  bool r = state==running || state==cmdmode;
					  bool c = cond==0;
					  if(r && c)
					  { //printf(" false r=%d, c=%d, cond=%f", r, c, cond);
						  dest = -3; //special value to skip rest of line
						  break; //the condition is false and executing so don't process the rest of the line, sym==THEN! or sym==GOTO!  (5)
					  }
					  if(sym==COMMAND && keyword==THEN)//condition is true or just parsing
					  { nextsym(); //printf(" true THEN ... ");
						if(sym==NUMBER) //linenumber, GOTO omitted   (3)
						{ dest = value;
						  nextsym();
						  break;
						}
					  }
					  else
					  { if(!(sym == COMMAND && keyword==GOTO)) ERRORMSG("IF <condition> must be followed by THEN or GOTO ");
					    //printf(" true GOTO ");
						break;
					  }
					  //sym==GOTO || sym is other other statement after THEN  (4)
					}
					break;
	   case GOSUB: if(sym==NUMBER)  //NOTE: GOSUB may not be the last statement on the line (:) in which case it must return to the next statement (not line)  (8)
				   { dest = value;
					 nextsym();
					 Target ret{lineno, scan->pos(), sym}; //so push the actual position and current symbol
					 if(state==running)
					   callStack.push_back(ret);
				   }
				   else ERRORMSG("expected line number ");
				   break;
	   case GO:    if(sym==COMMAND && keyword==TO) nextsym(); else ERRORMSG("Go without TO");
                   //fallthrough
	   case GOTO:  if(sym==NUMBER)   //  (1)
				   { dest = value;
					 nextsym();
                     if(state==cmdmode) { startProg(dest); dest = -1;}
				   }
				   else ERRORMSG("expected line number ");
				   break;
	   case FOR:   if (sym==REALVAR)  //NOTE: FOR may not be the last statement on the line in which case the loop starts at the next statement, not the next line  (6)
				   { //std::map<std::string, Var*>::iterator iter = vars.find(identifier);
					 VarIter iter = vars.find(identifier);
					 nextsym();
					 if(iter==vars.end())
					 { ERRORMSG("control variable not found");
					   break;
					 }
					 unsigned line = (state==running) ? lineno : 0;
					 //ForCtrl fc{iter->second, {line, 0}, 0, 1};
					 ForCtrl fc{iter->second.get(), {line, 0}, 0, 1};
					 if (!((sym == OPERATOR) && (oper == EQ))) { ERRORMSG("expected '='"); }
					 nextsym();
					 iter->second->set(parseExpression()); //initial value
					 if (sym==COMMAND && keyword==TO)
					 { nextsym();
					   fc.end = parseExpression();
					   if (sym==COMMAND && keyword==STEP)
					   { nextsym();
						 fc.step = parseExpression();
						 //if(fc.step==0) ERRORMSG("STEP = 0, loop will be executed once to prevent endless loop");
					   }
					   else fc.step = 1;
					 }
					 else ERRORMSG("expected 'TO <endvalue>'");
					 if(fc.cond(iter->second->get())) DIAGMESSAGE("termination condition is TRUE at the start of the FOR loop (will be executed once)");
					 fc.tgt.pos = scan->pos(); //sym==COLON || sym==EOLN
					 fc.tgt.sym = sym;
					 loopStack.push_back(fc);
				   }
				   else  ERRORMSG("control variable must be REAL");
				   break;
	   case NEXT:  dest = parseNextList();  //(7)
				   break;
	   case RETURN:if(state==running)     //(10)
				   { if(callStack.size()>0)
					 { Target ret = callStack.back();
					   callStack.pop_back();
					   scan->start(program[ret.nline], ret.pos);  //nline is the line with [ON} GOSUB, pos is the position after the SYM behind the linenumber(s) (typ. after the ':') or pos is at the null terminator
					   sym = ret.sym;                             //in case of : sym==COLON otherwise sym==EOLN
					   pc = program.find(ret.nline);
					 }
					 else
						ERRORMSG("RETURN without GOSUB");
				   }
				   break;
	   case END:   state = ended; break;
	   case DIM:   while (sym==INTARRAY || sym==REALARRAY || sym==STRARRAY)
				   { //std::map<std::string, Array*>::iterator arr = arrays.find(identifier);
					 ArrayIter arr = arrays.find(identifier);
					 nextsym();
					 if(arr != arrays.end())
						 dimension(arr);//dimension(arr.get()); //
					 else ERRORMSG("array variable name not found");
                     if(sym==COMMA) nextsym(); //comma separated list of DIM statements
				   }
				   break;
	   case READ:  parseVarList();
				   for(auto& v: varList)
				   { bool isString = v.v->Type()==Var::string || v.v->Type()==Var::stringarray;
					 { if(isString)
						 v.v->set(literals.getString(), v.i);
					   else
						 v.v->set(literals.getNumber(), v.i);
					 }
				   }
				   break;
	   case DATA:  break;   //parsed as part of the READ statement
	   case RESTORE: literals.restore();
				   //literals2.restore();
				   break;
	   case ON:    { unsigned cond = orExpr();
					 if(sym==COMMAND && (keyword==GOTO || keyword==GOSUB))
					 { keyw_t k = keyword;
					   nextsym();
					   unsigned i = 1;
					   while(sym==NUMBER)
					   { if(i==cond) dest = value;
						 nextsym();
						 if(sym==COMMA) nextsym();
						 i++;
					   }
					   if(dest==0) ERRORMSG("no valid ON GOTO/GOSUB target");
					   if (k==GOSUB) // (9)
					   { if(dest && state==running)
						 {  Target ret{lineno, scan->pos(), sym}; //so push the actual position and current symbol
							callStack.push_back(ret);
						 }
					   }//else k==GOTO (2)
					 }
					 else ERRORMSG("expected GOTO or GOSUB after 'ON' <condition> ");
				   }
				   break;
	   case DEF:   if(sym==FUNCNAME)
				   { std::string fnName = identifier;
					 nextsym();
					 if(sym==LPAR)
					 { nextsym();
					   if(sym==REALVAR)
					   { //NumVar *v = (NumVar*)vars[identifier];
						 NumVar *v = (NumVar*)vars[identifier].get();
						 nextsym();
						 if(sym==RPAR)
						 { nextsym();
						   FunctionDef fn(fnName);
						   if(sym==OPERATOR && oper==EQ)
						   { fn.tgt.nline = lineno;    //tgt is begin of expression
							 fn.tgt.pos = scan->pos(); //just after '='
							 //fn.tgt.sym = sym;         //hence sym is OPERATOR '=' not used
							 fn.par = v;   //formal parameter
							 nextsym();
							 //orExpr(); // the expression must be scanned because there may be more statements on the line (may reference uninitialised variables)
							 while(sym != EOLN && sym != COLON) nextsym(); //just scan to the next statement (may create new variables)
						   } else ERRORMSG("'=' expected in FN definition");
						   std::map<std::string, FunctionDef>::iterator def = functions.find(fnName);
						   if(def != functions.end()) info("redefining " + fnName);
						   functions[fnName] = fn;
						 }else ERRORMSG("')' expected after parameter in FN definition");
					   }else ERRORMSG("Real variable expected in FN definition");
					 }else ERRORMSG("'(' expected after FN name in FN definition");
				   } else ERRORMSG("FN name expected in FN definition");
				   break;
	   case OPEN:   parseOpenArgs();
					break;
	   case CLOSE:  { uint8_t unit = parseExpression();
					  files[unit].close();
					  files.erase(unit);
					}
					break;
#ifdef SIMONS
	   case HIRES:  parseParList(); if(parList.size()==2) hires(parList[0], parList[1]); break;//simons basic
	   case CIRCLE: parseParList(); if(parList.size()==5) circle(parList[0], parList[1], parList[2], parList[3], parList[4]); break;  //simons basic
	   case LINE:   parseParList(); if(parList.size()==5) line(parList[0], parList[1], parList[2], parList[3], parList[4]); break; //simons basic
	   case PLOT:   parseParList(); if(parList.size()==3) plot(parList[0], parList[1], parList[2]); break;  //simons basic
#endif //SIMONS
	   case POKE: parseParList();      //poked data obviously has no effect in this emulator
				  if(parList.size()==2)
					 poke(parList[0], parList[1]);
				  else ERRORMSG("POKE needs 2 parameters");
				  break;
	   case WAIT: //throw "WAIT not supported";
	              parseParList();
	              if(parList.size()==2 || parList.size()==3)
	              { uint8_t a = parList[1], x = 0;
	                uint16_t addr = parList[0];
	                if(parList.size()==3) x = parList[2];
	                while(!((peek(addr) ^ x) & a) && state!=stopped)/*wait*/ ;
	              }
	              else ERRORMSG("WAIT needs 2 or 3 parameters");
	              break;
	   case RUN:  if(state != running)    //command, just to start program from a different line
				  { if(sym==NUMBER)
					{ nextsym();
					  Run(value);
					}
					else Run();//will change state!
					dest = -1;
				  }
				  break;
	   case STOP:  Stop(); break;
	   case LOAD:
	   case SAVE: parseFileSpecification(cmd==LOAD);
				  break;
	   case VERIFY:Verify(); break;
	   case CONT: Cont(); dest = -1; break;
	   case LIST: { uint16_t beg = 0, end = 0;    //entire program
					if(sym==NUMBER) { beg = end = value; nextsym();}  //one line
					if(sym==OPERATOR && oper==MINUS) { end = 0;  nextsym();}           //until end of program
					if(sym==NUMBER) { end = value; nextsym();}       //range
					List(beg, end);
				  }
				  break;
	   case CLR:  Clr(); break;
	   case NEW:  New(); break;
	   case CMD:  if(sym==NUMBER)
				  { Cmd(value);
					nextsym();
					if(sym==COMMA)
					{ nextsym();
					  bool nl = true;
					  std::string tmp = parsePrintArgs(nl);
					  print(tmp, nl);
					}
				  }
				  else ERRORMSG("CMD needs logical filenumber");
				  break;
	   default:   ERRORMSG("unexpected command " + std::string(keywords[cmd]));
	              //throw "unsupported command";
				  break;
	 }
	 return dest;
   }
   //no command, hence assignment
   if (sym==COMMAND && keyword == LET) nextsym();  //skip over optional 'LET'
   if (sym==INTVAR || sym==REALVAR || sym==STRVAR)
   { //std::map<std::string, Var*>::iterator lhs = vars.find(identifier);
     VarIter lhs = vars.find(identifier);
   	 bool isString = sym==STRVAR;
	 if(lhs!=vars.end())
	 { nextsym(); //eat the varname
	   if (!((sym == OPERATOR) && (oper == EQ))) { ERRORMSG("expected '='"); }
	   nextsym(); //eat the '='
	   if(isString)
	   { std::string tmp = parseStringExpression();
	     Var* v = lhs->second.get();
	   	 //Var* v = lhs->second;
		 std::string name = lhs->first;
		 if(name=="TI$" || name=="TIME$") //TI$ time string, must also set TI number
		   setTime(tmp);
		 v->set(tmp);
	   }
	   else lhs->second->set(orExpr());
	 }
	 else ERRORMSG("unknown assignment variable " + identifier);
   }
   else if (sym==INTARRAY || sym==REALARRAY || sym==STRARRAY)
   { //std::map<std::string, Array*>::iterator lhs = arrays.find(identifier);
     ArrayIter lhs = arrays.find(identifier);
   	 bool isString = sym==STRARRAY;
	 if(lhs!=arrays.end())
	 { nextsym(); //eat the varname
	   int idx = handleSubscripts(lhs);
	   //int idx = handleSubscripts(lhs.get());
	   if (!((sym == OPERATOR) && (oper == EQ))) { ERRORMSG("expected '='"); }
	   nextsym(); //eat the '='
	   if(isString)
	   { std::string tmp = parseStringExpression();
	     //Array* v = lhs->second;
	   	 Array* v = lhs->second.get();
		 std::string name = lhs->first;
		 v->set(tmp, idx);
	   }
	   else lhs->second->set(parseExpression(), idx);
	 }
	 else ERRORMSG("unknown assignment variable " + identifier);
   }
   else
   { ERRORMSG("expected lvalue");
	 nextsym();
   }
   return 0;
}

/* parse/execute a full line of statements,   main entry point of the parser
   unless: sym==COLON and
		   interrupted by STOP (stopped, 0),  can be resumed  (scanner state?)
						  END (ended, 0)   ,  cannot be resumed
						  INPUT/GET (waiting, 0), can/will be resumed (scanner state unaffected but commands must be blocked)
						  [ON] GOSUB (running, dest), will be resumed after RETURN
*/
int parseLine() //parse a full BASIC line except the line number
{ int dest = 0;
  while (true)
  { if(((sym==COMMAND) && (keyword==REM || keyword==DATA)) || sym==EOLN) break; //skip the rest of this line
	dest = parseStatement();  //dest==0 indicates next/statemt/line, dest!=0 indicates a jump
	if(state==ended || state==stopped || state==waiting)  //resp. from END,STOP,INPUT/GET
	{  if(sym==EOLN) return 0;    //the above statement was the last on the line, proceed with next line
	   else if(sym==COLON) nextsym(); //the line is not finished but we need to interrupt processing
	   return -1; //indicate not to move the PC and do not read a new symbol
	}
	//state is running/idle/cmdmode
	if (sym==EOLN)  //line complete,
	  break;     //i.e. return dest
	else if(dest==-3 /*sym==COMMAND && (keyword==THEN || keyword==GOTO)*/)//if statement with false condition, the rest of the line is skipped, not just the 1st statement, so no need to return there
	  return 0;   //next line
	else if (sym==COLON) //next statement on the line
	{ nextsym(); //eat the ':'
	  if(dest>0)  //typically after GOSUB, the present state is on the call stack,  the line isn't finished but we must move to a new line
		 break;  //i.e. return dest
	}
	//else proceed with next statement on line (typically after THEN or after ':' in jumps (RETURN))
	if (state==running)
	   lineno = pc->first;  //restore the line number when next statement is a jump target
  }//while
  return dest;  //proceed to next line or jump target
}


