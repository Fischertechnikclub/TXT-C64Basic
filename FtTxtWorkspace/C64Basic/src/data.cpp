//---------------------------------------------------------------------------

#include "data.h"
//---------------------------------------------------------------------------

const std::string err(" ERROR ");
Variables vars;
Arrays arrays;


std::string numberToString(double num)  //convert float to string as integer, scientific or rounded
{ char buf[20];
  double intpart, frac = modf(num, &intpart);
  if(fabs(frac)<1e-6) //consider to be integer
	snprintf(buf, sizeof(buf), "%ld", (long)num);
  else if(fabs(num)>1e6 || fabs(num)<1e-6) //consider to be large/small float
	snprintf(buf, sizeof(buf), "%.8G", num);  //8 significant digits
  else
  { long ip = intpart;
	int i = 0;
	long fact = 1;
	while (ip) { ip/=10;  i++;}  //number of integer digits
	i = 6 - i; //max fractional digits
	while(i>0) { fact *=10; frac *=10; i--;}
	long fr = fabs(frac*10) + 0.5;
	while (fr) { int d = fr/fact; fr -= d*fact; fact/=10; i++;}  //i is actual fractional digits
	snprintf(buf, sizeof(buf), "%.*f", i, num);
  }
  return std::string(buf);
}


