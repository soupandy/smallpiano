#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Display *display;
static Window window;
static Drawable window1;
static Drawable window2;
static Drawable graphwindow;

static Pixmap pixmap1;
static Pixmap pixmap2;
static GC gc;
static int screen,Width,Height,depth;
static XSetWindowAttributes attr;
static XEvent event;
static Colormap cmap;
static XSizeHints hints;
static XImage *image;
static int connected=0;
static int activecolor=0;

static int lastx=0;
static int lasty=0;

char* name_small = "-misc-fixed-medium-r-normal--10-100-75-75-c-60-iso8859-7";
char* name_medium = "-misc-fixed-medium-r-normal--0-0-75-75-c-0-iso8859-7";
char* name_large =  "-*-*-*-r-*-*-*-200-*-*-*-*-*-7";
XFontStruct* font_small;
XFontStruct* font_medium;
XFontStruct* font_large;
XFontStruct* font;
void smallFonts() {
font=font_small;
XSetFont(display, gc, font->fid);
}
void mediumFonts() {
font=font_medium;
XSetFont(display, gc, font->fid);}
void largeFonts() {
font=font_large;
XSetFont(display, gc, font->fid);
}


void activateswap() {
  pixmap1=XCreatePixmap(display,window,Width,Height,depth);
  pixmap2=XCreatePixmap(display,window,Width,Height,depth);
  window1=pixmap1;
  window2=pixmap2;
  graphwindow=window1;
}

void swapscreens() {
  XCopyArea(display,graphwindow,window,gc,0,0,Width,Height,0,0);
  Window tempwindow=window1;
  window1=window2;
  window2=tempwindow;
  graphwindow=window1;
}

void connecttoserver()
{
display=XOpenDisplay("");
if (!display) {fprintf (stderr,"No X server found\n");exit(1);}
else
connected=1;
}
void disconnect()
{
XCloseDisplay(display);
}

void makewindow(int x,int y)
{
if (!connected) connecttoserver();
hints.flags=USPosition | PMinSize;/*USPosition; (for spesific positioning)*/
hints.min_width=x;
hints.min_height=y;
screen=DefaultScreen(display);
depth=DisplayPlanes(display,screen);
Width=x;
Height=y;
window=XCreateWindow(display,RootWindow(display,screen),
0,0,Width,Height,3,depth,InputOutput,
XDefaultVisual(display,screen),
CWColormap,&attr);
XSetStandardProperties(display,window,"window","window",None,0,0,&hints);
gc=XCreateGC(display,window,0,0);
XSetWindowBackground(display,window,WhitePixel(display,screen));
XSetBackground (display,gc,WhitePixel(display,screen));
XSetForeground (display,gc,BlackPixel(display,screen)); 
XClearWindow(display,window);
XSelectInput(display,window,ExposureMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | ButtonMotionMask );
XMapRaised(display,window);
//pixmap=XCreatePixmap(display,menubar,Width,THEIGHT,depth);
XNextEvent(display,&event);
while(event.type!=Expose) XNextEvent(display,&event);
font_small = XLoadQueryFont(display, name_small);
font_medium = XLoadQueryFont(display, name_medium);
font_large = XLoadQueryFont(display, name_large);
font=font_small;
XSetFont(display, gc, font->fid);

graphwindow=window;
}

void closewindow()
{
	XDestroyWindow(display,window);
}

void uiflush()
{
	XFlush(display);
}

void uievent()
{
	XNextEvent(display,&event);
}

Bool uiKeyEvent() {
return (XCheckWindowEvent(display, window,  KeyPressMask | KeyReleaseMask, &event));
}

Bool uiMouseEvent() {
return (XCheckWindowEvent(display, window, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ButtonMotionMask , &event));
}

int uikeypress(char c)
{
  if (event.type==KeyPress)
  {
  char k=XLookupKeysym(&(event.xkey),event.xkey.state);
  if (c==k) return 1;
  }
return 0;
}

char uikey() {
  char k=0;
  if (event.type==KeyPress) {
  k=XLookupKeysym(&(event.xkey),event.xkey.state);
 }
 return k;
}

int uimousebutton(void) {
return event.xbutton.button;
}

int uimouseclick(int *x,int *y)
{
 if (event.type==ButtonPress)
  {
  *x=event.xbutton.x;//_root;
  *y=event.xbutton.y;//_root;
  return 1;
  }	
  return 0;
}

int uimousemove(int *x,int *y)
{
	if (event.type==MotionNotify)
	{
	*x=event.xmotion.x;
	*y=event.xmotion.y;
	}
}

void uiclear(int c)
{
//XSetWindowBackground(display,window,c);
//XClearWindow(display,window);
XSetForeground(display,gc,c);
XFillRectangle(display,graphwindow,gc,0,0,Width,Height);
XSetForeground(display,gc,activecolor);
XFlush(display);
}

