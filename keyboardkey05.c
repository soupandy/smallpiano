typedef struct {
int x,y;
int width,height,raise;
unsigned int keycode;
char text[32];
int pressed;
} KeyboardKey;

int keyForeground=0;
int keyBackground=0xffffff;
int textheight=10;

void SetKey(KeyboardKey *k,int x,int y,int width,int height, int raise,unsigned int keycode,char *text) {
  k->x=x;k->y=y;k->keycode=keycode;
  k->width=width;k->height=height;k->raise=raise;
  k->pressed=0;
  //sprintf(k->text,"%8s",text);
  sprintf(k->text,"%s",text);
}

void ShowKey(KeyboardKey *k) {
  void drawrect(int x1,int y1,int x2,int y2,int hilight) {
    int y;
    
    uicolor (keyBackground);
    for (y=y1-k->raise;y<y1;y++) uiline(x1,y,x2,y);
    if (hilight) uicolor (0xffff00);
    for (;y<y2;y++) uiline(x1,y,x2,y);
    uicolor (keyForeground);
    uiline(x1,y1,x2,y1);uiline(x1,y2,x2,y2);
    uiline(x1,y1,x1,y2);uiline(x2,y1,x2,y2);
    
  }  
  //for (y=k->y-k->height/2;y<k->y+k->height/2;y++) uiline(k->x-k->width/2,y,k->x+k->width/2,y);
  
  drawrect(k->x-k->width/2,k->y-k->height/2,k->x+k->width/2,k->y+k->height/2,0);
  //uiline(k->x-k->width/2,k->y-k->height/2,k->x+k->width/2,k->y-k->height/2);
  //uiline(k->x-k->width/2,k->y+k->height/2,k->x+k->width/2,k->y+k->height/2);
  //uiline(k->x-k->width/2,k->y-k->height/2,k->x-k->width/2,k->y+k->height/2);
  //uiline(k->x+k->width/2,k->y-k->height/2,k->x+k->width/2,k->y+k->height/2);
  if (k->pressed) {
    drawrect(k->x-k->width/2+k->raise,k->y-k->height/2,//+k->raise,
	     k->x+k->width/2-k->raise,k->y+k->height/2-1,1);
    uitext(2+k->x-k->width/2+k->raise,2+k->y-k->height/2+textheight,k->text);
    uiline(k->x-k->width/2,k->y-k->height/2+k->raise,
	   k->x-k->width/2+k->raise,k->y-k->height/2);
    uiline(k->x+k->width/2,k->y-k->height/2+k->raise,
	   k->x+k->width/2-k->raise,k->y-k->height/2);
    uiline(k->x-k->width/2,k->y+k->height/2,
	   k->x-k->width/2+k->raise,k->y+k->height/2-1);
    uiline(k->x+k->width/2,k->y+k->height/2,
	   k->x+k->width/2-k->raise,k->y+k->height/2-1);
    
  } else {
    drawrect(k->x-k->width/2+k->raise,k->y-k->height/2-k->raise,
	     k->x+k->width/2-k->raise,k->y+k->height/2-k->raise,1);
    
    uitext(2+k->x-k->width/2+k->raise,k->y,k->text);
    
    uiline(k->x-k->width/2,k->y+k->height/2,
	   k->x-k->width/2+k->raise,k->y+k->height/2-k->raise);
    uiline(k->x+k->width/2,k->y+k->height/2,
	   k->x+k->width/2-k->raise,k->y+k->height/2-k->raise);
    uiline(k->x-k->width/2,k->y-k->height/2,
	   k->x-k->width/2+k->raise,k->y-k->height/2-k->raise);
    uiline(k->x+k->width/2,k->y-k->height/2,
	   k->x+k->width/2-k->raise,k->y-k->height/2-k->raise);
    uicolor (keyBackground);
    uiline(k->x-k->width/2+1,k->y-k->height/2,
	   k->x-k->width/2+k->raise-1,k->y-k->height/2);
    
    uiline(k->x+k->width/2-1,k->y-k->height/2,
	   k->x+k->width/2-k->raise+1,k->y-k->height/2);
	   
    uicolor (keyForeground);
  }
  /*
  uicolor(0x00ff00);
  uipoint(k->x,k->y);
  printf("text is '%s'\n",k->text);
  */
}

void CheckKey(KeyboardKey *k,XEvent event) {
  //printf("event type%d\n",event.type);
  if ((event.type==KeyPress && event.xkey.keycode==k->keycode) ||
    (event.type==ButtonPress && 
    (event.xbutton.x>k->x-k->width/2 && 
     event.xbutton.x<k->x+k->width/2 && 
     event.xbutton.y>k->y-k->height/2 &&
     event.xbutton.y<k->y+k->height/2 )
    )
  ) k->pressed=1; else
    
  k->pressed=0;
}
