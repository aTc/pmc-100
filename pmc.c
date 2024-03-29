#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <getopt.h>
#include <sndfile.h>

#include "pmc.h"

int debug=0;

// read 1 bit from the audio stream.
int pmc_audio_getbit(pmc_data_t *pmc)
{
    float fb[2];
    float f;
    int hp1=pmc->sfinfo.samplerate/2000; // # samples in half wave 1khz
    int hp2=pmc->sfinfo.samplerate/4000; // # samples in half wave 2khz
    int r=(hp1-hp2)/3; // valid range ( +/- 4 samples at 48kHz) 
    float noise=0.1f; // ignore anything below
    float pf=0;
    int d=0,d0=0,d1=0;
    float min=0,max=0;


    while(sf_read_float(pmc->sndfile,&fb[0],1)!=0) // read samples until eof
    {
        f=fb[0];
        d++;
        if(f>max) max=f;
        if(f<min) min=f;
        if(signbit(f)!=signbit(pf)) // if sample flips sign
        {
            if(fabs((max-min)) > noise)
            {
                if(d>(hp2-r) && d<(hp2+r)) // if in range of 2khz
                {
                    d0=0; 
                    d1++;
                    if(d1==4) return 1; // 4 flips = 2 waves of 2khz
                }
                else if (d>(hp1-r) && d<(hp1+r)) // 1khz 
                {
                    d1=0;
                    d0++;
                    if(d0==2) return 0; // 2 flips = 1 wave of 1khz
                }
            }
            max=0;min=0;
            d=0;
        }
        pf=f;
    }
    return -1;
}

void pmc_free(pmc_data_t *pmc)
{
    if(pmc==0) return;
    if(pmc->data!=0) free(pmc->data);
    if(pmc->sndfile!=0) sf_close(pmc->sndfile);
    free(pmc);
    return;
}

// read bits and turn them into bytes
// bits are encoded with 1 start bit, 2 stop bits
int pmc_audio_getbyte(pmc_data_t *pmc)
{
    int bit[12];
    for(int i=0;i<12;i++) bit[i]=-1; // set all invalid
    int len=0;
    do
    {
        bit[10]=pmc_audio_getbit(pmc); // read next bit
        len++;
        //printf("[%d %d] ",len,bit[10]);
        if(bit[0]==0 && bit[9]==1 && bit[10]==1) // valid start/stop bits
        {
            int b=0;
            for(int i=1;i<9;i++) // 8 bits inbetween are the data
                b+=bit[i]<<(i-1);
            if(debug==1)
            {
                if(len>11) printf("*** leader ***\n");
                printf("%05d : %d ",len,bit[0]);
                for(int i=1;i<9;i++)
                    printf("%d",bit[i]);
                printf(" %d%d : 0x%02x %d\n",bit[9],bit[10],b,b);
                    
            }
            //printf("%02x",b);
            return b;
        }
        for(int i=0;i<10;i++) // push down the line
            bit[i]=bit[i+1];
    } while(bit[10]!=-1);
    return -1;
}

// puts first 28 bytes into the struct
// does some basic sanity checks
int pmc_header_parse(pmc_data_t *pmc)
{
    pmc->data_size=(pmc->header[0]<<8)+pmc->header[1];

    //if(pmc->tape_bytes < pmc->data_size) return -1;


    pmc->style=pmc->header[3];
    pmc->tempo=pmc->header[4];
    pmc->accomp=pmc->header[5];
    
    pmc->transpose=pmc->header[6]-5;
    pmc->finetune=pmc->header[7]-34;

    pmc->instr_acc1=pmc->header[10];
    pmc->instr_acc2=pmc->header[11];
    pmc->instr_acc3=pmc->header[12];
    pmc->instr_acc4=pmc->header[13];
    pmc->instr_bass=pmc->header[14];
    pmc->instr_mel=pmc->header[15];

    pmc->vol_acc1=pmc->header[16]&0x0f;
    pmc->vol_acc2=pmc->header[17]&0x0f;
    pmc->vol_acc3=pmc->header[18]&0x0f;
    pmc->vol_acc4=pmc->header[19]&0x0f;
    pmc->vol_bass=pmc->header[20]&0x0f;
    pmc->vol_mel=pmc->header[21]&0x0f;

    pmc->start_block=pmc->header[22];
    pmc->mode=pmc->header[23];
    pmc->bytes=((pmc->header[24]-12)<<8)+pmc->header[25];
    pmc->steps=(pmc->header[26]<<8)+pmc->header[27];


    if(pmc->data_size> 8*1024) return -2; // only 8k of ram
    if(pmc->tempo<60 || pmc->tempo>200) return -3; // tempo range 60-200    
    if(pmc->accomp>3) return -4;
    if(pmc->transpose<-5 || pmc->transpose>6) return -5;
    if(pmc->finetune<-34 || pmc->finetune>26) return -6;
    if(pmc->style > 12) return -7;
    return 0;
}


