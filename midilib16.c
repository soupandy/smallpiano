#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

static int SILENTMIDILIB=0;

void MidiLibGoSilent(int v) {
  SILENTMIDILIB=v;
}

typedef struct {
unsigned char *data;
int readPos;
int size;
int memory;
} MidiTrack;

void genericMidiError() {
  fprintf(stderr,"generic error\n");
  exit(-1);
}

void memoryError() {
  fprintf(stderr,"out of memory\n");
  exit(-1);
}

void zeroTrack(MidiTrack *track) {
  track->data=NULL;
  track->readPos=0;
  track->size=0;
  track->memory=0;
}

void prepareTrack(MidiTrack *track,unsigned char *data,int size) {
  //data has been malloced with size size
  track->data=data;
  track->size=size;
  track->readPos=0;
  track->memory=size;
}

void appendTrackData(MidiTrack *track,unsigned char *data,int size) {
  if (track->size+size>=track->memory) {
    track->memory+=1024;
    track->data=realloc(track->data,track->memory);
    if (track->data==NULL) memoryError();
  }
  memcpy(track->data+track->size,data,size);
  track->size+=size;
}

void appendTrackByte(MidiTrack *track,unsigned char byte) {
  if (track->size+1>=track->memory) {
    track->memory+=1024;
    track->data=realloc(track->data,track->memory);
    if (track->data==NULL) memoryError();
  }
  (track->data)[track->size]=byte;
  (track->size)++;
}

void appendTrackValue(MidiTrack *track,int v) {
  if (v >= (1 << 28))
		appendTrackByte(track, 0x80 | ((v >> 28) & 0x03));
	if (v >= (1 << 21))
		appendTrackByte(track, 0x80 | ((v >> 21) & 0x7f));
	if (v >= (1 << 14))
		appendTrackByte(track, 0x80 | ((v >> 14) & 0x7f));
	if (v >= (1 << 7))
		appendTrackByte(track, 0x80 | ((v >> 7) & 0x7f));
	appendTrackByte(track, v & 0x7f);
}

void appendTrackEOF(MidiTrack *track) {
  appendTrackByte(track,0);
  appendTrackByte(track,0xff);
  appendTrackByte(track,47); //0x2f end of track event
  appendTrackByte(track,0);
}

int readTrackByte(MidiTrack *track) {
int pos=track->readPos;
if (pos<track->size) {
  (track->readPos)++;
  return track->data[pos];
  }
return -1;
}

int readTrackWord(MidiTrack *track) {
  int v;
  v=readTrackByte(track);
  v|=readTrackByte(track)<<8;
  v|=readTrackByte(track)<<16;
  v|=readTrackByte(track)<<24;
  if (track->readPos<=track->size) return v;
  return -1;
}

int readTrackInt(MidiTrack *track,int bytes) {
  	int c, v = 0;
	int m=bytes;
	do {
		c = readTrackByte(track);
		if (c == -1)
			return -1;
		v = (v << 8) | c;
	} while (--m);
	return v;
}

int readTrackVar(MidiTrack *track) {
  int v,c,l;
  l=0;
  c = readTrackByte(track);
  v = c & 0x7f;
  while ((c & 0x80) && (l<4)) {
	  c = readTrackByte(track);
	  v = (v << 7) | (c & 0x7f);
	  l++;
  }
  if (l>=4) return -1;
  if (track->readPos<=track->size) return v;
  return -1;
}


typedef struct 
{
unsigned int id;
unsigned int size;
short unsigned int type;
short unsigned int tracks;
short unsigned int time_div;
} headchunk;

void flipdata(char *where,int size) {
char buffer[size];
int f;
for (f=0;f<size;f++) buffer[f]=where[size-1-f];
for (f=0;f<size;f++) where[f]=buffer[f];
}

