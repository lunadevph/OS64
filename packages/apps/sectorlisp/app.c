/* OS64 port of jart/sectorlisp's portable reference. See LICENSE. */
#include "app_abi.h"

#define K_T 4
#define K_QUOTE 6
#define K_COND 12
#define K_READ 17
#define K_PRINT 22
#define K_ATOM 28
#define K_CAR 33
#define K_CDR 37
#define K_CONS 41
#define K_EQ 46
#define M (ram + 32768)

static const char builtins[]="NIL\0T\0QUOTE\0COND\0READ\0PRINT\0ATOM\0CAR\0CDR\0CONS\0EQ";
static int ram[65536],cx;
static const os64_api_t *os;
static const char *source;

static int car(int x){return M[x];}
static int cdr(int x){return M[x+1];}
static int cons(int a,int d){if(cx<=-32760)return 0;M[--cx]=d;M[--cx]=a;return cx;}
static int eval(int e,int a);
static int intern(const char *name){int pos=0;while(pos<32700&&M[pos]){int i=0;while(name[i]&&M[pos+i]==name[i])i++;if(!name[i]&&!M[pos+i])return pos;while(M[pos])pos++;pos++;}int start=pos;while(*name&&pos<32766)M[pos++]=(unsigned char)*name++;M[pos]=0;return start;}
static int assoc(int x,int list){while(list){if(x==car(car(list)))return cdr(car(list));list=cdr(list);}return 0;}
static int pairlis(int x,int y,int a){return x?cons(cons(car(x),car(y)),pairlis(cdr(x),cdr(y),a)):a;}
static int evlis(int m,int a){if(!m)return 0;int x=eval(car(m),a);return cons(x,evlis(cdr(m),a));}
static int evcon(int c,int a){if(!c)return 0;if(eval(car(car(c)),a))return eval(car(cdr(car(c))),a);return evcon(cdr(c),a);}
static void print_object(int x);
static void print_atom(int x){do{os->putc((char)M[x]);}while(M[x++]);}
static void print_list(int x){os->putc('(');print_object(car(x));while((x=cdr(x))){if(x<0){os->putc(' ');print_object(car(x));}else{os->write(" . ");print_object(x);break;}}os->putc(')');}
static void print_object(int x){if(x<0)print_list(x);else print_atom(x);}
static int apply(int f,int x,int a){if(f<0)return eval(car(cdr(cdr(f))),pairlis(car(cdr(f)),x,a));if(f>K_EQ)return apply(eval(f,a),x,a);if(f==K_EQ)return car(x)==car(cdr(x))?K_T:0;if(f==K_CONS)return cons(car(x),car(cdr(x)));if(f==K_ATOM)return car(x)<0?0:K_T;if(f==K_CAR)return car(car(x));if(f==K_CDR)return cdr(car(x));if(f==K_PRINT){if(x)print_object(car(x));os->putc('\n');return 0;}return 0;}
static int eval(int e,int a){if(e>=0)return assoc(e,a);if(car(e)==K_QUOTE)return car(cdr(e));if(car(e)==K_COND)return evcon(cdr(e),a);return apply(car(e),evlis(cdr(e),a),a);}

static void skip(void){while(*source==' '||*source=='\t'||*source=='\r'||*source=='\n')source++;}
static int parse_object(void);
static int parse_list(void){skip();if(*source==')'){source++;return 0;}if(!*source)return 0;int first=parse_object();return cons(first,parse_list());}
static int parse_object(void){skip();if(*source=='('){source++;return parse_list();}if(*source=='\''){source++;int value=parse_object();return cons(K_QUOTE,cons(value,0));}char token[64];unsigned n=0;while(*source&&*source!=' '&&*source!='\t'&&*source!='('&&*source!=')'&&n+1<sizeof token){char c=*source++;token[n++]=c>='a'&&c<='z'?(char)(c-32):c;}token[n]=0;return intern(token);}
static int read_line(char *line,unsigned capacity){unsigned n=0;os->write("* ");for(;;){unsigned key=os->terminal_read_key();if(key==3){os->write("^C\n");return 0;}if(key=='\n'||key=='\r'){os->putc('\n');line[n]=0;return 1;}if((key=='\b'||key==127)&&n){n--;os->write("\b \b");}else if(key>=32&&key<127&&n+1<capacity){line[n++]=(char)key;os->putc((char)key);}}}
static int equals_exit(const char *s){while(*s==' ')s++;const char*w="EXIT";while(*w){char c=*s++;if(c>='a'&&c<='z')c=(char)(c-32);if(c!=*w++)return 0;}while(*s==' ')s++;return !*s;}

int _start(const os64_api_t *api,const char *args){if(!api||api->version!=OS64_ABI_VERSION)return 126;if(args&&(*args)){api->write("sectorlisp: interactive McCarthy LISP; run without arguments\n");return 2;}if(!api->terminal_acquire()){api->write("sectorlisp: terminal is busy\n");return 1;}os=api;for(unsigned i=0;i<sizeof builtins;i++)M[i]=(unsigned char)builtins[i];api->write("SectorLISP for OS64\nType EXIT or Ctrl+C to return to the shell.\nExample: (CONS 'A '(B C))\n\n");char line[256];while(read_line(line,sizeof line)){if(equals_exit(line))break;source=line;cx=0;int expression=parse_object();int result=eval(expression,0);print_object(result);api->putc('\n');}api->terminal_release();return 0;}
