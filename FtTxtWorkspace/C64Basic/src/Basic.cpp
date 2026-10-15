//============================================================================
// Name        : helloworld.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <stdio.h>
//#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
//#include <string.h>
//#include <pwd.h>
//#include <grp.h>
//#include <time.h>
#include <glob.h>
#include <unistd.h>

#include <iostream>
#include <cstring>
#include <csignal>
#include <poll.h>
#include <unistd.h>
#include <termios.h>
#include "common.h"
#include "data.h"
#include "parser.h"
#include "files.h"
#include "user.h"
#include "display.h"

#ifndef ERRORMSG
#define ERRORMSG(x) throw x
#endif

using namespace std;

class Tokeniser
{ bool collapsespaces = false;
  int gettoken(const char*& src);  //get a single token from src and advance the pointer
public:
  std::string tokenise(const std::string& line); //convert the character line into a token string
};

Program program;
prog_iter pc;
Literals literals(program);
Tokeniser tokeniser;
state_t state = idle;
uint16_t lineno = 0;
uint8_t redirect = 0;
static uint16_t breakpoint = 0;//gui/debug only
static bool single_stepping = false; //gui/debug only
static unsigned cursorPos = 0;
static bool canContinue = false;

unsigned getCursorPos() { return cursorPos;}

#ifndef PROGCLASS
static std::string progname;
static uint8_t progdev=8;
#endif

//user (external) functions PRINT
static void print_(const std::string& s) //for printing to remote ANSI terminal (putty) or file (redirect)
{ if(redirect && redirect != 3)   //3 is screen
  {  if(files.find(redirect) != files.end())
	   files[redirect].write(s.c_str());    //redirected file output
	 else ERRORMSG("?FILE NOT OPEN ERROR");
  }
  else
  {     std::string str;
	    bool lc = true;
		for (unsigned i = 0; i < s.length(); i++) //translate PETSCII control codes to ANSI escape sequences
		{  switch(s[i])
		   {
#if 0 //colors
		      case 5: str += "\x1b[37m"; poke(646, 1); break;//white
		      case 28: str += "\x1b[31m"; poke(646, 2); break;//red
		      case 30: str += "\x1b[32m"; poke(646, 5); break;//green
		      case 31: str += "\x1b[34m"; poke(646, 6); break;//blue
		      case 129: str += "\x1b[35m";poke(646, 8);  break;//orange
		      case 144: str += "\x1b[30m"; poke(646, 0); break;//black
		      case 149: str += "\x1b[31;2m"; poke(646, 9); break;//brown
		      case 150: str += "\x1b[31;1m"; poke(646, 10); break;//pink
		      case 151: str += "\x1b[37;2m"; poke(646, 11); break;//dark grey
		      case 152: str += "\x1b[37;2m"; poke(646, 12); break;//grey
		      case 153: str += "\x1b[32;1m"; poke(646, 13); break;//light green
		      case 154: str += "\x1b[34;1m"; poke(646, 14); break;//light blue
		      case 155: str += "\x1b[37;1m"; poke(646, 15); break;//light grey
		      case 156: str += "\x1b[36;2m"; poke(646, 4); break;//purple
		      case 158: str += "\x1b[33m"; poke(646, 7); break;//yellow
		      case 159: str += "\x1b[36m"; poke(646, 3); break;//cyan
#endif
#if 0 //colors
		      case 5:   str += fgndcolor[WHITE];   poke(646, 1);  break;//white
		      case 28:  str += fgndcolor[RED];     poke(646, 2);  break;//red
		      case 30:  str += fgndcolor[GREEN];   poke(646, 5);  break;//green
		      case 31:  str += fgndcolor[BLUE];    poke(646, 6);  break;//blue
		      case 129: str += fgndcolor[ORANGE];  poke(646, 8);  break;//orange
		      case 144: str += fgndcolor[BLACK];   poke(646, 0);  break;//black
		      case 149: str += fgndcolor[BROWN];   poke(646, 9);  break;//brown
		      case 150: str += fgndcolor[PINK];    poke(646, 10); break;//pink
		      case 151: str += fgndcolor[dGREY];   poke(646, 11); break;//dark grey
		      case 152: str += fgndcolor[mGREY];   poke(646, 12); break;//grey
		      case 153: str += fgndcolor[ltGREEN]; poke(646, 13); break;//light green
		      case 154: str += fgndcolor[ltBLUE];  poke(646, 14); break;//light blue
		      case 155: str += fgndcolor[ltGREY];  poke(646, 15); break;//light grey
		      case 156: str += fgndcolor[PURPLE];  poke(646, 4);  break;//purple
		      case 158: str += fgndcolor[YELLOW];  poke(646, 7);  break;//yellow
		      case 159: str += fgndcolor[CYAN];    poke(646, 3);  break;//cyan
#endif
#if 1 //colors
		      case 5:   poke(646, 1);  break;//white
		      case 28:  poke(646, 2);  break;//red
		      case 30:  poke(646, 5);  break;//green
		      case 31:  poke(646, 6);  break;//blue
		      case 129: poke(646, 8);  break;//orange
		      case 144: poke(646, 0);  break;//black
		      case 149: poke(646, 9);  break;//brown
		      case 150: poke(646, 10); break;//pink
		      case 151: poke(646, 11); break;//dark grey
		      case 152: poke(646, 12); break;//grey
		      case 153: poke(646, 13); break;//light green
		      case 154: poke(646, 14); break;//light blue
		      case 155: poke(646, 15); break;//light grey
		      case 156: poke(646, 4);  break;//purple
		      case 158: poke(646, 7);  break;//yellow
		      case 159: poke(646, 3);  break;//cyan
#endif
#if 1 //cursor
		      case 17: str += "\x1b[B"; break; //down
		      case 19: str += "\x1b[H"; cursorPos = 0; break;//home
		      case 29: str += "\x1b[C"; cursorPos=(cursorPos+1)%40; break; //right, wrap at 40 is questionable
		      case 145: str += "\x1b[A"; break; //up
		      case 147: str += "\x1b[2J"; cursorPos = 0; break;//clear
		      case 157: str += "\x1b[D"; if(cursorPos) cursorPos--; break; //left
#endif
#if 1 //charset
		      case 14: lc = true; break;//lo/up
		      case 18: str += "\x1b[7m"; break;//rev on
		      case 142: lc = false; break;//up/gfx
		      case 146: str += "\x1b[27m"; break;//rev off
		      case 0xde: str += '\xe3'; cursorPos++; break; //pi alt-227 on codepage 437, not tested, 0xDE seem to be the correct code,  effect on the terminal unknown (may need specific font or codepage)
#endif
		      default: //if((s[i]&0x7F) < 0x20) { str.push_back('{'); str+=std::to_string((int)s[i]); str.push_back('}'); break;}
		    	       if(lc) //lo/up
		    	       { if(s[i]<128) str.push_back(tolower(s[i]));
		    	         else str.push_back(toupper(s[i]&0x7F));
		    	       }
		               else //up/gfx
		               { if (s[i]<128) str.push_back(s[i]);
		                 else str.push_back('.');
		               }
		    	       cursorPos++;
		               break;
		   }
		}
        cout << str;
  }
}

