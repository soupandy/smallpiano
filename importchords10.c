int selectedTimeDiv=0;
int time_div_count;
int selectedChord[12];
int chordNotes[4];
int chordsChannel=-1;

void clearSelectedChord(){
  int i;
  for (i=0;i<12;i++) selectedChord[i]=0;
}

void addChordNote(int note) {
  selectedChord[note%12]=1;
}

int selectChordNotes() {
  int i,o,of;
  for (i=0;i<4;i++) chordNotes[i]=-1;
  o=of=0;
  for (i=0;i<12;i++) if (selectedChord[i]) {
    chordNotes[o]=i;
    o++;
    if (o>4) {
      printf("\n NOTE OVERFLOW\n");
      of=1;
    }
    o%=4;
  }
  return o+of*4;
}

int chordNoteCount() {
  int count=0;
  int i;
  for (i=0;i<12;i++) if (selectedChord[i]) {
    count++;
  }
  return count;
}
void skipBeatTrack(MidiTrack *track) {
  appendTrackValue(track,time_div); //to forward beat
  appendTrackByte(track,0xff);
  appendTrackByte(track,6); //dummy text metadata
  appendTrackByte(track,0); //zero length
}

void skipQuorterBeatTrack(MidiTrack *track) {
  appendTrackValue(track,time_div/4); //to forward beat
  appendTrackByte(track,0xff);
  appendTrackByte(track,6); //dummy text metadata
  appendTrackByte(track,0); //zero length
}


void addChordToTrack(MidiTrack *track) {
  int i;
  int octaves=4;
  //turn on all notes of chord
  for (i=0;i<4;i++) if (chordNotes[i]!=-1) {
    appendTrackValue(track,0);
    appendTrackByte(track,0x9e);
    appendTrackByte(track,12*octaves+chordNotes[i]);
    appendTrackByte(track,127);
  } 
  skipBeatTrack(track);
  //turn off all notes of chord
  for (i=0;i<4;i++) if (chordNotes[i]!=-1) {
    appendTrackValue(track,0);
    appendTrackByte(track,0x9e);
    appendTrackByte(track,12*octaves+chordNotes[i]);
    appendTrackByte(track,0);
  } 
}

void addChordAccToTrack(MidiTrack *track) {
  int i;
  int octaves=4;
  //turn on all notes of chord
  for (i=0;i<4;i++) {
    if (chordNotes[i]!=-1) {
    appendTrackValue(track,0);
    appendTrackByte(track,0x9e);
    appendTrackByte(track,12*octaves+chordNotes[i]);
    appendTrackByte(track,127);
    }
    skipQuorterBeatTrack(track);
    if (chordNotes[i]!=-1) {
    appendTrackValue(track,0);
    appendTrackByte(track,0x9e);
    appendTrackByte(track,12*octaves+chordNotes[i]);
    appendTrackByte(track,0);
    }
  }
}


void printChordDescription() {
  int base=-1;
  int minor=0;
  int seventh=0;
  int sixth=0;
  int augmented=0;
  int diminished=0;
  int notes=chordNoteCount();
  selectChordNotes();
  int i,t;
  for (t=ChordTypeMajor;t<=ChordTypeMinorMajorSeventh;t++) {
    for (i=0;i<12;i++) {
      int d1=ChordDistanceList[t].v[0];
      int d2=ChordDistanceList[t].v[1];
      int d3=ChordDistanceList[t].v[2];
      int d4=ChordDistanceList[t].v[3];
      int noteCount=4;
      if (d4==-1) noteCount=3;
      if (selectedChord[(i+d1)%12] && selectedChord[(i+d2)%12] && selectedChord[(i+d3)%12] && ((noteCount==3 && notes==3) || (noteCount==4 && notes==4 && selectedChord[(i+d4)%12]))) {
	base=activeChord=i;
	chordType=t;
	
      }
    }
  }
  if (base==-1) {
    printf("\"X?X?\"");
  } else {
    printf("\"%c%c %s\"",notedescr[base*2],notedescr[base*2+1],ChordDistanceList[chordType].d);
  }
}