// prints header info
int pmc_header_dump(pmc_data_t *pmc)
{
    if(pmc==0) return 0;
    printf("Data size :%d\n",pmc->data_size);
    printf("Accomp style: %d %s\n",pmc->style,pmc_style_str[pmc->style]);
    printf("Accomp mode: %d %s\n",pmc->accomp,pmc_accomp_str[pmc->accomp]);
    printf("Tempo: %d\n",pmc->tempo);
    printf("Transpose: %d\n",pmc->transpose);
    printf("Finetune: %d\n",pmc->finetune);
    printf("Instr Acc1 : V%02d (%03d) %s\n",pmc->vol_acc1,pmc->instr_acc1,pmc_instr_str[pmc->instr_acc1]);
    printf("Instr Acc2 : V%02d (%03d) %s\n",pmc->vol_acc2,pmc->instr_acc2,pmc_instr_str[pmc->instr_acc2]);
    printf("Instr Acc3 : V%02d (%03d) %s\n",pmc->vol_acc3,pmc->instr_acc3,pmc_instr_str[pmc->instr_acc3]);
    printf("Instr Acc4 : V%02d (%03d) %s\n",pmc->vol_acc4,pmc->instr_acc4,pmc_instr_str[pmc->instr_acc4]);
    printf("Instr Bass : V%02d (%03d) %s\n",pmc->vol_bass,pmc->instr_bass,pmc_instr_str[pmc->instr_bass]);
    printf("Instr Mel  : V%02d (%03d) %s\n",pmc->vol_mel,pmc->instr_mel,pmc_instr_str[pmc->instr_mel]);
    printf("Start block : %d\n",pmc->start_block);
    printf("Mode: %d %s\n",pmc->mode,pmc_mode_str[pmc->mode]);
    printf("Bytes: %d\n",pmc->bytes);
    printf("Steps: %d\n",pmc->steps);
    
    return 0;
}

// writes data read from audio to binary file
// without leaders and start/stop bits
int pmc_write_bin(char *filename,pmc_data_t *pmc)
{
    FILE *f;
    f=fopen(filename,"wb");
    if(f==0) return 0;
    int i;
    fwrite(&pmc->header,sizeof(pmc->header),1,f);
    fwrite(pmc->data,pmc->tape_bytes,1,f);
    fclose(f);

}

// helper function to write variable size midi data
int write_var(int v,FILE *f)
{
    uint64_t buffer;
    int l=0;
    buffer= v & 0x7f;
    while((v >>=7) >0)
    {
        buffer <<=8;
        buffer |=0x80;
        buffer +=(v & 0x7f);
    }

    while(1)
    {
        fputc(buffer,f);
        l++;
        if(buffer & 0x80)
            buffer>>=8;
        else
            break;
    }
    return l;
}


                                                                
// midi helper function to write chord note on/off
int write_chord(int ch,int n,int c,int on,FILE *f)
{
    uint8_t cmd;
    if(on==0) cmd=0x80+ch; else cmd=0x90+ch;

    fputc(cmd,f);
    fputc(n,f);
    fputc(96,f);

    fputc(0x00,f);
    fputc(cmd,f);
    fputc(n+pmc_chord_ofs[c][0],f);
    fputc(96,f);
   
    fputc(0x00,f);
    fputc(cmd,f);
    fputc(n+pmc_chord_ofs[c][1],f);
    fputc(96,f);

    if(pmc_chord_ofs[c][2]!=0)
    {
        fputc(0x00,f);
        fputc(cmd,f);
        fputc(n+pmc_chord_ofs[c][2],f);
        fputc(96,f);
    }

}