void print(const std::string& s, bool nl)
{ print_(s);
  if(nl) { cout << "\x1b[27m" << endl; cursorPos = 0;} //reset possible 'reverse' on every newline
  if(TextDisplay::Instance()) TextDisplay::Instance()->print(s, nl);
  else if(Display::Instance()==nullptr)
  { TextDisplay* display = new TextDisplay;
    display->setFG((color_t)peek(CURSORCOL));
    display->print(s, nl);
  }
  else cerr << "No TextDisplay" << endl;
}

void info(const std::string& s) //diagnostics output
{ cerr << s << endl;;
}

std::string deTokenize(const std::string& src);

void list(uint16_t num, const std::string& line)  //writes list output to screen
{ std::string ln;
  try
  { ln = std::to_string(num) + " " + deTokenize(line);
    if(redirect && redirect != 3)
    { ln += "\r\n";
	  if(files.find(redirect) != files.end())
	    files[redirect].write(ln.c_str());    //redirected file output
	  else ERRORMSG("?FILE NOT OPEN ERROR");
    }
    else
	  print(ln);
  }
  catch(...)
  {
	  throw "error listing line " + std::to_string(num) + ": " + ln;
  }
}

//function called from 'startProg' (below) or 'resume' (main.cpp)
void runLoop()   //program execution loop, for now executes from program(Program), will be changed to progje(Program2)
{ static bool skip = false;
  state = running;
  canContinue = true;
  //info("cont@"+ std::to_string(pc->first) + " sym=" + symbol + " command=" + std::string(keywords[keyword & 0x7F]));
  do
  { lineno = pc->first;
	if ((single_stepping || lineno==breakpoint) && !skip) //for debugging only
	{ skip = true;
	  state = stopped;
	  break;   //or place a real breakpoint here
	}
	else if(skip) //resume from breakpoint
	{ skip = false;
	}
	try
	{ int dest = parseLine();  //it is not always possible to process the entire line because it may contain stop/end/input/get that need additional processing
	  if (dest > 0)  //a jump
	  { auto jmp = program.find(dest);
		if (jmp != program.end())
		  pc = jmp;     //these jumps are always to the beginning of the line, jumps like RETURN/NEXT/FN are handled in the parser
		else throw("?UNDEF'D STATEMENT ERROR IN line " + std::to_string(dest));
	  }
	  else if(dest==0) //next line
	  { pc++;
	    lineno = pc->first;
	  }
	  else  if(dest==-1)//line wasn't finished, do not bump pc and do not get a new line (i.e. preserve scanner state
		  break;  //exit the loop because suspended
	  else if(dest==-2)  //a command (e.g. run 1000) returned
		  break;   //not sure this is useful, needs testing
	} //try
	catch(const char *msg)
	{ info(std::string(msg) + " in line " + std::to_string(lineno));
	  pc++; //skip rest of problematic line
	} //catch
	if(pc==program.end())
	{  state = ended;
	   break;
	}
	nextsym(pc->second); //called if and only if the pc was changed in this function
  } while(state==running);
  //leave the loop on: dest<0 (END, STOP, INPUT, GET in middle of line),  state = ended, stopped or waiting
  //                   state!=running (stopped(brkpnt, ss, STOP,END,INPUT,GET) at end of line),   state = ended, stopped or waiting
  //                   pc==end() (beyond last line)    state = ended
}

void startProg(unsigned ln)
{  if(ln != 0)
   { pc = program.find(ln);
	 if(pc==program.end()) { print("?UNDEF'D STATEMENT ERROR IN line " + std::to_string(lineno)); return;}
   }
   else
   { pc = program.begin();
     if(pc==program.end()) { info("program is not loaded"); return;}
   }
   auto line = pc->second;
   nextsym(line);   //read the first statement, start the scanner
   runLoop();
}

