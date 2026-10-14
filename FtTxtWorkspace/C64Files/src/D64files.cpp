//---------------------------------------------------------------------------


#include "D64files.h"
#include "lexer.h"
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

D64disk  diskA;  //only one disk drive for now (device 10)

D64file* D64disk::open(std::string name, uint8_t t)
{     if(!diskA.isConnected())
		 if (!diskA.connect(name.c_str())) return 0;
	  DirIterator it(diskA);
	  while(it.nextDir())
	  { if( (*it).type==t && (*it).Name()==name )
		{ D64file *f = t==0x82 ? new D64PRGfile(it) : new D64file(it);
		  f->open();
		  return f;
		}
	  }
	  return 0;
}

void D64disk::close(D64file*f)
{ delete f;
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
extern const char*tokens[];  //see below, should be changed to the tokens from the lexer

std::string deTokenize(const std::string& src)
{   std::string result;
	bool quoted = false, comment = false, lower = true;
	for (int i = 0; src[i]; i++)
	{   uint8_t chr = (uint8_t)src[i];
		if (chr == '"')
			quoted = !quoted;
		if (!quoted && !comment && (chr & 0x80))
		{	result += std::string(tokens[chr & 0x7F]);  //print the token
			if(chr == 0x8F) comment = true; //REM==8F
		}
		else if(chr < 0x20) //control char                                        00-1F
		{	 if (chr==0x0E) lower = true;
		}
		else if(chr < 0x60)
			result += lower ? tolower(chr) : chr;    //upper case, in C64 also shifted lower case        20-3F,40-5F
		else if((uint8_t)(chr) < 0x80)
			result += chr;    //lower case but unused in C64 (duplicate of C0-DF) 60-7F
		else if ((uint8_t)(chr) < 0xA0) //control char                                      80-9F
		{	 if (chr==0x8E) lower = false;
		}
		else if((uint8_t)(chr)  < 0xC0)
			result += ".";    //graphics                                          A0-BF
		else if((uint8_t)(chr) < 0xE0)
			result += toupper((char)(chr&0x7F));//graphics or shifted upper case  C0-DF, always shown as upper case
		else
			;                 // unused in C64, (almost) duplicate of A0-BF       E0-FF
	}
	return result;
}


int Tokeniser::gettoken(const char*& src)
{   int n = 128;//sizeof(tokens)/sizeof(char*);
	for(int i = 0; i < n; i++)
	{ int len = strlen(tokens[i]);
	  if(strnicmp(tokens[i], src, len)==0)
	  {  src += len;
		 return i | 0x80;
	  }
	}
	return -1;
}

std::string Tokeniser::tokenise(const std::string& src)
{   std::string dest;
	const char *sp = src.c_str();
	bool quoted = false, rem = false;
	while (*sp && *sp != '\r')
	{ if (collapsespaces && !(rem || quoted))
		while (*sp && isspace(*sp)) ++sp;
	  if (*sp == '"') quoted = !quoted;
	  if (!rem && !quoted)   //do not tokenize strings and remarks
	  { int token = gettoken(sp);
		if (token != -1)
		{ if (token == REM) rem = true;
			dest.push_back((unsigned char)token);
			continue;
		}
	  }
	  dest.push_back((unsigned char)*sp++);
	}
	return dest;
}

Tokeniser tokeniser;

/*
int TForm1::readD64file(D64PRGfile *f)
{ f->open();
  uint16_t link, line;
  uint8_t buf[81];
  while(true)
  { int res = f->gets(buf, sizeof(buf)-1, link, line);
	if(res<0)
	{ switch(res)
	  { case -1: Memo3->Lines->Add("end of file"); break;
		case -2: Memo3->Lines->Add("link error"); break;
		case -3: Memo3->Lines->Add("line nr error"); break;
		case -4: Memo3->Lines->Add("premature eof"); break;
		case -5: Memo3->Lines->Add("previous error"); break;
		default: Memo3->Lines->Add("unknown error"); break;
	  }
	  return res;
	}
	AnsiString str = AnsiString(line)+" ";
	str += deTokenize((char*)buf);
	Memo1->Lines->Add(str);
  }
  return 0;
}
*/

const char *tokens[128] = {
	"END",		// 80
	"FOR",		// 81
	"NEXT",		// 82
	"DATA",		// 83
	"INPUT#",		// 84
	"INPUT",		// 85
	"DIM",		// 86
	"READ",		// 87

	"LET",		// 88
	"GOTO",		// 89
	"RUN",		// 8a
	"IF",		// 8b
	"RESTORE",		// 8c
	"GOSUB",		// 8d
    "RETURN",		// 8e
    "REM",		// 8f

    "STOP",		// 90
    "ON",		// 91
    "WAIT",		// 92
    "LOAD",		// 93
    "SAVE",		// 94
    "VERIFY",		// 95
    "DEF",		// 96
    "POKE",		// 97

    "PRINT#",		// 98
    "PRINT",		// 99
    "CONT",		// 9a
    "LIST",		// 9b
    "CLR",		// 9c
    "CMD",		// 9d
    "SYS",		// 9e
    "OPEN",		// 9f

    "CLOSE",		// a0
    "GET",		// a1
    "NEW",		// a2
    "TAB(",		// a3
    "TO",		// a4
    "FN",		// a5
    "SPC(",		// a6
    "THEN",		// a7

    "NOT",		// a8
    "STEP",		// a9
    "+",		// aa
    "-",		// ab
    "*",		// ac
    "/",		// ad
    "^",		// ae
    "AND",		// af

    "OR",		// b0
    ">",		// b1
    "=",		// b2
    "<",		// b3
    "SGN",		// b4
    "INT",		// b5
    "ABS",		// b6
    "USR",		// b7

    "FRE",		// b8
    "POS",		// b9
    "SQR",		// ba
    "RND",		// bb
    "LOG",		// bc
    "EXP",		// bd
    "COS",		// be
    "SIN",		// bf

    "TAN",		// c0
    "ATN",		// c1
    "PEEK",		// c2
    "LEN",		// c3
    "STR$",		// c4
    "VAL",		// c5
    "ASC",		// c6
    "CHR$",		// c7

    "LEFT$",		// c8
    "RIGHT$",		// c9
    "MID$",		// ca
    "GO",		// cb
    "{cc}",
    "{cd}",
    "{ce}",
    "{cf}",

    "{d0}",
    "{d1}",
    "{d2}",
    "{d3}",
    "{d4}",
    "{d5}",
    "{d6}",
    "{d7}",

    "{d8}",
    "{d9}",
    "{da}",
    "{db}",
    "{dc}",
    "{dd}",
    "{de}",
    "{df}",

    "{e0}",
    "{e1}",
    "{e2}",
    "{e3}",
    "{e4}",
    "{e5}",
    "{e6}",
    "{e7}",

    "{e8}",
    "{e9}",
    "{ea}",
    "{eb}",
    "{ec}",
    "{ed}",
    "{ee}",
    "{ef}",

    "{f0}",
    "{f1}",
    "{f2}",
    "{f3}",
    "{f4}",
    "{f5}",
    "{f6}",
    "{f7}",

    "{f8}",
    "{f9}",
    "{fa}",
    "{fb}",
    "{fc}",
    "{fd}",
    "{fe}",
    "{pi}"		// ff
};
