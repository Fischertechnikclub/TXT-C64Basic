//============================================================================
// Name        : helloworld.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <glob.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <sys/xattr.h>
#include <regex>

#include <iostream>
#include <cstring>
#include <csignal>
#include <poll.h>
#include <unistd.h>
#include <termios.h>
//#include "common.h"
//#include "lexer.h"
#include "D64files.h"

#ifndef ERRORMSG
#define ERRORMSG(x) throw x
#endif

//to keep the linker silent
unsigned int DebugFlags;
FILE *DebugFile;

using namespace std;


int getType(const std::string& fn)
{ uint8_t type = 0;
  ssize_t ret = getxattr(fn.c_str(), "user.type", &type, sizeof(type));
  return ret<0 ? 0 : type;
}

void setType(const std::string& fn, uint8_t type)
{ ssize_t ret = setxattr(fn.c_str(), "user.type", &type, sizeof(type), 0);
  if(ret <0) perror("setxattr");
}


static void writeTrailer(FILE *f, uint16_t free = 0)
{ struct rec
	{ uint16_t head, size;
	  const char msg[16];
	} trailer = {0x0101, free, " BLOCKS FREE.\0\0"};
    fwrite(&trailer, 1, sizeof(trailer), f);
}

//NOTE! not exactly same function as in C64Basic
void initDir(FILE *f, const std::string& name, const std::string& id)
{   char buffer[40] = "\x01\x04\x01\x01\x00\x00\x12";//load address $0401, link = 0x0101 (unused), linenr = 0 (drive 0), reverse ON
	fwrite(buffer, 7, 1, f);
	int len = strlen(name.c_str());
	if(len>16) len = 16;
	snprintf(buffer, sizeof(buffer), "\"%.16s\" %*s 2A", name.c_str(), 16+2-len, id.c_str());
	fwrite(buffer, 25, 1, f);
	fprintf(stderr, "initiated new directory file for disk %s\n", buffer);
}

//---------------------------------------------------------------------------



// =============================================================================
// extract.cpp  –  Skeleton archive extraction utility for Linux
//
// Build:   g++ -std=c++17 -Wall -Wextra -o extract extract.cpp
// Usage:   extract [options] <archive>
//
//   -h | -?            Show this help / usage message
//   -l                 List the contents of the archive
//   -f <filename>      Extract a single named file from the archive
//   -a                 Extract all files from the archive
// =============================================================================


// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void printUsage(const char* programName);
static void listContents(/*const std::string& archive*/);
static void extractFile(const std::string& dirname/*archive*/, const std::string& filename);
static void detokeniseFile(/*const std::string& archive,*/ const std::string& filename);
static void directoryFile(/*const std::string& archive,*/ const std::string& filename);
static void extractAll(const std::string& dirname/*archive*/);

// ---------------------------------------------------------------------------
// printUsage
// ---------------------------------------------------------------------------
static void printUsage(const char* programName)
{
    std::cout
        << "\nUsage: " << programName << " [options] <archive>\n\n"
        << "Options:\n"
        << "  -h, -?              Show this help message\n"
        << "  -l                  List the contents of the archive\n"
        << "  -f <filename>       Extract a single file from the archive\n"
		<< "  -f '<regexp>'       Extract files using wild cards\n"
		<< "  -t <filename>       Detokenise a single file to stdout\n"
		<< "  -d <filename>       Create a C64 directory file\n"
        << "  -a                  Extract all files from the archive\n\n"
        << "Examples:\n"
        << "  " << programName << " -l  disk1.d64\n"
        << "  " << programName << " -f program.bas disk1.d64\n"
        << "  " << programName << " -f '*.64' disk1.d64\n"
        << "  " << programName << " -d .dir disk1.d64\n"
        << "  " << programName << " -a  disk1.d64\n\n";
}

// ---------------------------------------------------------------------------
// listContents  –  Stub: open archive and print its file table
// ---------------------------------------------------------------------------
static void listContents(/*const std::string& archive*/)
{     DirIterator it(diskA);
	  do
	  { if(it.nextDir()==0) break;
		if((*it).type>0)    //fails on empty directory
		{ std::string str = (*it).Name();
#if 0
		  switch((*it).type)
		  { case 0: break; //deleted entry
			case 0x80: str += " DEL "; break;   //no info
			case 0x81: str += " SEQ "; /*f = new D64file(it);*/    break;
			case 0x82: str += " PRG "; /*f = new D64PRGfile(it);*/ break;
			case 0x83: str += " USR "; /*f = new D64file(it);*/   break;
			case 0x84: str += " REL "; break;   //not supported
		  }
		  str += " " + std::to_string((*it).ssize);
		  cout << str << endl;;
#else
		  const char *ext[] = {"DEL", "SEQ", "PRG", "USR", "REL"};
		  const char * s = str.c_str();
          printf("%5d  \"%.16s\" %*s\n", (*it).ssize, s, 16+3-strlen(s), ext[(*it).type & 0x07]);
#endif
		}
	  } while(!it.end());
      std::cout << std::to_string(diskA.freeBlocks()) << " FREE BLOCKS.\n";
}

