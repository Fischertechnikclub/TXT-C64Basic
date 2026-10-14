
#include "files.h"
#include <sys/xattr.h>
//---------------------------------------------------------------------------

std::map<uint8_t,C64File> files;   //list of open files

int8_t C64File::status = 0;

int getType(const std::string& fn)
{ uint8_t type = 0;
  ssize_t ret = getxattr(fn.c_str(), "user.type", &type, sizeof(type));
  return ret<0 ? 0 : type;
}

void setType(const std::string& fn, uint8_t type)
{ ssize_t ret = setxattr(fn.c_str(), "user.type", &type, sizeof(type), 0);
  if(ret <0) perror("setxattr");
}

#if 1
void C64File::handleDiskCmd(const std::string& nm)
{ unsigned p = nm.find_first_of(':'), old, id; //check for special commands
  if(p!=std::string::npos)
  { switch(nm[0])
    { case 'N': /* NEW format floppy*/
    	       id = nm.find_first_of('=', p);
    	       if(id != std::string::npos)
    	       { std::string ident = nm.substr(id+1, 2) + "  ";
    	         initDir(nm.substr(p+1, id-p-1), ident.substr(0, 2));
    	       }
    	       else
     	          initDir(nm.substr(p+1), "  ");
    	       break;
      case 'S': //SCRATCH delete
      	      status = remove(nm.substr(p+1).c_str()) ? errno : 0;
       	      info("deleted " + nm.substr(p+1));
      	      break;
      case 'R': //RENAME new name = old name
      	      old = nm.find_first_of('=', p);
      	      if(old != std::string::npos)
      	      status = rename(nm.substr(old+1).c_str(), nm.substr(p+1, old-p-1).c_str()) ? errno : 0; //old name, new name
      	      info("renamed " + nm.substr(old+1) + " to " + nm.substr(p+1, old-p-1));
      	      break;
      case 'I':	 /* INITIALIZE reset the drive (e.g. after error) */
      case 'C': /* COPY */ break;
    }
  }
}

bool C64File::parseFilename(const std::string& nm)
{ unsigned begin = nm.find_first_of(':'); //check for drive number
  if(begin==1 && isdigit(nm[0]))
  { drive = nm[0] - '0';
    begin++;
  }
  else if(begin!=std::string::npos)
  { ERRORMSG("expected drive number before ':'");
    return false;
  }
  begin = 0;
  unsigned end = nm.find_first_of(',', begin);
  filename = nm.substr(begin, end-begin);
  if(filename=="") return false;
  if(end!=std::string::npos && end+1 < nm.length())
  { char tt = toupper(nm[end+1]); //expect SPURL for file types or RWAM for access if type nor specified or a comma for explicitly omitted type
	if(tt!=',')
	{ const char *types = "SPUL";//SEQ, PRG, USR, apparently .L50 is a REL file with a record length of 50
	  const char *t = strchr(types, tt);
	  if(t)
	  { if (t-types < 3)
	      type = (t-types) + 0x81;
	    else
	    { ERRORMSG("REL types are not supported");
	      return false;
	    }
	  }//no ,type clause so use default type
	}//else type explicitly omitted, use default type
	unsigned access = nm.find_first_of(',', end+1) ;//parse the name string to find the access method
	if(access!=std::string::npos && access+1 < nm.length() && chan > 1) //do not change access method on reserved LOAD/SAVE channels
	{ acc[0] = tolower(nm[access+1]);
	  if(strchr("rwa", acc[0])==0)
	  { ERRORMSG("illegal access mode");//read write append, modify not supported
	    return false;
	  }
	}
  }//else no type or access specifier, use defaults
  return true;
}