void flipheaddata(headchunk *head) {
flipdata((char*)&head->size,4);
flipdata((char*)&head->type,2);
flipdata((char*)&head->tracks,2);
flipdata((char*)&head->time_div,2); 
}

int time_div2msec(int time_div,int tempo) {
  int tick;
  if (time_div & 0x8000) {
    int frames_per_sec=(time_div & 0x7f00)>>8;
    int frame_resolution=time_div &0xff;
    printf("%d resolution in %d frames/sec\n",frame_resolution,frames_per_sec);
    tick=1000000/(frames_per_sec*frame_resolution);
  } else {
    printf("quorter note div:%d ticks/quorternote\n",time_div);
    tick=tempo/time_div;
    printf("tick is %d microseconds for tempo %d microseconds ",tick,tempo);
    int bpm=60000000/tempo;
    printf("with beat per minute:%d\n",bpm);
    //int bpm=
    //quorternote is defined by tempo 
    //tempo is microseconds per quorternote 500000 by default
    //
  }
  return tick;
}
void addKaraokeText(MidiTrack *karaoke,char *sometext,int delay)
  {
  appendTrackValue(karaoke,delay); 
  appendTrackByte(karaoke,0x0ff); //meta event
  appendTrackByte(karaoke,5); //meta type not 6(text marker) or   1(text) but 5 (lyric)
  int datalen=strlen(sometext);
  printf("text len is %d\n",datalen);
  appendTrackValue(karaoke,datalen);
  appendTrackData(karaoke,sometext,datalen);
  }

void addKaraokeTextUtf8(MidiTrack *karaoke,char *sometext,int delay,int l)
  {
  appendTrackValue(karaoke,delay); 
  appendTrackByte(karaoke,0x0ff); //meta event
  appendTrackByte(karaoke,5); //meta type not 6(text marker) or   1(text) but 5 (lyric)
  int datalen=l;
  printf("text len is %d\n",datalen);
  appendTrackValue(karaoke,datalen);
  appendTrackData(karaoke,sometext,datalen);
  }

void addKaraokeTextBOM(MidiTrack *karaoke,char *sometext,int delay,int l)
  {
  appendTrackValue(karaoke,delay); 
  appendTrackByte(karaoke,0x0ff); //meta event
  appendTrackByte(karaoke,5); //meta type not 6(text marker) or   1(text) but 5 (lyric)
  int datalen=l;
  printf("text len is %d\n",datalen);
  appendTrackValue(karaoke,datalen+3);
  // 0xEF, 0xBB, 0xBF  
  appendTrackByte(karaoke,0xEF);
  appendTrackByte(karaoke,0xBB);
  appendTrackByte(karaoke,0xBF);
  appendTrackData(karaoke,sometext,datalen);
  }
  

  

void reporthead(headchunk *head) {
  int tempo;
  int bpm;
  //printf("ID:%.*s\n",4,(char *)&head->id);
  printf("ID:%.4s\n",(char *)&head->id);
  printf("size:%d(%x)\n",head->size,head->size);
  printf("type:%d(%x)\n",head->type,head->type);
  printf("tracks:%d(%x)\n",head->tracks,head->tracks);
  printf("time_div:%d(%x)\n",head->time_div,head->time_div);
  //time_div2msec(head->time_div,500000);//default tempo is 500000 microseconds
  bpm=150; 
  tempo=1000000*60/bpm;
  printf("with bpm %d\n",bpm);
  time_div2msec(head->time_div,tempo);
  
  bpm=60;//60 beats per minute (1 note per second)
  tempo=1000000*60/bpm;
  printf("with bpm %d\n",bpm);
  time_div2msec(head->time_div,tempo);
  
  bpm=120;//120 beats per minute (2 notes per second)
  tempo=1000000*60/bpm;
  printf("with bpm %d\n",bpm);
  printf("tick select %d\n",time_div2msec(head->time_div,tempo));
  printf("note duration is 4*%d=%d ticks\n",head->time_div,4*head->time_div);
}