int uigetpixel(int x,int y)
{
	image=XGetImage(display,window,x,y,1,1,-1,ZPixmap);
	int v=XGetPixel(image,0,0);
	XDestroyImage(image);
	return v;
}

void uipoint(int x,int y)
{
	XDrawPoint(display,graphwindow,gc,x,y);
	lastx=x;
	lasty=y;
}

void uilineto(int x,int y)
{
	XDrawLine(display,graphwindow,gc,lastx,lasty,x,y);
	lastx=x;
	lasty=y;
}

void uirect(int x1,int y1,int x2,int y2) 
{
XFillRectangle(display,graphwindow,gc,x1,y1,x2-x1,y2-y1);
}
void uicircle(int x,int y,int r) {
XDrawArc(display,graphwindow,gc,x-r,y-r,r*2,r*2,0,64*360);
}
void uifillcircle(int x,int y,int r) {
XFillArc(display,graphwindow,gc,x-r,y-r,r*2,r*2,0,64*360);
}

void uiline(int x1,int y1,int x2,int y2)
{
	XDrawLine(display,graphwindow,gc,x1,y1,x2,y2);
	lastx=x2;
	lasty=y2;
}

void uicolor(int c)
{
	activecolor=c;
	XSetForeground(display,gc,activecolor);
	
}

void uitest()
{
	int x,y;
	char c;
	makewindow (640,480);
	uicolor(0+255*256+0);
	uiline(0,0,50,50);
	int q=0;
	int m=0;
	while (!q)
	{
	uievent();
	if (uimouseclick(&x,&y))
		{
		printf("click at %d %d\n",x,y);
		if (m==0) uilineto(x,y);
		if (m==1) printf("pixel value %d\n", uigetpixel(x,y));
		}
	if (uikeypress('q'))
		{
		printf("q pressed\n");
		q=1;
		}
	if (uikeypress('1'))
		{
		uiclear(0);
		}
	if (uikeypress('2'))
		{
		uicolor(255*256*256);
		}
	if (uikeypress('3'))
		{
		m=1;
		}
	if (uikeypress('4'))
		{
		m=0;
		}
	if (uimousemove(&x,&y))
		{
		printf("move at %d %d\n",x,y);
		if (m==0) uilineto(x,y);
		}		
	}
}
void uitext(int x,int y,char *text)
{
XDrawString(display,graphwindow,gc,x,y,text,strlen(text));
}

void convertUTF8(unsigned char *dest,unsigned char *src,int length,int *rl) {
  int sp=0;
  int tp=0;
  while (sp<length) {
    unsigned int iv=src[sp];
    if (iv & 0x80) {
      int totalbytes=1;
      if ((iv&0xf0)==0xf0) {
	totalbytes=4;
      }
      else if ((iv&0xe0)==0xe0) {
	totalbytes=3;
      unsigned int byte1=src[sp];
      unsigned int byte2=src[sp+1];
      unsigned int byte3=src[sp+2];
      unsigned int value1=byte1 & 0x1f;
      unsigned int value2=byte2 & 0x3f;
      unsigned int value3=byte3 & 0x3f;
      unsigned int value=value1<<12 | value2<<6 | value3;
      dest[tp+1]=(unsigned char) (value & 0xff);
      dest[tp]=(unsigned char) ((value & 0xff00)>>8);
	printf("3 bytes: %x %x %x values %x %x %x value: %x\n",byte1,byte2,byte3,value1,value2,value3,value);
      }
      else if ((iv&0xc0)==0xc0) {
	totalbytes=2;
      unsigned int byte1=src[sp];
      unsigned int byte2=src[sp+1];
      unsigned int value1=byte1 & 0x3f;
      unsigned int value2=byte2 & 0x3f;
      unsigned int value=value1<<6 | value2;
      dest[tp+1]=(unsigned char) (value & 0xff);
      dest[tp]=(unsigned char) ((value & 0xff00)>>8);
      printf("2 bytes: %x %x values %x %x value %x\n",byte1,byte2,value1,value2,value);
      }
      else {
	printf("SOMETHING IS COMPLETELY FUCKED UP sp %d(%x) tp %d\n",sp,src[sp],tp);
      }
//      printf("total bytes at %d: %d\n",sp,totalbytes);
      sp+=totalbytes;
    } else {
      dest[tp]=0;
      dest[tp+1]=iv;
      sp++;
      printf("ascii char at %d: %c\n",sp,iv);
    }
    tp+=2;
  }
  *rl=tp;
}

void uitextutf8(int x,int y,char *text,int len)
{
char target[len*2];
int rl;
convertUTF8(target,text,len,&rl);
XDrawString16(display,graphwindow,gc,x,y,(XChar2b*)target,rl/2);
}

//void main() {uitest();}
XEvent uilastevent() {
  return event;
}