void C64File::open(uint8_t dev, uint8_t sec, std::string& nm)
{ kind = (devices)((devices)dev>=disk ? disk : dev);
  device = dev;
  chan = sec;
  type = chan<2 ? 0x82 : 0x81; //for chan 0/1 default is PRG, otherwise SEQ
  acc[0] = chan==1 ? 'w' : 'r'; //read access by default except for SAVE
  if (kind==disk && chan==15)
  {   handleDiskCmd(nm);
	  return;
  }
  if(parseFilename(nm))
  {	switch (kind)
	{ case datasette:  // sec: 0=read, 1=write, 2=write+EOT (end-of-tape)
					   return;
	  case disk:  //sec: 0=load, 1=save,15=error, rest=user  //handle load/save too here???
				  if (chan>15)
				  { ERRORMSG("illegal secondary dev");
				    return;
				  }
				  if(nm[0]=='$')
				  { //directory_file("."); //(re)create the directory file from the linux directory
                    filename = ".dir";
                    list_directory(filename.c_str(), ".", false);//(re)create the directory file from the linux directory
				  }
				  if(acc[0]=='w') setType(nm, type); //change the type when (re) creating the file
				  else
				  { uint8_t ft = getType(nm);
				    if(ft && ft != type)
				    	info("file type conflict while reading/appending " + nm);
				  }
				  f = fopen(filename.c_str(), acc);
				  if(f) { err = 0; status = 0;}
				  else
				  { status = err = errno;
					info("error opening file " + filename);
				  }
				  break;
	  case keyboard: //already open
	  case screen: return;
	  case printer0:
	  case printer1: return;
	  default: ERRORMSG("device not supported");//not supported
			   return;
	}
  }
}//open

#else

void C64File::open(uint8_t dev, uint8_t sec, std::string& nm)
{ kind = (devices)((devices)dev>=disk ? disk : dev);
	device = dev;
	unsigned p = nm.find_first_of(',') ; //parse the name string to find the type
	if(p!=std::string::npos && p+1 < nm.length())
	{ char tt = toupper(nm[p+1]);
	  if(tt==',') //type omitted, default to SEQ/PRG
		  type = sec<2 ? 0x82 : 0x81; //for chan 0/1 default is PRG, otherwise SEQ
	  else
	  { const char *types = "SPUR";//SEQ, PRG, USR, REL
	    const char *t = strchr(types, tt);
	    if(t==0) { type = 0; ERRORMSG("illegal file type");}
	    else type = (t-types) + 0x81;
	  }
	  unsigned q = nm.find_last_of(',') ;//parse the name string to find the access method
	  if(q!=std::string::npos && q+1 < nm.length() && p!=q)
	  { acc[0] = tolower(nm[q+1]);
	    if(strchr("rwa", acc[0])==0) ERRORMSG("illegal access mode");//read write append
	  }
	}
	nm = nm.substr(0, p);//truncate at first ',' if any
  //const char *exts[]={".SEQ",".PRG",".USR",".REL"};
	switch (kind)
	{ case datasette:  chan = sec; //0=read, 1=write, 2=write+EOT (end-of-tape)
					   return;
	  case disk:  chan = sec; //0=load, 1=save,15=error, rest=user  //handle load/save too here???
				  if (chan==0) acc[0] = 'r';
				  else if (chan==1) acc[0] = 'w';
				  else if (chan>15) { ERRORMSG("illegal secondary dev"); return; }
#if 0
				  p = nm.find_last_of('.');//check file type/extension
				  if(p!=std::string::npos)
				  { std::string ext = nm.substr(p,4);
				    if(acc[0]=='r' && ext!=exts[type-0x81])
				    	info("extension does not correspond to filetype");
				  }
				  else if(acc[0]=='w')//write file that has no extension yet
				    nm = nm + exts[type-0x81];
#else
				  if(acc[0]=='w') setType(nm, type);
				  else if(acc[0]=='r')
				  { uint8_t ft = getType(nm);
				    if(ft && ft != type)
				    	info("file type conflict while reading " + nm);
				  }
#endif
				  if(chan==15) //handle some floppy commands, should also be possible with PRINT# logical,8,15 "S:filename"  not implemented
				  { p = nm.find_first_of(':'); //check for special commands
				    if(p==1)
				    { switch(nm[0])
				      { case 'N': /*format floppy*/ break;
				        case 'S': //scratch
				        	      remove(nm.substr(2).c_str());
				        	      break;
				        case 'R': //rename
				        	      p = nm.find_first_of(':');
				        	      rename(nm.substr(p+1).c_str(), nm.substr(2, p-3).c_str());
				        	      break;
				        case 'C': /* copy */ break;
				      }
				    }
				  }
				  else
				  { f = fopen(nm.c_str(), acc);
				    if(!f) err = errno; else err = 0;
				  }
				  break;
	  case keyboard: //already open
	  case screen: return;
	  default: ERRORMSG("device not supported");//not supported
			   return;
	}
}//open
#endif