void Run(uint16_t lineno)
{   Clr();
    if(Turtle::Instance())
    {  delete Turtle::Instance();
       TextDisplay *txt = new TextDisplay;
       txt->clear();
    }
	startProg(lineno);
}

void Stop()  //continue with Cont() or a GOTO
{ if (state==running || state==waiting)  //waiting state only for GUI, not used in console
	state = stopped;
}

void Cont()
{ if (canContinue)
  { auto line = pc->second;
	nextsym(line);   //read the first statement, start the scanner
	runLoop();
  }
  else print("?CAN'T CONTINUE ERROR");
}

void List(uint16_t from, uint16_t until)   //requires special parsing <number> '-' <number>
{ prog_iter f = program.begin(), u = program.end();
  if(until && from > until) return;
  if(from) f = program.lower_bound(from);
  //if(f==program.end()) { print("?UNDEF'D STATEMENT ERROR IN line " + std::to_string(from)); return;}
  if(until) {u = program.lower_bound(until);
  if (u!=program.end()) u++;} //include the 'until' line
  for(auto& it = f; it != u; it++)
	 list(it->first, it->second);   //list Program
  if(state==running) state = ended; //behaviour of C64 basic
}

void Clr()
{ for(auto& v: vars) delete v.second;
  for(auto& v: arrays) delete v.second;
  vars.clear();
  arrays.clear();
  ::literals.reset();
  callStack.clear();
  loopStack.clear();
  functions.clear();
  //state = idle;         //CLR can appear in programs which just continue
  for(auto& f: files) f.second.close();
}

void Cmd(uint8_t file)  //implies a PRINT statement for additional parameters
{ if(files.find(value)==files.end())
  { ERRORMSG("?FILE NOT OPEN ERROR");
	return;
  }
  redirect = file;
}

void New()   //clears program and data
{ Clr();
  program.clear();
  state = idle;
  canContinue = false;
}

#if 0
static void list_directory(const char *path)//just lists to screen, should actually load to RAM after which LIST can list it
{   DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[1024];
    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }
    while ((entry = readdir(dir)) != NULL)
    {   if (entry->d_name[0] == '.') continue; // Skip hidden files
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &file_stat) == -1)
        {  perror("stat");
           continue;
        }
#if 0
        char *p = strstr(entry->d_name, ".SEQ");
        if(p==0) p = strstr(entry->d_name, ".PRG");
        if(p==0) p = strstr(entry->d_name, ".USR");
        if(p==0) p = strstr(entry->d_name, ".REL");
        if(p)
        { *p++ = '\0';
          printf("%8lld %16s %3s\n", ((long long)file_stat.st_size+127)/128, entry->d_name, p);
        }
#else
        uint8_t value = getType(entry->d_name);;
        if(0x81 <= value && value <= 0x84)
        { const char *ext[] = {"SEQ", "PRG", "USR", "REL"};
          //printf("%-8lld %-16s %3s\n", ((long long)file_stat.st_size+127)/128, entry->d_name, ext[value-0x81]);
          char buffer[40];
          snprintf(buffer, sizeof(buffer), "%-8lld \"%.20s\" %*s\n", ((long long)file_stat.st_size+255)/256, entry->d_name, 20-strlen(entry->d_name), ext[value-0x81]);
          print(buffer);
        }
#endif
    }
    closedir(dir);
}
#endif

static int fgetrec(uint8_t *buf, int buflen, FILE* f)//read a 'line' from a PRG file, return the line in buf incl. link/lineno, return the number of bytes read incl. '\0'
{ if(buflen < 5) return -1;
  int n = fread(buf, 1, 4, f); //read //link and linenr
  if(n==4)
  {  do  //read the null terminated string
	 { int c = fgetc(f);
	   if(c>0) buf[n++] = c;
	   else break;
	 } while(n<buflen);  //on exit: n==buflen || c==0 || EOF(c<0)
	 if(n<buflen) buf[n++] = 0;
  }
  return n;
}

static void writeTrailer(FILE *f, uint16_t free = 0)// write the free blocks trailer to directory file f
{ struct rec
	{ uint16_t head, size;
	  const char msg[16];
	} trailer = {0x0101, free, " BLOCKS FREE.\0\0"};
    fwrite(&trailer, 1, sizeof(trailer), f);
}

void list_dir(const char *fn) //read directory file fn and list all records to the print device
{ FILE* f = fopen(fn, "rb");
  if(f==0) throw"directory file does not exist";
  uint16_t la;
#pragma pack(push, 1)
          struct rec
          { uint16_t head[2];
            char str[28];
          } buffer;
#pragma pack (pop)
  fread(&la, 2, 1, f);
  while(fgetrec((uint8_t*)&buffer, sizeof(buffer), f)>0)
  { if( buffer.head[0]==0) break; //end of basic program, in the last basic line the link pointer points just beyond the final NULL, i.e. the next link, this final link is zero and the record has no line number or data anymore
    char buf[40];
    snprintf(buf, sizeof(buf) ,"%5u%s", buffer.head[1], buffer.str);//human readable size
    print(buf);
  }
  fclose(f);
}

