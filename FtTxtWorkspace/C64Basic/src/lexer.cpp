//---------------------------------------------------------------------------
#include <iostream>
#include "lexer.h"
#include "data.h"
#include "parser.h"
#include "common.h"
//---------------------------------------------------------------------------

#define CASE(c) toupper((c))     //consider all input as uppercase
//#define CASE(c) (c)            //preserve case

/* C64 basic has a few oddities:
   PRINT# and INPUT# are single token symbols but GET#  is two GET and #
   SPC( and TAB( are single token symbols
   single character operators are single tokens (without advantage) but double character operators are not (which would have been an advantage)
   variable names and numbers are not tokenised
*/

unsigned wedge(const char* line);


const char*symnames[] = {"NONE","NUMBER","STRING","INTVAR","REALVAR","STRVAR","FUNCNAME","COMMAND","FUNCTION","OPERATOR","EOLN","HASH","LPAR","RPAR","COMMA","COLON","SEMI","INTARY","REALARY","STRARY"};//for debugging only

const char *keywords[] = {"END","FOR","NEXT","DATA","INPUT#","INPUT","DIM","READ","LET","GOTO","RUN","IF","RESTORE","GOSUB","RETURN","REM","STOP","ON","WAIT","LOAD","SAVE","VERIFY","DEF","POKE",
						  "PRINT#","PRINT","CONT","LIST","CLR","CMD","SYS","OPEN","CLOSE","GET","NEW",
						  "TAB(","TO","FN","SPC(","THEN","NOT","STEP", "+","-","*","/","^", "AND","OR", ">","=","<",
						  "SGN","INT","ABS","USR","FRE","POS","SQR","RND","LOG","EXP","COS","SIN","TAN","ATN","PEEK","LEN","STR$","VAL","ASC","CHR$","LEFT$","RIGHT$", "MID$","GO",
#ifdef SIMONS
						  "CIRCLE","HIRES","PLOT","LINE",
#endif
						  "pi"
						  };
const size_t Nkeywords = sizeof(keywords)/sizeof(char*);

//global copies of the scanner properties
keyw_t keyword = NOKEYW;
sym_t sym = NONE;
oper_t oper = NOPER;
double value = 0.0;
std::string identifier;

std::string symbol;  //debugging

Scanner mainScanner;
Scanner *scan = &mainScanner;

static sym_t nextsym(const std::string& ln, bool restart/* = false*/)
{ if(restart) scan->start(ln);
  scan->nextsym(/*ln*/);
  sym = scan->Sym();
  keyword = scan->Keyword();
  oper = scan->Operator();
  identifier = scan->Identifier();
  symbol = scan->Symbol();
  value = scan->Value();
  return sym;
}

sym_t nextsym() { return nextsym("", false);}
sym_t nextsym(const std::string& ln) { return nextsym(ln, true);}

//used for user defined functions (FN)
Scanner* saveAndStart(const std::string& line)
{ Scanner *prev = scan; //save the current scanner
  scan = new Scanner(line); //create a new one
  scan->nextsym(/*line*/); //and start it
  sym = scan->Sym();
  keyword = scan->Keyword();
  oper = scan->Operator();
  identifier = scan->Identifier();
  symbol = scan->Symbol();
  value = scan->Value();
  return prev;  //return the old one
}

void restore(Scanner *prev)
{ if(scan==&mainScanner) throw "Attempt to delete main Scanner";
  delete scan; //delete the current scanner
  scan = prev; //and restore the previous one
  sym = prev->Sym();   //restore all global scanner vars
  keyword = prev->Keyword();
  oper = prev->Operator();
  identifier = prev->Identifier();
  symbol = prev->Symbol();
  value = prev->Value();
}

