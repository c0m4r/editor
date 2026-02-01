#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
typedef struct{char*d;size_t l,c;}L;typedef struct{L*l;size_t n,c;int x,y,o,p,r,s,d;char*f;}E;struct termios o;E e;
void die(const char*s){write(1,"\x1b[2J\x1b[H",7);perror(s);exit(1);}
void dr(){tcsetattr(0,2,&o);}
void er(){if(tcgetattr(0,&o)==-1)die("tcgetattr");atexit(dr);struct termios r=o;r.c_iflag&=~(BRKINT|ICRNL|INPCK|ISTRIP|IXON);r.c_oflag&=~(OPOST);r.c_cflag|=(CS8);r.c_lflag&=~(ECHO|ICANON|IEXTEN|ISIG);r.c_cc[VMIN]=0;r.c_cc[VTIME]=1;if(tcsetattr(0,2,&r)==-1)die("tcsetattr");}
int rk(){char c;while(read(0,&c,1)!=1);if(c!='\x1b')return c;char s[3];if(read(0,&s[0],1)!=1||read(0,&s[1],1)!=1)return '\x1b';if(s[0]=='['){if(s[1]>='0'&&s[1]<='9'){if(read(0,&s[2],1)!=1)return '\x1b';if(s[2]=='~'){switch(s[1]){case '1':case '7':return 'H';case '3':return 4;case '4':case '8':return 'E';case '5':return 'P';case '6':return 'N';}}}else{switch(s[1]){case 'A':return 'U';case 'B':return 'D';case 'C':return 'R';case 'D':return 'L';case 'H':return 'H';case 'F':return 'E';}}}return '\x1b';}
void il(L*l){l->d=NULL;l->l=l->c=0;}
void icl(L*l,int a,int c){if(a<0||a>l->l)a=l->l;if(l->l+1>=l->c){l->c=l->c?l->c*2:8;l->d=realloc(l->d,l->c);}memmove(&l->d[a+1],&l->d[a],l->l-a);l->d[a]=c;l->l++;}
void dcl(L*l,int a){if(a<0||a>=l->l)return;memmove(&l->d[a],&l->d[a+1],l->l-a-1);l->l--;}
void insl(int a){if(a<0||a>e.n)return;if(e.n>=e.c){e.c=e.c?e.c*2:8;e.l=realloc(e.l,sizeof(L)*e.c);}memmove(&e.l[a+1],&e.l[a],sizeof(L)*(e.n-a));il(&e.l[a]);e.n++;e.d=1;}
void dell(int a){if(a<0||a>=e.n)return;free(e.l[a].d);memmove(&e.l[a],&e.l[a+1],sizeof(L)*(e.n-a-1));e.n--;e.d=1;}
void ic(int c){if(e.y==e.n)insl(e.n);icl(&e.l[e.y],e.x,c);e.x++;e.d=1;}
void inw(){if(e.y<e.n){L*l=&e.l[e.y];insl(e.y+1);if(e.x<l->l){L*n=&e.l[e.y+1];n->d=malloc(l->l-e.x);memcpy(n->d,&l->d[e.x],l->l-e.x);n->l=n->c=l->l-e.x;l->l=e.x;}}else insl(e.n);e.y++;e.x=0;}
void dc(){if(e.y==e.n||(e.x==0&&e.y==0))return;if(e.x>0){dcl(&e.l[e.y],e.x-1);e.x--;}else{L*p=&e.l[e.y-1];e.x=p->l;if(e.l[e.y].l>0){p->d=realloc(p->d,p->l+e.l[e.y].l);memcpy(&p->d[p->l],e.l[e.y].d,e.l[e.y].l);p->l+=e.l[e.y].l;}dell(e.y);e.y--;}e.d=1;}
void df(){if(e.y>=e.n)return;L*l=&e.l[e.y];if(e.x<l->l){dcl(l,e.x);e.d=1;}else if(e.y<e.n-1){L*n=&e.l[e.y+1];if(n->l>0){l->d=realloc(l->d,l->l+n->l);memcpy(&l->d[l->l],n->d,n->l);l->l+=n->l;}dell(e.y+1);e.d=1;}}
void opf(char*f){e.f=strdup(f);FILE*p=fopen(f,"r");if(!p){insl(0);return;}char*l=NULL;size_t c=0;ssize_t n;while((n=getline(&l,&c,p))!=-1){while(n>0&&(l[n-1]=='\n'||l[n-1]=='\r'))n--;insl(e.n);L*r=&e.l[e.n-1];r->d=malloc(n);memcpy(r->d,l,n);r->l=r->c=n;}free(l);fclose(p);e.d=0;}
int sf(){FILE*p=fopen(e.f,"w");if(!p)return-1;for(size_t i=0;i<e.n;i++){fwrite(e.l[i].d,1,e.l[i].l,p);fputc('\n',p);}fclose(p);e.d=0;return 0;}
void sc(){if(e.y<e.o)e.o=e.y;if(e.y>=e.o+e.r)e.o=e.y-e.r+1;if(e.x<e.p)e.p=e.x;if(e.x>=e.p+e.s)e.p=e.x-e.s+1;}
void rs(){sc();char b[65536];int l=sprintf(b,"\x1b[?25l\x1b[H");for(int y=0;y<e.r;y++){int r=y+e.o;if(r>=e.n)b[l++]='~';else{int s=e.l[r].l-e.p;if(s<0)s=0;if(s>e.s)s=e.s;if(s>0){memcpy(&b[l],&e.l[r].d[e.p],s);l+=s;}}l+=sprintf(&b[l],"\x1b[K\r\n");}l+=sprintf(&b[l],"\x1b[7m%.20s - %zu lines %s",e.f,e.n,e.d?"(modified)":"");int w=20+10+(e.d?10:0);char p[20];int n=sprintf(p,"%d/%zu",e.y+1,e.n);while(w++<e.s-n)b[l++]=' ';l+=sprintf(&b[l],"%s\x1b[m\r\n\x1b[K^S:Save ^X:Save+Quit ^Q:Quit\x1b[%d;%dH\x1b[?25h",p,e.y-e.o+1,e.x-e.p+1);write(1,b,l);}
void mc(int k){L*r=(e.y<e.n)?&e.l[e.y]:NULL;switch(k){case 'L':if(e.x>0)e.x--;else if(e.y>0){e.y--;e.x=e.l[e.y].l;}break;case 'R':if(r&&e.x<r->l)e.x++;else if(r&&e.y<e.n-1){e.y++;e.x=0;}break;case 'U':if(e.y>0)e.y--;break;case 'D':if(e.y<e.n)e.y++;break;}r=(e.y<e.n)?&e.l[e.y]:NULL;if(e.x>(r?r->l:0))e.x=r?r->l:0;}
void q(){write(1,"\x1b[2J\x1b[H",7);exit(0);}
void pk(){int c=rk();switch(c){case '\r':inw();break;case 'H':e.x=0;break;case 'E':if(e.y<e.n)e.x=e.l[e.y].l;break;case 'P':case 'N':for(int i=e.r;i--;)mc(c=='P'?'U':'D');break;case 'U':case 'D':case 'L':case 'R':mc(c);break;case 127:case 8:dc();break;case 4:df();break;case 19:sf();break;case 24:sf();q();break;case 17:q();break;default:if(c>=32&&c<127)ic(c);break;}}
int main(int c,char**v){if(c<2){fprintf(stderr,"Usage: %s <filename>\n",v[0]);return 1;}er();struct winsize w;if(ioctl(0,TIOCGWINSZ,&w)==-1)die("ioctl");e.r=w.ws_row-2;e.s=w.ws_col;opf(v[1]);while(1){rs();pk();}return 0;}