static void skip(int bytes,MidiTrack *track)
{
	while (bytes > 0)
		readTrackByte(track), --bytes;
}

void copyBytes(MidiTrack *srcp, MidiTrack *destp,int len) {
  int i;
  for (i=0;i<len;i++) appendTrackByte(destp,readTrackByte(srcp));
}

void modifyTrack(MidiTrack *srcp, MidiTrack *destp,int (*selectID) (int id), int copyNotesFlag){
MidiTrack src;
MidiTrack dest;

printf("srcpointer: %p destpointer: %p src: %p dest:%p\n",srcp,destp,&src,&dest);
memcpy (&src,srcp,sizeof(MidiTrack));
src.readPos=0;
zeroTrack(&dest);
int src_end=src.size;
	unsigned char d[3];
	int tick = 0;
	unsigned char last_cmd = 0;
	unsigned char port = 0;

	/* the current file position is after the src ID and length */
	while (src.readPos < src_end) {
		unsigned char cmd;
		//struct event *event;
		int delta_ticks, len, c;

		delta_ticks = readTrackVar(&src);
		
		if (delta_ticks < 0)
			break;
		tick += delta_ticks;
		appendTrackValue(&dest,delta_ticks);

		c = readTrackByte(&src);
		if (c < 0)
			break;

		if (c & 0x80) {
			/* have command */
			cmd = c;
			if (cmd < 0xf0)last_cmd = cmd;
			if ((cmd >>4 == 0x8 || cmd>>4 == 0x9)) {
			  if (copyNotesFlag) appendTrackByte(&dest,c);
			} else {
			  appendTrackByte(&dest,c); 
			}
		} else {
		  if (!SILENTMIDILIB) printf("NO COMMAND, last command is %d\n",last_cmd);
			/* running status */
			src.readPos--;
			cmd = last_cmd;
			if (!cmd){
			   fprintf(stderr,"WTF AGAIN?\n");
			   exit(-1);
			}
		}
		if (!SILENTMIDILIB) printf("event:%x\n",cmd);
		switch (cmd >> 4) {
			// maps SMF events to ALSA sequencer events 
		case 0x8: /* channel msg with 2 parameter bytes */
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xe:
			//event = new_event(src, 0);
			//event->type = cmd_type[cmd >> 4];
			//event->port = port;
			//event->tick = tick;
			d[0] = cmd & 0x0f;
			d[1] = readTrackByte(&src) & 0x7f;
			d[2] = readTrackByte(&src) & 0x7f;
			
			if ((cmd >>4 == 0x8 || cmd>>4 == 0x9)) {
			  if (copyNotesFlag) {
			    appendTrackByte(&dest,d[1]);
			    appendTrackByte(&dest,d[2]);
			  }
			} else {
			appendTrackByte(&dest,d[1]);
			appendTrackByte(&dest,d[2]);
			}
			if (cmd>>4 == 0x8) if (!SILENTMIDILIB)  printf("channel %d noteOFF %d off %d\n",
			  d[0],d[1],d[2]);
			if (cmd>>4 == 0x9) if (!SILENTMIDILIB) printf("channel %d noteON %d on %d\n",
			  d[0],d[1],d[2]);
			break;
		case 0xc: /* channel msg with 1 parameter byte */
		case 0xd:
			//event = new_event(src, 0);
			//event->type = cmd_type[cmd >> 4];
			//event->port = port;
			//event->tick = tick;
			d[0] = cmd & 0x0f;
			d[1] = readTrackByte(&src) & 0x7f;
			appendTrackByte(&dest,d[1]);
			break;

		case 0xf:
			switch (cmd) {
			case 0xf0: /* sysex */
			case 0xf7: /* continued sysex, or escaped commands */
				len = readTrackVar(&src);
				if (len < 0)
					genericMidiError();
				appendTrackValue(&dest,len);
				if (cmd == 0xf0)
					++len;
				if (cmd == 0xf0) {
				//	event->sysex[0] = 0xf0;
					c = 1;
				} else {
					c = 0;
				}
				for (; c < len; ++c) {
					 int v=readTrackByte(&src);
					 appendTrackByte(&dest,v);
					  }
				break;

			case 0xff: /* meta event */
				c = readTrackByte(&src);
				len = readTrackVar(&src);
				if (len < 0)
					genericMidiError();
				switch (c) {
				case 0x21: /* port number */
					if (len < 1)
						genericMidiError();
					//read port
					appendTrackByte(&dest,c);
					appendTrackValue(&dest,len);
					copyBytes(&src,&dest,len);
					break;

				case 0x2f: /* end of src */
					//src->end_tick = tick;
					appendTrackByte(&dest,c);
					appendTrackValue(&dest,len);
					copyBytes(&src,&dest,src_end - src.readPos);
					memcpy(destp,&dest,sizeof(MidiTrack));
					return ;

				case 0x51: /* tempo */
					if (len < 3)
					genericMidiError();
					appendTrackByte(&dest,c);
					appendTrackValue(&dest,len);
					copyBytes(&src,&dest,len);
					break;

				default: /* ignore all other meta events */
					if (!SILENTMIDILIB) printf("at tick :%d\n",tick);
					//skip_report(len,&src);
					if ((*selectID)(c))
					{
					appendTrackByte(&dest,c);
					appendTrackValue(&dest,len);
					copyBytes(&src,&dest,len);
					} else {
/*fucked up
					  if (dest.size>0 && dest.data[dest.size-1]==0xff) {
					    printf("must remove meta event at position %d\n",dest.size);
					    dest.size--; //unwrite the meta event
					  }
					  */
					  appendTrackByte(&dest,c);
					  appendTrackValue(&dest,0);
					  skip(len,&src);
					}
					
					break;
				}
				break;

			default: 
			  fprintf(stderr,"this should never have happened\n");
			}
			break;

		}
	}
	memcpy(destp,&dest,sizeof(MidiTrack));
}