//commands END_NEW TAB( TO FN SPC( THEN NOT STEP operators +_< functions SGN_MID$ GO pi
sym_t Scanner::nextsym(/*const std::string& ln*/)
{ identifier.clear();
  //s = ln.c_str();
  while(s[i])
  {	if(s[i] & 0x80) //for a universal scanner I should also check for tokens , quoted strings are read as one symbol, remarks are ignored by the parser
	{ keyw_t t = (keyw_t)s[i];  //it is a token or a non-ascii character
	  switch(t)
	  { case PLUSt:  sym = OPERATOR; oper = PLUS; break;
		case MINUSt: sym = OPERATOR; oper = MINUS; break;
		case MULTt:  sym = OPERATOR; oper = MULT; break;
		case DIVt:   sym = OPERATOR; oper = DIV; break;
		case EXPt:   sym = OPERATOR; oper = EXP; break;
		case LTt:  sym = OPERATOR;
				   if((uint8_t)s[i+1]==GTt) { i++; oper = NE; }
				   else if((uint8_t)s[i+1]==EQt)  { i++; oper = LE; }
				   else oper = LT;
				   break;
		case GTt:  sym = OPERATOR;
				   if((uint8_t)s[i+1]==EQt) { i++; oper = GE; }
				   else oper = GT;
				   break;
		case EQt:  sym = OPERATOR; oper = EQ; break;
		case PI:   sym = NUMBER; value = 3.141592654; identifier="pi";
				   break;
		case TABt:
		case SPCt: sym = FUNCTION; keyword = (keyw_t)t;
				   break;
		case NOTt: sym = OPERATOR; oper = NOT; break;
		case ANDt: sym = OPERATOR; oper = AND; break;
		case ORt:  sym = OPERATOR; oper = OR; break;
		case FN :  sym = FUNCNAME;
				   while(s[i+1]==' ' || s[i+1]=='\t') i++; //skip trailing whitespace
				   if(isalpha(s[i+1]))   //identifier is now the function name
					 do { identifier.push_back(s[++i]); } while(isalnum(s[i+1])); //looking ahead, no stepping back
				   if (functions.find(identifier) == functions.end()) info("new function name: " + identifier); //NOTE the function is not entered into the function table (done only at DEF)
				   break;
		default:   if(t <= STEP || t > MIDS) { sym = COMMAND;  keyword = (keyw_t)t;}  //above MIDSt is only GO, non-standard commands and pi, below STEP are also TAB( TO FN SPC( and NOT but they have been dealt with
				   else if(t >= SGN)         { sym = FUNCTION; keyword = (keyw_t)t;}  //below SGNt are only commands(<=STEPt) operators and SPCt/TABt, all have been dealt with already (condition is superfluous)
		//TODO: add condition for illegal tokens
				   break;
	  }
	  if(identifier.empty()) identifier = keywords[t-END]; //for debugging to see which token was matched
	} else
	switch(s[i])  //non-tokens
	{ case ' ': case '\t': i++;     //skip
				continue;
/*	  case '+': case '-': case '*': case '/': case '^': case '=':   //operators, this will not happen with tokenised input
				oper = (oper_t)s[i]; sym = OPERATOR;
				break; */
	  case ',': case ':': case ';': case '\n': case '(': case ')':  //separators
				sym = (sym_t)s[i];
				break;
	  case '#': sym = (sym_t)s[i]; break; //used only after GET
/*	  case '?': sym = COMMAND; keyword = PRINT; //tokeniser will have snatched ? < > =
				break;
	  case '<': sym = OPERATOR;
				if(s[i+1]=='>') { i++; oper = NE;}
				else if(s[i+1]=='=')  { i++; oper = LE;}
				else oper = LT;
				break;
	  case '>': sym = OPERATOR;
				if(s[i+1]=='=') { i++; oper = GE;}
				else oper = GT;
				break;*/
	  case '"': sym = STRING;     //quoted string
				i++;
				while(s[i] && s[i] != '"') identifier.push_back(s[i++]);
				if(s[i] == '\0')
				{ //throw  "Closing \" missing";
			      info( "Closing \" missing");//many examples have missing closing quotes, throwing may crash these programs
				  i--; //otherwise we will step over the terminating '\0'
				}
				break;
	  case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '.':    //numbers
				{ char *p = 0;
				  value = strtod(s+i, &p);
				  identifier = std::string(s+i, p-(s+i));
				  i = p-s-1;
				  sym = NUMBER;
				}
				break;
	  case '%': case '$': throw "?SYNTAX ERROR (type char (%$) without identifier)";
	  default:  sym = NONE;
				if(isalpha(s[i]))  //varnames, function names and keywords (after tokenisation only varnames)
				{ //expect an identifier
				  identifier.push_back(CASE(s[i++]));
				  while(isalnum(s[i]))  //[A-Z][A-Z0-9]*
					identifier.push_back(CASE(s[i++]));
				  Var::type t = Var::real; //default
				  if (s[i]=='%')   //integer
				  { identifier.push_back(s[i++]);
					sym = INTVAR;
					t = Var::integer;
				  }
				  else if (s[i]=='$')//string
				  { identifier.push_back(s[i++]);
					sym = STRVAR;
					t = Var::string;
				  }
				  else
					sym = REALVAR;
				  i--; //stay at the last stored character
				  while(s[i+1]==' ' || s[i+1]=='\t') i++; //skip trailing whitespcace
				  if(s[i+1]=='(') //look ahead, do not consume
				  { //array variable
					switch(t)
					{ case Var::string:  case Var::stringarray: sym = STRARRAY; break;
					  case Var::real:    case Var::realarray:   sym = REALARRAY; break;
					  case Var::integer: case Var::integerarray:sym = INTARRAY; break;
					}
					if (arrays.find(identifier) == arrays.end())
					{ switch(t)
					  { //case Var::string: arrays[identifier] = new StrArray(); break;
						//case Var::real:   arrays[identifier] = new RealArray(); break;
						//case Var::integer:arrays[identifier] = new IntArray(); break;
						case Var::string: arrays[identifier] = std::make_unique<StrArray>(); break;
						case Var::real:   arrays[identifier] = std::make_unique<RealArray>(); break;
						case Var::integer:arrays[identifier] = std::make_unique<IntArray>(); break;
						default: break;
					  }
					  info("new arrayname: " + identifier);
					}
				  }
				  else
				  { //regular variable
					if (vars.find(identifier) == vars.end())
					{ if(t==Var::string)
					  {	//vars[identifier] = new StrVar(identifier);
					    vars[identifier] = std::make_unique<StrVar>(identifier);
					    info("new varname: " + identifier);
					  }
					  else
					  {  if(identifier.substr(0,2)!="TI" && identifier.substr(0,2)!="ST")//do not add TIME and STATUS pseudo variables to the varlist
					     {  //vars[identifier] = new NumVar(t, identifier);
					        vars[identifier] = std::make_unique<NumVar>(t, identifier);
					        info("new varname: " + identifier);
					     }
					  }
					}
				  }//if
				}
				else
				{
#ifdef PARSERWEDGE //let the parser handle the wedge
				  //  if(s[i]!=specialChar) throw "?SYNTAX ERROR (unexpected character)"; //specialChar defined in user.cpp, optional: parser can handle it
				  identifier.push_back(s[i]); //typically '\' $5C
				  while(isalnum(s[i+1]) || (s[i+1]&0x80)) identifier.push_back(s[++i]); //typically 2 characters or token(s), typically followed by COMMA, COLON, EOLN which are non-tokens and not alnums
				  sym = COMMAND;
				  keyword = WEDGE;
#else //the lexer handles the wedge
				  unsigned n = wedge(s+i); //n is the number of characters eaten by the wedge (including the trigger char)
				  if(n>0)
				  { i += n;
                    if(i >= strlen(s)) { sym = EOLN; return EOLN;}  //wedge function misbehaves, reports more characters than on the line
					continue; //do not pass anything to the parser
                  }
				  throw "?SYNTAX ERROR (unexpected character)";
#endif
				}
				break;
	} //switch
	i++;  //step to first unused character
	char tmp[] = {(char)sym, '\0'};   //the operator symbols have non-sequential values!
	symbol = sym <= OPERATOR ? std::string(symnames[sym]) : std::string(tmp);
	//std::cerr << '[' << symbol << " _ " << identifier << ']' << std::endl;
	return sym;
  } //while
  sym = EOLN;
  symbol = "EOLN";
  return EOLN;
}
//***************************************************************************************************************************
//***************************************************************************************************************************
//***************************************************************************************************************************

