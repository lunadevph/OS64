#include "tty.h"
static void copy(char*d,const char*s,size_t cap){size_t n=0;while(s&&s[n]&&n+1<cap){d[n]=s[n];n++;}d[n]=0;}
void tty_init(tty_t*t){if(!t)return;t->length=t->cursor=t->line_count=0;t->input[0]=0;}
void tty_clear(tty_t*t){if(!t)return;t->line_count=0;}
void tty_write_line(tty_t*t,const char*s){if(!t)return;if(t->line_count==TTY_SCROLLBACK){for(size_t i=1;i<TTY_SCROLLBACK;i++)copy(t->lines[i-1],t->lines[i],TTY_LINE_LENGTH);t->line_count--;}copy(t->lines[t->line_count++],s,TTY_LINE_LENGTH);}
void tty_write_char(tty_t*t,char c){if(!t)return;if(!t->line_count)tty_write_line(t,"");if(c=='\r')return;if(c=='\n'){tty_write_line(t,"");return;}char*line=t->lines[t->line_count-1];size_t n=0;while(line[n]&&n+1<TTY_LINE_LENGTH)n++;if(c=='\b'){if(n)line[n-1]=0;return;}if(n+1>=TTY_LINE_LENGTH){tty_write_line(t,"");line=t->lines[t->line_count-1];n=0;}line[n]=c;line[n+1]=0;}
int tty_input_key(tty_t*t,unsigned key,char*command,size_t cap){
 if(!t)return 0;
 if(key=='\n'){if(command&&cap)copy(command,t->input,cap);t->length=t->cursor=0;t->input[0]=0;return 1;}
 if((key==8||key==127)&&t->cursor){for(size_t i=--t->cursor;i<t->length;i++)t->input[i]=t->input[i+1];t->length--;return 0;}
 if(key==0x13&&t->cursor){t->cursor--;return 0;}if(key==0x14&&t->cursor<t->length){t->cursor++;return 0;}
 if(key>=32&&key<127&&t->length+1<TTY_LINE_LENGTH){for(size_t i=t->length+1;i>t->cursor;i--)t->input[i]=t->input[i-1];t->input[t->cursor++]=(char)key;t->length++;}
 return 0;
}
