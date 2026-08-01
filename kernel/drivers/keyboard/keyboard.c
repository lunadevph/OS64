#include "keyboard.h"
#include "io.h"
#include <stdint.h>
static int shift,ctrl,alt,caps,extended,ready;
static int input_ready(void){for(unsigned n=0;n<100000;n++)if(!(inb(0x64)&2))return 1;return 0;}
static int output_ready(void){for(unsigned n=0;n<100000;n++)if(inb(0x64)&1)return 1;return 0;}
static int command(uint8_t v){if(!input_ready())return 0;outb(0x64,v);return 1;}
static int data(uint8_t v){if(!input_ready())return 0;outb(0x60,v);return 1;}
static uint8_t read_data(void){return output_ready()?inb(0x60):0;}
static int keyboard_command(uint8_t v){for(int retry=0;retry<3;retry++){if(!data(v))return 0;uint8_t r=read_data();if(r==0xfa)return 1;if(r!=0xfe)return 0;}return 0;}
int keyboard_init(void){shift=ctrl=alt=caps=extended=ready=0;command(0xad);command(0xa7);while(inb(0x64)&1)inb(0x60);if(!command(0x20))return 0;uint8_t config=read_data();config=(uint8_t)((config|0x40)&~0x10);if(!command(0x60)||!data(config)||!command(0xae))return 0;if(!data(0xff))return 0;uint8_t ack=read_data();if(ack!=0xfa)return 0;uint8_t bat=read_data();if(bat!=0xaa&&bat!=0)return 0;if(!keyboard_command(0xf0)||!keyboard_command(0x02)||!keyboard_command(0xf4))return 0;ready=1;return 1;}
static char letter(char lower){int upper=(caps?1:0)^(shift?1:0);return upper?(char)(lower-'a'+'A'):lower;}
char keyboard_poll(void){
 static const char normal[128]={[1]=27,[2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',[12]='-',[13]='=',[14]='\b',[15]='\t',[26]='[',[27]=']',[28]='\n',[39]=';',[40]='\'',[41]='`',[43]='\\',[51]=',',[52]='.',[53]='/',[57]=' '};
 static const char shifted[128]={[2]='!',[3]='@',[4]='#',[5]='$',[6]='%',[7]='^',[8]='&',[9]='*',[10]='(',[11]=')',[12]='_',[13]='+',[26]='{',[27]='}',[39]=':',[40]='"',[41]='~',[43]='|',[51]='<',[52]='>',[53]='?',[57]=' '};
 if(!ready||!(inb(0x64)&1))return 0;
 uint8_t s=inb(0x60);
 if(s==0xe0){extended=1;return 0;}
 int released=s&0x80;
 s&=0x7f;
 if(s==42||s==54){shift=!released;return 0;}if(s==29){ctrl=!released;return 0;}if(s==56){alt=!released;return 0;}if(s==58&&!released){caps=!caps;return 0;}if(released){extended=0;return 0;}
 if(extended){extended=0;if(s==72)return 0x11;if(s==80)return 0x12;if(s==75)return 0x13;if(s==77)return 0x14;if(s==71)return (char)0x91;if(s==79)return (char)0x92;if(s==73)return (char)0x93;if(s==81)return (char)0x94;if(s==82)return (char)0x95;if(s==83)return (char)0x96;return 0;}
 if(s>=59&&s<=68)return (char)(0x80+s-58);
 if(s==15&&shift)return (char)0x8f;
 char value=0;if(s>=16&&s<=25){static const char q[]="qwertyuiop";value=letter(q[s-16]);}else if(s>=30&&s<=38){static const char a[]="asdfghjkl";value=letter(a[s-30]);}else if(s>=44&&s<=50){static const char z[]="zxcvbnm";value=letter(z[s-44]);}else value=shift?shifted[s]:normal[s];
 if(ctrl&&value>='a'&&value<='z')return (char)(value-'a'+1);
 if(alt&&value)return (char)((uint8_t)value|0x80);
 return value;
}