// create midi file out of read data
int pmc_write_midi(char *filename,pmc_data_t *pmc)
{
    char midi_header0[]={'M','T','h','d',0,0,0,6,0,0,0,1,0,24};
    char midi_track_header[]={'M','T','r','k',0,0,0,0};
    char midi_header1[]={'M','T','h','d',0,0,0,6,0,1,0,2,0,24};

    if (pmc==0) return 0;

    FILE *f;
    f=fopen(filename,"wb");
    //int tempo=60000000/pmc->tempo; // real bpm 
    int tempo=59054166/pmc->tempo; // pmc timing is slightly off

    // realtime mode has list of chords + list of notes
    // step mode combines them, write single track mode0 midi file for step,
    // write multi track mode1 midi file for realtime
    if(pmc->mode==1) 
        fwrite(midi_header0,sizeof(midi_header0),1,f);
    else if (pmc->mode >=2 && pmc->mode<=5)
        fwrite(midi_header1,sizeof(midi_header1),1,f);

    // start first track
    fwrite(midi_track_header,sizeof(midi_track_header),1,f);
    long midi_start=ftell(f); // to calculate track length in bytes

    // write tempo
    fputc(0x00,f);
    fputc(0xff,f);
    fputc(0x51,f);
    fputc(0x03,f);
    fputc((tempo>>16)&0xff,f);
    fputc((tempo>>8)&0xff,f);
    fputc((tempo&0xff),f);

    int o=0;
    int c=0,n=0;
    int pc=-1,pn=-1,pcc=0;
    int delta=0;
    int midi_len=0;

    if(pmc->mode==1) // step mode
    {
        o=0;
        do{
            if(pmc->data[o]!=0xff)
            {
                if(pmc->data[o]==128) // if chord data
                {
                    n=pmc->data[o+1]&0xf;
                    c=(pmc->data[o+1]>>4)&0xf;
                    if(pc!=-1) // turn off previous chord
                    {
                        write_var(delta,f);
                        write_chord(1,pc+48+7,pcc,0,f);
                        delta=0;
                    }
                    if(c!=0xe) // if not rest
                    {
                        write_var(delta,f);
                        write_chord(1,n+48+7,c,1,f);
                        delta=0;
                        pc=n;pcc=c;
                    }
                    else
                        pc=-1;
                }
                else  // note data
                {
                    n=pmc->data[o];        // note
                    c=pmc->data[o+1]&0x3f; // length
                    if(pn!=-1) // turn off previous note
                    {
                        write_var(delta,f);
                        fputc(0x80,f);
                        fputc(pn,f);
                        fputc(0,f);
                        delta=0;
                    }
                    if(n!=0) // if not rest
                    {
                        write_var(delta,f);
                        fputc(0x90,f);
                        fputc(n,f);
                        fputc(96,f);
                        pn=n;
                        delta=0;
                    }
                    else
                        pn=-1;
                    delta+=pmc_len_count[c]; // time until next note
                }
            }
        } while(pmc->data[o+=2]!=0xff); // 0xff==end of data

        // turn off any notes/chord that are still playing
        if(pc!=-1)
        {
            write_var(delta,f);
            write_chord(1,pc+48+7,pcc,0,f);
            fputc(0x81,f);
            fputc(48+7+pc,f);
            fputc(96,f);
            delta=0;
        }
        if(pn!=-1)
        {
            write_var(delta,f);
            fputc(0x80,f);
            fputc(pn,f);
            fputc(96,f);
            delta=0;
        }
    
        //write end of track
        fputc(0x00,f);
        fputc(0xff,f);
        fputc(0x2f,f);
        fputc(0x00,f);
        midi_len=ftell(f)-midi_start; // how many bytes did we write 
        fseek(f,midi_start-4,SEEK_SET); // rewind to begin of track header
        fputc((midi_len>>24)&0xff,f); // write length
        fputc((midi_len>>16)&0xff,f);
        fputc((midi_len>> 8)&0xff,f);
        fputc((midi_len    )&0xff,f);

    }
    else if(pmc->mode>=2 && pmc->mode<=5) // realtime modes
    {
        o=0;
        do  // first write chords
        {
            if(pmc->data[0]!=0xff) 
            {
                n=pmc->data[o+1]&0xf;
                c=(pmc->data[o+1]>>4)&0xf;
                if(pc!=-1)
                {
                    write_var(delta,f);
                    fputc(0x81,f);
                    fputc(48+7+pc,f);
                    fputc(96,f);
                    delta=0;
                }
                if(c!=0xe)
                {
                    write_var(delta,f);
                    fputc(0x91,f);
                    fputc(40+7+n,f);
                    fputc(96,f);
                    delta=0;
                    pc=n;
                }
                else
                    pc=-1;
                delta+=pmc_len_count[pmc->data[o+2]];
            }
        } while(pmc->data[o+=3]!=0xff);
     
        // turn off any chord still playing
        if(pc!=-1)
        {
            fputc(delta,f);
            fputc(0x81,f);
            fputc(48+7+pc,f);
            fputc(96,f);
            delta=0;
        }
        // end of track
        fputc(0x00,f);
        fputc(0xff,f);
        fputc(0x2f,f);
        fputc(0x00,f);
        midi_len=ftell(f)-midi_start; // calculate length
        fseek(f,midi_start-4,SEEK_SET); // rewind to header
        fputc((midi_len>>24)&0xff,f); // write length
        fputc((midi_len>>16)&0xff,f);
        fputc((midi_len>>8 )&0xff,f);
        fputc((midi_len   )&0xff,f);
        fseek(f,0,SEEK_END); // go to end of file
        fwrite(midi_track_header,sizeof(midi_track_header),1,f); // next track
        midi_start=ftell(f); // to keep track of bytes written
        
        c=0;n=0;pc=-1;pn=-1;delta=0;
        o=256*3;
        do // write notes to the new track
        {
            if(pmc->data[o]!=0xff)
            {
                n=pmc->data[o];
                c=pmc->data[o+1]&0x3f;
                if(pn!=-1) 
                {
                    write_var(delta,f);
                    fputc(0x80,f);
                    fputc(pn,f);
                    fputc(0,f);
                    delta=0;
                }
                if(n!=0)
                {
                    write_var(delta,f);
                    fputc(0x90,f);
                    fputc(n,f);
                    fputc(96,f);
                    pn=n;
                    delta=0;
                }
                else
                    pn=-1;
                delta+=pmc_len_count[c];
            }
        } while(pmc->data[o+=2]!=0xff);

        if(pc!=-1)
        {
            fputc(delta,f);
            fputc(0x81,f);
            fputc(48+7+pc,f);
            fputc(96,f);
            delta=0;
        }
        fputc(0x00,f);
        fputc(0xff,f);
        fputc(0x2f,f);
        fputc(0x00,f);
        midi_len=ftell(f)-midi_start;
        fseek(f,midi_start-4,SEEK_SET);
        fputc((midi_len>>24)&0xff,f);
        fputc((midi_len>>16)&0xff,f);
        fputc((midi_len>>8 )&0xff,f);
        fputc((midi_len   )&0xff,f);
    }
    fclose(f);
    return 0;
}