void reportChordNotes() {
  int i;
  printf("|");
  for (i=0;i<4;i++) if (chordNotes[i]!=-1) printf("%2d|",chordNotes[i]); else printf("  |");
}

void procChordDiv(int time,int type,int channel,int note,int vel,void *data) {
  if (!time_div) {
    fprintf(stderr,"no time_div, cant process event\n");
    return;
  }
  if (channel!=9 && time>=selectedTimeDiv*time_div && time<(selectedTimeDiv+1)*time_div & type==0x90 && vel!=0) {
    int midiTime=time % time_div;
    addChordNote(note);
  }
}

void procImportChordDiv(int time,int type,int channel,int note,int vel,void *data) {
  if (!time_div) {
    fprintf(stderr,"no time_div, cant process event\n");
    return;
  }
  if (channel!=9 && time>=selectedTimeDiv*time_div && time<(selectedTimeDiv+1)*time_div & type==0x90 && vel!=0) {
    int midiTime=time % meterLength;
    int chordSeqTime=midiTime*4000000/meterLength;
    int id=noteToId(note);
    if (id>=0) addChord(id,chordSeqTime);
  }
}

void importChordMeter(int meter,int trackid,int chord,int chordtype, int chordextend, int id) {
  //if (id==0) clearAllChords(); //we shoud do this outside this function but heck...
  int i;
  chordType=chordtype;
  chordExtend=chordextend;
  activeChord=chord;
  selectedMeter=meter;
  selectedTimeDiv=meter*8+id;
  parseTrack(&tracks[trackid],procImportChordDiv,NULL,NULL);  
}

void importChordAtTimeDiv(int time_id,int trackid) {
  clearSelectedChord();
  selectedTimeDiv=time_id;
  parseTrack(&tracks[trackid],procChordDiv,NULL,NULL);
}

void importChordAtTimeDivAlltracks(int time_id) {
  clearSelectedChord();
  selectedTimeDiv=time_id;
  int i;
  for (i=0;i<nTracks;i++) parseTrack(&tracks[i],procChordDiv,NULL,NULL);
}

