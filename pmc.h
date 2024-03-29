#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <sndfile.h>

typedef struct pmc_data_t
{
    uint8_t header[28];
    uint8_t *data;
    uint16_t data_size;
    uint8_t style;
    uint8_t tempo;
    uint8_t accomp;
    int8_t transpose;
    int8_t finetune;
    uint8_t instr_acc1;
    uint8_t instr_acc2;
    uint8_t instr_acc3;
    uint8_t instr_acc4;
    uint8_t instr_bass;
    uint8_t instr_mel;
    uint8_t vol_acc1;
    uint8_t vol_acc2;
    uint8_t vol_acc3;
    uint8_t vol_acc4;
    uint8_t vol_bass;
    uint8_t vol_mel;
    uint8_t start_block;
    uint8_t mode;
    uint16_t bytes;
    uint16_t steps;
    SNDFILE *sndfile;
    SF_INFO sfinfo;
    int tape_bytes;
} pmc_data_t;

uint8_t pmc_chord_ofs[][3]={ {0,0,0}, // 0
                             {4,7,0}, // M
                             {3,7,0}, // m
                             {4,7,10},// 7
                             {3,7,10},//m7
                             {4,7,11},//M7 
                             {4,7,9}, // 6
                             {3,6,9}, //m6
                             {4,6,10},//+5
                             {3,6,10},//m7-5
                             {3,6,0}, //DIM
                             {5,7,0}, //SUS4
                             {2,4,7}, //M+9
                             {2,3,7}//m+9
                            };

char midi_header[]={'M','T','h','d',0,0,0,6,0,0,0,1,0,24,
                    'M','T','r','k',0,0,0,0};

char *pmc_style_str[13]={"Slow Rock","Ballad","Swing","March","Country","Waltz","Disco","Funk","Rock'N' Roll","Pop","Reggae","Latin","User"};

char *pmc_accomp_str[4]={"Arranged","Varistrum","Sustained","Off"};

char *pmc_mode_str[5]={"Demo","StepTime","Gling","SuperGling","Promode"};

char *pmc_chord_str[16]={"0   ","M   ","m   ","7   ","m7  ","M7  ","6   ","m6  ","+5   ","m7-5","DIM ","SUS4","M+9 ","m+9 ","NC  ","--- "};
                    //0,     47 ,  37      47 10  3710   47 11   479    369    46 10   36 10   36     57     247    237   
char *pmc_chordnote_str[16]={"G ","G#","A ","A#","B ","C ","C#","D ","D#","E ","F ","F#","-","-","-","-"};

char *pmc_note_str[12]={"C ","C#","D ","D#","E ","F ","F#","G ","G#","A ","A#","B "};

char *pmc_len_str[18]={
    "1/1   ","1/2   ","1/4   ","1/8   ","1/16  ","1/32  ",
    "1/1.  ","1/2.  ","1/4.  ","1/8.  ","1/16. ","----- ",
    "1/1 3 ","1/2 3 ","1/4 3 ","1/8 3 ","1/16 3","1/32 3" };

char pmc_len_count[18]={
    32*3,16*3,8*3,4*3,2*3,1*3,
    48*3,24*3,12*3,6*3,3*3,0*3,
    64,32,16,8,4,2};
                   

char *pmc_instr_str_nl[101]={
"0","Violen","Spaanse gitaar","Synthesizer-piano","Steel drum","Clarinet","Marimba","Koper","Mondharmonica","Hoorn",
"Synthesizer","Getokkeld","Vibrafoon","Rock-bas","Elektrische bas","Banjo","Funk-bas","Slagbas","Synthesizer-bas","Slam bas",
"Click bas","Bas","Rasp bas","Piano","Elektrische piano","Elektrische piano 2","Akoestische piano","Speelgoedpiano","Clavecimbel","Accordi",
"Synth lead","Synth klok","Synth sweep","Synth pipe","Synth-bel","Synth solo","Synth fuzz","Synth fuzz solo","Synthi","Synth shimmer",
"Synth wah","---","Blues gitaar","Rock-gitaar","Lead-gitaar","Slaggitaar","Hit gitaar","Sitar","Harp","Pop-hard",
"Koto","Synth snaren","Viool","Klokkenspel","Vibrochime","Chimese","Sideglock","Jime","Kerstklokje","Fuzz chime",
"Astro chime","Bel","Handbel","Ruimte-bel","Stick bell","Fuzz bell","Bell synth","Sizzle gong","Trompet","Trombone",
"Tuba","Wah brass","Bell brass","Brass Synth","Dwarsfluit","Piccolo","Hobo","Saxofoon","Fagot","Doedelzak",
"Panfluit","Breath pipe","Jazz pipe","Andes-fluit","Mondharp","Alien","Spacewarp","Wow","Zime","Sparkle",
"Quack","Space ring","Fuzz star","Tonk","Planet X","Fuzz wah","Zip","Ping","Fuzz ring","Reverse synth",
"Vocodex"};

char* pmc_instr_str[101]={
    "0","Strings","Spanish guitar","Synth piano","Steel drum","Clarinet","Marimba","Brass","Harmonica","Horn",
    "Synth","Plucked","Vibraphone","Rock bass","Elec. bass","Banjo","Funk bass","Slap bass","Synth bass","Slam bass",
    "Click bass","Bass","Rasp bass","Piano","Elec. piano","Elec. piano 2","Live piano","Toy piano","Harpsichord","Accordi",
    "Synth lead","Synth bell","Synth sweep","Synth pipe","Synth chime","Synth solo","Synth fuzz","Synth fuzz solo","Synthi","Synthonic",
    "Synth shimmer","Synth wah","Blues guitar","Rock guitar","Lead guitar","Guitar strum","Guitar hit","Sitar","Harp","Pop harp",
    "Koto","Synth string","Violin","Glockenspiel","Vibrochime","Chimese","Sideglock","Jime","Xmas chime","Fuzz chime",
    "Astro chime","Bell","Hand bell","Space bell","Stick bell","Fuzz bell","Bell synth","Sizzle gong","Trumpet","Trombone",
    "Tuba","Wah brass","Bell brass","Brass synth","Flute","Piccolo","Oboe","Saxophone","Bassoon","Bagpipes",
    "Pan pipes","Breath pipe","Jazz pipe","Andes pipe","Jaws harp","Alien","Spacewarp","Wow","Zime","Sparkle",
    "Quack","Space ring","Fuzz star","Tonk","Planet X","Fuzz wah","Zip","Ping","Fuzz ring","Reverse synth",
    "Vocodex"};