static void write_entries(const char *dn, FILE *f) //write the C64 files in directory dn to file f
{   DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[1024];
    dir = opendir(dn);
    if (dir == NULL) {
       perror("opendir");
       return;
    }
    while ((entry = readdir(dir)) != NULL)
    {   if (entry->d_name[0] == '.') continue; // Skip hidden files
        snprintf(full_path, sizeof(full_path), "%s/%s", dn, entry->d_name);
        if (stat(full_path, &file_stat) == -1)
        {  perror("stat");
           continue;
        }
        uint8_t value = getType(entry->d_name);;
        if(0x81 <= value && value <= 0x84) //also skip the non-C64 files
        { const char *ext[] = {"DEL", "SEQ", "PRG", "USR", "REL"};
#pragma pack(push, 1)
          struct rec
          { uint16_t head[2];
            char str[28];
          } buffer = { {0x0101, (uint16_t)((file_stat.st_size+253)/254)}, {' '} };
#pragma pack (pop)
          snprintf(buffer.str, 28, "  \"%.16s\" %*s   ", entry->d_name, 16+3-strlen(entry->d_name), ext[value&7]);
          fwrite(&buffer, sizeof(buffer), 1, f); //32 bytes per dir entry
        }
    }
}

//if the directory file dfn exists and is not older than directory dn, list the file (if list is true)
//otherwise create or update the file with the contents of directory dn
void list_directory(const char *dfn, const char *dn, bool list)
{   char hdr[40] = "\x01\x04\x01\x01\x00\x00\x12"; //load addr = 0x0401, link =0x0101 (unused), lineno =0 (drive nr), reverse on
    char path[40];
    if(strlen(dfn)+strlen(dn)+2 > sizeof(path)) throw("path too long");
    strcpy(path, dn);
    strcat(path, "/");
    strcat(path, dfn);
    char full[PATH_MAX];
    struct stat df_stat;
    int n = 0;
    if(stat(dfn, &df_stat))
    { if(errno!=ENOENT)	{ perror("stat dfn"); return;}
      realpath(dn, full); //file dfn does not exist, it must be created and will use the real name of dn as the 'disk' name
      char *p = strrchr(full, '/'); if(p==0) throw "path has no root";
      n = snprintf(hdr+7, sizeof(hdr)-7, "\"%.16s\" %2s 2A", p+1, "FT");
      n +=  1 + 4 + 1; // +1 for '\0', +4 for link/line, +1 for REV
    }
    else //the file does exist, check if it is up to date
    { time_t dftime = df_stat.st_mtime;
      if(stat(dn, &df_stat)) throw "couldn't' stat directory";
      time_t dtime = df_stat.st_mtime;
      if(dtime > dftime) //directory was modified after the file was last modified
      { FILE *f = fopen(path, "rb");
        if(f==0) throw "dirfile MUST exist";
        fread(hdr, 2, 1, f); //load address
        n = fgetrec((uint8_t*)hdr+2, sizeof(hdr)-2, f); //get the 'disk name' from the file
        fclose(f);
      }
      else //the file is up to date, list it and return
      { if(list)list_dir(path);
        return;
      }
    }
    FILE *f = fopen(path, "wb"); //the file did not exist or was not up to date, rewrite it
    if(f==0) { perror("open write"); return;}
    setType(path, 0x82);
    fwrite(hdr, n+2, 1, f); //+2 for the load address
    write_entries(dn, f);
    writeTrailer(f);
    fclose(f);
    if(list)list_dir(path);
}

void initDir(const std::string& name, const std::string& id, const char* full_path)
{   FILE *f = fopen(full_path, "wb");//creating a file that looks like a "$" file
    setType(full_path, 0x82);
	char buffer[40] = "\x01\x04\x01\x01\x00\x00\x12";//load address $0401, link = 0x0101 (unused), linenr = 0 (drive 0), reverse on
	fwrite(buffer, 7, 1, f);
	int len = strlen(name.c_str());
	if(len>16) len = 16;
	snprintf(buffer, sizeof(buffer), "\"%.16s\" %*s 2A", name.c_str(), 16+2-len, id.c_str());
	fprintf(stderr, "initiated new .dir %s\n", buffer);
	fwrite(buffer, 25, 1, f);
	writeTrailer(f);
	fclose(f);
}
/*
//'path' is the path to the directory to list, typically ".", 'name' is the name of the file where the list is stored (typ. "/.dir"), when 'list' is true the list will also be displayed
void directory_file(const char *path, const char *name, bool list)//just lists to screen, should actually load to RAM after which LIST can list it
{   DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[1024];
    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }
    stat(path, &file_stat);
    time_t dtime = file_stat.st_mtime;
    strncpy(full_path, path, 1000);
    strcat(full_path, name);
    stat(full_path, &file_stat);
    time_t ftime = file_stat.st_mtime;
    if(ftime >= dtime) //the dir file is uptodate
    { if(list) list_directory(full_path);
    	return;
    }
    if (truncate(full_path, 33) < 0)//keep the disk name but delete all file entries
    { if(errno!=ENOENT) { perror("truncate"); return;}
      char full[PATH_MAX];
      realpath(path, full);
      char *p = strrchr(full, '/'); if(p==0) throw "path has no root";
      initDir(p+1, "FT"); //use the directory name as disk name on new .dir files
    }
    FILE *f = fopen(full_path, "ab");//append all filenames in the directory to a file that looks like a "$" file
    setType(full_path, 0x82);
    fputs("\x01\x04", f); //load address $0401
    if(f==0) return;
    while ((entry = readdir(dir)) != NULL)
    {   if (entry->d_name[0] == '.') continue; // Skip hidden files
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &file_stat) == -1)
        {  perror("stat");
           continue;
        }
        uint8_t value = getType(entry->d_name);;
        if(0x81 <= value && value <= 0x84) //also skip the non-C64 files
        { const char *ext[] = {"SEQ", "PRG", "USR", "REL"};
#pragma pack(push, 1)
          struct rec
          { uint16_t head[2];
            char str[28];
          } buffer = { {0x0101, (uint16_t)((file_stat.st_size+2+253)/254)}, {' '} };
#pragma pack (pop)
          snprintf(buffer.str, 28, "  \"%.16s\" %*s   ", entry->d_name, 16+3-strlen(entry->d_name), ext[value-0x81]);
          fwrite(&buffer, sizeof(buffer), 1, f); //32 bytes per dir entry
          if(list)
          { char buf[40];
            snprintf(buf, sizeof(buf) ,"%5u%s", buffer.head[1], buffer.str);//human readable size
            print(buf);
          }
        }
    }
	writeTrailer(f);//the free blocks record
    fclose(f);
    closedir(dir);
}
*/
/*
static void list_directory(const char *path)//just lists to screen, should actually load to RAM after which LIST can list it
{ directory_file(".", "/.dir", true);

}
*/