void importChords() {
  if (time_div) time_div_count=1+songlength/time_div; else time_div_count=1;
    printf("%d tracks %d time_divisions\n",nTracks,time_div_count);
    
    int i,j;
    ChordNoteSequence importedSequence;
    MidiTrack chordsTrack;
    zeroTrack(&chordsTrack);
    printf("--- --- -- |");
    for (j=0;j<nTracks;j++) {printf("%2x|",j);}
    printf("\n--- --- -- |");
    for (j=0;j<nTracks;j++) {printf("--|");}
    printf("\n");
    for (i=0;i<time_div_count;i++) {
    importChordAtTimeDivAlltracks(i);
    printf("div %3d(%2d)|",i,chordNoteCount());
    chordsChannel=-1;
    int extraChordsChannel=-1;
    if (i%8==0) {
	    //if its the first time_div of the meter store the existing one and then clear
	    importedSequence.chords=chords;
	    importedSequence.chordsDuration=chordsDuration;
	    importedSequence.chordsSize=chordsSize;
	    
	    int bp=inChordNoteSequence(&importedSequence);
	    int prevbarid=bp;
	    if (bp) printf("\n%d already in bank %d\n",i,bp); else {
	      printf("\n%d not in any banks\n",i);
	      int storeid=storeFreeChordNoteSeq();
		if (storeid==-1) printf("out of free banks storing %d\n",i);
		else {
		  printf("stored in id %d\n",storeid);
		  prevbarid=storeid;
		}
	      }
	    clearAllChords();
	    if(i>0 && prevbarid!=-1) {
	      currenttime.bar=i/8-1;
	      currenttime.q=(i%8)/2;currenttime.tick=64*(i%2);
	      addAccompan(SelectAccopaniament,prevbarid);
	    }
	    if (i==0) {
                currenttime.bar=0;currenttime.q=0;currenttime.tick=0;
		addAccompan(AccopaniamentFlag,1);
	    }
	  }
    for (j=0;j<nTracks;j++) {
      importChordAtTimeDiv(i,j);
      int notecount=chordNoteCount();
      printf("%2d|",notecount);
      if (notecount==3 || notecount==4) {
	if (chordsChannel!=-1) {
	  //printf("\n multiple chord channels already at %d now at %d\n",chordsChannel,j);
	  extraChordsChannel=chordsChannel;
	}
	chordsChannel=j;
	int cncount=selectChordNotes();
	if (cncount>=4) {
	  printf("\nnote overflow, this should not have happend with a 3 or 4 note channel\n");
	  }
	}
      }
      printf("*");
      	  
      	if (chordsChannel!=-1) {
	  importChordAtTimeDiv(i,chordsChannel);
	  printf("tr%02d",chordsChannel);
	  selectChordNotes();
	  reportChordNotes();
	  printChordDescription();//this also sets activeChord,chordType,chordExtend currently
	  //this adds the chord to an extra track for the export mid
	  addChordAccToTrack(&chordsTrack);
	  //now lets try to import the accompaniament pattern 

	  //now import current time_div from midi //for id 0 we have cleared
	  importChordMeter(i/8,chordsChannel,activeChord,chordType,chordExtend, i%8);
	  //and place the chord in our song
	  currenttime.bar=i/8;currenttime.q=(i%8)/2;currenttime.tick=64*(i%2);
	  addAccompan(SelectChord,activeChord|chordType<<4|chordExtend<<5);
	  
	} else skipBeatTrack(&chordsTrack);
	if (extraChordsChannel!=-1) {
	  importChordAtTimeDiv(i,extraChordsChannel);
  	  printf("\textra tr%02d",extraChordsChannel);
	  selectChordNotes();
	  reportChordNotes();
	  printChordDescription();
	}
     printf("\n");
    }
}