// ---------------------------------------------------------------------------
// directoryFile  –  Stub: open archive and print its file table
// ---------------------------------------------------------------------------
static void directoryFile(/*const std::string& archive,*/ const std::string& filename)
{     const char *full_path = filename.c_str();
      FILE *f = fopen(full_path, "wb");//creating a file that looks like a "$" file
      setType(filename, 0x82);
      union
	  { uint32_t id;
	    char ids[4];
	  } id = { diskA.ID() };
      initDir(f, diskA.Label(), id.ids);
	  DirIterator it(diskA);
	  while(it.nextDir())
	  { if(it.nextDir()==0) break;
		if((*it).type>0)
		{ const char* str = (*it).Name().c_str();
          uint8_t value = (*it).type;
          if(0x81 <= value && value <= 0x84)
          { const char *ext[] = {"SEQ", "PRG", "USR", "REL"};
#pragma pack(push, 1)
            struct rec
            { uint16_t head[2];
              char str[28];
            } buffer = { {0x0101, (*it).ssize}, {' '} };
#pragma pack (pop)
            snprintf(buffer.str, 28, "  \"%.16s\" %*s   ", str, 16+3-strlen(str), ext[value-0x81]);
            fwrite(&buffer, sizeof(buffer), 1, f);
          }
		}
	  }
	  writeTrailer(f);
	  fclose(f);
}

// ---------------------------------------------------------------------------
// extractFile  –  Stub: locate one named file or wildcard files and write it to disk
// ---------------------------------------------------------------------------

static std::string wildcardToRegex(const std::string& pattern)
{   std::string result = "^";
    for (char c : pattern) {
        switch (c) {
            case '*':  result += ".*"; break;   // any sequence of chars
            case '?':  result += ".";  break;   // any single char
            case '.':
                //throw std::invalid_argument("'.' is not permitted in pattern.");
            // Escape all other regex metacharacters so they are treated literally
            case '\\': case '^': case '$': case '+':
            case '{':  case '}': case '[': case ']':
            case '(':  case ')': case '|':
                result += '\\';
                result += c;
                break;
            default:
                result += c;
        }
    }
    result += "$";
    return result;
}

static void extractFile(const std::string& dirname/*archive*/, const std::string& filename)
{   cout << "finding matches for \"" << filename  << "\"" << endl;
    std::regex pattern(wildcardToRegex(filename));
	  DirIterator it(diskA);
	  while(it.nextDir())
	  { if(((*it).type &0xFC) == 0x80)
		{ std::string str = (*it).Name();
		  if(std::regex_match(str, pattern))
		  { D64file *f = new D64file(it);
		    cout << "extracting " << str << " to " << dirname <<endl;
		    str = dirname + "/" + str;
		    FILE *g = fopen(str.c_str(), "w");
		    f->open();
		    if(g)
		    {   uint8_t buf[256];
		        while(1)
		        { int len = f->read(buf, 254);
		          if(len < 0) break;
		          fwrite(buf, len, 1, g);
		        }
		    	fclose(g);
		    }
		    delete f;
	        //printf("setting type to %02X\n", (*it).type);
		    setType(str, (*it).type);
		  }
		}
	  }
    cout << "Done!" << endl;
}

// ---------------------------------------------------------------------------
// detokeniseFile  –  Stub: locate one named file and write it to disk
// ---------------------------------------------------------------------------
static void detokeniseFile(/*const std::string& archive,*/ const std::string& filename)
{	  DirIterator it(diskA);
	  while(it.nextDir())
	  {   std::string str = (*it).Name();
		  if(str==filename)
		  { if((*it).type != 0x82)
		    { cerr << filename << " is not a .PRG file" << endl;
			  break;
		    }
			D64PRGfile *f = new D64PRGfile(it);
		    fprintf(stderr, "Detokenising %s, load address = %04X\n\n", str.c_str(), f->start());
		    f->open();
		    uint8_t buf[256];
		    while(1)
		    { uint16_t line=0, link =0;
		      int len = f->gets(buf, 256, link, line);
		      if(len < 0) break;
		      std::string cooked = deTokenize((char*)buf);
		      fprintf(stdout,"%5d %s\n", line, cooked.c_str());
		    }
		    cerr << "\nDone!" << endl;
		    delete f;
		    return;
		  }
	  }
	  cerr << "file " << filename << " could not be found" << endl;
}

// ---------------------------------------------------------------------------
// extractAll  –  Stub: extract every file in the archive
// ---------------------------------------------------------------------------
static void extractAll(const std::string& dirname/*archive*/)
{	  DirIterator it(diskA);
	  while(it.nextDir())
	  { if(((*it).type &0xFC) == 0x80)
		{ std::string str = (*it).Name();
		  { D64file *f = new D64file(it); //same as diskA.open(str, (*it).type)
		    cout << "extracting " << str << " to " << dirname << endl;
		    str = dirname + "/" + str;
		    FILE *g = fopen(str.c_str(), "w");
		    f->open();
		    if(g)
		    {   uint8_t buf[256];
		        while(1)
		        { int len = f->read(buf, 254);//254 is the net sector size
		          //cerr << len << endl;
		          if(len < 0) break;
		          fwrite(buf, len, 1, g);
		        }
		    	fclose(g);
		    }
		    delete f; //same as diskA.close(f) and f->close() which does a self destruct
		    setType(str, (*it).type);
		  }
		}
	  }
      cout << "Done!" << endl;
}