#if 0
static int getType(std::string& fn)
{ size_t p, q = fn.find_last_of('.');
  int type = 0;
  if((p=fn.find(".PRG"))!=std::string::npos && p>=q) type = 0x82;
  else if((p=fn.find(".SEQ"))!=std::string::npos && p>=q) type = 0x81;
  else if((p=fn.find(".USR"))!=std::string::npos && p>=q) type = 0x83;
  else if((p=fn.find(".REL"))!=std::string::npos && p>=q) type = 0x84;
  else //no type defining extension in name
  { if(access(fn.c_str(), F_OK)==0) return 1; //file exists, type unknown
    else if(access((fn+".PRG").c_str(), F_OK)==0) { fn += ".PRG"; return 0x82;}  //file + extension exists
    else if(access((fn+".SEQ").c_str(), F_OK)==0) { fn += ".SEQ"; return 0x81;}
    else if(access((fn+".USR").c_str(), F_OK)==0) { fn += ".USR"; return 0x83;}
    else if(access((fn+".REL").c_str(), F_OK)==0) { fn += ".REL"; return 0x84;}
  }
  if(access(fn.c_str(), F_OK)==0) return type; //file as specified exists and has type ...
  return 0; //file does not exist
}
#endif

int globber(std::string& fn)//process wildcards
{   glob_t results;
    int n = 0;
    int ret = glob(fn.c_str(), GLOB_TILDE, NULL, &results);//first try files as specified
    //std::string temp = fn + ".*"; ret = glob(temp.c_str(), GLOB_TILDE|GLOB_APPEND, NULL, &results);//the try the spec with type extension
    if (ret == 0)
    { n = results.gl_pathc;
      if(n>=1)
      { fn = results.gl_pathv[0]; //return the first result if any
        if (n > 1) for(int i = 0; i < n; i++) printf("%d: %s\n", i, results.gl_pathv[i]); //print all results when more than 1
      }
    }
    else if (ret == GLOB_NOMATCH) n = 0;
    else perror("glob");
    globfree(&results);
    return n;
}

bool Program::Load(bool bas, bool abs)
{ status = 0;
  if(bas) //ascii basic program, for now load into program (Program)
  { FILE *f = fopen(progName.c_str(), "rt");
	if(f)
	{ char buf[256];
	  while(fgets(buf, sizeof(buf), f))
	  { unsigned ln, pos;
		if(sscanf(buf, "%u%n", &ln, &pos)<1)
		{ info("no linenumber found in input file " + progName);
		  if(buf[0]==1 && buf[1]==8) info("probably .PRG file,try: LOAD \"" + progName + "\",8");
		  break;
		}
		(*this)[ln] = tokeniser.tokenise(std::string(buf+pos));
	  }
	  fclose(f);
      startAddress = 0x0801;
	}
	else
		status = errno;
  }
  else  //tokenized .PRG file
  { FILE *f = fopen(progName.c_str(), "rb");
	if(f)
	{ uint8_t buf[256];
	  fread(&startAddress, 2, 1, f);
	  if(startAddress == 0x801 || !abs)//regular basic program
	  { clear(); //delete the old program
	    info("Loading " + progName + " to BASIC area $0801");
		while(fgetrec(buf, sizeof(buf), f)>0)
		{ uint16_t ln = *(int16_t*)(buf+2);
		  uint16_t link = *(int16_t*)buf;  //links discarded
		  if(link==0) break; //end of basic program, in the last basic line the link pointer points just beyond the final NULL, i.e. the next link, this final link is zero and the record has no line number or data anymore
		  (*this)[ln] = std::string((const char*)(buf+4)); //uses regular insert functions, rebuilding the list
		}
	  }
	  else// machine programs
	  { uint16_t addr = startAddress, end = 0;
		uint8_t *base = 0;
		if(addr>=HIGHRAMSTART && addr < HIGHRAMSTART+HIGHRAMLEN)//highram ,START=0xC000, LEN=0x1000
		{ addr -= HIGHRAMSTART;
		  base = highRam;
		  end = HIGHRAMLEN;
		}
		else if(addr >= CARTRAMSTART && addr < CARTRAMSTART+CARTRAMLEN) //cartridge
		{ addr -= CARTRAMSTART;
		  base = cartridge;
		  end = CARTRAMLEN;
		}
		else if(addr >= LOWRAMSTART && addr < LOWRAMSTART+LOWRAMLEN) //fischer.pic, directories
		{ addr -= LOWRAMSTART;
		  base = cartridge;
		  end = LOWRAMLEN;
		}
		else
		{ info("Skip loading " + progName + " (machine language or .BAS file)");//other pictures
		  return true;
		}
        info("loading " + progName + " @" + std::to_string(startAddress));
		while(addr<end && (base[addr] = fgetc(f))!=EOF) addr++;
	  }
	  fclose(f);
	}
	else
		status = errno;
  }
  return status==0;
}

