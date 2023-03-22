#include "graphlib-8859-7.c"
#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include "keyboardkey05.c"
#include "midilib16.c"

#define TEMPOMULTIPLIER 2

void reset_midi();
void stopallsound();
void layoutSong();
void importDrums();

static snd_seq_t *seq;
static int client;
static snd_seq_addr_t outport;
static int queue;
char *portarg;
char notedescr[]="C C#D D#E F F#G G#A A#B C C#D D#E F F#G G#A A#B C C#D D#E F F#G G#A A#B C C#D D#E F F#G G#A A#B ";
KeyboardKey menukeys[80];
KeyboardKey keys[80];
int duration=32;
int instrument=1;
char reply[256];
int octave=3;
int activePercussionFlag=0;
int activeAccopaniamentFlag=0;
int activeChord=0;
typedef struct {
  int time;
  int note; 
  int duration;
}  NoteEvent;
typedef struct {
  int time;
  int channel;
  int type;
  int note;
} MidiEvent;
enum ControlEventType{
  SelectPercussion,
  SelectAccopaniament,
  PercussionFlag,
  AccopaniamentFlag,
  SelectChord,
  TextEvent};
typedef struct {
  int time;
  enum ControlEventType type;
  int value;
} ControlEvent;

typedef struct {
  int bar,q,tick;
} timePos;
timePos currenttime;

NoteEvent *song=NULL;
char *lyrics=NULL;
int lyricsSize=0;
int songDuration=0;
int songSize=0;
ControlEvent *accompan=NULL;
int accompanDuration=0;
int accompanSize=0;
MidiEvent *midiList=NULL;
int midiDuration=0;
int midiSize=0;


//#include "drumsseq.c"
long long int starttime;
long long int now;

typedef struct {
  NoteEvent *drums;
  int drumsDuration;
  int drumsSize;
}
PercussionSequence;

static PercussionSequence percSeq[128];

void initPercSeq() {
  int i;
  for (i=0;i<128;i++) {
    percSeq[i].drums=NULL;
    percSeq[i].drumsDuration=0;
    percSeq[i].drumsSize=0;
  }
}

void clearPercSeq() { //must have been initialized before this clearAllDrums
  int i;
  for (i=0;i<128;i++) {
    if (percSeq[i].drums!=NULL) free (percSeq[i].drums);
  }
  initPercSeq();
}

void freePercSeqId(int id) {
    if (percSeq[id].drums!=NULL) free (percSeq[id].drums);
    percSeq[id].drumsDuration=0;
    percSeq[id].drumsSize=0;   
    percSeq[id].drums=NULL;
}

NoteEvent *drums=NULL;
int drumsDuration=0;
int drumsSize=0;
int drumCursor=0;
int editDrumsMode=0;
int metronomeActive=1;

int freePercSeq() {
  int i;
  for (i=0;i<128;i++) if (percSeq[i].drums==NULL) return i;
  return -1;
}


void storePercSeq(int id) {
  if (drums==NULL) return;
  freePercSeqId(id);
  percSeq[id].drumsDuration=drumsDuration;
  percSeq[id].drumsSize=drumsSize;
  percSeq[id].drums=realloc(percSeq[id].drums,drumsSize*sizeof(NoteEvent));
  if (percSeq[id].drums==NULL) {
    fprintf(stderr,"out of memory\n");
    exit(-1);
  }
  printf("memcpy start %p %p %d\n",percSeq[id].drums,drums,drumsSize*sizeof(NoteEvent));
  memcpy(percSeq[id].drums,drums,drumsSize*sizeof(NoteEvent));
  printf("memcpy done %p %p %d\n",percSeq[id].drums,drums,drumsSize*sizeof(NoteEvent));
}

int storeFreePercSeq() {
  int freeid=freePercSeq();
  if (freeid>=0) storePercSeq(freeid); 
  return freeid;
}


void restorePercSeq(int id) {
  printf("restoring from mem %p\n",percSeq[id].drums);
  if (percSeq[id].drums==NULL) return;
  if (drums!=NULL) free(drums);
  drumsDuration=percSeq[id].drumsDuration;
  drumsSize=percSeq[id].drumsSize;
  drums=NULL;
  drums=realloc(drums,drumsSize*sizeof(NoteEvent));
  printf("mem restore start %p %p %d\n",drums,percSeq[id].drums,drumsSize*sizeof(NoteEvent));
  memcpy(drums,percSeq[id].drums,drumsSize*sizeof(NoteEvent));
}

int countPercBanks() {
  int count=0;
  int i;
  for (i=0;i<128;i++) {
    if (i<8) printf("%3d/%3d(%p)\t",count,i,percSeq[i].drums);
    if (percSeq[i].drums!=NULL) count++;
  }
  return count;
}

void increaseDrumsSize() {
  drumsSize+=1024;
  drums=realloc(drums,drumsSize*sizeof(NoteEvent));
  if (drums==NULL) {
    fprintf(stderr,"out of memory\n");
    exit(-1);
  }
}

void clearAllDrums() {
  if (drums!=NULL) free(drums);
  drums=NULL;
  drumsDuration=0;
  drumsSize=0;
  stopallsound();
}

void addDrum(int note,int time) {
 if (drumsSize<=drumsDuration) increaseDrumsSize();
  drums[drumsDuration].note=note;
  drums[drumsDuration].duration=duration;
  drums[drumsDuration].time=time;
  drumsDuration++;
}

void deleteLastDrum() {
  if (drumsDuration>0) drumsDuration--;
  stopallsound();
}
/*
void clearAllDrums() {
  drumsDuration=0;
}
*/
void copyDrum(int src,int dest) {
  memcpy(&drums[dest],&drums[src],sizeof(NoteEvent));
}

void removeDrumAt(int index) {
  int i;
  printf("removing event at index %d from %d events\n",index,drumsDuration);
  for (i=index;i<drumsDuration-1;i++) copyDrum(i+1,i);
  drumsDuration--;
  stopallsound();
}

void removeDrumType(int id) {
  int i;
  printf("removing drum type %d from sequence with %i events\n",id,drumsDuration);
  for (i=0;i<drumsDuration;i++) {
    while (i<drumsDuration && drums[i].note==id) removeDrumAt(i);
  }
}

void startdrum(int id,int pressure) {
  sendmusicevent(SND_SEQ_EVENT_NOTEON,9,id,pressure);
}

void stopdrum(int id,int pressure) {
  sendmusicevent(SND_SEQ_EVENT_NOTEOFF,9,id,pressure);
}

void drumEvents(int bt, int nt) {
  int i;
  for (i=0;i<drumsDuration;i++) {
    if (drums[i].time>=bt && drums[i].time<nt)
      startdrum(drums[i].note,127);
    int endpos=(drums[i].time+drums[i].duration*10000)%4000000;
    if (endpos>=bt && endpos<nt)
      stopdrum(drums[i].note,127);
    if (bt>nt) //we have just looped
    {
      if (drums[i].time<nt) startdrum(drums[i].note,127);
//      if (drums[i].time+drums[i].duration*10000>=pt) stopdrum(drums[i].note,127);
//      if (drums[i].time>pt-drums[i].duration*10000) stopdrum(drums[i].note,127);
    }
  }
  if (metronomeActive) {
    if (bt>nt) //we have just looped
      startdrum(76,127);
    int metronome_end=12800;
    if (metronome_end>=bt && metronome_end<nt) stopdrum(76,127);
    int metronome_start;
    for (metronome_start=1000000;metronome_start<4000000;metronome_start+=1000000) {   
      if (metronome_start>=bt && metronome_start<nt) startdrum(76,127);
      metronome_end=12800+metronome_start;
      if (metronome_end>=bt && metronome_end<nt) stopdrum(76,127);
    }
    for (metronome_start=500000;metronome_start<4000000;metronome_start+=1000000) {   
      if (metronome_start>=bt && metronome_start<nt) startdrum(77,127);
      metronome_end=12800+metronome_start;
      if (metronome_end>=bt && metronome_end<nt) stopdrum(77,127);
    } 
  }
}

void playSequence() {
  int i;
  snd_seq_event_t ev;
  snd_seq_ev_clear(&ev);
  ev.queue = queue;
  ev.source.port = 0;
  ev.flags = SND_SEQ_TIME_STAMP_TICK;
  if (snd_seq_start_queue(seq, queue, NULL)<0) {fprintf(stderr,"start queue failed\n");exit(-1);}
  ev.type=SND_SEQ_EVENT_TEMPO;
  snd_seq_ev_set_fixed(&ev);
  ev.time.tick = 0;
  ev.dest.client = SND_SEQ_CLIENT_SYSTEM;
  ev.dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
  ev.data.queue.queue = queue;
  ev.data.queue.param.value = 100000; //set tempo to 1000000
  if (snd_seq_event_output(seq, &ev)<0) {fprintf(stderr,"set tempo failed\n");exit(-1);}
  snd_seq_ev_clear(&ev);
  ev.queue = queue;
  ev.source.port = 0;
  ev.type=SND_SEQ_EVENT_PGMCHANGE;
  ev.dest = outport;
  ev.time.tick = 0;
  ev.data.control.channel = 9;
  ev.data.control.param = 0;
  ev.data.control.value = 1;
  if (snd_seq_event_output(seq, &ev)<0) {fprintf(stderr,"set instrument failed\n");exit(-1);}
  for (i=0;i<drumsDuration;i++) {
	  printf("%d)play %d at %d\n",i,drums[i].note,drums[i].time);
    	  ev.type=SND_SEQ_EVENT_NOTEON;
	  ev.dest = outport;
	  ev.time.tick = drums[i].time/1000; 
	  snd_seq_ev_set_fixed(&ev);
	  ev.data.note.channel = 9;
	  ev.data.note.note = drums[i].note;
	  ev.data.note.velocity = 127;
	  if (snd_seq_event_output(seq, &ev)<0) {fprintf(stderr,"note on failed \n");exit(-1);}
	  ev.type=SND_SEQ_EVENT_NOTEOFF;
	  ev.time.tick = drums[i].time/1000+duration; // a little delay 
	  ev.dest = outport;
	  snd_seq_ev_set_fixed(&ev);
	  ev.data.note.channel = 9;
	  ev.data.note.note = drums[i].note;;
	  ev.data.note.velocity = 127;
	  if (snd_seq_event_output(seq, &ev)<0) {fprintf(stderr,"note off failed\n");exit(-1);}
  }
  if (snd_seq_drain_output(seq)<0) {fprintf(stderr,"failed to drain sequencer output\n");exit(-1);}
  //if (snd_seq_stop_queue(seq,queue,NULL)<0) {fprintf(stderr,"queue stop failed\n");exit(-1);}
}

void showDrumCursor(int color) {
  uicolor(color);
  uiline(drumCursor/10000,100,drumCursor/10000,600);
}

void showSequence() {
  int i;
  showDrumCursor(editDrumsMode==0?0xff0000:0x0000ff);
  uicolor(0xffffff);
  for (i=0;i<drumsDuration;i++) {
    if (i==drumsDuration-1) uicolor(0xff0000);
    //uicircle(drums[i].time/10000,110+drums[i].note,3);
    uirect(drums[i].time/10000,110+3*drums[i].note,
      2+drums[i].time/10000,112+3*drums[i].note);
  }
}

long long int nowtime() {
struct timeval t;
gettimeofday(&t,NULL);
long long int r;
r=t.tv_sec*1000000+t.tv_usec;
return r;
}

void clearDrumScreen() {
  uicolor(0x000000);
  uirect(0,100,400,600);
  uicolor(0xffff7f);
  int i;
  for (i=0;i<=400;i+=100) uiline(i,105,i,600);
  uicolor(0x7f7f00);
  for (i=50;i<=400;i+=100) uiline(i,105,i,600);
  uicolor(0xffff00);
  showSequence();
  
}