// print song data
int pmc_song_dump(pmc_data_t *pmc)
{
    if(pmc==0) return 0;

    int step=1;
    int o=0;
    int n,c;
    if(pmc->mode==1) // step mode
    {
        do
        {
            printf("%04d ",step);
            if(pmc->data[o]==128) // chord
            {
                n=pmc->data[o+1]&0xf;
                c=(pmc->data[o+1]>>4)&0xf;
                printf("Chord: %s %s ",pmc_chordnote_str[n],pmc_chord_str[c]);
            }
            else if(pmc->data[o]!=0xff) // note
            {
                n=pmc->data[o];
                c=n/12; //octave
                if(n==0) 
                    printf(" -- ");
                else
                    printf(" %s%d",pmc_note_str[n%12],c);

                n=pmc->data[o+1]&0x3f;  // lower bits=note length
                if(pmc->data[o+1]&0x40)  // upper bits=tie
                    printf(" | ");
                else
                    printf("   ");
                printf(" %s",pmc_len_str[n]);
                step++;
            }
            printf("\n");
        } while(pmc->data[o+=2]!=0xff);
        printf("%04d *** END ***\n",step);
        return 0;
    }

    if(pmc->mode>=2 && pmc->mode<=5) // realtime mode
    {
        printf("*** Chords ***\n");
        do
        {
            if(pmc->data[o]!=0xff)
            {
                printf("%04d ",step++);                
                n=pmc->data[o+1]&0xf;
                c=(pmc->data[o+1]>>4)&0xf;
                printf("Chord: %s %s ",pmc_chordnote_str[n],pmc_chord_str[c]);
                n=pmc->data[o+2]&0x3f;
                if(pmc->data[o+2]&0x40) 
                    printf(" | "); 
                else
                    printf("   ");
                printf("%s",pmc_len_str[n]);
            }
            printf("\n");
        } while(pmc->data[o+=3]!=0xff);
        printf("%04d *** END ***\n",step);

        step=1;
        o=(pmc->start_block+1)*256;
        printf("*** Notes ***\n");
        do
        {
         if(pmc->data[o]!=0xff) // note
            {
                printf("%04d ",step++);
                n=pmc->data[o];
                c=n/12; //octave
                if(n==0) 
                    printf(" -- ");
                else
                    printf(" %s%d",pmc_note_str[n%12],c);

                n=pmc->data[o+1]&0x3f;  // lower bits=note length
                if(pmc->data[o+1]&0x40)  // upper bits=tie
                    printf(" | ");
                else
                    printf("   ");
                printf(" %s",pmc_len_str[n]);
            }
            printf("\n");
        } while(pmc->data[o+=2]!=0xff);
        printf("%04d *** END ***\n",step);
        return 0;
    }
    return 0;
}

