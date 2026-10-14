//---------------------------------------------------------------------------

#ifndef D64filesH
#define D64filesH
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
#include <string>
#include <cstdio>
#include <cstring>

/* These classes are intended to read .D64 disk images
   It has a GUI TForm (Form5) to show the directory and open the file
*/

#define SECT    256 //sector size

#pragma pack(push, 1)
struct TrackSector  //combination of track [1-35] and sector [0-21] and translation to logical sector number
{ uint8_t t, s;
  TrackSector(uint8_t t, uint8_t s): t(t), s(s) {}
  TrackSector(): t(0), s(0) {}
  static uint16_t sect(uint8_t t, uint8_t s)  //t [1-40], s [0 - 20/18/17/16]
  {if(t < 1) throw "bad track";
   if(t < 18) { if(s>=21) throw "bad sector"; return  (t-1)*21                                + s;}
   if(t < 25) { if(s>=19) throw "bad sector"; return (18-1)*21+ (t-18)*19                     + s;}
   if(t < 31) { if(s>=18) throw "bad sector"; return (18-1)*21+(25-18)*19+ (t-25)*18          + s;}
   if(s>=17) throw "bad sector";
   if(t>40) throw "bad track";
   return                                            (18-1)*21+(25-18)*19+(31-25)*18+(t-31)*17 + s;
  }
  uint16_t sect() { return sect(t, s);}
};

struct dir_entry    //directory entry ; DO NOT CHANGE LAYOUT  , defined by .d64 format
{ uint8_t ndt, nds, type, t, s;
  uint8_t name[16];                      //PETSCII, $A0 padded
  uint8_t sst, sss, rrlen, unused[6]; //for REL files only
  uint16_t ssize; //size in sectors
  TrackSector Start() const { return TrackSector(t, s);}
  TrackSector Next() const { return TrackSector(ndt, nds);}
  std::string Name() const { std::string s; const uint8_t *p = name; while(p-name < 16 && *p != 0xA0) s.push_back(*p++); return s;}
};

struct bam_t     //block availability map   ; DO NOT CHANGE LAYOUT   , defined by .d64 format
{ uint8_t dt, ds, ver, unused;   //dir track/sector (unsused, assumed to be 18,0), version 'A'
  union trk_t
  { struct  track_t
	{ uint8_t free;   //nr of free sectors on this track
	  uint8_t map[3]; //24 bit free sectors bitmap
	} asStruct;
	uint32_t asInt;
  } tracks[35];   //04-8F (35 tracks)
  uint8_t name[16]; //90-9F                     //PETSCII, $A0 padded
  uint16_t res, id;  //A0-A1 (0xA0,0xA0)  A2-A3 (DiskID)
  uint8_t res2;   //A4  (0xA0)
  uint16_t dos;   //A5-A6  ("2A")
  uint8_t res3[4]; //A7-AA  (0xA0...)
  uint8_t res4;    //AB   (0x00)
  trk_t dolphin[5]; //AC-BF  (tracks 36-40)
  trk_t speed[5]; //C0-D3    (tracks 36-40)
  uint8_t reserved[44]; //D4-FF (0x00...)
};
#pragma pack (pop)

class D64file;

class D64disk
{ bam_t bam;
  FILE *disk = 0;
  uint32_t sectMap(uint8_t t)
  { t--; if (t >= 35) return 0;   //or an impossible value
	uint32_t bitmap = bam.tracks[t].asInt;
	bitmap >>= 8; //lose the 'free' byte
	bitmap &= 0x001FFFF; //keep 21 LSBs
	return bitmap;
  }
  bool sectIsFree(TrackSector ts) { return sectMap(ts.t) & (1<<ts.s);}
public:
  D64disk(): disk(0) { }
  ~D64disk() { disconnect();}
  bool connect(const char* fname) //open the .d64 file and read the BAM
  {   disk = fopen(fname, "rb");  //read-only for now
	  if(disk==0) return false;
	  fseek(disk, TrackSector::sect(18,0)*SECT, SEEK_SET);
	  fread(&bam, sizeof(bam_t), 1, disk);
	  std::string name; //effectively unused
	  unsigned i = 0; while(i < sizeof(bam.name) && bam.name[i] != 0xA0) name.push_back(bam.name[i++]); bam.name[i] = '\0';  //disk label, null terminated
	  return true;
  }
  void disconnect() { if(disk) fclose(disk); disk = 0;}
  const char *Label() { return (char*)bam.name;}
  uint16_t freeBlocks() { uint16_t free_blocks = 0; for(int j = 0; j < 35; j++) free_blocks += bam.tracks[j].asStruct.free; return free_blocks;}
  int read(void *dst, TrackSector ts, uint8_t offset=0, int len=SECT) //raw read, including next sector info
  { if (fseek(disk, ts.sect()*SECT + offset, SEEK_SET)) return -1;
	return fread(dst, len, 1, disk);
  }
  D64file* open(std::string name, uint8_t t);   //open a file with name and type on this disk
  void close(D64file*f); //{ delete f;}
  bool isConnected() { return disk != 0;}
  uint16_t ID() { return bam.id;}
};

