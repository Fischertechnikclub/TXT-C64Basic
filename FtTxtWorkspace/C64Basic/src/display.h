/*
 * display.h
 *
 *  Created on: 10 Apr 2026
 *      Author: ad
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <cstdint>
#include <string>
#include <cmath>
#include <cstring>
#include <memory>
#include "common.h"


#define FB_WIDTH	240 //display width in normal orientation(portrait), we however rotate the display to lanscape to match the Commodore screen
#define FB_HEIGHT	320
#define ORIENTATION 3
#define COLUMNS		40
#define MAXROWS		25
#define VIEWPORT	200
#define TOPMARGIN	16
//#define TRANSPARENT	16 //color index used for transparency, not used by C64

enum color_t {BLACK, WHITE, RED, CYAN, PURPLE, GREEN, BLUE, YELLOW, ORANGE, BROWN, PINK, dGREY, mGREY, ltGREEN, ltBLUE, ltGREY, TRANSPARENT}; //the compiler makes this 32 bit while 4 would be enough
//the C64 color to RGB565 palette
extern const uint16_t colors[]; //0 black, 1 white, 2 red, 3 cyan, 4 purple, 5 green, 6 blue, 7 yellow,
                                //8 orange, 9 brown, 10 pink, 11 dark grey, 12 medium grey, 13 light green, 14 light blue, 15 light grey
extern const uint8_t charrom[4096]; //4 sets of 128 characters of 8 bytes each, 0: up/gfx, 1: up/gfx rev, 2: lo/up, 3: lo/up rev.  order within set: upper/lower, punct-digits, gfx/upper, gfx2

class Display
{ int fd;
  uint16_t *fb;
  static uint8_t topmargin, lines;//primitive viewport, C64 uses only 200 lines out of the 240 of the LCD, the rest is 'border'. when a picture is loaded the viewport takes the height of the picture
  uint16_t loadAddress = 0xA000;//pictures are often loaded here, sometimes at @2000
  inline unsigned address(uint16_t x, uint16_t y) //x, y in user screen coordinates
  {
#if ORIENTATION==0
#warning "the program is designed for landscape"
	  unsigned i = FB_WIDTH*y+x;
#elif ORIENTATION==1        //landscape motors top CCW
	  unsigned i = FB_WIDTH*x+(FB_WIDTH-1-y);
#elif ORIENTATION==2		//portrait upside down
#warning "the program is designed for landscape"
	  unsigned i = FB_WIDTH*(FB_HEIGHT-1-y)+(FB_WIDTH-1-x);
#elif ORIENTATION==3		//landscape inputs top CW
	  unsigned i = FB_WIDTH*(FB_HEIGHT-1-x)+y;
#else
#error ILLEGAL ORIENTATION
#endif
	  if(i>=FB_WIDTH*FB_HEIGHT)	throw "framebuffer bounds";
	  return i;
  }
protected:
  static std::unique_ptr<Display> instance;
  int X=0, Y=0; //current position in landscape orientation, relative to upper left corner
  color_t fg=BLACK, bg=WHITE; //current foreground and background colors (C64 index)
  static std::string pictfile;

  virtual void plot(color_t col = TRANSPARENT) //transparent here means that the current foreground color is used
  { plot(X, Y, col);
  }
  virtual void plot(int x, int y, color_t col = TRANSPARENT) //transparent here means that the current foreground color is used
  { if(col>15) col = fg;
    y += topmargin;
	if((unsigned)x < FB_HEIGHT && (unsigned)y < FB_WIDTH)//HEIGHT and WIDTH exchanged due to portrait mode
	  fb[address(x, y)] = colors[col];//map C64 colors to RGB565
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, color_t color);
  void fillRectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, color_t color)//Rel to display corner
  { if(x1>=FB_HEIGHT) x1 = FB_HEIGHT-1;
    if(y1>=FB_WIDTH) y1 = FB_WIDTH-1;
	for(uint16_t x=x0; x<=x1; x++)
	  for(uint16_t y=y0; y<=y1; y++)
	    fb[address(x, y)] = colors[color&15];//map C64 colors to RGB565
  }
  bool writeChar(uint8_t r, uint8_t c, const uint8_t ch[8], color_t f, color_t b)//write 8x8 pixels row r and column c in f and b colors (C64)
  { uint16_t x = 8*c, y = 8*r+topmargin;
	for(int row = 0; row<8; row++)
	{ uint8_t byte = ch[row];
	  for(int col = 0; col < 8; col++)
	  { bool pix = byte & (0x80>>col);
	    if(pix)
		  fb[address(x+col, y+row)] = colors[f];
	    else if (b!=TRANSPARENT) fb[address(x+col, y+row)] = colors[b];
	  }
	}
	return true;
  }
  void copyRectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t *buffer)//Rel. to viewport
  { if(x1>=FB_HEIGHT) x1 = FB_HEIGHT-1;
    y0+=topmargin;
    y1+=topmargin;
    if(y1>=FB_WIDTH) y1 = FB_WIDTH-1;
    uint16_t stride = x1-x0+1;
	for(uint16_t x=x0; x<=x1; x++)
	  for(uint16_t y=y0; y<=y1; y++)
	    buffer[stride*(y-y0)+x-x0] = fb[address(x, y)];
  }
  void pasteRectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint16_t *buffer)//Rel. to viewport
  { if(x1>=FB_HEIGHT) x1 = FB_HEIGHT-1;
    y0+=topmargin;
    y1+=topmargin;
    if(y1>=FB_WIDTH) y1 = FB_WIDTH-1;
    uint16_t stride = x1-x0+1;
	for(uint16_t x=x0; x<=x1; x++)
	  for(uint16_t y=y0; y<=y1; y++)
	    fb[address(x, y)] = buffer[stride*(y-y0)+x-x0];
  }
  void fillBorder(color_t col) //fills the top and bottom margins (5 rows)
  { fillRectangle(0, 0, FB_HEIGHT-1, topmargin-1, col);
    fillRectangle(0, topmargin+VIEWPORT-1, FB_HEIGHT-1, FB_WIDTH-1, col);
  }
  void fillViewport(color_t col)//fills the main screen are (25 rows)
  { fillRectangle(0, topmargin, FB_HEIGHT-1, topmargin+lines-1, col);
  }
  int height() { return lines;} //the height of the loaded picture or viewport height (200)
#if ORIENTATION==1 || ORIENTATION==3
  int width() { return FB_HEIGHT;} //landscape
#else
  uint16_t width() { return FB_WIDTH;} //portrait
#endif
public:
	Display();
	Display(const Display&) = delete;
	Display& operator=(const Display&) = delete;
	virtual ~Display();
	static Display* Instance() { return instance.get();}
	void cls(color_t c = ltGREY);
	void load(const char*fn, color_t fg = BLACK, color_t bg = WHITE, color_t c1 = BLACK, color_t c2 = BLACK);//default is black(0) on white(1), bg==16 will give transparent background
	void save(const char*fn, color_t bg = BLACK, uint8_t rows = MAXROWS, color_t c1 = TRANSPARENT, color_t c2 = TRANSPARENT); //pixel set when color differs from background color bg
	virtual void moveTo(int x, int y) { X=x; Y=y;} //set X,Y relative to viewport
	virtual void lineTo(int x, int y, color_t col = TRANSPARENT);
	void setFG(color_t c) {fg = (color_t)(c&15);}
	void setBG(color_t c) {bg = (color_t)(c&15);}
	uint8_t read(uint16_t x, uint16_t y);
	virtual const char* Name() { return "Display";}
	uint8_t Lines() { return lines;}
};

#ifdef TURTLEGRAPHICS
class Turtle: virtual public Display //at the moment only HIRES without using screen- or colorRam
{ color_t tcolors[4] = {ltGREY, BLUE, RED, GREEN};//subset of colors for turtle, indexes the global colors[] which in turn contains the RGB565 values {ltGrey, Blue, Red, Green} (in fact the 4 colors of the MULTICOLOR mode
  color_t spriteColor = BLACK;
  uint8_t bgnd = 0;//index in tcolors[]
  int scale = 1; //scales between 'user' coordinates and screen coordinates of the turtle (VICE uses scale 1)
  bool pen = true;
  double tx=0, ty=0; //turtle coordinates relative to center , tx to the right(on the TXT down), ty upwards (on the TXT to the right)
  double heading = 0.0;

  struct point
  { int16_t x, y;
    point(int16_t x, int16_t y): x(x), y(y) {}
    point():x(0), y(0) {}
  }  arrow[3]={{10, 0},{-5, 5},{-5, -5}}; //transformed model
  const point triangle[3]={{10, 0},{-5, 5},{-5, -5}};//model, in centre, pointing to the right

  struct Clip //clipboard for temporarily saving/pasting a screen rectangle in rgb565 colors
  { int x0, x1, y0, y1;
    uint16_t *buf;
    void init(Turtle&t, uint8_t r)
    { x0=t.scale*t.tx+t.width()/2; x1=x0+r; y0=t.height()/2-t.scale*t.ty; y1=y0+r;
  	  if(x0>r) x0-=r; else { x0 = 0; if(x1<0) x1=0;}
  	  if(y0>r) y0-=r; else { y0 = 0; if(y1<0) y1=0;}
  	  size_t sz = (y1-y0+1)*(x1-x0+1);
  	  if(sz>FB_HEIGHT*FB_WIDTH) throw "insane size";
  	  buf = new uint16_t[sz];
    }
  } clip = {0,0,0,0,0};

  void transform() //NOT A COORDINATE TRANSFORMATION, arror and triangle are in turtle coordinates but rotated(first) and translated to a scaled point
  {  double phi = heading + M_PI/2; //turtle graphics takes 0 degrees as 'up'
	 for(int i = 0; i < 3; i++)    //compute new arrow from triangle
       arrow[i] = point( scale*tx + triangle[i].x*cos(phi) - triangle[i].y*sin(phi),
					     scale*ty + triangle[i].x*sin(phi) + triangle[i].y*cos(phi)) ;
  }
  void copy(Clip& c, uint8_t r = 10) //r is the 'radius' of the copied square (i.e. half the length), defaults to the radius of the arrow
  { c.init(*this, r);
	copyRectangle(c.x0, c.y0, c.x1, c.y1, c.buf);
  }
  void paste(Clip& c)
  { if(c.buf)
	  { pasteRectangle(c.x0, c.y0, c.x1, c.y1, c.buf);
        delete c.buf;
	  }
  }
public:
	Turtle():Display() { fg = tcolors[1]; fprintf(stderr,"Creating turtle\n");}
	~Turtle() { fprintf(stderr,"deleting turtle\n");}
	void Triangle(point p[3], color_t col);
	void graph(char cmd, double arg, int cc=16);//here a colorcode 'cc' of 16 means that the code is not used
	virtual void moveTo(double x, double y) { tx=x; ty=y; Display::moveTo(scale*x+width()/2, height()/2-scale*y);}
	virtual void lineTo(double x, double y, color_t col = TRANSPARENT)
	{ //tx=x; ty=y;
	  Display::lineTo(scale*x+width()/2, height()/2-scale*y, col);
#ifdef LIMITXY
	  if(X<=0 || X >= width()-1) tx = (X-width()/2)/scale; //limit tx and ty to the screen
	  if(Y<=0 || Y >= height()-1) ty = (height()/2-Y)/scale;
#endif
	}
	uint8_t get() { uint8_t c = read(tx+width()/2, height()/2-ty); for (int i = 0; i < 4; i++) if (tcolors[i]==c) return i; return 4;} //return the turtle color @tx,ty or 4 when the actual color is not a turtle color
	virtual const char* Name() { return "Turtle";}
	static Turtle* Instance() { return dynamic_cast<Turtle*>(Display::Instance());}
};
#endif //TURTLEGRAPHICS

class TextDisplay: virtual public Display
{ bool upgfx=false, reverse=false;
  uint8_t rows = MAXROWS;
  virtual void scroll(); // { row=0; firstRow=25-rows + (firstRow+1)%rows;}
  uint8_t screenRam[MAXROWS][COLUMNS];
  color_t colorRam[MAXROWS][COLUMNS];
protected:
  uint8_t row = 0, col = 0; //the cursor position
  void writeChar(uint8_t r, uint8_t c)//write from screenRam to framebuffer
  { uint8_t sc = screenRam[r][c];
	uint16_t idx = sc<<3;
    //fprintf(stderr, "WC 0x%02x @%d,%d %d/%d\n", (int)sc, (int)r, (int)c, (int)colorRam[r][c], (int)bg);
    if(!upgfx)    idx |= 0x800;
	Display::writeChar(r, c, &charrom[idx], colorRam[r][c], bg);
  }
public:
	TextDisplay();
	~TextDisplay() { fprintf(stderr, "deleting TextDisplay\n");}
	void clear();
	virtual uint8_t Rows() { return rows;}
	virtual const char* Name() { return "Text";}
	static TextDisplay* Instance() { return dynamic_cast<TextDisplay*>(Display::Instance());}
	uint8_t cursorRow() { return row;}
	uint8_t cursorCol() { return col;}
	void print(char c);
	void print(const std::string& s, bool nl)
	{ //fprintf(stderr, "PE '%s' @%d,%d %d against %d", s.c_str(), row,col, fg, bg);
	  for(unsigned i = 0; i < s.length(); i++) print(s[i]);
	  if(nl) print('\r');
	  refresh();
	}
    void writeColor(uint8_t r, uint8_t c, color_t col)//write to colorRam
	{ colorRam[r][c] = col;
	  refresh();
	}
    virtual void refresh()
    { uint8_t firstRow = MAXROWS - Rows();
	  for(int r = 0; r<Rows(); r++)
	    for(int c = 0; c < COLUMNS; c++)
		  writeChar(firstRow+r, c);
    }
};

class Mixed: public Turtle, TextDisplay
{  uint8_t rows;
protected:
   //virtual void scroll()  { row=0; firstRow=25-rows + (firstRow+1)%rows;}
public:
  Mixed(uint8_t rows=4):Turtle(), TextDisplay(), rows(rows)
  { row = MAXROWS-rows;
	fprintf(stderr," Mixed %d lines and %d rows\n", Lines(), rows);
  }
  ~Mixed() { fprintf(stderr, "deleting Mixed\n");}
  virtual uint8_t Rows() { return rows;}
  virtual const char* Name() { return "Mixed";}
  static Mixed* Instance() { return dynamic_cast<Mixed*>(Display::Instance());}
};

#endif /* DISPLAY_H_ */