// read pmc data from audio file
pmc_data_t *pmc_audio_read(char *filename)
{

    pmc_data_t *pmc;
    pmc=(pmc_data_t*)calloc(1,sizeof(pmc_data_t));
    
    if (! (pmc->sndfile = sf_open (filename,SFM_READ,&pmc->sfinfo)))
    {
        return 0;
    }  

    int s=0;
    int b=0;

    int i;
    for(i=0;i<28;i++) // first 28 bytes are the header
    {
        b=pmc_audio_getbyte(pmc);
        //printf("%d : %x\n",i,b);
        if(b==-1) break;
        pmc->header[i]=b;
    }
    if(b==-1) printf("read fail\n");
    i=pmc_header_parse(pmc); // fill in the struct and check if data valid
    if(i!=0) { printf("%d\n",i);pmc_free(pmc);return 0;}

    // data after the header comes in 256 byte blocks
    // so there should be a multiple of 256 bytes folowing
    pmc->tape_bytes=(pmc->data_size&0xff00)+0x100; 
    pmc->data=(uint8_t*)calloc(pmc->tape_bytes,1);
    //printf("tape bytes: %d\n",pmc->tape_bytes);
    for(i=0;i<pmc->tape_bytes;i++) 
    {
        b=pmc_audio_getbyte(pmc);
        if(b==-1) break;
        pmc->data[i]=b;
    }
    // invalid if not enough bytes could be read
    if(b==-1) { printf("data read error: %d\n",i);pmc_free(pmc);return 0;} 

    return pmc;
}