void drumsSeq(){
  
  
  
  
  uiclear(0x7f7f7f);
  int q=0;
  int bank=0;
  //clearDrums();
  
  void drumsHelp() {
    uicolor(0xffff00);
    uirect(490,0,800,197);
    char report[64];
    sprintf(report,"F9 F10    :change bank:%d",bank);
    uicolor(0);
    uitext(500,11,report);
    sprintf(report,"F12       : delete last type");
    uitext(500,25,report);
    sprintf(report,"<-Backsp  : delete last event");
    uitext(500,39,report);
    sprintf(report,"Del       : clear");
    uitext(500,53,report);
    sprintf(report,"Enter     : return");
    uitext(500,67,report);
    sprintf(report,"keys      : drums");
    uitext(500,95,report);
    sprintf(report,"arrows    : move cursor");
    uitext(500,110,report);
    sprintf(report,"pg up/down: move cursor");
    uitext(500,120,report);
    sprintf(report,"end       : toggle cursor/time insert");
    uitext(500,130,report);
    sprintf(report,"home      : toggle metronome");
    uitext(500,140,report);
  }
  
  
  clearDrumScreen();
  drumsHelp();
  starttime=nowtime();
  int meter=0;
  int lastmeter=0;
  int bt=(int)(nowtime()-starttime)%4000000LL;
  int lastAdded=0;
  
  
  
  while (!q) {
   XEvent event;  
   if (uiKeyEvent())     
   { 
     //uievent();
     event=uilastevent();
     /*
     while (event.type!=KeyPress && event.type!=KeyRelease) {
      uievent();
      event=uilastevent();
     }
     */
    now=(nowtime()-starttime)%4000000LL; 
    printf("event %d %Ld\n",event.xkey.keycode,now);
    int x,y;
    if (event.type==KeyPress) {
    if (event.xkey.keycode==75) bank++;
    if (event.xkey.keycode==76) bank+=3;
    
    bank%=4;
    drumsHelp();
    if (event.xkey.keycode==36) q=1;
    
    uicolor(0xff0000);
    if (event.type==KeyPress) {
    keypos(event.xkey.keycode,&x,&y);
    printf("key: %d %d\n",x,y);
    if (event.xkey.keycode==115) {
      editDrumsMode=!editDrumsMode;
      clearDrumScreen();
    }
    if (event.xkey.keycode==110) {
      metronomeActive=!metronomeActive;
      clearDrumScreen();
    }
    if (event.xkey.keycode==117) {
      //showDrumCursor(0);
      drumCursor+=250000;
      drumCursor%=4000000;
      //showDrumCursor(0xff0000);
      clearDrumScreen();
    }
    if (event.xkey.keycode==112) {
      //showDrumCursor(0);
      drumCursor+=4000000-250000;
      drumCursor%=4000000;
      //showDrumCursor(0xff0000);
      clearDrumScreen();
    }

    if (event.xkey.keycode==114) {
      //showDrumCursor(0);
      drumCursor+=10000;
      drumCursor%=4000000;
      //showDrumCursor(0xff0000);
      clearDrumScreen();
    }
    if (event.xkey.keycode==113) {
      //showDrumCursor(0);
      drumCursor+=4000000-10000;
      drumCursor%=4000000;
      //showDrumCursor(0xff0000);
      clearDrumScreen();
    }
    if (event.xkey.keycode==22) {
      deleteLastDrum();
      clearDrumScreen();
    }
    if (event.xkey.keycode==119) {
      clearAllDrums();
      clearDrumScreen();
    }
    if (event.xkey.keycode==96) {
      removeDrumType(lastAdded);
      clearDrumScreen();
    }
    if (x>0) {
      uifillcircle(10+x*20,10+y*20,5);
      int noteToAdd=(15+40*bank+y*10+x)%128;
      //playNoteEvent((15+40*bank+y*10+x)%128,duration,9,1);
      if (editDrumsMode) addDrum(noteToAdd,now);
      else addDrum(noteToAdd,drumCursor);
      lastAdded=noteToAdd;
      showSequence();
      }
    }
    }
    uicolor(0xffff00);
    if (event.type==KeyRelease) {
    keypos(event.xkey.keycode,&x,&y);
    if (x>0) uifillcircle(10+x*20,10+y*20,5);
    }
   }
   else
   {
     meter=((nowtime()-starttime)/4000000LL);
     int nt=(int)(nowtime()-starttime)%4000000LL;
     drumEvents(bt,nt);
     printf("%4d %10d\r",meter,nt);
     
     int xpos;
     uicolor(0);
     xpos=bt/10000;
     uiline(xpos,100,xpos,105);
     uicolor(0xff00ff);
     xpos=nt/10000;
     uiline(xpos,100,xpos,105);
     uiflush();
     if (meter>lastmeter) {
//       reset_midi();
       clearDrumScreen();
       //showSequence();
       //playSequence();
       lastmeter=meter;
     }
     usleep(1000);
     bt=nt;
   }
  } 
  uiclear(0xffffff);
  stopallsound();
  //reset_midi();
}

void copyPercussionSequence(PercussionSequence *dest,PercussionSequence *src) {
  dest->drumsSize=src->drumsSize;
  dest->drumsDuration=src->drumsDuration;
  if (dest->drumsSize==0) {
    dest->drums=NULL;
    return;
  }
  dest->drums=malloc(dest->drumsSize*sizeof(NoteEvent));
  if (dest->drums==NULL) memoryError();
  memcpy(dest->drums,src->drums,dest->drumsSize*sizeof(NoteEvent));
}

void freePerussionSequenceData(PercussionSequence *p) {
  if (p->drums==NULL) return;
  free(p->drums);
}


int drumExists(PercussionSequence *p,int note,int time) {
  int i;
  for (i=0;i<p->drumsDuration;i++) if (p->drums[i].time==time && p->drums[i].note==note) return 1;
  return 0;
}

int sameSequences(PercussionSequence *p1,PercussionSequence *p2) {
  if (p1==NULL && p2==NULL) return 1;
  if (p1==NULL || p2==NULL) return 0;
  int i;
  for (i=0;i<p1->drumsDuration;i++) if (!drumExists(p2,p1->drums[i].note,p1->drums[i].time)) return 0;
  for (i=0;i<p2->drumsDuration;i++) if (!drumExists(p1,p2->drums[i].note,p2->drums[i].time)) return 0;
  return 1;
}

int inbankSequence(PercussionSequence *p) {
  int i;
  for (i=0;i<128;i++) if (percSeq[i].drums!=NULL) if (sameSequences(&(percSeq[i]),p)) return 1+i;
  return 0;
}
//end of #include "drumsseq.c"
//#include "chordsseq.c"
long long int starttime;
long long int now;

typedef struct {
  NoteEvent *chords;
  int chordsDuration;
  int chordsSize;
}
ChordNoteSequence;

static ChordNoteSequence chordNoteSeq[128];

void initChordNoteSeq() {
  int i;
  for (i=0;i<128;i++) {
    chordNoteSeq[i].chords=NULL;
    chordNoteSeq[i].chordsDuration=0;
    chordNoteSeq[i].chordsSize=0;
  }
}

void clearChordNoteSeq() { //must have been initialized before this clearAllChords
  int i;
  for (i=0;i<128;i++) {
    if (chordNoteSeq[i].chords!=NULL) free (chordNoteSeq[i].chords);
  }
  initChordNoteSeq();
}

void freeChordNoteSeqId(int id) {
    if (chordNoteSeq[id].chords!=NULL) free (chordNoteSeq[id].chords);
    chordNoteSeq[id].chordsDuration=0;
    chordNoteSeq[id].chordsSize=0;   
    chordNoteSeq[id].chords=NULL;
}

NoteEvent *chords=NULL;
int chordsDuration=0;
int chordsSize=0;
int chordCursor=0;
int editChordsMode=0;


void storeChordNoteSeq(int id) {
  if (chords==NULL) return;
  freeChordNoteSeqId(id);
  chordNoteSeq[id].chordsDuration=chordsDuration;
  chordNoteSeq[id].chordsSize=chordsSize;
  chordNoteSeq[id].chords=realloc(chordNoteSeq[id].chords,chordsSize*sizeof(NoteEvent));
  if (chordNoteSeq[id].chords==NULL) {
    fprintf(stderr,"out of memory\n");
    exit(-1);
  }
  printf("memcpy start\n");
  memcpy(chordNoteSeq[id].chords,chords,chordsSize*sizeof(NoteEvent));
  printf("memcpy done\n");
}

void restoreChordNoteSeq(int id) {
  printf("restoring from mem %p\n",chordNoteSeq[id].chords);
  if (chordNoteSeq[id].chords==NULL) return;
  if (chords!=NULL) free(chords);
  chordsDuration=chordNoteSeq[id].chordsDuration;
  chordsSize=chordNoteSeq[id].chordsSize;
  chords=NULL;
  chords=realloc(chords,chordsSize*sizeof(NoteEvent));
  memcpy(chords,chordNoteSeq[id].chords,chordsSize*sizeof(NoteEvent));
}

int countChordNoteBanks() {
  int count=0;
  int i;
  for (i=0;i<128;i++) {
    if (i<8) printf("%3d/%3d(%p)\t",count,i,chordNoteSeq[i].chords);
    if (chordNoteSeq[i].chords!=NULL) count++;
  }
  return count;
}

void increaseChordsSize() {
  chordsSize+=1024;
  chords=realloc(chords,chordsSize*sizeof(NoteEvent));
  if (chords==NULL) {
    fprintf(stderr,"out of memory\n");
    exit(-1);
  }
}

void clearAllChords() {
  if (chords!=NULL) free(chords);
  chords=NULL;
  chordsDuration=0;
  chordsSize=0;
  stopallsound();
}

void addChord(int note,int time) {
 if (chordsSize<=chordsDuration) increaseChordsSize();
  chords[chordsDuration].note=note;
  chords[chordsDuration].duration=duration;
  chords[chordsDuration].time=time;
  chordsDuration++;
}

void deleteLastChord() {
  if (chordsDuration>0) chordsDuration--;
  stopallsound();
}
/*
void clearAllChords() {
  chordsDuration=0;
}
*/
void copyChord(int src,int dest) {
  memcpy(&chords[dest],&chords[src],sizeof(NoteEvent));
}

void removeChordAt(int index) {
  int i;
  printf("removing event at index %d from %d events\n",index,chordsDuration);
  for (i=index;i<chordsDuration-1;i++) copyChord(i+1,i);
  chordsDuration--;
  stopallsound();
}

void removeChordType(int id) {
  int i;
  printf("removing chord type %d from sequence with %i events\n",id,chordsDuration);
  for (i=0;i<chordsDuration;i++) {
    while (i<chordsDuration && chords[i].note==id) removeChordAt(i);
  }
}

int chordType=0;
int chordExtend=0;

int idToNote(int id) {
  int r=0;
  if (id%3==0) r=(12*(id/3));
  if (id%3==1) {
    if (chordType==0) r=4+(12*(id/3));
      else r=3+(12*(id/3));
  }
  if (id%3==2) {
    if (chordExtend==0) r=7+(12*(id/3));
      else r=10+(12*(id/3));
  }
  if (r>108) r%=12;
  if (r<12) r+=12;
  return r;
}

void startchord(int id,int pressure) {
  sendmusicevent(SND_SEQ_EVENT_NOTEON,0,idToNote(id),pressure);
}

void stopchord(int id,int pressure) {
  sendmusicevent(SND_SEQ_EVENT_NOTEOFF,0,idToNote(id),pressure);
}

void chordEvents(int bt, int nt) {
  int i;
  for (i=0;i<chordsDuration;i++) {
    if (chords[i].time>=bt && chords[i].time<nt)
      startchord(chords[i].note,127);
    int endpos=(chords[i].time+chords[i].duration*10000)%4000000;
    if (endpos>=bt && endpos<nt)
      stopchord(chords[i].note,127);
    if (bt>nt) //we have just looped
    {
      if (chords[i].time<nt) startchord(chords[i].note,127);
//      if (chords[i].time+chords[i].duration*10000>=pt) stopchord(chords[i].note,127);
//      if (chords[i].time>pt-chords[i].duration*10000) stopchord(chords[i].note,127);
    }
  }
  if (metronomeActive) {
    if (bt>nt) //we have just looped
      startdrum(76,127);
    int metronome_end=12800;
    if (metronome_end>=bt && metronome_end<nt) stopdrum(76,127);
    int metronome_start;
    for (metronome_start=1000000;metronome_start<4000000;metronome_start+=1000000) {   
      if (metronome_start>=bt && metronome_start<nt) startdrum(76,127);
      metronome_end=12800+metronome_start;
      if (metronome_end>=bt && metronome_end<nt) stopdrum(76,127);
    }
    for (metronome_start=500000;metronome_start<4000000;metronome_start+=1000000) {   
      if (metronome_start>=bt && metronome_start<nt) startdrum(77,127);
      metronome_end=12800+metronome_start;
      if (metronome_end>=bt && metronome_end<nt) stopdrum(77,127);
    } 
  }
}
void showChordCursor(int color) {
  uicolor(color);
  uiline(chordCursor/10000,100,chordCursor/10000,600);
}

void showChordSequence() {
  int i;
  showChordCursor(editChordsMode==0?0xff0000:0x0000ff);
  
  for (i=0;i<chordsDuration;i++) {
    int id=chords[i].note%3;
    
    
    if (i==chordsDuration-1) {
      uicolor(0xff0000);
      //uirect(chords[i].time/10000-1,595-3*chords[i].note,2+chords[i].time/10000+1,600-3*chords[i].note);
    } else uicolor(0);
    uirect(chords[i].time/10000,595-3*chords[i].note,2+chords[i].time/10000,600-3*chords[i].note);
    //uicircle(chords[i].time/10000,110+chords[i].note,3);
    if (id==0) uicolor(0xffffff); else if (id==1) uicolor(0xffff00);
    else if (id==2) uicolor(0x00ffff);
    uirect(chords[i].time/10000,596-3*chords[i].note,
      2+chords[i].time/10000,599-3*chords[i].note);
  }
}

void clearChordScreen() {
  uicolor(0x000000);
  uirect(0,100,400,600);
  uicolor(0x3f3f3f);
  int i;
  for (i=0;i<=400;i+=100) uiline(i,105,i,600);
  uicolor(0x1f1f1f);
  for (i=50;i<=400;i+=100) uiline(i,105,i,600);
  uicolor(0xffff00);
  showChordSequence();
}

void chordHelp() {
    uicolor(0xffff00);
    uirect(490,0,800,197);
    char report[64];
    uicolor(0);
    sprintf(report,"F12       : delete last type");
    uitext(500,25,report);
    sprintf(report,"<-Backsp  : delete last event");
    uitext(500,39,report);
    sprintf(report,"Del       : clear");
    uitext(500,53,report);
    sprintf(report,"Enter     : return");
    uitext(500,67,report);
    sprintf(report,"keys      : chords");
    uitext(500,95,report);
    sprintf(report,"arrows    : move cursor");
    uitext(500,110,report);
    sprintf(report,"pg up/down: move cursor");
    uitext(500,120,report);
    sprintf(report,"end       : toggle cursor/time insert");
    uitext(500,130,report);
    sprintf(report,"home      : toggle metronome");
    uitext(500,140,report);
}

