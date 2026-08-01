#include <ctype.h>
int isalpha(int c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z');}int isdigit(int c){return c>='0'&&c<='9';}int isalnum(int c){return isalpha(c)||isdigit(c);}int isspace(int c){return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v';}int tolower(int c){return c>='A'&&c<='Z'?c+32:c;}int toupper(int c){return c>='a'&&c<='z'?c-32:c;}
