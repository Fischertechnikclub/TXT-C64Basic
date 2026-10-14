//---------------------------------------------------------------------------

#ifndef filesH
#define filesH
#include "common.h"

#include <map>
#include <string>
#include <cstdio>

#include "errno_to_1541.h"

#ifndef ERRORMSG
#define ERRORMSG(x) info(x)
#endif
void info(const std::string& s); //in main.cpp

//C64File's are regular windows files but are handled by C64 commands and may contain any data
//D64file's are files within a .d64 disk archive, only reading is supported
//device 10 should correspond to a D64disk diskA  (very partial implementation)

int getType(const std::string& fn);
void setType(const std::string& fn, uint8_t type);


class C64File
{ uint8_t device=1, chan=0, type=0x82;//default PRG file
  uint8_t drive  = 0;
  char acc[2] = "r";
  uint8_t statusPos = 0; //read of the status channel returns errnum, errstr, trk, sect in that order and restarts at every get# statement
  enum devices { keyboard, datasette, modem, screen, printer0, printer1, disk=8} kind;
  FILE* f = 0;
  std::string filename;
  int err; //last error on this file
  bool parseFilename(const std::string& nm);
  void handleDiskCmd(const std::string& cmd);
  static int8_t status; //last error on the disk
  int stat()
  { if(kind==disk)
	  switch(statusPos++)
      { //case 0: return err; //last error, not useful because the status channel is a file in itself
        case 0: return errno_to_cbm_code(status); //last error
        case 1: throw "expecting string argument for second INPUT# variable";
        case 2: case 3: return 0; //track and sector unknown
        default: throw "only 4 status parameters available";
      }
    return 0;
  }
public:
  void open(uint8_t dev, uint8_t sec, std::string& nm);
  void close()
  { err = 0;
    if(fclose(f))
	      status = err = errno;
  }
  char read()
  { err = status = 0;
    if(f==0) { status = EBADF; ERRORMSG("file not open");}
    int x = fgetc(f);
	if(x!=EOF) return x;
	status = err = EISDIR;
	return 199;   //https://www.c64-wiki.com/wiki/GET(hash)
  }
  bool istty() { return kind==keyboard || kind==screen;}
  uint8_t Channel() { return chan;}
  void Restart() { statusPos = 0;}
  void write(const char *data)
  { if(kind == disk && chan==15)
    { handleDiskCmd(data);
	  return;
    }
	status = err = 0;
	if(f==0) { status = EBADF; ERRORMSG("file not open");}
	int x = fputs(data, f);
	if(x==EOF) status = err = errno;
  }
  double readNum()
  { if(chan==15) return stat();
	double v = 0;
	status = err = 0;
	if(f==0) { status = EBADF; ERRORMSG("file not open");}
	int n = fscanf(f,"%lf", &v);
	if(n!=1) { status = err = errno; ERRORMSG("input is not numeric");}
	return v;
  }
  int readInteger()
  { if(chan==15) return stat();
	int v = 0;
	status = err = 0;
	if(f==0) { status = EBADF; ERRORMSG("file not open");}
	int n = fscanf(f,"%d", &v);
	if(n!=1) { status = err = errno; ERRORMSG("input is not numeric");}
	return v;
  }
  void readStr(char *str, int len)
  { if(chan==15)
    { if (statusPos==1)
      { const char *e = errno_to_1541_table[errno_to_cbm_index(status)].cbm_message;
        strncpy(str, e, len);
        statusPos++;
      }
      else throw "only second INPUT# variable can be a string argument";
      return;
    }
	char buf[81];  int k = 0;
	status = err = 0;
	if(f==0) { status = EBADF; ERRORMSG("file not open");}
	int n = fscanf(f, "\"%80[^\"]%n\"", str, &k); //try quoted string first
	char c = '\0';
	if(n < 2)
	{ n=fscanf(f, "%80[^,;:\r]%c", str, &c); //then unquoted string
	  if(n==EOF) status = err = errno;
	  else if(n==0 /* || feof(f) || strchr(",;:\r",c)==0*/) { status = err = EILSEQ; ERRORMSG("unexpected end of string");}
	}
	else if(str[k]!='"') ERRORMSG("unterminated string");
	strncpy(str, buf, len);
  }
  static int8_t getStatus() { return status ? 64 : 0;}
};

extern std::map<uint8_t,C64File> files;   //list of open files

//---------------------------------------------------------------------------
#endif