bool Load(std::string fn, uint8_t dev, uint8_t sec)  //in commandMode implies Clr()
{ if(dev<8 || dev>15) { ERRORMSG("unsupported device number"); return false;}
  if(fn[0]=='$')
  { //list_directory("."); //NOTE change of parameters
    list_directory();
	return true;
  }
  int n = globber(fn);
  if(n==0) { ERRORMSG("no match for filename " + fn); return false;}
  if(n>1) { info("loading first match " + fn); }
  int type = getType(fn);
  fprintf(stderr, "file type is %02X\n", type); //debug
  if(type==0) { print("?FILE NOT FOUND ERROR"); return false;}
  if(type==0x83 || type==0x84) { print("?.REL and .USR file not supported"); return false;}
  program.set(fn, dev);
  if(state!=running) New();
  if(dev==9) //ascii basic program, for now load into program (Program)
  { if(type==0x82) info("device 9 is for .SEQ files");
	if(!program.Load(true, false))
	{ print("?FILE NOT FOUND ERROR");
	  info(strerror(program.Status()));
	  return false;
	}
  }
  else if(dev==8) //tokenized .PRG file
  { if(type==0x81) info("device 8 is for .PRG files");
	if(!program.Load(false,sec==1))
	{ print("?FILE NOT FOUND ERROR");
	  info(strerror(program.Status()));
	  return false;
	}
  }
  else
  { print("?FILE NOT FOUND ERROR");
	info("device not supported");
	return false;
  }
  //if(program.getStartAddress()!=0x0801) return true;
  if(state==running) //do not clear variables
  { ::literals.reset(); //old data statements are not available anymore
	callStack.clear();  //returns are not possible anymore
	loopStack.clear();  //loops are gone
	functions.clear();  //functions are gone too
	for(auto& f: files) f.second.close(); //questionable, files can be inherited
	startProg();   //Chain load operation
  }
  //else List();//optional, C64 doesn't do this
  return true;
}

bool Program::Save(bool bas)  //sec is ignored (implicit 0) because no machine programs or tape , D64Disk not supported for writing , see devicemap[]
{ status = 0;
  if(bas) //ascii basic program, for now save from program (Program)
  { FILE *f = fopen(progName.c_str(), "wt");
	if(f)
	{ for(const auto& it: program)
	  { std::string line = std::to_string(it.first) + " " + deTokenize(it.second);
		fputs(line.c_str(), f);
		fputc('\n', f);
	  }
	  fclose(f);
	  setType(progName, 0x81);
	}
	else
	  status = errno;
  }
  else  //tokenized .PRG file
  { FILE *f = fopen(progName.c_str(), "wb");
	if(f)
	{ uint16_t addr = startAddress; //default load address, the actual link is not used, this has the further effect that the file will be sorted
	  fwrite(&addr, 1, 2, f);
	  for(const auto& i: program)
	  { uint16_t hdr[2] = {0, (uint16_t)i.first};
		std::string s = i.second;
		addr += 4 + s.length() + 1;
		hdr[0] = addr;
		fwrite(hdr, 1, 4, f);
		fputs(s.c_str(), f);
		fputc('\0', f);
	  }
	  addr = 0;
	  fwrite(&addr, 1, 2, f);
	  fclose(f);
	  setType(progName, 0x82);
	}
	else
	  status = errno;
  }
  return status==0;
}

bool Save(std::string fn, uint8_t dev, uint8_t sec)  //sec is ignored (implicit 0) because no machine programs or tape
{ if(dev<8 || dev>15) { ERRORMSG("unsupported device number"); return false;}
  if(dev==8 || dev==9)
  { if(!program.Save(dev==9))
    { print("?FILE NOT FOUND ERROR");
      return false;
    }
  }
  else
  { print("?FILE NOT FOUND ERROR");
	info("device not supported");
	return false;
  }
  program.set(fn, dev);
  return true;
}

void Verify(std::string fn, uint8_t dev)
{// N/I
}

// ESC CSI [0-?]*[!-/]*[@-~] often ESC CSI <decimal>OPT CHAIN ';' [!-/]*[@-~] in our case ESC CSI (<decimal> ~ | [A-D])
int getInputKey()  //GET from SSH (Putty)input (default escape mode ESC[n~ ), see also https://en.wikipedia.org/wiki/ANSI_escape_code#CSIsection
{  struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
   int ret = poll(&pfd, 1, 0);
   if(ret<=0) return 0; //no character available
   int Key = getchar();
   //printf(" '%c' 0x%02X ", Key, Key);
   if(Key != 0x1B)
   { //printf("{'%c' 0x%02X}", Key, Key);
     return Key=='\n' ? '\r' : Key; //NORMAL CHARACTER
   }
   //ESC
   //ret = poll(&pfd, 1, 100); if(ret<=0) { printf("{ESC}"); return 0x1B;} //no next character available, return the ESC, poll returns <=0 even with esc sequence like ^[[11~
   Key = getchar();
   if (Key != '[') { printf(" '%c' 0x%02X ", Key, Key); return Key;}//DEBUG: control sequence introducer (CSI), second char in escape sequence MUST be '['
   int n = 0;
   Key = getchar(); //expect digit or A-D
   //printf(" '%c' 0x%02X ", Key, Key);
   while(isdigit(Key)) //Fn or home/end
   { n = 10*n + Key-'0';
     Key = getchar(); //expect digit or ~
   }
   if(Key == '~') //translate ANSI escape sequences to PETSCII control codes
   {   //printf("{ESC[%d~}", n);
	   switch(n)
	   { case 1     : return 19;  //home
	//	 case 4     : return ??;  //end
		 case 3     : return 20;  //delete
		 case 2     : return 148; //insert
		 case 11    : return 133; //F1
		 case 12    : return 137; //F2
		 case 13    : return 134; //F3
		 case 14    : return 138; //F4
		 case 15    : return 135; //F5
		 case 17    : return 139; //F6
		 case 18    : return 136; //F7
		 case 19    : return 140; //F8
		 default    : return 0;   //unknown
	   }
   }
   else //A-D => cursor keys
   {   //printf("{ESC[%c}", Key);
	   switch(Key)
	   {   case 'B'  : return 17;  //down
		   case 'C'  : return 29;  //right
		   case 'A'  : return 145; //up
		   case 'D'  : return 157; //left
		   default   : return 0;   //unknown
	   }
   }
}