// create wav file that can be read back into the pmc
int pmc_wav(char *filename,pmc_data_t *pmc)
{
    int16_t s1[48];
    int16_t s0[48];
    int16_t *sn;
    int i=0;
    if(pmc==0) return 0;
    SF_INFO *sfinfo;
    SNDFILE *sf;
    sn=calloc(4000,sizeof(int16_t)); // silence
    sfinfo=calloc(1,sizeof(SF_INFO));
    sfinfo->samplerate=48000;
    sfinfo->channels=1;
    sfinfo->format=SF_FORMAT_WAV|SF_FORMAT_PCM_16;
    sf=sf_open(filename,SFM_WRITE,sfinfo);

    // s0 = 1 period of 1kHz
    // s1 = 2 periods of 2kHz
    for(int i=0;i<48;i++)
    {
        s0[i]=25000*sin((float)i*0.1308996939);
        s1[i]=25000*sin((float)i*0.2617993878);
    }
    sf_write_short(sf,sn,4000); // silence;

    for(i=0;i<4400;i++)
        sf_write_short(sf,s1,48); // leader
                                  
    for(i=0;i<28;i++) // write header
    {
        sf_write_short(sf,s0,48); // 1 start bit

        for(int b=0;b<8;b++) // go through each bit
        {
            if(pmc->header[i]&(1<<b))
                sf_write_short(sf,s1,48); // 1
            else
                sf_write_short(sf,s0,48); // 0
        }
        sf_write_short(sf,s1,48); // 2 stop bits
        sf_write_short(sf,s1,48);
    }

    int bw=0;
    do
    {
        sf_write_short(sf,sn,4000); // silence;
        for(i=0;i<500;i++)
            sf_write_short(sf,s1,48);  //leader
        for(i=0;i<256;i++)
        {
            sf_write_short(sf,s0,48);
            for(int b=0;b<8;b++)
                if(pmc->data[bw]&(1<<b))
                    sf_write_short(sf,s1,48);
                else
                    sf_write_short(sf,s0,48);
            sf_write_short(sf,s1,48);
            sf_write_short(sf,s1,48);
            bw++;
        }
    } while (bw<pmc->tape_bytes);

        sf_write_short(sf,sn,4000); // silence;

    sf_close(sf);
    free(sfinfo);
}

void usage()
{
                printf("Usage: \n");
                printf("pmc [options] [input audio file]\n\n");
                printf("parses and converts audio data from Philips PMC 100\n");
                printf("-m, --midi FILENAME \t write midi file\n");
                printf("-w, --wav  FILENAME \t write wav\n");
                printf("-b, --binary FILENAME\t write raw binary data to file\n");
                printf("-p, --print          \t print header/song data (default)\n");
                printf("-d, --debug          \t output debug info while reading\n");

    return;
}



int main(int argc,char **argv)
{
    pmc_data_t *pmc;
    
    static struct option longopts[]={
        {"midi",required_argument,NULL,'m'},
        {"print",no_argument,NULL,'p'},
        {"wav",required_argument,NULL,'w'},
        {"binary",required_argument,NULL,'b'},
        {"debug",no_argument,NULL,'d'},
        {NULL,0,NULL,0}
    };
   
    int ch;
    int p=0;
    char *mname=NULL;
    char *wname=NULL;
    char *bname=NULL;
    while((ch=getopt_long_only(argc,argv,"",longopts,NULL)) !=-1)
    {
        switch(ch){
            case 'm':
                if(optarg!=0)
                {
                    mname=optarg;

                }
                break;
            case 'w':
                if(optarg!=0)
                {
                    wname=optarg;
                }
                break;
            case 'p':
                p=1;
                break;
            case 'b':
                if(optarg!=0)
                    bname=optarg;
                break;
            case 'd':
                debug=1;
                break;
            case 0:
                printf("*** 0 ***\n");
                break;
            default:
                usage();
                return 0;
                break;
        }
    }
    
    //if(wname!=0) printf("wname: %s\n",wname);
    //if(mname!=0) printf("mname: %s\n",mname);

    //printf("%d \n",optind);

    if(optind < argc)
    {
        pmc=pmc_audio_read(argv[optind++]);
        if(pmc==0) return 0;
        if(p==1 || (wname==0 && mname==0 && bname==0))
        {
            pmc_header_dump(pmc);
            printf("\n\n");
            pmc_song_dump(pmc);
        }
        if(mname!=0) pmc_write_midi(mname,pmc);
        if(bname!=0) pmc_write_bin(bname,pmc);
        if(wname!=0) pmc_wav(wname,pmc);

         if(pmc!=0) pmc_free(pmc);
        //printf(" input: %s\n",argv[optind++]);
    }
    else
    {
        printf(" need input file\n");
        usage();
    }
        
    


    return 0;
    if(argc!=2) return 1;

    pmc=pmc_audio_read(argv[1]);
    if(pmc==0) return 0; 
    pmc_header_dump(pmc);
    pmc_write_bin("out1.pmc",pmc);
    printf("-----------------------\n");
    pmc_song_dump(pmc);
    pmc_write_midi("pmc.mid",pmc);
    pmc_wav("pmc.wav",pmc);

    if(pmc!=0) pmc_free(pmc);

    return 0;
}