static void ensureDir(const std::string& path)
{   struct stat st{};
    if (stat(path.c_str(), &st) == 0) {
        if (!S_ISDIR(st.st_mode))
            throw std::runtime_error(path + " exists but is not a directory");
        // directory already exists, nothing to do
    } else {
        if (mkdir(path.c_str(), 0755) != 0)
            throw std::runtime_error("could not create directory " + path
                                     + ": " + strerror(errno));
    }
}
// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    // -----------------------------------------------------------------------
    // Need at least one argument beyond the program name
    // -----------------------------------------------------------------------
    if (argc < 2) {
        std::cerr << "Error: no arguments supplied.\n";
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // -----------------------------------------------------------------------
    // Parse command-line arguments
    // -----------------------------------------------------------------------
    enum class Action { NONE, HELP, LIST, EXTRACT_FILE, EXTRACT_ALL, DETOKENISE, DIRECTORY };

    Action      action       = Action::NONE;
    std::string targetFile;   // used with -f -t -d
    std::string archive;      // the archive path (last positional argument)

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);

        if (arg == "-h" || arg == "-?") {
            action = Action::HELP;
            // Help takes priority; print and exit immediately.
            printUsage(argv[0]);
            return EXIT_SUCCESS;

        } else if (arg == "-l") {
            action = Action::LIST;

        } else if (arg == "-a") {
            action = Action::EXTRACT_ALL;

        } else if (arg == "-f") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -f requires a filename argument.\n";
                printUsage(argv[0]);
                return EXIT_FAILURE;
            }
            action     = Action::EXTRACT_FILE;
            targetFile = argv[++i];   // consume next token as filename

        } else if (arg == "-t") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -t requires a filename argument.\n";
                printUsage(argv[0]);
                return EXIT_FAILURE;
            }
            action     = Action::DETOKENISE;
            targetFile = argv[++i];   // consume next token as filename

        } else if (arg == "-d") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -d requires a filename argument.\n";
                printUsage(argv[0]);
                return EXIT_FAILURE;
            }
            targetFile = argv[++i];   // consume next token as filename
            action     = Action::DIRECTORY;

        } else if (arg[0] == '-') {
            std::cerr << "Error: unknown option \"" << arg << "\".\n";
            printUsage(argv[0]);
            return EXIT_FAILURE;

        } else {
            // Treat the first non-option argument as the archive path.
            if (archive.empty()) {
                archive = arg;
            } else {
                std::cerr << "Error: unexpected argument \"" << arg << "\".\n";
                printUsage(argv[0]);
                return EXIT_FAILURE;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Validate that we have an archive path
    // -----------------------------------------------------------------------
    if (archive.empty()) {
        std::cerr << "Error: no archive specified.\n";
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // -----------------------------------------------------------------------
    // Validate that at least one action was requested
    // -----------------------------------------------------------------------
    if (action == Action::NONE) {
        std::cerr << "Error: no action specified (-l, -f, -d, -t or -a).\n";
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // -----------------------------------------------------------------------
    // Dispatch to the appropriate handler
    // -----------------------------------------------------------------------

    try
    {   unsigned dot = archive.find_last_of('.');
        if(dot==std::string::npos || !(archive.substr(dot)==".d64" || archive.substr(dot)==".D64"))
		{ cerr << "disk archive must be a .d64 or .D64 file" << endl;
		  return EXIT_FAILURE;
		}
        std::string dirname = archive.substr(0, dot);
        ensureDir(dirname);
        if(!diskA.connect(archive.c_str()))
        		{ cerr << "couldn't open disk archive " << archive << endl;
        		  return EXIT_FAILURE;
        		}
        cout << "opened: " << diskA.Label() << endl;
      switch (action) {
        case Action::LIST:
            listContents(/*archive*/);
            break;

        case Action::EXTRACT_FILE:
            extractFile(dirname/*archive*/, targetFile);
            break;

        case Action::DETOKENISE:
            detokeniseFile(/*archive,*/ targetFile);
            break;

        case Action::DIRECTORY:
            directoryFile(/*dirname archive,*/ dirname + "/" + targetFile);
            break;

        case Action::EXTRACT_ALL:
            extractAll(dirname/*archive*/);
            break;

        default:
            // Should be unreachable given the checks above.
            std::cerr << "Error: internal state error.\n";
            return EXIT_FAILURE;
    }

    }
    catch(const char *msg)
    {
    	cerr << "exception " << msg << endl;
    }
    catch(...)
    {
    	cerr << "Unknown exception" << endl;
    }
    if(diskA.isConnected()) diskA.disconnect();
    return EXIT_SUCCESS;
}