int testChords(int argc,char **argv) {
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
  initChordNoteSeq();
  if (argc>1) {
    MidiLibGoSilent(1);
    printf("calling loadMidi\n");
    loadMidi(argv[1]);
    if (time_div) time_div_count=1+songlength/time_div; else time_div_count=1;
    printf("loadMidi finished %d tracks %d time_divisions\n",nTracks,time_div_count);
    
    int i,j;
    ChordNoteSequence importedSequence;
    MidiTrack chordsTrack;
    zeroTrack(&chordsTrack);
    printf("--- --- -- |");
    for (j=0;j<nTracks;j++) {printf("%2x|",j);}
    printf("\n--- --- -- |");
    for (j=0;j<nTracks;j++) {printf("--|");}
    printf("\n");
    for (i=0;i<time_div_count;i++) {
    importChordAtTimeDivAlltracks(i);
    printf("div %3d(%2d)|",i,chordNoteCount());
    chordsChannel=-1;
    int extraChordsChannel=-1;
    if (i%8==0) {
	    //if its the first time_div of the meter store the existing one and then clear
	    importedSequence.chords=chords;
	    importedSequence.chordsDuration=chordsDuration;
	    importedSequence.chordsSize=chordsSize;
	    int bp=inChordNoteSequence(&importedSequence);
	    if (bp) printf("\n%d already in bank %d\n",i,bp); else {
	      printf("\n%d not in any banks\n",i);
	      int storeid=storeFreeChordNoteSeq();
		if (storeid==-1) printf("out of free banks storing %d\n",i);
		else printf("stored in id %d\n",storeid);
	      }
	    clearAllChords();	      
	  }
    for (j=0;j<nTracks;j++) {
      importChordAtTimeDiv(i,j);
      int notecount=chordNoteCount();
      printf("%2d|",notecount);
      if (notecount==3 || notecount==4) {
	if (chordsChannel!=-1) {
	  //printf("\n multiple chord channels already at %d now at %d\n",chordsChannel,j);
	  extraChordsChannel=chordsChannel;
	}
	chordsChannel=j;
	int cncount=selectChordNotes();
	if (cncount>=4) {
	  printf("\nnote overflow, this should not have happend with a 3 or 4 note channel\n");
	  }
	}
      }
      printf("*");
      	  
      	if (chordsChannel!=-1) {
	  importChordAtTimeDiv(i,chordsChannel);
	  printf("tr%02d",chordsChannel);
	  selectChordNotes();
	  reportChordNotes();
	  printChordDescription();//this also sets activeChord,chordType,chordExtend currently
	  //this adds the chord to an extra track for the export mid
	  addChordAccToTrack(&chordsTrack);
	  //now lets try to import the accompaniament pattern 

	  //now import current time_div from midi //for id 0 we have cleared
	  importChordMeter(i/8,chordsChannel,activeChord,chordType,chordExtend, i%8);
	} else skipBeatTrack(&chordsTrack);
	if (extraChordsChannel!=-1) {
	  importChordAtTimeDiv(i,extraChordsChannel);
  	  printf("\textra tr%02d",extraChordsChannel);
	  selectChordNotes();
	  reportChordNotes();
	  printChordDescription();
	}
     printf("\n");
    }
    chordRestoreMenu();
    while(chords!=NULL) {
      chordsSeq();
      chordRestoreMenu();
    }
    FILE *outfile;
    char filename[128];
    sprintf(filename,"outfile.mid");
    outfile=fopen(filename,"rb");
    if (outfile) {
      fprintf(stderr,"%s exists already\n",filename);
      fclose(outfile);
      return -1;
    }
    outfile=fopen(filename,"wb");
    if (!outfile) {
      fprintf(stderr,"cant open %s for write\n",filename);
      return -1;
    }
    headchunk swappedheader;
    memcpy(&swappedheader.id,"MThd",4);
    swappedheader.size=6;
    swappedheader.type=1;
    swappedheader.tracks=nTracks+1;
    swappedheader.time_div=time_div;
    flipheaddata(&swappedheader);
    fwrite(&swappedheader,14,1,outfile);
    appendTrackEOF(&chordsTrack);
    int cTrack;
    for (cTrack=0; cTrack < nTracks; cTrack++) {
      int tracksize;
      char id[4];
      printf("writing track %d\n",cTrack);
      if (fwrite("MTrk",4,1,outfile)!=1) {fprintf(stderr,"cant write track header\n");exit(1);}
      tracksize=tracks[cTrack].size;
      printf("track size:%d\n",tracksize);
      flipdata((char*)&tracksize,4);
      if (fwrite(&tracksize,4,1,outfile)!=1) {fprintf(stderr,"cant write track size\n");exit(1);}
      if (fwrite(tracks[cTrack].data,tracks[cTrack].size,1,outfile)!=1) {fprintf(stderr,"failed to write track data\n");exit(-1);}
      if (tracks[cTrack].data!=NULL) free(tracks[cTrack].data);
      }

    printf("writing chords track\n");
    if (fwrite("MTrk",4,1,outfile)!=1) {fprintf(stderr,"cant write track header\n");exit(1);}
    int tracksize=chordsTrack.size;
    flipdata((char*)&tracksize,4);
    if (fwrite(&tracksize,4,1,outfile)!=1) {fprintf(stderr,"cant write track size\n");exit(1);}
    if (fwrite(chordsTrack.data,chordsTrack.size,1,outfile)!=1) {fprintf(stderr,"failed to write track data\n");exit(-1);}
    if (chordsTrack.data!=NULL) free(chordsTrack.data);
    fclose(outfile);
    //mainApp(1,NULL);
    
  }
}