void chordsSeq(){
  uiclear(0x7f7f7f);
  int q=0;

  //clearChords();
  clearChordScreen();
  chordHelp();
  starttime=nowtime();
  int meter=0;
  int lastmeter=0;
  int bt=(int)(nowtime()-starttime)%4000000LL;
  int lastAdded=0;
  while (!q) {
   XEvent event;  
   if (uiKeyEvent())     
   { 
     //uievent();
     event=uilastevent();
     /*
     while (event.type!=KeyPress && event.type!=KeyRelease) {
      uievent();
      event=uilastevent();
     }
     */
    now=(nowtime()-starttime)%4000000LL; 
    printf("event %d %Ld\n",event.xkey.keycode,now);
    int x,y;
    if (event.type==KeyPress) {
    chordHelp();
    if (event.xkey.keycode==36) q=1;
    
    uicolor(0xff0000);
    if (event.type==KeyPress) {
    keypos(event.xkey.keycode,&x,&y);
    printf("key: %d %d\n",x,y);
    if (event.xkey.keycode==115) {
      editChordsMode=!editChordsMode;
      clearChordScreen();
    }
    if (event.xkey.keycode==110) {
      metronomeActive=!metronomeActive;
      clearChordScreen();
    }
    if (event.xkey.keycode==117) {
      //showChordCursor(0);
      chordCursor+=250000;
      chordCursor%=4000000;
      //showChordCursor(0xff0000);
      clearChordScreen();
    }
    if (event.xkey.keycode==112) {
      //showChordCursor(0);
      chordCursor+=4000000-250000;
      chordCursor%=4000000;
      //showChordCursor(0xff0000);
      clearChordScreen();
    }

    if (event.xkey.keycode==114) {
      //showChordCursor(0);
      chordCursor+=10000;
      chordCursor%=4000000;
      //showChordCursor(0xff0000);
      clearChordScreen();
    }
    if (event.xkey.keycode==113) {
      //showChordCursor(0);
      chordCursor+=4000000-10000;
      chordCursor%=4000000;
      //showChordCursor(0xff0000);
      clearChordScreen();
    }
    if (event.xkey.keycode==22) {
      deleteLastChord();
      clearChordScreen();
    }
    if (event.xkey.keycode==119) {
      clearAllChords();
      clearChordScreen();
    }
    if (event.xkey.keycode==96) {
      removeChordType(lastAdded);
      clearChordScreen();
    }
    if (x>0 && y<=4) {
      uifillcircle(10+x*20,10+y*20,5);
      int noteToAdd=((4-y)*6+x-1)%128;
      //int noteToAdd=((y-1)*10+x)%128;
      //int noteToAdd=x%128;
      printf("adding %d\n",noteToAdd);
      //playNoteEvent((15+40*bank+y*10+x)%128,duration,9,1);
      if (editChordsMode) addChord(noteToAdd,now);
      else addChord(noteToAdd,chordCursor);
      lastAdded=noteToAdd;
      showChordSequence();
      }
    }
    uicolor(0xffff00);
    }

    if (event.type==KeyRelease) {
    keypos(event.xkey.keycode,&x,&y);
    if (x>0) uifillcircle(10+x*20,10+y*20,5);
    }
   }
   else
   {
     meter=((nowtime()-starttime)/4000000LL);
     int nt=(int)(nowtime()-starttime)%4000000LL;
     chordEvents(bt,nt);
     printf("%4d %10d\r",meter,nt);
     
     int xpos;
     uicolor(0);
     xpos=bt/10000;
     uiline(xpos,100,xpos,105);
     uicolor(0xff00ff);
     xpos=nt/10000;
     uiline(xpos,100,xpos,105);
     uiflush();
     if (meter>lastmeter) {
//       reset_midi();
       clearChordScreen();
       //showChordSequence();
       lastmeter=meter;
     }
     usleep(1000);
     bt=nt;
   }
  } 
  uiclear(0xffffff);
  stopallsound();
  //reset_midi();
}
//end of #include "chordsseq.c"


void increaseMidiSize() {
  midiSize+=1024;
  midiList=realloc(midiList,midiSize*sizeof(MidiEvent));
  if (midiList==NULL) {
    fprintf(stderr,"out of memory\n");
    exit(-1);
  }
}

void addMidiEvent(int time,int channel,int type,int note) {
  if (midiSize<=midiDuration) increaseMidiSize();
  midiList[midiDuration].note=note;
  midiList[midiDuration].type=type;
  midiList[midiDuration].channel=channel;
  midiList[midiDuration].time=time;
  midiDuration++;
}

void clearMidiList() {
 if (midiList!=NULL) free(midiList);
 midiList=NULL;
 midiDuration=0;
 midiSize=0;
}

void reportMidiList() {
  int i;
  for (i=0;i<midiDuration;i++) {
    printf("event %d: time %d channel %d type %d note %d\n",i,
	   midiList[i].time,
	   midiList[i].channel,
	   midiList[i].type,
	   midiList[i].note);
  }
}

void increaseSongSize() {
  songSize+=1024;
  song=realloc(song,songSize*sizeof(NoteEvent));
  if (song==NULL) {
    fprintf(stderr,"out of memory\n");
    exit(-1);
  }
}

void increaseAccompanSize() {
  accompanSize+=1024;
  accompan=realloc(accompan,accompanSize*sizeof(ControlEvent));
  if (accompan==NULL) {
    fprintf(stderr,"out of memory\n");
    exit(-1);
  }
}

void setLyrics(char *text) {
  if (lyrics!=NULL) free(lyrics);
  lyricsSize=strlen(text);
  if (lyricsSize==0) {
    lyrics=NULL;
    return;
  }
  lyrics=malloc(lyricsSize);
  if (lyrics==NULL) {
    fprintf(stderr,"out of memory\n");
    exit(-1);
  }
  memcpy(lyrics,text,lyricsSize);
}

void addSongNote(int note) {
  if (songSize<=songDuration) increaseSongSize();
  song[songDuration].note=note;
  song[songDuration].duration=duration;
  int bar,q,tick;
  bar=currenttime.bar;q=currenttime.q;tick=currenttime.tick;
  int curtime=tick+q*128+bar*512;
  song[songDuration].time=curtime;
  songDuration++;
}

void addAccompan(enum ControlEventType type,int value) {
  if (accompanSize<=accompanDuration) increaseAccompanSize();
  accompan[accompanDuration].type=type;
  accompan[accompanDuration].value=value;
  int bar,q,tick;
  bar=currenttime.bar;q=currenttime.q;tick=currenttime.tick;
  int curtime=tick+q*128+bar*512;
  accompan[accompanDuration].time=curtime;
  accompanDuration++;
}

void copyNote(int src,int dest) {
memcpy(&song[dest],&song[src],sizeof(NoteEvent));
}

void removeNoteAt(int index) {
  int i;
  for (i=index;i<songDuration-1;i++) copyNote(i+1,i);
  songDuration--;
  if (songDuration<0) songDuration=0;
}

void deleteNotesHere(int pullback) {
  int i;
  int cursortime=currenttime.bar*512+currenttime.q*128+currenttime.tick;
  for (i=0;i<songDuration;i++) {
    int cnote=song[i].note;
    int ctime=song[i].time;
    int cduration=song[i].duration;
    while (ctime>=cursortime && ctime<cursortime+duration && i<songDuration) {
      removeNoteAt(i);
      cnote=song[i].note;
      ctime=song[i].time;
      cduration=song[i].duration;
    }
  }
  if (pullback) for (i=0;i<songDuration;i++) {
    int ctime=song[i].time;
    if (ctime>=cursortime+duration) song[i].time-=duration;
  }
}

void copyEvent(int src,int dest) {
  memcpy(&accompan[dest],&accompan[src],sizeof(ControlEvent));
}

void removeEventAt(int index) {
  int i;
  for (i=index;i<accompanDuration-1;i++) copyEvent(i+1,i);
  accompanDuration--;
  if (accompanDuration<0) accompanDuration=0;
}

void pushSongOn() {
  int i;
  int cursortime=currenttime.bar*512+currenttime.q*128+currenttime.tick;
  for (i=0;i<songDuration;i++) {
    int ctime=song[i].time;
    if (ctime>=cursortime) song[i].time+=duration;
  }
  for (i=0;i<accompanDuration;i++) {
    int ctime=accompan[i].time;
    if (ctime>=cursortime) accompan[i].time+=duration;
  }
}

void deleteEventsHere(int pullback) {
  int i;
  int cursortime=currenttime.bar*512+currenttime.q*128+currenttime.tick;
  for (i=0;i<accompanDuration;i++) {
    int ctime=accompan[i].time;
    while (ctime>=cursortime && ctime<cursortime+duration && i<accompanDuration) {
      removeEventAt(i);
      ctime=accompan[i].time;
    }
  }
  if (pullback) for (i=0;i<accompanDuration;i++) {
    int ctime=accompan[i].time;
    if (ctime>=cursortime+duration) accompan[i].time-=duration;
  }
}

int sendmusicevent(int type,int channel,int note,int velocity) {
  int r;
  int port=0;
  //printf("sending event %d params:%d %d %d\n",type,channel,note,velocity);
  snd_seq_event_t ev;
  snd_seq_ev_clear(&ev);
  snd_seq_ev_set_source(&ev, port);
  snd_seq_ev_set_subs(&ev);
  snd_seq_ev_set_direct(&ev);
  ev.type=type;ev.dest=outport;ev.data.note.channel = channel;
  ev.data.note.note = note;ev.data.note.velocity = velocity;
  r=snd_seq_event_output(seq, &ev);
  if (r<0) {fprintf(stderr,"error sending event\n");return r;}
  r=snd_seq_drain_output(seq);
  if (r<0) {fprintf(stderr,"error draining output\n");return r;}
  return r;
}

int sendcontrolevent(int type,int channel,int param,int value) {
  int r;
  int port=0;
  snd_seq_event_t ev;
  snd_seq_ev_clear(&ev);
  snd_seq_ev_set_source(&ev, port);
  snd_seq_ev_set_subs(&ev);
  snd_seq_ev_set_direct(&ev);
  ev.type=type;
  ev.dest = outport;
  ev.data.control.channel = channel;
  ev.data.control.param = param;ev.data.control.value = value;
  r=snd_seq_event_output(seq, &ev);
  if (r<0) {fprintf(stderr,"error sending event\n");return r;}
  r=snd_seq_drain_output(seq);
  if (r<0) {fprintf(stderr,"error draining output\n");return r;}
  return r;  
}

void stopallsound() {
  sendcontrolevent(SND_SEQ_EVENT_CONTROLLER,0,0x7b,0);
}

void sendnote(int note,int pressure) {
  sendmusicevent(SND_SEQ_EVENT_NOTEON,0,note,pressure);
  usleep(100000);
  sendmusicevent(SND_SEQ_EVENT_NOTEOFF,0,note,pressure);
}

void startnote(int note,int pressure) {
  sendmusicevent(SND_SEQ_EVENT_NOTEON,0,note,pressure);
}

void stopnote(int note,int pressure) {
  sendmusicevent(SND_SEQ_EVENT_NOTEOFF,0,note,pressure);
}

void sendkeypress(int note,int pressure) {
  //aftertouch
  sendmusicevent(SND_SEQ_EVENT_KEYPRESS,0,note,pressure);
}

void set_instrument(int instrument) {
  sendcontrolevent(SND_SEQ_EVENT_PGMCHANGE,0,0,instrument);
}

void playnote(int note,int duration) {
  sendmusicevent(SND_SEQ_EVENT_NOTEON,0,note,127);
  usleep(duration*1000);
  sendmusicevent(SND_SEQ_EVENT_NOTEOFF,0,note,127);
}

void addQueueEvent(int time,int channel,int type, int value,int param) {
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	ev.queue = queue;
	ev.source.port = 0;
	ev.flags = SND_SEQ_TIME_STAMP_TICK;
	ev.dest = outport;
	ev.time.tick = time;
	snd_seq_ev_set_fixed(&ev);
	ev.type=type;
	ev.data.note.channel = channel;
	ev.data.note.note = value;
	ev.data.note.velocity = param;
	if (snd_seq_event_output(seq, &ev)<0) {fprintf(stderr,"event send failed \n");}
}

void initTempo(snd_seq_t *handle, int queue)
{
        snd_seq_queue_tempo_t *tempo;
        snd_seq_queue_tempo_alloca(&tempo);
        snd_seq_queue_tempo_set_tempo(tempo, 1000000); 
        snd_seq_queue_tempo_set_ppq(tempo, 128); // 128 PPQ
        snd_seq_set_queue_tempo(handle, queue, tempo);
}

void setTempo(int tempo) {
  	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	ev.queue = queue;
	ev.source.port = 0;
	ev.flags = SND_SEQ_TIME_STAMP_TICK;
	  ev.type=SND_SEQ_EVENT_TEMPO;
          snd_seq_ev_set_fixed(&ev);
	  ev.time.tick = 0;
	  ev.dest.client = SND_SEQ_CLIENT_SYSTEM;
	  ev.dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
	  ev.data.queue.queue = queue;
	  ev.data.queue.param.value = tempo; //set tempo to 100000
	  if (snd_seq_event_output(seq, &ev)<0) {fprintf(stderr,"set tempo failed\n");exit(-1);}
}