extern void stopOutputs();

void pauseContinue(uint8_t outputs) //looks for ESC (as C= (Commodore key)) to pause the turtle, any characacter will continue
{ 	struct termios old, raw;
    tcgetattr(STDIN_FILENO, &old);
    raw = old;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    int ret = poll(&pfd, 1, 0);
    if(ret<=0) return; //no character available
    int Key = getchar();
    if(Key == 0x1B)  //ESC
    { stopOutputs();
      getchar(); //wait for any key to continue
      //restart motors, will be done on resumption
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
}


void processInputKey() //assigns the input key(s) to the  variables in varList
{ 	struct termios old, raw;
    fflush(stdout);
    print("", false); //flush
    tcgetattr(STDIN_FILENO, &old);
    raw = old;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	for(unsigned idx = 0; idx < varList.size(); idx++)
	{ VarRef ref = varList[idx];
	  std::string str = "";
	  int inputKey = getInputKey();
	  if(inputKey) { str.push_back((char)inputKey); /*fprintf(stderr, " {%c %d} ", (char)inputKey, inputKey);*/}
	  double val = 0;
	  if(!(ref.v->Type()==Var::type::string || ref.v->Type()==Var::type::stringarray))//numeric type
	  { std::string num;
		if(isdigit(inputKey) || inputKey=='.' || inputKey=='+' || inputKey=='-' )
		{ do
		  { num.push_back(inputKey);
		    inputKey = getInputKey();
		  } while(isdigit(inputKey) || inputKey=='.');
		  val = std::stod(num);
		}
		else ERRORMSG("?SYNTAX ERROR");//this will signal an error on every GET <numvar> when there is no input (yet)
	  }
	  switch(ref.v->Type())
	  { case Var::type::integer:
		case Var::type::real:        ref.v->set(val);        break;
		case Var::type::string:      ref.v->set(str);        break;
		case Var::type::integerarray:
		case Var::type::realarray:   ref.v->set(val, ref.i); break;
		case Var::type::stringarray: ref.v->set(str, ref.i); break;
	  }
	}
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
}

bool processInput()  //parses the input string s according to 'varList' and assigns the variables in that order
{ fflush(stdout);
  std::string s;
  InputScanner sc(s);
  try
  {	for(unsigned idx = 0; idx < varList.size(); idx++)
	{ VarRef ref = varList[idx];
	  if(sc.atEnd())
	  { print(" ? ", false);
	    print("", false); //flush
	    getline(std::cin, s);
	    sc.set(s, 0);
	  }
	  switch(ref.v->Type())
	  { case Var::type::integer:      //ref.v->set(sc.getShort());  break;  //will generate error when reading float value
		case Var::type::real:         ref.v->set(sc.getReal());          break;
		case Var::type::string:       ref.v->set(sc.getString());        break;
		case Var::type::integerarray: //ref.v->set(sc.getShort(), ref.i);  break;
		case Var::type::realarray:    ref.v->set(sc.getReal(), ref.i);   break;
		case Var::type::stringarray:  ref.v->set(sc.getString(), ref.i); break;
	  }
	}
  }
  catch(...)   //type mismatch/conversion error
  {  return false;
  }
  if (!sc.atEnd()) print("?EXTRA IGNORED");
  return true;
}

//---------------------------------------------------------------------------


int Tokeniser::gettoken(const char*& src)
{   int n = Nkeywords;
    if(*src=='?') { src++; return PRINT;}
	for(int i = 0; i < n; i++)
	{ int len = strlen(keywords[i]);
	  if(strnicmp(keywords[i], src, len)==0)
	  {  src += len;
	     if(i==n-1) return PI;
		 return i | 0x80;
	  }
	}
	return NOKEYW;
}

std::string Tokeniser::tokenise(const std::string& src)
{   std::string dest;
	const char *sp = src.c_str();
	bool quoted = false, rem = false;
	while (*sp && *sp != '\r' && *sp != '\n')
	{ if (collapsespaces && !(rem || quoted))
		while (*sp && isspace(*sp)) ++sp;
	  if (*sp == '"') quoted = !quoted;
	  if (!rem && !quoted)   //do not tokenize strings and remarks
	  { int token = gettoken(sp);
	    //cerr << "token is " << token << endl;
		if (token != NOKEYW)
		{ if (token == REM) rem = true;
			dest.push_back((unsigned char)token);
			continue;
		}
	  }
	  dest.push_back((unsigned char)*sp++);
	}
	return dest;
}

std::string deTokenize(const std::string& src)
{   std::string result;
	bool quoted = false, comment = false, lower = true;
	for (int i = 0; src[i]; i++)
	{   uint8_t chr = (uint8_t)src[i];
		if (chr == '"')
			quoted = !quoted;
		if (!quoted && !comment && (chr & 0x80))
		{	if((chr&0x7F) >= Nkeywords) result += " ### UNKNOWN TOKEN ### ";
			result += std::string(keywords[chr & 0x7F]);  //print the token
			if(chr == REM) comment = true; //REM==8F
		}
		else if(chr < 0x20) //control char                                        00-1F
		{	 if (chr==0x0E) lower = true;
		}
		else if(chr < 0x60)
			result += lower ? tolower(chr) : chr;    //upper case, in C64 also shifted lower case        20-3F,40-5F
		else if((uint8_t)(chr) < 0x80)
			result += chr;    //lower case but unused in C64 (duplicate of C0-DF)           60-7F
		else if ((uint8_t)(chr) < 0xA0) //control char                                      80-9F
		{	 if (chr==0x8E) lower = false;
		}
		else if((uint8_t)(chr)  < 0xC0)
			result += ".";    //graphics                                          A0-BF
		else if((uint8_t)(chr) < 0xE0)
			result += toupper((char)(chr&0x7F));//graphics or shifted upper case  C0-DF, always shown as upper case
		else if((uint8_t)(chr)  == 0xFF)
			result += '\xde';    //greek PI                                          FF
		else
			result += '@';                 // unused in C64, (almost) duplicate of A0-BF       E0-FE
	}
	return result;
}

void handleCommand(std::string cmd)  //command line
{ size_t pos = 0;
  std::string command;
  state = cmdmode;
  while(isspace(cmd[pos]))pos++; //skipspace
  if(isdigit(cmd[pos])) //program line
  { uint16_t lineno = std::stoul(cmd, &pos); //get the line number
    while(isspace(cmd[pos]))pos++; //skipspace
    if(cmd.substr(pos).empty())
    	program.erase(lineno);
    else
    { command = tokeniser.tokenise(cmd.substr(pos)); //tokenise the rest of the line and insert into program
      program[lineno] = command;
    }
    ::literals.reset(); //adding or removing lines invalidates the DATA cursor
    canContinue = false;
  }
  else//command line, pos is at first non-digit, non-space character
  { command = tokeniser.tokenise(cmd.substr(pos));
    lineno = 0;  //defined number for jump/return statements GOSUB/RETURN, FOR/NEXT
  	nextsym(command);
  	parseLine(); //handle the command/assignment
    if(state==stopped) print("BREAK IN " + std::to_string(pc->first));
    else  print("READY.");
  }
}

static bool quit = false;
static void sighandler(int sig)
{ cerr << "Caught Signal " << sig << endl;
  if (sig==SIGINT)
  { quit = state!=running;
    //print("STOP");
	Stop();
	return;
  }
  quit = true;
}

static struct termios orgTerminalSettings;
void restoreTerminalSettings()
{	tcsetattr(STDIN_FILENO, TCSANOW, &orgTerminalSettings);
}

int main() {
	bool remote = getenv("SSH_CLIENT") || getenv("SSH_TTY");
	const char* tty = ttyname(STDIN_FILENO);
	if (tty)
	{ if(strncmp(tty, "/dev/pts", 9)==0) cerr << "pseudo terminal " << tty << endl;
	  else cerr << "normal terminal " << tty << endl;
	}
	bool is_a_tty = isatty(STDIN_FILENO);
	if(is_a_tty) cerr << "stdin is a tty" << endl;
	else  cerr << "stdin is a not tty" << endl;
	if (remote)
		cerr << "SSH session " << getenv("SSH_CLIENT") << " " <<  getenv("SSH_TTY") << endl;
	else
	{ cerr << "local terminal" << endl;
	  print("NO TERMINAL CONNECTED", true);
	  sleep(5);
	  return 1;
	}
    tcgetattr(STDIN_FILENO, &orgTerminalSettings);
    struct termios settings = orgTerminalSettings;
    settings.c_lflag |= ISIG;
    tcsetattr(STDIN_FILENO, TCSANOW, &settings);
	struct sigaction action = {sighandler};
	sigaction(SIGINT, &action, 0);//^C, no response to putty special commands
	sigaction(SIGHUP, &action, 0); //no response to putty special commands
	sigaction(SIGTERM, &action, 0); //no response to putty special commands
	initRam();
	cout << "\x1b[0m";
	print("**** COMMODORE BASIC V2.0 ****", true);
	print("", true);
	print(" 64K RAM SYSTEM", true);
	print("READY.", true);
	do
	{ std::string cmd;
	  tcsetattr(STDIN_FILENO, TCSANOW, &settings);
      std::getline(std::cin, cmd);
      if(TextDisplay::Instance()) TextDisplay::Instance()->print(cmd, true);//echo the command to the display
      try
      { handleCommand(cmd);
      }
      catch(const char *e)
      { cerr << "Caught exception*: " << e << endl;
        canContinue = false;
      }
      catch(const std::string& e)
      { cerr << "Caught exception&: " << e << endl;
        canContinue = false;
      }
      catch(...)
      { cerr << "Caught unknown exception" << endl;
        canContinue = false;
      }
	} while(!quit);
	restoreTerminalSettings();
	cerr << "\x1b[0mBye!" << endl;
	return 0;
}
