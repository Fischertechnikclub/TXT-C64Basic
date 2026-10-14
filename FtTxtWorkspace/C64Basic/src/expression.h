//---------------------------------------------------------------------------

#ifndef expressionH
#define expressionH

#include "data.h"

double parseExpression();
std::string parseStringExpression();
double orExpr();
//int handleSubscripts(std::map<std::string, Array*>::iterator v);
int handleSubscripts(ArrayIter v);
void setTime(std::string& ts);
#define ERRORMSG(x) throw x

//---------------------------------------------------------------------------
#endif