static void set_instrument_old(int instrument) {
  	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	ev.queue = queue;
	ev.source.port = 0;

	  ev.type=SND_SEQ_EVENT_PGMCHANGE;
	  ev.dest = outport;
	  ev.time.tick = 0;
	  ev.data.control.channel = 0;
	  ev.data.control.param = 0;
	  ev.data.control.value = instrument;
	  if (snd_seq_event_output(seq, &ev)<0) {fprintf(stderr,"set instrument failed\n");exit(-1);}
}

int last_port() {
  int r=-1;
  if(snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0)<0) {fprintf(stderr,"open failed\n");exit(-1);}
  snd_seq_client_info_t *cinfo;
  snd_seq_port_info_t *pinfo;
  snd_seq_client_info_alloca(&cinfo);
  snd_seq_port_info_alloca(&pinfo);
  snd_seq_client_info_set_client(cinfo, -1);
  while (snd_seq_query_next_client(seq, cinfo) >= 0) {
    int client = snd_seq_client_info_get_client(cinfo);
    printf ("client: %d\n",client);
    snd_seq_port_info_set_client(pinfo, client);
    snd_seq_port_info_set_port(pinfo, -1);
    //r=client;
    while (snd_seq_query_next_port(seq, pinfo) >= 0) {
      printf ("port:\n");
      if (!(snd_seq_port_info_get_type(pinfo)
	    & SND_SEQ_PORT_TYPE_MIDI_GENERIC))
	      {printf("\nnot generic midi %d\n",client);};
      /* we need both WRITE and SUBS_WRITE */
      if ((snd_seq_port_info_get_capability(pinfo)
	    & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
	  != (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
	      {printf("\n non writable %d %d \n",
		snd_seq_port_info_get_client(pinfo),
	      snd_seq_port_info_get_port(pinfo));} else {
      printf("%3d:%-3d  %-32.32s %s\n",
	      snd_seq_port_info_get_client(pinfo),
	      snd_seq_port_info_get_port(pinfo),
	      snd_seq_client_info_get_name(cinfo),
	      snd_seq_port_info_get_name(pinfo));
	      r=client;
	      }

    }
  }
  snd_seq_close(seq); 
 return r;
}


void init_midi(char *portarg) {
  	if(snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0)<0) {fprintf(stderr,"open failed\n");exit(-1);}
	/* set our name (otherwise it's "Client-xxx") */
	if (snd_seq_set_client_name(seq, "smallpiano")<0) {fprintf(stderr,"set name failed\n");exit(-1);}
	/* find out who we actually are */
	client = snd_seq_client_id(seq);
	if (client<0) {fprintf(stderr,"client get failed\n");exit(-1);}
	printf("client:%d\n",client);
	printf("portarg:%s\n",portarg);
	if (snd_seq_parse_address(seq, &outport, portarg)<0) {fprintf(stderr,"invalid port %s\n",portarg);exit(-1);}
	
	snd_seq_port_info_t *pinfo;
	snd_seq_port_info_alloca(&pinfo);
	/* the first created port is 0 anyway, but let's make sure ... */
	snd_seq_port_info_set_port(pinfo, 0);
	snd_seq_port_info_set_port_specified(pinfo, 1);
	snd_seq_port_info_set_name(pinfo, "smallpiano");
	snd_seq_port_info_set_capability(pinfo, 0); /* sic */
	snd_seq_port_info_set_type(pinfo,
				   SND_SEQ_PORT_TYPE_MIDI_GENERIC |
				   SND_SEQ_PORT_TYPE_APPLICATION);
	if (snd_seq_create_port(seq, pinfo)<0)  {fprintf(stderr,"create port failed\n");exit(-1);}
	queue = snd_seq_alloc_named_queue(seq, "smallpiano");
	if (queue<0) {fprintf(stderr,"open failed\n");exit(-1);}
	
	if(snd_seq_connect_to(seq, 0, outport.client,outport.port)<0)
	{fprintf(stderr,"connect to port %d:%d failed\n",outport.client, outport.port);exit(-1);}
}

void reset_midi() {
snd_seq_close(seq); 
init_midi(portarg);
set_instrument(instrument); 
}

int inrange(int v,int s,int e) {
if (v>=s && v<s+e) return 1+v-s;
return 0;
}

int issharp(int note) {
  int n=note%12;
  int sharps[]={1,3,6,8,10};
  int i;
  for (i=0;i<5;i++) if (n==sharps[i]) return 1;
  return 0;
}

int keypos(int v,int *x,int *y) {
  *x=*y=0;
  int row1=(inrange(v,10,10));
  int row2=(inrange(v,24,10));
  int row3=(inrange(v,38,10));
  int row4=(inrange(v,52,10));
  if (row1) {*y=1;*x=row1;}
  if (row2) {*y=2;*x=row2;}
  if (row3) {*y=3;*x=row3;}
  if (row4) {*y=4;*x=row4;}
}

int keynote(int x,int y) {
  int bottom[]={0,2,4,5,7,9,11,12,14,16};
  int toprow[]={-1,1,3,-1,6,8,10,-1,13,15};
  if (y==4) return bottom[x-1];
  if (y==3) return toprow[x-1];
  if (y==2 ) return bottom[x-1]+12;
  if (y==1 && toprow[x-1]!=-1 ) return toprow[x-1]+12;
  return -1;
}


void zeroTime(timePos *t) {
  t->bar=0;t->q=0;t->tick=0;
}

void showTime(timePos *t,int x,int y) {
  char report[32];
  sprintf(report,"%3d:%2d:%3d",1+t->bar,1+t->q,t->tick);
  uicolor(0x00ffff);
  uirect(x,y-11,x+70,y+1);
  uicolor(0);
  uitext(x,y,report);
}

void modTime(timePos *t,int ticks) {
  int bar,q,tick;
  bar=t->bar;q=t->q;tick=t->tick;
  int curtime=tick+q*128+bar*512;
  curtime+=ticks;
  if (curtime<0) curtime=0;
  bar=curtime/512;
  q=(curtime/128)%4;
  tick=curtime%128;
  t->bar=bar;t->q=q;t->tick=tick;
}


char *textAnswer(char *question,int x,int y) {
  uiclear(0xffffff);
  uicolor(0);
  uitext(0+x,20+y,question);
  int done=0;
  int pos=0;
  int length=0;
  reply[pos]=0;
  int charw=7;
  while (!done) {
   uicolor(0x00ff00);
   uirect(0+x,20+y,100+x,42+y);
   uicolor(0);
   int i;
   for (i=0;i<length;i++) {
     char onechar[2];
     sprintf(onechar,"%c",reply[i]);
     uitext(i*charw+x,40+y,onechar);
   }
   //uitext(0,40,reply);
   int charpos=pos*charw;
   uiline(charpos+x,40+y,charpos+charw+x,40+y);
   uievent();
   XEvent event=uilastevent();
   if (event.type==KeyPress) {
     char k=uikey();
     printf("key:%c(%d)\n",k,(int)k);
     int keycode=event.xkey.keycode;
     if (keycode==119 || keycode==22) {
       int i;
       if (keycode==22) {
	 pos--;
	 if (pos<0) pos=0;
       }
       for (i=pos;i<length;i++) reply[i]=reply[i+1];
       reply[length]=0;
       length--;
     } else if (keycode==113) {
       pos--;
       if (pos<0) pos=0;
     } else if (keycode==114) {
       pos++;
       if (pos>length) pos=length;
     } else if (keycode!=9) {
       if (k=='\n' || k=='\r') done=1; else {
	 length++;
	 int i;
	 for (i=length;i>pos;i--) reply[i]=reply[i-1];
	 reply[pos]=k;
	 pos++;
       }
     }
   }
  }
  uiclear(0xffffff);
  return reply;
}

void showBanks(ChordNoteSequence *bank) {
  int x,y,i;
  for (y=0;y<8;y++)
    for (x=0;x<16;x++) {
      char report[16];
      i=x*8+y;
      sprintf(report,"%d",i+1);
      void *p=bank[i].chords;
      if (p==NULL) uicolor(0xffff00); else uicolor(0xff0000);
      uirect(x*50,600-75*y-73,x*50+48,600-75*y);
      uicolor(0);
      if (p==NULL) uitext(x*50,600-75*y-30,"emtpy"); 
      else uitext(x*50,600-75*y-30,"used");
      uitext(x*50,588-75*y,report);
    }
}

int bankAnswer(char *question,ChordNoteSequence *bank) {
  int ans=-1;
  uiclear(0xffffff);
  uicolor(0);
  int q=0;
  int bankid=0;
  while (!q) {
  uievent();
  showBanks(bank);
  uitext(0,20,question);
  uicolor(0xff0000);
  uiline(bankid*50,0,bankid*50,600);
  uiline(bankid*50+47,0,bankid*50+47,600);
  XEvent event=uilastevent();
  if (event.type==ButtonPress) {
    int x=event.xbutton.x/50;
    int y=(600-event.xbutton.y)/75;
    int i=x*8+y;
    printf("click %d\n",i);
    sprintf(reply,"%d",i+1);
    ans=i;
    q=1;
    }
  if (event.type==KeyPress){
    printf("%d\n",event.xkey.keycode);
    if (event.xkey.keycode==19) bankid++;
    if (event.xkey.keycode==18) bankid+=15;
	 bankid%=16;
    if (event.xkey.keycode>=10 && event.xkey.keycode<=17) 
    {
      ans=bankid*8+event.xkey.keycode-10;
      sprintf(reply,"%d",ans);
      q=1;
    }
    if (event.xkey.keycode==36) q=1; 
    }
  }
 uiclear(0xffffff);
 return ans;
}

int karaokeEnd() {
  int karstart=0;
  int i;
  for (i=0;i<accompanDuration;i++) {
    enum ControlEventType type=accompan[i].type;
    int value=accompan[i].value;
    if (type==TextEvent) karstart+=value;
  }
  return karstart;
}


int karaokeAnswer() {
  if (lyrics==NULL) return 0;
  int startpos=karaokeEnd();
  int length=1;
  int charsize=7;
  int i;
  int q=0;
  int phraselimit=40;
  while (!q) {
    uiclear(0xffffff);
    uicolor(0);
    uiline(300,0,300,600);
    uiline(300+length*charsize,0,300+length*charsize,600);
    char report[2];
    for (i=startpos;i>=0 && (startpos-i)*charsize<200;i--) {
      sprintf(report,"%c",lyrics[i]);
      uitext(300-(startpos-i)*charsize,100,report);
    }
    for (i=startpos;i<lyricsSize && (i-startpos)*charsize<400;i++) {
      sprintf(report,"%c",lyrics[i]);
      if (i<startpos+length) uicolor(0xff0000); else uicolor(0);
      uitext(300-(startpos-i)*charsize,100,report);
    }
    uievent();
    XEvent event=uilastevent();
    if (event.type==KeyPress) {
      if (event.xkey.keycode==36) {q=1;}
      if (event.xkey.keycode==114) {length++;}
      if (event.xkey.keycode==113) {length--;}
      if (length>phraselimit) length=phraselimit;
      if (length+startpos>lyricsSize) length=lyricsSize-startpos;
      if (length<0) length=0;
    }
  }
  sprintf(reply,"%d",length);
}

void drumStoreMenu() {
  char infotext[64];
  int bank;
  sprintf(infotext,"select percussion bank to store to:(0-127)");
  //textAnswer(infotext,150,200);
  bankAnswer(infotext,(ChordNoteSequence *)percSeq);
  sscanf(reply,"%d",&bank);
  printf("storing bank %d\n",bank);
  storePercSeq(bank);
  printf("active banks:%d\n",countPercBanks());
}

void drumRestoreMenu() {
  char infotext[64];
  int bank;
  sprintf(infotext,"enter percussion bank to read from:(0-127)");
  //textAnswer(infotext,150,200);
  bankAnswer(infotext,(ChordNoteSequence *)percSeq);
  sscanf(reply,"%d",&bank);
  printf("restoring bank %d\n",bank);
  restorePercSeq(bank);
  printf("active banks:%d\n",countPercBanks());
}

void chordStoreMenu() {
  char infotext[64];
  int bank;
  sprintf(infotext,"enter accopaniament bank to store to:(0-127)");
  //textAnswer(infotext,150,200);
  bankAnswer(infotext,chordNoteSeq);
  sscanf(reply,"%d",&bank);
  printf("storing chord bank %d\n",bank);
  storeChordNoteSeq(bank);
  printf("active banks:%d\n",countChordNoteBanks());
}

void chordRestoreMenu() {
  char infotext[64];
  int bank;
  sprintf(infotext,"enter accopaniament bank to read from:(0-127)");
  //textAnswer(infotext,150,200);
  bankAnswer(infotext,chordNoteSeq);
  sscanf(reply,"%d",&bank);
  printf("restoring chord bank %d\n",bank);
  restoreChordNoteSeq(bank);
  printf("active banks:%d\n",countChordNoteBanks());
}

int booleanAnswer() {
  int i,q,v=-1;
  SetKey(&menukeys[0],300,50,100,20,4,19,"0: OFF");
  SetKey(&menukeys[1],300,80,100,20,4,10,"1: ON");
  q=0;
    while (!q) {
      uievent();
      XEvent event=uilastevent();
      uiclear(0xffff00);    
      for (i=0;i<2;i++) {CheckKey(&menukeys[i],event); ShowKey(&menukeys[i]);uiflush();}
      if (menukeys[0].pressed) {v=0;q=1;}
      if (menukeys[1].pressed) {v=1;q=1;}
    }
    return v;
}

int chordMenu() {
  int i,q,r,base;
  int minor,seventh,sharp;
  SetKey(&menukeys[0],300,50,100,20,4,24,"Q: C");
  SetKey(&menukeys[1],300,80,100,20,4,25,"W: D");
  SetKey(&menukeys[2],300,110,100,20,4,26,"E: E");
  SetKey(&menukeys[3],300,140,100,20,4,27,"R: F");
  SetKey(&menukeys[4],300,170,100,20,4,28,"T: G");
  SetKey(&menukeys[5],300,200,100,20,4,29,"Y: A");
  SetKey(&menukeys[6],300,230,100,20,4,30,"U: B");
  SetKey(&menukeys[7],500,50,100,20,4,58,"M:minor");
  SetKey(&menukeys[8],500,80,100,20,4,16,"7:seventh");
  SetKey(&menukeys[9],500,110,100,20,4,19,"0:#");
  SetKey(&menukeys[10],500,140,100,20,4,36,"enter: done");
  q=0;base=0;
  minor=0;seventh=0;sharp=0;
  while (!q) {
    char report[16];
    uievent();
    XEvent event=uilastevent();
    uiclear(0xffffff);    
    for (i=0;i<11;i++) {CheckKey(&menukeys[i],event); ShowKey(&menukeys[i]);uiflush();}
    int values[]={0,2,4,5,7,9,11};
    for (i=0;i<7;i++) if (menukeys[i].pressed) base=values[i];
    if (menukeys[7].pressed) minor=!minor;
    if (menukeys[8].pressed) seventh=!seventh;
    if (menukeys[9].pressed) sharp=!sharp;
    if (menukeys[10].pressed) {q=1;}
    r=base+sharp;
    sprintf(report,"%c%c%c%c",notedescr[r*2],notedescr[r*2+1],
      minor?'m':'-',seventh?'7':'-');
    uitext(250,20,report);
  }
  uiclear(0xffffff);
  for (i=0;i<10;i++) menukeys[i].pressed=0;
  int final=r;
  if (minor) final=final | 0x10;
  if (seventh) final=final | 0x20;
  return final;
}

void InsertControlMenu() {
  
  int i,q;
  enum ControlEventType type;
  SetKey(&menukeys[0],650,420,200,20,4,10,"1 SelectPercussion");
  SetKey(&menukeys[1],650,450,200,20,4,11,"2 SelectAccopaniament");
  SetKey(&menukeys[2],650,480,200,20,4,12,"3 PercussionFlag");
  SetKey(&menukeys[3],650,510,200,20,4,13,"4 SelectChord");
  SetKey(&menukeys[4],650,540,200,20,4,14,"5 AccopaniamentFlag");
  SetKey(&menukeys[5],650,570,200,20,4,15,"6 Karaoke");
  
  q=0;
  while (!q) {
    char report[16];
    uievent();
    XEvent event=uilastevent();
    uicolor(0);
    uirect(500,400,800,600);
    uicolor(0xffffff);
    uirect(502,402,798,598);
    
    for (i=0;i<6;i++) {CheckKey(&menukeys[i],event); ShowKey(&menukeys[i]);uiflush();}
    if (menukeys[0].pressed) {type=SelectPercussion;q=1;}
    if (menukeys[1].pressed) {type=SelectAccopaniament;q=1;}
    if (menukeys[2].pressed) {type=PercussionFlag;q=1;}
    if (menukeys[3].pressed) {type=SelectChord;q=1;}
    if (menukeys[4].pressed) {type=AccopaniamentFlag;q=1;}
    if (menukeys[5].pressed) {type=TextEvent;q=1;}
  }
  
  char infotext[128];
  int value;
  if (type==SelectChord) {
    value=chordMenu();
    printf("menu gave %d\n",value);
  } else if (type==SelectAccopaniament) {
    sprintf(infotext,"enter accopaniament bank to select:(0-127)");
    bankAnswer(infotext,chordNoteSeq);
    sscanf(reply,"%d",&value);
  } else if (type==SelectPercussion) {
    sprintf(infotext,"enter percussion bank to select:(0-127)");
    bankAnswer(infotext,(ChordNoteSequence *)percSeq);
    sscanf(reply,"%d",&value);
  } else if (type==TextEvent) {
    karaokeAnswer();
    sscanf(reply,"%d",&value);
  } else if (type==PercussionFlag || type==AccopaniamentFlag) {
    value=booleanAnswer();
  }
  else {
  sprintf(infotext,"value:");
  textAnswer(infotext,150,200);
  sscanf(reply,"%d",&value);
  }
  printf("adding %d %d\n",type,value);
  addAccompan(type,value);
  printf("added accompaniament\n");
  uiclear(0xffffff);
}



void InsertControlMenu_old() {
  char infotext[128];
  int bank;
  sprintf(infotext,"enter type(SelectPercussion:%d)(SelectAccopaniament:%d)(PercussionFlag:%d)(SelectChord:%d)(AccopaniamentFlag:%d)",SelectPercussion,SelectAccopaniament,PercussionFlag,SelectChord,AccopaniamentFlag);
  textAnswer(infotext,150,200);
  //enum ControlEventType type;
  int type;
  int value;
  sscanf(reply,"%d",&type);
  if (type==SelectChord) {
    value=chordMenu();
    printf("menu gave %d\n",value);
  } else {
  sprintf(infotext,"value:");
  textAnswer(infotext,150,200);
  sscanf(reply,"%d",&value);
  }
  printf("adding %d %d\n",type,value);
  addAccompan(type,value);
  printf("added accompaniament\n");
}


void readInstrument() {
  char infotext[64];
  sprintf(infotext,"enter instrument:(0-128,current:%d)",instrument);
  textAnswer(infotext,150,200);
  sscanf(reply,"%d",&instrument);
}

void readDuration() {
  char infotext[64];
  sprintf(infotext,"enter duration:(1-1024,current:%d)",duration);
  textAnswer(infotext,150,200);
  sscanf(reply,"%d",&duration);
}

void readOctave() {
  char infotext[64];
  sprintf(infotext,"enter octave:(2-4,current:%d)",octave);
  textAnswer(infotext,150,200);
  sscanf(reply,"%d",&octave);
}

void saveSong() {
  printf("active banks before save start:%d - %d\n",countPercBanks(),countChordNoteBanks());
  char infotext[64];
  textAnswer("save song to filename:",150,200);
  FILE *file;
  file=fopen(reply,"wb");
  if (!file) {
    fprintf(stderr,"cant open %s for writing\n",reply);
    return;
  }
  fwrite("SONG",1,4,file);
  fwrite(&songDuration,sizeof(songDuration),1,file);
  fwrite(song,sizeof(NoteEvent),songDuration,file);
  fwrite(&drumsDuration,sizeof(drumsDuration),1,file);
  fwrite(drums,sizeof(NoteEvent),drumsDuration,file);
  int banks=countPercBanks();
  printf("storing %d banks\n",banks);
  fwrite(&banks,sizeof(banks),1,file);
  int i;
  for (i=0;i<128;i++) if (percSeq[i].drums!=NULL) {
    fwrite(&i,sizeof(i),1,file);
    fwrite(&(percSeq[i].drumsDuration),sizeof(drumsDuration),1,file);
    fwrite(percSeq[i].drums,sizeof(NoteEvent),percSeq[i].drumsDuration,file);
  }
  banks=countChordNoteBanks();
  printf("storing %d banks\n",banks);
  fwrite(&banks,sizeof(banks),1,file);
  for (i=0;i<128;i++) if (chordNoteSeq[i].chords!=NULL) {
    fwrite(&i,sizeof(i),1,file);
    fwrite(&(chordNoteSeq[i].chordsDuration),sizeof(chordsDuration),1,file);
    fwrite(chordNoteSeq[i].chords,sizeof(NoteEvent),chordNoteSeq[i].chordsDuration,file);
  }
  fwrite(&accompanDuration,sizeof(accompanDuration),1,file);
  fwrite(accompan,sizeof(ControlEvent),accompanDuration,file);
  fwrite(&lyricsSize,sizeof(lyricsSize),1,file);
  fwrite(lyrics,lyricsSize,1,file);
  fclose(file);
}

void exportSong() {
  if (midiList==NULL) {
    layoutSong();
  }
  if (midiList==NULL) {
    printf("NO SONG DATA\n");
    uiclear(0x00ffff);
    uitext(0,10,"NO MIDI DATA");
    return;
  }
  char infotext[64];
  textAnswer("export song to filename:",150,200);
  FILE *file;
  file=fopen(reply,"wb");
  if (!file) {
    fprintf(stderr,"cant open %s for writing\n",reply);
    return;
  }
  headchunk midiheader;
  memcpy(&midiheader.id,"MThd",4);
  midiheader.size=6;
  midiheader.type=1;
  midiheader.tracks=2;
  midiheader.time_div=128/TEMPOMULTIPLIER; //since we want 128 with tempo 1000000 (1sec) but default tempo is 500000 (0.5sec)
  flipheaddata(&midiheader);
  fwrite(&midiheader,14,1,file);
  //now write the music track
  MidiTrack track;
  zeroTrack(&track);
  int i;
  int timestamp=0;
  // we know midiList is sorted so just parse it...
  for (i=0;i<midiDuration;i++) {
    int ctime=midiList[i].time;
    int channel=midiList[i].channel;
    int type=midiList[i].type;
    int note=midiList[i].note;
    int timediff=ctime-timestamp;
    timestamp=ctime;
    appendTrackValue(&track,timediff);
    if (type==SND_SEQ_EVENT_NOTEON) {
      int cmd=0x90+channel;
      appendTrackByte(&track,cmd);
      appendTrackByte(&track,note);
      appendTrackByte(&track,127);
    }
    if (type==SND_SEQ_EVENT_NOTEOFF) {
      int cmd=0x90+channel;
      appendTrackByte(&track,cmd);
      appendTrackByte(&track,note);
      appendTrackByte(&track,0);
    }
  }
  appendTrackEOF(&track);
  int tracksize=track.size;
  printf("writing music track\n");
  if (fwrite("MTrk",4,1,file)!=1) {fprintf(stderr,"cant write track header\n");exit(1);}
  flipdata((char*)&tracksize,4);
  if (fwrite(&tracksize,4,1,file)!=1) {fprintf(stderr,"cant write track size\n");exit(1);}
  if (fwrite(track.data,track.size,1,file)!=1) {fprintf(stderr,"failed to write track data\n");exit(-1);}
  if (track.data!=NULL) free(track.data);

  //now write the karaoke track
  zeroTrack(&track);
  timestamp=0;
  int karstart=0;
  for (i=0;i<accompanDuration;i++) {
    int atime=accompan[i].time;
    enum ControlEventType type=accompan[i].type;
    int value=accompan[i].value;
    int timediff;
    if (type==TextEvent && karstart+value<=lyricsSize) {
      timediff=atime-timestamp;
      char text[value+1];
      strncpy(text,lyrics+karstart,value);
      text[value]=0;
      addKaraokeText(&track,text,timediff);
      karstart+=value;
      timestamp=atime;
      }
    }
    appendTrackEOF(&track);
  tracksize=track.size;
  printf("writing karaoke track\n");
  if (fwrite("MTrk",4,1,file)!=1) {fprintf(stderr,"cant write track header\n");exit(1);}
  flipdata((char*)&tracksize,4);
  if (fwrite(&tracksize,4,1,file)!=1) {fprintf(stderr,"cant write track size\n");exit(1);}
  if (fwrite(track.data,track.size,1,file)!=1) {fprintf(stderr,"failed to write track data\n");exit(-1);}
  if (track.data!=NULL) free(track.data);
  fclose(file);
}

void loadSong() {
  char infotext[64];
  textAnswer("load song from filename:",150,200);
  FILE *file;
  file=fopen(reply,"rb");
  if (!file) {
    fprintf(stderr,"cant open %s for reading\n",reply);
    return;
  }
  char magic[5];
  fread(magic,1,4,file);
  magic[4]=0;
  printf("magic:%s\n",magic);
  if (strcmp("SONG",magic)!=0) {
    fprintf(stderr,"file is not a SONG file\n");
    fclose(file);
    return;
  }
  fread(&songDuration,sizeof(songDuration),1,file);
  while (songSize<=songDuration) increaseSongSize();
  fread(song,sizeof(NoteEvent),songDuration,file);
  fread(&drumsDuration,sizeof(drumsDuration),1,file);
  while (drumsSize<=drumsDuration) increaseDrumsSize();
  fread(drums,drumsDuration,sizeof(NoteEvent),file);
  int banks,i,id;
  fread(&banks,sizeof(banks),1,file);
  printf("total %d banks to load\n",banks);
  for (i=0;i<banks;i++) {
    fread(&id,sizeof(id),1,file);
    printf("loading sequence %d to id %d\n",i,id);
    freePercSeqId(id);
    fread(&(percSeq[id].drumsDuration),sizeof(drumsDuration),1,file);
    percSeq[id].drumsSize=percSeq[id].drumsDuration;
    percSeq[id].drums=realloc(percSeq[id].drums,percSeq[id].drumsSize*sizeof(NoteEvent));
    if (percSeq[id].drums==NULL) {
      fprintf(stderr,"out of memory\n");
      exit(-1);
    }
    fread(percSeq[id].drums,percSeq[id].drumsDuration,sizeof(NoteEvent),file);
  }
  fread(&banks,sizeof(banks),1,file);
  printf("total %d chord banks to load\n",banks);
  for (i=0;i<banks;i++) {
    fread(&id,sizeof(id),1,file);
    printf("loading sequence %d to id %d\n",i,id);
    freeChordNoteSeqId(id);
    fread(&(chordNoteSeq[id].chordsDuration),sizeof(chordsDuration),1,file);
    chordNoteSeq[id].chordsSize=chordNoteSeq[id].chordsDuration;
    chordNoteSeq[id].chords=realloc(chordNoteSeq[id].chords,chordNoteSeq[id].chordsSize*sizeof(NoteEvent));
    if (chordNoteSeq[id].chords==NULL) {
      fprintf(stderr,"out of memory\n");
      exit(-1);
    }
    fread(chordNoteSeq[id].chords,chordNoteSeq[id].chordsDuration,sizeof(NoteEvent),file);
  }
  fread(&accompanDuration,sizeof(accompanDuration),1,file);
  while (accompanSize<=accompanDuration) increaseAccompanSize();
  fread(accompan,sizeof(ControlEvent),accompanDuration,file);
  fread(&lyricsSize,sizeof(lyricsSize),1,file);
  if (lyricsSize!=0) {
    if (lyrics!=NULL) free(lyrics);
      lyrics=malloc(lyricsSize);
      if (lyrics==NULL) {
	fprintf(stderr,"out of memory\n");
	exit(-1);
      }
      fread(lyrics,lyricsSize,1,file);
  }
  fclose(file);
}

void loadLyrics(char *filename) {
  FILE *file;
  file=fopen(filename,"rb");
  if (!file) return;
  fseek(file,0,SEEK_END);
  int l=ftell(file);
  char *buffer=malloc(l);
  if (!buffer) {
    fprintf(stderr,"out of memory loading lyrics\n");
    fclose(file);
    return;
  }
  rewind(file);
  fread(buffer,l,1,file);
  fclose(file);
  setLyrics(buffer);
  free(buffer);
}

void getDuration() {
  int i,q;
  SetKey(&menukeys[0],300,50,200,20,4,10,"1: full");
  SetKey(&menukeys[1],300,75,200,20,4,11,"2: half");
  SetKey(&menukeys[2],300,100,200,20,4,12,"3: quorter");
  SetKey(&menukeys[3],300,125,200,20,4,13,"4: eighth");
  SetKey(&menukeys[4],300,150,200,20,4,14,"5: sixteenth");
  SetKey(&menukeys[5],300,175,200,20,4,15,"6: 1/32");
  SetKey(&menukeys[6],300,200,200,20,4,16,"7: 1/64");
  uiclear(0xffffff);
  q=0;
  while (!q) {
    uievent();
    XEvent event=uilastevent();
    int iduration=512;
    for (i=0;i<7;i++) {
      uicircle(150,50+i*25,5);
      if (i>0) uiline(155,50+i*25,155,30+i*25);
      if (i>1) uifillcircle(150,50+i*25,5);
      int t;
      for (t=0;t<i-2;t++) uiline(155,30+t*2+i*25,158,32+t*2+i*25);
      uicolor(0xff0000);
      if (iduration==duration) uifillcircle(130,50+i*25,3);
      uicolor(0);
      iduration>>=1;
    }
    for (i=0;i<7;i++) {CheckKey(&menukeys[i],event); ShowKey(&menukeys[i]);uiflush();}
    if (menukeys[0].pressed) {duration=512;q=1;}
    if (menukeys[1].pressed) {duration=256;q=1;}
    if (menukeys[2].pressed) {duration=128;q=1;}
    if (menukeys[3].pressed) {duration=64;q=1;}
    if (menukeys[4].pressed) {duration=32;q=1;}
    if (menukeys[5].pressed) {duration=16;q=1;}
    if (menukeys[6].pressed) {duration=8;q=1;}
  }
  uiclear(0xffffff);
  for (i=0;i<7;i++) menukeys[i].pressed=0;
}


void mainmenu() {
  int i,q;
  
  SetKey(&menukeys[0],300,50,200,20,4,10,"1: set instrument");
  SetKey(&menukeys[1],300,80,200,20,4,11,"2: set duration");
  SetKey(&menukeys[2],300,110,200,20,4,12,"3: duration as note");
  SetKey(&menukeys[3],300,140,200,20,4,13,"4: octave");
  SetKey(&menukeys[4],300,170,200,20,4,14,"5: save song");
  SetKey(&menukeys[5],300,200,200,20,4,15,"6: load song");
  SetKey(&menukeys[6],300,230,200,20,4,16,"7: load lyrics");
  SetKey(&menukeys[7],300,260,200,20,4,17,"8: export to MIDI (after enter)");
  SetKey(&menukeys[8],300,290,200,20,4,18,"9: import drums from MIDI");
  uiclear(0xffffff);
  q=0;
  while (!q) {
    uievent();
    XEvent event=uilastevent();
    for (i=0;i<9;i++) {CheckKey(&menukeys[i],event); ShowKey(&menukeys[i]);uiflush();}
    if (menukeys[0].pressed) {readInstrument();q=1;}
    if (menukeys[1].pressed) {readDuration();q=1;}
    if (menukeys[2].pressed) {printf("Duration selected\n");getDuration();q=1;}
    if (menukeys[3].pressed) {readOctave();q=1;}
    if (menukeys[4].pressed) {saveSong();q=1;}
    if (menukeys[5].pressed) {loadSong();q=1;}
    
    if (menukeys[6].pressed) {
      textAnswer("lyrics file to load",300,300);
      loadLyrics(reply);q=1;
    }
    if (menukeys[7].pressed) {exportSong();q=1;}
    if (menukeys[8].pressed) {importDrums();q=1;}
  }
}

void showSong() {
  int i;
  int cursortime=currenttime.bar*512+currenttime.q*128;
  for (i=0;i<songDuration;i++) {
    int cnote=song[i].note-octave*12;
    int ctime=song[i].time;
    int cduration=song[i].duration;
    //printf("event %d time %d note %d duration %d\n",i,ctime,cnote,cduration);
    uicolor(0);
    int startpos=428+ctime-cursortime;
    int endpos=428+ctime+cduration-cursortime;
    if (startpos<300) startpos=300;
    if (startpos>600) startpos=600;
    if (endpos<300) endpos=300;
    if (endpos>600) endpos=600;
    //uifillcircle(startpos,580-cnote*20,4);
    //uiline(startpos,580-cnote*20,endpos,580-cnote*20);
    uirect(startpos,580-(cnote+12)*10-2,endpos,580-(cnote+12)*10+2);
  }
  char report[32];
  int karstart=0;
  for (i=0;i<accompanDuration;i++) {
    int atime=accompan[i].time;
    enum ControlEventType type=accompan[i].type;
    int value=accompan[i].value;
    int startpos=428+atime-cursortime;
    int location=88;
    
    if (startpos>=300 && startpos<=600-32) {
      if (type==SelectPercussion) {
	sprintf(report,"P:%d",value);
	location=76;
      } else if (type==PercussionFlag) {
	location=64;
	if (value==0) sprintf(report,"P-off");
	else sprintf(report,"P-on");
      } else if (type==SelectAccopaniament) {
	sprintf(report,"A:%d",value);
	location=52;
      } else if (type==AccopaniamentFlag) {
	location=40;
	if (value==0) sprintf(report,"A-off");
	else sprintf(report,"A-on");
      } else if (type==SelectChord) {
	location=28;
	int v=(value & 0x0f)%12;
	char minor=' ';
	char seventh=' ';
	if (value&0x10) minor='m';
	if (value&0x20) seventh='7'; 
	printf("value is %d v is %d minor is %c seventh is %c\n",value,v,minor,seventh);
	sprintf(report,"%c%c%c%c",notedescr[v*2],notedescr[v*2+1],minor,seventh);
      } else if (type==TextEvent) {
	location=14;
	if (lyrics==NULL) sprintf(report,"%d-%d txt",karstart,karstart+value);
	else {
	  int mv=value;
	  if (mv>31) //report size
	    mv=31;
	  if (karstart+mv<=lyricsSize) {
	    strncpy(report,lyrics+karstart,mv);
	    report[mv]=0;
	  }
	  else sprintf(report,"End of text");
	}
      }
      
      uicolor(0xffff00);
      uirect(startpos,location,startpos+32,location+12);
      uicolor(0);
      uitext(startpos,location+11,report);
    }
    if (type==TextEvent) karstart+=value;
  }
}



void playCurrentPhrase() {
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	ev.queue = queue;
	ev.source.port = 0;
	ev.flags = SND_SEQ_TIME_STAMP_TICK;
	initTempo(seq,queue);
	if (snd_seq_start_queue(seq, queue, NULL)<0) {fprintf(stderr,"start queue failed\n");exit(-1);}
	//setTempo(500000);
	//loop to send events
	//int cursortime=currenttime.bar*512+currenttime.q*128+currenttime.tick;
	int cursortime=currenttime.q*128+currenttime.tick;
	int i;
	
	int actualtime=currenttime.tick+currenttime.q*128+currenttime.bar*512;
	int chordFlags=areChordsActive(actualtime);
	printf("FLAGS for time %d:%d\n",actualtime,chordFlags);
	if (chordFlags & 2) for (i=0;i<drumsDuration;i++) {
	  //int ctime=drums[i].time*512/4000000; // the same as
	  int ctime=drums[i].time*TEMPOMULTIPLIER/15625;
	  if (ctime>=cursortime && ctime<cursortime+duration) {
	  addQueueEvent(ctime-cursortime,9,SND_SEQ_EVENT_NOTEON,drums[i].note,127); //drum start
	  addQueueEvent(ctime-cursortime+duration,9,SND_SEQ_EVENT_NOTEOFF,drums[i].note,127); //drum stop
	  }
	}
	if (chordFlags & 1) for (i=0;i<chordsDuration;i++) {
	  int ctime=chords[i].time*TEMPOMULTIPLIER/15625;
	  if (ctime>=cursortime && ctime<cursortime+duration) {
	  printf("%d<=%d<%d\n",cursortime,ctime,cursortime+duration);
	  printf("TEMP CHORD %d(%d+%d(%d))",
	      activeChord+idToNote(chords[i].note),
		 activeChord,idToNote(chords[i].note),
		 chords[i].note);
	  addQueueEvent(ctime-cursortime,0,SND_SEQ_EVENT_NOTEON,activeChord+idToNote(chords[i].note),127); //chord note start
	  addQueueEvent(ctime-cursortime+duration,0,SND_SEQ_EVENT_NOTEOFF,activeChord+idToNote(chords[i].note),127); //chord note stop
	  }
	}
	  //addQueueEvent(0,0,SND_SEQ_EVENT_NOTEON,60,127); //note start
	  //addQueueEvent(0,9,SND_SEQ_EVENT_NOTEON,2,127); //drum start
	  //addQueueEvent(duration,0,SND_SEQ_EVENT_NOTEOFF,60,127); //note stop
	  //addQueueEvent(duration,9,SND_SEQ_EVENT_NOTEOFF,2,127); //drum stop
	if (snd_seq_drain_output(seq)<0) {fprintf(stderr,"failed to drain sequencer output\n");exit(-1);}
	printf("current phrase done\n");
	//if (snd_seq_sync_output_queue(seq)<0) {fprintf(stderr,"sync output failed\n");exit(-1);}
	if (snd_seq_stop_queue(seq,queue,NULL)<0) {fprintf(stderr,"queue stop failed\n");exit(-1);}
}

void queueChord(int start,int end,int id,int value) {
  restoreChordNoteSeq(id);
  int v=(value & 0x0f)%12;
  chordType=0;
  chordExtend=0;
  activeChord=v;
  if (value&0x10) chordType=1;
  if (value&0x20) chordExtend=1;
  int i,j;
  int barstart=(start/512)*512;
  printf("active Chord %d minor:%d seventh:%d\n",
	 activeChord,chordType,chordExtend);
  for (i=0;i<chordsDuration;i++) {
    int ctime=chords[i].time*TEMPOMULTIPLIER/15625;
    //printf("from %d to %d\n",ctime+barstart,end);
    for (j=ctime+barstart;j<end;j+=512) {
      addQueueEvent(j,0,SND_SEQ_EVENT_NOTEON,
		    activeChord+idToNote(chords[i].note),127); //chord note start
      addQueueEvent(j+8,0,SND_SEQ_EVENT_NOTEOFF,
		    activeChord+idToNote(chords[i].note),127); //chord note stop
      
    }
    if (ctime+barstart<end) printf("queue %d(%d+%d(%d)) every 512 from %d to %d ",
    activeChord+idToNote(chords[i].note),activeChord,idToNote(chords[i].note),chords[i].note,
    ctime+barstart,end);
  }
}

void queueMidiPerc(int start,int end,int id) {
  restorePercSeq(id);
  int i,j;
  int barstart=(start/512)*512;
  for (i=0;i<drumsDuration;i++) {
  int ctime=drums[i].time*TEMPOMULTIPLIER/15625;
    for (j=ctime+barstart;j<end;j+=512) if (j>=start) {
      int activechord=areChordsActive(j);
      if (activechord & 2) {
      addMidiEvent(j,9,SND_SEQ_EVENT_NOTEON,
		    drums[i].note);
      addMidiEvent(j+drums[i].duration,9,SND_SEQ_EVENT_NOTEOFF,
		    drums[i].note); //chord note stop
      }
    }
  }
}

void queueMidiChord(int start,int end,int id,int value) {
  restoreChordNoteSeq(id);
  int v=(value & 0x0f)%12;
  chordType=0;
  chordExtend=0;
  activeChord=v;
  if (value&0x10) chordType=1;
  if (value&0x20) chordExtend=1;
  int i,j;
  int barstart=(start/512)*512;
  printf("active Chord %d minor:%d seventh:%d\n",
	 activeChord,chordType,chordExtend);
  for (i=0;i<chordsDuration;i++) {
    int ctime=chords[i].time*TEMPOMULTIPLIER/15625;
    //printf("from %d to %d\n",ctime+barstart,end);
    for (j=ctime+barstart;j<end;j+=512) if (j>=start) {
      int activechord=areChordsActive(j);
      if (activechord & 1) {
      addMidiEvent(j,1,SND_SEQ_EVENT_NOTEON,
		    activeChord+idToNote(chords[i].note)); //chord note start
      addMidiEvent(j+chords[i].duration,1,SND_SEQ_EVENT_NOTEOFF,
		    activeChord+idToNote(chords[i].note)); //chord note stop
      }
    }
    if (ctime+barstart<end) printf("queue %d(%d+%d(%d)) every 512 from %d to %d ",
    activeChord+idToNote(chords[i].note),activeChord,idToNote(chords[i].note),chords[i].note,
    ctime+barstart,end);
  }
}

void reportChord() {
  int i;
  for (i=0;i<chordsDuration;i++) {
    int ctime=chords[i].time*TEMPOMULTIPLIER/15625;
    int cnote=chords[i].note;
    int cduration=chords[i].duration;
    printf("chord event %d : %d %d %d\n",i,ctime,cnote,cduration);
  }
  
}

void echoSong() {
  int i;
  int cursortime=currenttime.bar*512+currenttime.q*128+currenttime.tick;
  for (i=0;i<songDuration;i++) {
    int cnote=song[i].note;
    int ctime=song[i].time;
    int cduration=song[i].duration;
    printf("time %d note %d duration %d\n",ctime,cnote,cduration);
    if (cursortime>=ctime && cursortime<ctime+cduration) playnote(cnote,cduration);   
  }
  for (i=0;i<accompanDuration;i++) {
  int atime=accompan[i].time;
  enum ControlEventType type=accompan[i].type;
  int value=accompan[i].value;
  if (cursortime>=atime && cursortime<atime+duration) {
      if (type==SelectPercussion) {
	printf("P:%d",value);
	restorePercSeq(value);
      } else if (type==PercussionFlag) {
	activePercussionFlag=value;
	if (value==0) printf("P-off");
	else printf("P-on");
      } else if (type==SelectAccopaniament) {
	restoreChordNoteSeq(value);
      } else if (type==AccopaniamentFlag) {
	activeAccopaniamentFlag=value;
	if (value==0) printf("A-off");
	else printf("A-on");
      } else if (type==SelectChord) {
	int v=(value & 0x0f)%12;
	char minor=' ';chordType=0;
	char seventh=' ';chordExtend=0;
	activeChord=v;
	if (value&0x10) {minor='m';chordType=1;}
	if (value&0x20) {seventh='7';chordExtend=1;} 
	printf("value is %d v is %d minor is %c seventh is %c\n",value,v,minor,seventh);
	printf("%c%c%c%c",notedescr[v*2],notedescr[v*2+1],minor,seventh);
//	playnote(24+v+idToNote(0),32);
//	playnote(24+v+idToNote(1),32);
//	playnote(24+v+idToNote(2),32);
      }
    }
  }
  playCurrentPhrase();
}

int songEnd() {
  int i;
  int end=0;
  for (i=0;i<songDuration;i++) {
    int cend=song[i].time+song[i].duration;
    printf("COMPARE %d with %d\n",end,cend);
    if (cend>end) end=cend;
  }
  for (i=0;i<accompanDuration;i++) {
    int cend=accompan[i].time+duration;
    printf("COMPARE %d with %d\n",end,cend);
    if (cend>end) end=cend;
  }
  return end+4*duration;
}

static int cmpMidiTime(const void *ip1,const void *ip2) {
  int r=-1;
  int t1,t2;
  t1=((MidiEvent*)ip1)->time;
  t2=((MidiEvent*)ip2)->time;
  if (t1==t2) r=0;
  if (t1>t2) r=1;
  //if (p1->time<p2->time) 
  //printf("compare %p (%d) %p (%d) : %d\n",ip1,t1,ip2,t2,r);
  return r;
}

static int cmpAccTime(const void *ip1,const void *ip2) {
  int r=-1;
  int t1,t2;
  t1=((ControlEvent*)ip1)->time;
  t2=((ControlEvent*)ip2)->time;
  if (t1==t2) r=0;
  if (t1>t2) r=1;
  return r;

}


void queueMidiList() {
  int i;
  for (i=0;i<midiDuration;i++) {
    int time=midiList[i].time;
    int channel=midiList[i].channel;
    int type=midiList[i].type;
    int note=midiList[i].note;
    addQueueEvent(time,channel,type,note,127);
  }
}

typedef struct {
  int time;
  int flag;
} FlagEvent;

static int cmpFlagEventTime(const void *ip1,const void *ip2) {
  int r=-1;
  int t1,t2;
  t1=((FlagEvent*)ip1)->time;
  t2=((FlagEvent*)ip2)->time;
  if (t1==t2) r=0;
  if (t1>t2) r=1;
  //if (p1->time<p2->time) 
  //printf("compare %p (%d) %p (%d) : %d\n",ip1,t1,ip2,t2,r);
  return r;
}

int areChordsActive(int time) {
  int i;
  int accompFlagsCount=0;
  int percFlagsCount=0;
  for (i=0;i<accompanDuration;i++) {
    if (accompan[i].type==AccopaniamentFlag) accompFlagsCount++;
    if (accompan[i].type==PercussionFlag) percFlagsCount++;
  }
  printf("%d accomp flags, %d perc flags time ref:%d\n",accompFlagsCount,percFlagsCount,time);
  int accIndex=0;
  int percIndex=0;
  FlagEvent chordFlags[accompFlagsCount];
  FlagEvent drumFlags[percFlagsCount];
  for (i=0;i<accompanDuration;i++) {
    if (accompan[i].type==AccopaniamentFlag) {
      printf("%d accopaniamentFlag at %d:%d\n",i,accompan[i].time,accompan[i].value);
      chordFlags[accIndex].time=accompan[i].time;
      chordFlags[accIndex].flag=accompan[i].value;
      accIndex++;
    }
    if (accompan[i].type==PercussionFlag) {
      printf("%d percFlag at %d:%d\n",i,accompan[i].time,accompan[i].value);
      drumFlags[percIndex].time=accompan[i].time;
      drumFlags[percIndex].flag=accompan[i].value;
      percIndex++;
    }
  }
  qsort(chordFlags,accompFlagsCount,sizeof(FlagEvent),cmpFlagEventTime);
  qsort(drumFlags,percFlagsCount,sizeof(FlagEvent),cmpFlagEventTime);
  accIndex=-1;percIndex=-1;
  printf("accompflags: \n");
  for (i=0;i<accompFlagsCount;i++) printf("(%d)%d:%d ",i,chordFlags[i].time,chordFlags[i].flag);
  printf("\npercflags: \n");
  for (i=0;i<percFlagsCount;i++) printf("(%d)%d:%d ",i,drumFlags[i].time,drumFlags[i].flag);
  printf("\n");
  for (i=0;i<accompFlagsCount-1;i++) {
    if (chordFlags[i].time<=time && chordFlags[i+1].time>time) accIndex=i;
  }
  if (chordFlags[accompFlagsCount-1].time<=time) accIndex=accompFlagsCount-1;
  for (i=0;i<percFlagsCount-1;i++) {
    if (drumFlags[i].time<=time && drumFlags[i+1].time>time) percIndex=i;
  }
  if (drumFlags[percFlagsCount-1].time<=time) percIndex=percFlagsCount-1;
  
  int rv=0;
  if (accIndex>=0) rv|=chordFlags[accIndex].flag;
  if (percIndex>=0) rv|=drumFlags[percIndex].flag<<1;
  return rv;
}


void layoutSong() {
  int songend=songEnd();
  printf("song end:%d\n",songend);
  clearMidiList();
  stopallsound();
  qsort(accompan,accompanDuration,sizeof(ControlEvent),cmpAccTime);
  snd_seq_event_t ev;
  snd_seq_ev_clear(&ev);
  ev.queue = queue;
  ev.source.port = 0;
  ev.flags = SND_SEQ_TIME_STAMP_TICK;
  initTempo(seq,queue);
  if (snd_seq_start_queue(seq, queue, NULL)<0) {fprintf(stderr,"start queue failed\n");exit(-1);}
  //setTempo(500000);
  int i;
  for (i=0;i<songDuration;i++) {
    int cnote=song[i].note;
    int ctime=song[i].time;
    int cduration=song[i].duration;
    //addQueueEvent(ctime,0,SND_SEQ_EVENT_NOTEON,cnote,127);
    //addQueueEvent(ctime+duration,0,SND_SEQ_EVENT_NOTEOFF,cnote,127);
    
    addMidiEvent(ctime,0,SND_SEQ_EVENT_NOTEON,cnote);
    addMidiEvent(ctime+duration,0,SND_SEQ_EVENT_NOTEOFF,cnote);
    printf("%d %d %d\n",ctime,cduration,cnote);
  }
  printf("ONLY NOTES:\n");
  reportMidiList();
  int lastChord=-1;
  int lastChordPos=0;
  int lastPercPos=0;
  int currChordId=0;
  int currPercId=0;
  for (i=0;i<accompanDuration;i++) {
    if (accompan[i].type==SelectAccopaniament) printf("\nselect accopaniament (%d) time: %d value: %d (lastpos:%d)\n",i,accompan[i].time,accompan[i].value,lastChordPos);
    if (accompan[i].type==SelectAccopaniament) {
      printf("setting id from %d",currChordId);
      if (lastChord!=-1) {
	
	
	queueMidiChord(lastChordPos,accompan[i].time,currChordId,lastChord);
      }
      currChordId=accompan[i].value;
      printf("to %d\n",currChordId);
      lastChordPos=accompan[i].time;
    } //its ok because list is time sorted
    if (accompan[i].type==SelectChord) {
      if (lastChord!=-1) {
	printf("\nchord %d with id %d from %d to %d\n",lastChord,currChordId,lastChordPos,accompan[i].time);
	queueMidiChord(lastChordPos,accompan[i].time,currChordId,lastChord);
      }
      lastChord=accompan[i].value;
      lastChordPos=accompan[i].time;
    }
    if (accompan[i].type==SelectPercussion) {
      printf("setting percussion id from %d",currPercId);
      queueMidiPerc(lastPercPos,accompan[i].time,currPercId);
      currPercId=accompan[i].value;
      printf("to %d\n",currPercId);
      lastPercPos=accompan[i].time;
    }
  }
  printf("setting end perc addition: %d %d %d and chord %d %d %d %d\n",lastPercPos,songend,currPercId,lastChordPos,songend,currChordId,lastChord);
  queueMidiPerc(lastPercPos,songend,currPercId);
  queueMidiChord(lastChordPos,songend,currChordId,lastChord);
  
  printf("before sort:\n");
  reportMidiList();
  qsort(midiList,midiDuration,sizeof(MidiEvent),cmpMidiTime);
  printf("after sort:\n");
  reportMidiList();
  reportChord();
  queueMidiList();
  if (snd_seq_drain_output(seq)<0) {fprintf(stderr,"failed to drain sequencer output\n");exit(-1);}
  printf("current layout done\n");
  //if (snd_seq_sync_output_queue(seq)<0) {fprintf(stderr,"sync output failed\n");exit(-1);}
  if (snd_seq_stop_queue(seq,queue,NULL)<0) {fprintf(stderr,"queue stop failed\n");exit(-1);}
}

int mainApp(int argc,char **argv) {

char defaultport[16];
sprintf(defaultport,"128:0");
int lastport=last_port();
if (lastport>0) {
  fprintf(stderr,"using port %d:0\n",lastport);
  sprintf(defaultport,"%d:0",lastport);
}

portarg=defaultport;
if (argc>1) portarg=argv[1];
init_midi(portarg);
set_instrument(instrument);
makewindow(800,600);
int q=0;
setLyrics("");
initPercSeq();
initChordNoteSeq();

//SetKey(&keys[0],50,50,50,30,4,52,"z");
int i;

for (i=0;i<10;i++) {
  char txt[3];
  char labels[]="ZXCVBNM,./";
  sprintf(txt,"%c",labels[i]);
  int note=keynote(i+1,4);
  SetKey(&keys[i],70,580-note*20,30,15,4,52+i,txt);
}
for (i=0;i<10;i++) {
  char txt[3];
  char labels[]="QWERTYUIOP";
  sprintf(txt,"%c",labels[i]);
  int note=keynote(i+1,2);
  SetKey(&keys[i+10],150,580-note*20,30,15,4,24+i,txt);
}
int j=0;
for (i=0;i<10;i++) {
  char txt[3];
  char labels[]="ASDFGHJKL;";
  sprintf(txt,"%c",labels[i]);
  int note=keynote(i+1,3);
  if (note>=0) {
  j++;
  SetKey(&keys[j+20],30,580-note*20,30,15,4,38+i,txt);
  }
}
for (i=0;i<10;i++) {
  char txt[3];
  char labels[]="1234567890";
  sprintf(txt,"%c",labels[i]);
  int note=keynote(i+1,1);
  if (note>=0) {
  j++;
  SetKey(&keys[j+20],110,580-note*20,30,15,4,10+i,txt);
  }
}
SetKey(&keys[j+21],700,40,100,20,4,65,"space bar:menu");
SetKey(&keys[j+22],700,80,100,20,4,113,"<-");
SetKey(&keys[j+23],700,120,100,20,4,111,"^ reset  midi");
SetKey(&keys[j+24],700,160,100,20,4,114,"->");
SetKey(&keys[j+25],700,200,100,20,4,119,"DEL");
SetKey(&keys[j+26],700,240,100,20,4,22,"<=Backspace");
SetKey(&keys[j+27],700,280,100,20,4,118,"INS");
SetKey(&keys[j+28],700,320,100,20,4,67,"F1:drums");
SetKey(&keys[j+29],700,360,100,20,4,68,"F2:drums store");
SetKey(&keys[j+30],700,400,100,20,4,69,"F3:drums load");
SetKey(&keys[j+31],700,440,100,20,4,70,"F4:chords");
SetKey(&keys[j+32],700,480,100,20,4,71,"F5:chord store");
SetKey(&keys[j+33],700,520,100,20,4,72,"F6:chord load");
SetKey(&keys[j+34],700,560,100,20,4,73,"F7:control");


printf("%d keys\n",j);
while (!q) {
 uievent();
 printf("banks:%d \n",countPercBanks());
 XEvent event=uilastevent();
 uicolor(0x00ff00);
 uirect(240,0,260,600);
 uirect(300,10,600,590);
 uicolor(0x00ffff);
 uirect(300,590,600,600);
 uicolor(0);
 uiline(180,0,180,600);
 for (i=300;i<601;i+=128) uiline(i,10,i,590);
 showSong();
 uiline(600,0,600,600);
 uicolor(0xff00ff);
 uiline(300+128+currenttime.tick,10,300+128+currenttime.tick,590);
 if (300+128+duration+currenttime.tick<600) uiline(300+128+currenttime.tick+duration,10,300+128+currenttime.tick+duration,590);
 else uiline(599,10,599,590);
 uicolor(0);
 for (i=0;i<29;i++) {
  uiline(10,580-i*20-10,250,580-i*20-10);
  uiline(300,580+5-(i+12)*10-10,600,580+5-(i+12)*10-10);
  uiline(250,580-i*20-10,300,580+5-(i+12)*10-10);
  if (issharp(i)) uirect(180,580-i*20-10,200,580-i*20-10+20);
  char txt[3];
  sprintf(txt,"%c%c",notedescr[i*2],notedescr[i*2+1]);
  uicolor(0);
  uitext(168,580-20*i+2,txt);
  uicolor(0x0000ff);
  uitext(300+128-20,580+3-10*(i+12)+2,txt);
  uicolor(0);
 }
 
 for (i=0;i<23+j+1+11;i++) {CheckKey(&keys[i],event); ShowKey(&keys[i]);uiflush();}
 showTime(&currenttime,428,10);
 //printf("keys pressed");   for (i=0;i<20+j+1;i++) printf(":%d",keys[i].pressed);   printf("\n");
    if (keys[20+j+1].pressed) {     mainmenu();  }
    if (keys[20+j+2].pressed) {  modTime(&currenttime,-duration); echoSong();  }
    if (keys[20+j+3].pressed) {  reset_midi();  }
    if (keys[20+j+4].pressed) {  modTime(&currenttime,duration); echoSong(); }
    if (keys[20+j+5].pressed) {  deleteNotesHere(1); deleteEventsHere(1); }
    if (keys[20+j+6].pressed) {  modTime(&currenttime,-duration); deleteNotesHere(0); deleteEventsHere(0);}
    if (keys[20+j+7].pressed) {  pushSongOn();  }
    if (keys[20+j+8].pressed) {  drumsSeq();  }
    if (keys[20+j+9].pressed) {  drumStoreMenu();  }
    if (keys[20+j+10].pressed) {  drumRestoreMenu();  }
    if (keys[20+j+11].pressed) {  chordsSeq();  }
    if (keys[20+j+12].pressed) {  chordStoreMenu();  }
    if (keys[20+j+13].pressed) {  chordRestoreMenu();  }
    if (keys[20+j+14].pressed) {  InsertControlMenu();  }

   for (i=0;i<20+j+1;i++) if (keys[i].pressed) {
     int note=(580-keys[i].y)/20;
     printf("note to play is %d\n",note);
     //uicircle(250,580-note*20,9);
     uifillcircle(250,580-note*20,9);
     uicolor(0xffff00);
     if (note>=0 && note<=28) {
         char txt[3];
	 sprintf(txt,"%c%c",notedescr[note*2],notedescr[note*2+1]);
         uitext(250-2-2*issharp(note),580-20*note+4,txt);
	 addSongNote(note+octave*12);
	 modTime(&currenttime,duration);
     }
     uicolor(0);
     uiflush();
     playnote(note+octave*12,duration);    
    }
 if (uikeypress(27)) q=1;
 if (event.type==KeyPress) {
   printf("event.xkey.keycode:%d\n",event.xkey.keycode);
   if (event.xkey.keycode==111) printf("up\n");
   if (event.xkey.keycode==114) printf("right\n");
   if (event.xkey.keycode==113) printf("left\n");
   if (event.xkey.keycode==116) printf("down\n");
   if (event.xkey.keycode==65) {
     //printf("space pressed, enter instrument (%d)\n",instrument);
     //scanf("%d",&instrument);
     printf("setting instrument to %d\n",instrument);
     set_instrument(instrument);
   }
   if (event.xkey.keycode==36) {
     layoutSong();
     /*
     printf("enter duration(%d)\n",duration);
     scanf("%d",&duration);
     printf("duration set to %d\n",duration);
     */
   }
   if (event.xkey.keycode==50) printf("left shift\n");
   if (event.xkey.keycode==37) printf("left control\n");
   if (event.xkey.keycode==64) printf("left alt\n");
   if (event.xkey.keycode==108) printf("right alt\n");
   if (event.xkey.keycode==105) printf("right control\n");
   if (event.xkey.keycode==62) printf("right shift\n");
   /*
   int x,y;
   keypos(event.xkey.keycode,&x,&y);
   int note=keynote(x,y);
   printf("y: %d x: %d note: %d\n",y,x,note);
   if (note>=0) playnote(note+36,duration);
   */
   
   }
 }
snd_seq_close(seq); 
}

//#include "importperc.c"
int selectedMeter=0;
int time_div=0;
int nTracks=0;
int songlength=0;
int meterLength=0;
int songMeters=0;
MidiTrack *tracks=NULL;

void procNote(int time,int type,int channel,int note,int vel,void *data) {
   if (songlength<time) songlength=time;
}


void loadMidi(char *filename) {
  FILE *file=fopen(filename,"rb");
  if (!file) {
  fprintf(stderr,"cant open file %s\n",filename);
  exit(-1);
  }
  headchunk fileheader;
  headchunk swappedheader;
  printf("size of headchunk:%d\n",sizeof(headchunk));
  fread(&fileheader,14,1,file);//use 14 because sizeof(headchunk) alligns to 4byte multiples
  memcpy(&swappedheader,&fileheader,sizeof(headchunk));
  flipheaddata(&swappedheader);
  reporthead(&swappedheader);
  time_div=swappedheader.time_div;
  int cTrack;
  nTracks=swappedheader.tracks;
  if (tracks!=NULL) free(tracks);
  tracks=malloc(nTracks*sizeof(MidiTrack));
  if (!tracks) memoryError();
  songlength=0;
  for (cTrack=0; cTrack < nTracks; cTrack++) {
    int tracksize;
    char id[4];
    printf("reading track %d\n",cTrack);
    if (fread (id, 4,1,file)!=1) {fprintf(stderr,"cant read track header\n");exit(1);}
    printf("ID:%.4s\n",(char *)&id);
	  if (strncmp(id,"MTrk",4)==0) {
	    printf("track no %d found\n",cTrack+1);
	    if (fread(&tracksize,4,1,file)!=1) {fprintf(stderr,"cant read track size\n");exit(1);}
	    flipdata((char*)&tracksize,4);
	    printf("track size:%d\n",tracksize);
	    char *data;
	    data=malloc(tracksize);
	    if (data==NULL) memoryError();
	    if (fread(data,tracksize,1,file)!=1) {fprintf(stderr,"failed to read track data\n");exit(-1);}
	    MidiTrack track;
	    zeroTrack(&track);
	    prepareTrack(&track,data,tracksize);
	    memcpy(&tracks[cTrack],&track,sizeof(MidiTrack)); 
	  }
	  else {
	    fprintf(stderr,"track %d: this is not a track\n",cTrack);
	    exit(1);
	  }
    }
  fclose(file);
  printf("file %s closed, now to count size\n",filename);
  int i;
  for (i=0;i<nTracks;i++) {
    printf("parsing track %d\n",i);
    parseTrack(&(tracks[i]),procNote,NULL,NULL);
    printf("track %d done\n",i);
  }
  meterLength=time_div*4*TEMPOMULTIPLIER;
  if (meterLength) songMeters=1+songlength/meterLength; else songMeters=1;
  if (meterLength) printf("song length: %d time_div:%d meter length:%d meters:%d\n",songlength,time_div,meterLength,songMeters);
  else printf("NO time_div? meteLength is ZERO\n");
}

void procDrumMeter(int time,int type,int channel,int note,int vel,void *data) {
  if (!meterLength) {
    fprintf(stderr,"no meteLength, cant process event\n");
    return;
  }
  if (channel==9 && time>=selectedMeter*meterLength && time<(selectedMeter+1)*meterLength & type==0x90 && vel!=0) {
    int midiTime=time % meterLength;
    int drumSeqTime=midiTime*4000000/meterLength;
    addDrum(note,drumSeqTime);
  }
}

void importDrumMeter(int meter,int trackid) {
  clearAllDrums();
  selectedMeter=meter;
  parseTrack(&tracks[trackid],procDrumMeter,NULL,NULL);
}

void importDrumMeterAlltracks(int meter) {
  clearAllDrums();
  selectedMeter=meter;
  int i;
  for (i=0;i<nTracks;i++) parseTrack(&tracks[i],procDrumMeter,NULL,NULL);
}

//end of #include "importperc.c"

void testDrums(int argc,char **argv) {
  char defaultport[16];
  sprintf(defaultport,"128:0");
  int lastport=last_port();
  if (lastport>0) {
    fprintf(stderr,"using port %d:0\n",lastport);
    sprintf(defaultport,"%d:0",lastport);
  }
  portarg=defaultport;
  init_midi(portarg);
  set_instrument(instrument);
  makewindow(800,600);
  if (argc>1) {
    printf("calling loadMidi\n");
    loadMidi(argv[1]);
    printf("loadMidi finished %d tracks %d meters\n",nTracks,songMeters);
    printf("meter to read:");
    int meter;
    scanf("%d",&meter);
    meter++;
    meter%=songMeters;
    printf("call importDrumMeterAll(%d)\n",meter);
    importDrumMeterAlltracks(meter);
    PercussionSequence importedSequence;
    importedSequence.drums=drums;
    importedSequence.drumsSize=drumsSize;
    importedSequence.drumsDuration=drumsDuration;   
  }
  drumsSeq();
}

void importDrums() {
  textAnswer("import Drums from midi filename:",150,200);
  loadMidi(reply);
  printf("loading first meter\n");
  importDrumMeterAlltracks(0);
  PercussionSequence importedSequence;//,lastSequence;
  importedSequence.drums=drums;
  importedSequence.drumsSize=drumsSize;
  importedSequence.drumsDuration=drumsDuration;
  printf("copying first meter\n");
  //copyPercussionSequence(&lastSequence,&importedSequence);
  MidiLibGoSilent(1);
  storeFreePercSeq();
  printf("looping \n");
  int i;
  for (i=0;i<songMeters;i++) {
    importDrumMeterAlltracks(i);
    importedSequence.drums=drums;
    importedSequence.drumsSize=drumsSize;
    importedSequence.drumsDuration=drumsDuration;
    int bp=inbankSequence(&importedSequence);
    if (bp) printf("%d already in bank %d\n",i,bp); else {
      printf("%d not in banks\n",i);
      if (storeFreePercSeq()==-1) printf("out of free banks storing %d\n",i);
      //copyPercussionSequence(&lastSequence,&importedSequence);
    }
  }
}

//#define drumsapp
int main(int argc,char **argv) {
#ifdef drumsapp
  testDrums(argc,argv);
#else
  mainApp(argc,argv);
#endif
  
}