void parseTrack(MidiTrack *trackp,
		void (*processNote) (int time,int type, int channel,int note, int vel,void *data), 
		void (*processData) (int time,char *text,void *data),
		void *data) {
MidiTrack track;
memcpy (&track,trackp,sizeof(MidiTrack));
track.readPos=0;
int track_end=track.size;
//int file_offset=track.readPos;
unsigned char d[3];
	int tick = 0;
	unsigned char last_cmd = 0;
	unsigned char port = 0;

	/* the current file position is after the track ID and length */
	while (track.readPos < track_end) {
		unsigned char cmd;
		//struct event *event;
		int delta_ticks, len, c;

		delta_ticks = readTrackVar(&track);
		if (delta_ticks < 0)
			break;
		tick += delta_ticks;

		c = readTrackByte(&track);
		if (c < 0)
			break;

		if (c & 0x80) {
			/* have command */
			cmd = c;
			if (cmd < 0xf0)
				last_cmd = cmd;
		} else {
		  if (!SILENTMIDILIB) printf("NO COMMAND, last command is %d\n",last_cmd);
			/* running status */
			//ungetc(c, file);
			track.readPos--;
			cmd = last_cmd;
			if (!cmd){
			   fprintf(stderr,"WTF AGAIN?\n");
			   exit(-1);
				
			}
		}
		
		if (!SILENTMIDILIB) printf("event:%x\n",cmd);
		//printf("%d",cmd);
		//int a=cmd;

		switch (cmd >> 4) {
			// maps SMF events to ALSA sequencer events 
			static const unsigned char cmd_type[] = {
				[0x8] = SND_SEQ_EVENT_NOTEOFF,
				[0x9] = SND_SEQ_EVENT_NOTEON,
				[0xa] = SND_SEQ_EVENT_KEYPRESS,
				[0xb] = SND_SEQ_EVENT_CONTROLLER,
				[0xc] = SND_SEQ_EVENT_PGMCHANGE,
				[0xd] = SND_SEQ_EVENT_CHANPRESS,
				[0xe] = SND_SEQ_EVENT_PITCHBEND
			};
			

		case 0x8: /* channel msg with 2 parameter bytes */
			  
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xe:
			//event = new_event(track, 0);
			//event->type = cmd_type[cmd >> 4];
			//event->port = port;
			//event->tick = tick;
			d[0] = cmd & 0x0f;
			d[1] = readTrackByte(&track) & 0x7f;
			d[2] = readTrackByte(&track) & 0x7f;
			{
		        int channel=cmd & 0xf; 
			int type=cmd &0xf0;
			int notenum=d[1];
			int vel=d[2];
			//if (channel!=9 && (type==0x80 || type==0x90)) (*processNote) (tick,type,channel,notenum,vel,data);
			//if (channel!=9 && processNote) (*processNote) (tick,type,channel,notenum,vel,data);
			if (processNote) (*processNote) (tick,type,channel,notenum,vel,data);
			}
			if (cmd>>4 == 0x8) if (!SILENTMIDILIB)  printf("channel %d noteOFF %d off %d\n",
			  d[0],d[1],d[2]);
			if (cmd>>4 == 0x9) if (!SILENTMIDILIB) printf("channel %d noteON %d on %d\n",
			  d[0],d[1],d[2]);
			break;

		case 0xc: /* channel msg with 1 parameter byte */
		case 0xd:
			//event = new_event(track, 0);
			//event->type = cmd_type[cmd >> 4];
			//event->port = port;
			//event->tick = tick;
			d[0] = cmd & 0x0f;
			d[1] = readTrackByte(&track) & 0x7f;
			break;

		case 0xf:
			switch (cmd) {
			case 0xf0: /* sysex */
			case 0xf7: /* continued sysex, or escaped commands */
				len = readTrackVar(&track);
				if (len < 0)
					genericMidiError();
				if (cmd == 0xf0)
					++len;
				if (cmd == 0xf0) {
				//	event->sysex[0] = 0xf0;
					c = 1;
				} else {
					c = 0;
				}
				for (; c < len; ++c)
					 readTrackByte(&track);
				break;

			case 0xff: /* meta event */
				c = readTrackByte(&track);
				len = readTrackVar(&track);
				if (len < 0)
					genericMidiError();

				switch (c) {
				case 0x21: /* port number */
					if (len < 1)
						genericMidiError();
					//read port
					readTrackByte(&track);
					skip(len - 1,&track);
					break;

				case 0x2f: /* end of track */
					//track->end_tick = tick;
					skip(track_end - track.readPos,&track);
					return ;

				case 0x51: /* tempo */
					if (len < 3)
						genericMidiError();
						skip(len,&track);
					/*
					if (smpte_timing) {
						// SMPTE timing doesn't change 
						skip(len,&track);
					} else {
						//read 3 tempo bytes
						readTrackByte(&track) << 16;
						readTrackByte(&track) << 8;
						readTrackByte(&track);
						skip(len - 3,track);
					}*/
					
					break;

				default: /* ignore all other meta events */
					if (!SILENTMIDILIB) printf("at tick :%d\n",tick);
					//skip_report(len,&track);
					{
					  int dummy;
					  char report[len+1];
					  for (dummy=0;dummy<len;dummy++) {
					  unsigned char c=readTrackByte(&track);
					  report[dummy]=c;
					  }
					  report[dummy]=0;
					  if (processData) (*processData) (tick,report,data);
					  if (!SILENTMIDILIB) printf("meta data:%s\n",report);
					}
					
					break;
				}
				break;

			default: 
			  fprintf(stderr,"this should never have happened\n");
			}
			break;

		}
	}
}