class DirIterator //iterator over a disk directory
{ uint8_t e = 0;  //nr of entry in current sector
  TrackSector curr, next;   //track (always 18) and sector of current and next directory sector
  dir_entry dir;
  D64disk& d;
  bool last;
public:
   DirIterator(D64disk& d): d(d) {begin(); memset(&dir, 0, sizeof(dir));}
   void begin() { curr = {18, 1}; e = 0; last = false;}
   const dir_entry* nextDir()
   { if(d.read(&dir, curr, e*sizeof(dir_entry), sizeof(dir_entry)) < 0) return 0; //throw "directory error";
	 if(e==0) { next = dir.Next();}  //save the next sector number
	 e++;
	 if(e>=8) {curr = next; e = 0;} //move to the next sector
	 if(curr.t == 0) { last = true; return 0;}
	 return &dir;
   }
   bool end() { return last;}
   const dir_entry& operator*() { return dir;}
   D64disk& Disk() { return d;}
};

class D64file
{ D64disk& dsk;
  uint8_t buffer[SECT];
  uint8_t type;  //copied from dir_entry, filename is not copied
  uint16_t blks; //nr of sectors in the file, copied from dir_entry
  int avail;
  uint8_t *p;
  TrackSector start, curr;  //start copied from dir_entry
protected:
  bool getSect()  //read 1 entire current sector and set 'current' to the next sector
  { if(curr.t==0) return false;
	if(!dsk.read(buffer, curr)) return false;      //read whole sector
	curr.t = buffer[0]; curr.s = buffer[1];   //next sector to read in this file
	if(curr.t==0) avail = curr.s;
	else avail = SECT-2;
	p = buffer+2;
	return true;
  }
public:
  D64file(D64disk& dsk, dir_entry& d): dsk(dsk), type(d.type), blks(d.ssize), avail(0), p(buffer+2), start(d.Start()), curr(0, 0) {}
  D64file(DirIterator& it): dsk(it.Disk()), type((*it).type), blks((*it).ssize), avail(0), p(buffer+2), start((*it).Start()), curr(0, 0) {}
  virtual ~D64file() {}
  int available() { return avail;}   //available bytes in the buffer
  virtual int read(uint8_t *b, int len) //read len bytes or less, less does not imply eof
  { if(avail==0)
	  if(!getSect()) return -1;    //read the next sector when no more bytes available
	if(len > avail) len = avail;
	if (b) memcpy(b, p, len);
	avail -= len;
	p += len;
	return len;
  }
  int get() //read 1 byte
  { uint8_t c;
	if(read(&c, 1)==1)
	  return c;
	return -1;
  }
  int readWord(uint16_t& word) //LE order
  { int n = get(); if(n < 0) return n;
	word = n;
	n = get(); if(n < 0) return n;
	word |= n<<8;
	return 2;
  }
  uint8_t Type() { return type;}
  uint16_t Size() {return blks;}
  virtual bool open()
  { curr = start;
	avail = 0;
	return true;
  }
  virtual void close()
  { dsk.close(this);   //self destruct!
  }
};

class D64PRGfile: public D64file
{ uint16_t ldaddr, curr; //these addresses refer to C64 RAM, not to file offsets
  bool err;
public:
  D64PRGfile(D64disk& dsk, dir_entry& d): D64file(dsk, d) { err = d.type != 0x82; }
  D64PRGfile(DirIterator& it): D64file(it) { err = (*it).type != 0x82; }
  int gets(uint8_t *buf, uint16_t len, uint16_t& link, uint16_t& line)  //read a line of BASIC into buf, len is the buffer length
  { if(err) return -5;   //some previous error condition
	//assume we are at the beginning of a line (curr)
	if (readWord(link) < 0) return -2;    //read the link to the next line
	if(link==0)             return -1; //regular end of file
	if (readWord(line) < 0) return -3;    //read the BASIC line number (binary)
	uint8_t* q = buf;
	while(len>0)
	{ int n = get();
	  if(n<0) { err = true; return -4;} //premature end of file or other error
	  *q = n;
	  if(n==0) break;  //found end of line(string)
	  len--; q++;
	}
	if(*q)
	{ err = true;   //buffer too small
	  while(get()>0); //skip the rest of the line
	}
	return q - buf;   //return the number of characters put in the buffer
  } //gets
  uint16_t start()
  { return ldaddr;
  }   //the load address of the PRG file
  virtual bool open()
  { D64file::open();
	err = Type() != 0x82 || read((uint8_t*)&ldaddr, 2)!=2;  //get the load address
	curr = ldaddr;                                  //and make the current position the load address
	return !err;
  }
  virtual int read(uint8_t *b, unsigned len) //read len bytes
  { uint8_t *p = b;
	int bytes = 0;
	int n = 1; //just to get the loop started
	while(len>0 && n > 0)
	{ n = D64file::read(p, len);
	  if(n > 0)
	  { bytes += n;
		len -= n;
		p += n;
	  }
	}
	return bytes;
  }
};

class Tokeniser
{ bool collapsespaces = false;
  int gettoken(const char*& src);  //get a single token from src and advance the pointer
public:
  std::string tokenise(const std::string& line); //convert the character line into a token string
};



std::string deTokenize(const std::string& src);

extern D64disk diskA;  //only one disk drive for now
extern Tokeniser tokeniser;

//---------------------------------------------------------------------------
#endif
