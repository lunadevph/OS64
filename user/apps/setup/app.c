#include "tui.h"

static char hostname[64]="os64",username[64]="luna",pass1[64]="",pass2[64]="";
static char tz_buf[64]="Asia/Manila",ip[64]="",nm[64]="",gw[64]="",dns_buf[64]="";
static int use_dhcp=1,cur_step=0;
#define ACCOUNT 0
#define TIMEZONE 1
#define NETWORK 2
#define SUMMARY 3
#define INSTALL 4
#define COMPLETE 5

static const char*tz_names[]={
 "Africa/Cairo","Africa/Johannesburg","America/New_York","America/Chicago",
 "America/Denver","America/Los_Angeles","Asia/Dubai","Asia/Kolkata",
 "Asia/Shanghai","Asia/Tokyo","Asia/Manila","Australia/Sydney",
 "Europe/London","Europe/Paris","Europe/Berlin","Europe/Moscow",
 "Pacific/Auckland","UTC"
};
static int tz_count=sizeof(tz_names)/sizeof(tz_names[0]),tz_sel=10;

static struct tui_application*app;
static struct tui_window*win;
static struct tui_widget*lbl_step,*lbl_title,*btn_back,*btn_next,*sbar;

static struct tui_widget*lbl_hostname,*txt_hostname;
static struct tui_widget*lbl_username,*txt_username;
static struct tui_widget*lbl_password,*txt_password;
static struct tui_widget*lbl_verify,*txt_verify;
static struct tui_widget*lbl_err;

static struct tui_widget*lst_tz;

static struct tui_widget*lbl_dhcp;
static struct tui_widget*lbl_ip,*txt_ip;
static struct tui_widget*lbl_nm,*txt_nm;
static struct tui_widget*lbl_gw,*txt_gw;
static struct tui_widget*lbl_dns,*txt_dns;
static struct tui_widget*lbl_net_err;

static struct tui_widget*lbl_summ[6];

static struct tui_widget*lbl_prog,*prog_bar,*lbl_prog_status;

static struct tui_widget*lbl_done,*lbl_reboot_info,*btn_reboot;

static void copy(char*d,const char*s){size_t i=0;while(s[i]&&i<127){d[i]=s[i];i++;}d[i]=0;}

static void set_all_visible(int step){
 size_t i;
 for(i=0;i<win->child_count;i++)win->children[i]->visible=0;
 lbl_step->visible=1;btn_back->visible=1;btn_next->visible=1;sbar->visible=1;
 lbl_title->visible=1;
 if(step==ACCOUNT){
  lbl_title->text[0]=0;copy(lbl_title->text,"Account Configuration");
  lbl_hostname->visible=txt_hostname->visible=1;
  lbl_username->visible=txt_username->visible=1;
  lbl_password->visible=txt_password->visible=1;
  lbl_verify->visible=txt_verify->visible=1;
  lbl_err->visible=1;
  btn_back->visible=0;
  copy(btn_next->text,"  Next  ");
 }else if(step==TIMEZONE){
  lbl_title->text[0]=0;copy(lbl_title->text,"Timezone Selection");
  lst_tz->visible=1;
  if(lst_tz->selected!=(size_t)tz_sel)lst_tz->selected=tz_sel;
  copy(btn_back->text,"  Back  ");
  copy(btn_next->text,"  Next  ");
 }else if(step==NETWORK){
  lbl_title->text[0]=0;copy(lbl_title->text,"Network Configuration");
  lbl_dhcp->visible=1;
  lbl_ip->visible=!use_dhcp;txt_ip->visible=!use_dhcp;
  lbl_nm->visible=!use_dhcp;txt_nm->visible=!use_dhcp;
  lbl_gw->visible=!use_dhcp;txt_gw->visible=!use_dhcp;
  lbl_dns->visible=!use_dhcp;txt_dns->visible=!use_dhcp;
  lbl_net_err->visible=1;
  copy(btn_back->text,"  Back  ");
  copy(btn_next->text,"  Next  ");
 }else if(step==SUMMARY){
  lbl_title->text[0]=0;copy(lbl_title->text,"Installation Summary");
  for(i=0;i<6;i++)lbl_summ[i]->visible=1;
  copy(btn_back->text,"  Back  ");
  copy(btn_next->text,"Install");
 }else if(step==INSTALL){
  lbl_title->text[0]=0;copy(lbl_title->text,"Installing OS64...");
  lbl_prog->visible=prog_bar->visible=lbl_prog_status->visible=1;
  btn_back->visible=0;btn_next->visible=0;
 }else if(step==COMPLETE){
  lbl_title->text[0]=0;copy(lbl_title->text,"Installation Complete");
  lbl_done->visible=lbl_reboot_info->visible=btn_reboot->visible=1;
  btn_back->visible=0;btn_next->visible=0;
 }
 tui_request_redraw();
}

static void update_step_indicator(void){
 char buf[128];
 size_t n=0;
 const char*names[]={"Account","Timezone","Network","Summary"};
 for(int i=0;i<4;i++){
  if(n&&n<126)buf[n++]=' ';
  if(i==cur_step){if(n<126)buf[n++]='>';if(n<126)buf[n++]='>';}
  else if(n<126)buf[n++]=' ';
  const char*s=names[i];
  while(*s&&n<126)buf[n++]=*s++;
  if(i==cur_step){if(n<126)buf[n++]='<';if(n<126)buf[n++]='<';}
  else if(n<126)buf[n++]=' ';
  if(n<126)buf[n++]=' ';
 }
 buf[n]=0;
 copy(lbl_step->text,buf);
 tui_widget_repaint(lbl_step);
}

static int vhost(void){
 if(!hostname[0])return 0;
 for(int i=0;hostname[i];i++){
  char c=hostname[i];
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'))return 0;
 }
 return 1;
}
static int vuser(void){
 if(!username[0])return 0;
 for(int i=0;username[i];i++)if(username[i]<32||username[i]>126)return 0;
 return 1;
}

static int mismatch(void){
 return pass1[0]||pass2[0]?pass1[0]!=pass2[0]:0;
}
static int short_pass(void){
 int n=0;while(pass1[n])n++;
 return n>0&&n<4;
}

static void validate(void){
 if(cur_step==ACCOUNT){
  if(!vhost()){copy(lbl_err->text,"Hostname: invalid or empty");lbl_err->custom_fg=15;lbl_err->custom_bg=4;}
  else if(!vuser()){copy(lbl_err->text,"Username: cannot be empty");lbl_err->custom_fg=15;lbl_err->custom_bg=4;}
  else if(short_pass()){copy(lbl_err->text,"Password: minimum 4 characters");lbl_err->custom_fg=15;lbl_err->custom_bg=4;}
  else if(mismatch()){copy(lbl_err->text,"Password: confirmation does not match");lbl_err->custom_fg=15;lbl_err->custom_bg=4;}
  else{copy(lbl_err->text,"");lbl_err->custom_fg=0;lbl_err->custom_bg=0;}
  tui_widget_repaint(lbl_err);
 }else if(cur_step==NETWORK&&!use_dhcp){
  if(!ip[0]){copy(lbl_net_err->text,"IP address required");lbl_net_err->custom_fg=15;lbl_net_err->custom_bg=4;}
  else{copy(lbl_net_err->text,"");lbl_net_err->custom_fg=0;lbl_net_err->custom_bg=0;}
  tui_widget_repaint(lbl_net_err);
 }
}

static int can_advance(void){
 if(cur_step==ACCOUNT)return vhost()&&vuser()&&pass1[0]==pass2[0]&&!short_pass();
 if(cur_step==TIMEZONE)return 1;
 if(cur_step==NETWORK)return use_dhcp||ip[0];
 return 1;
}

static void build_summary(void){
 int i;
 copy(lbl_summ[0]->text,"Hostname:");
 copy(lbl_summ[1]->text,hostname);
 copy(lbl_summ[2]->text,"Username:");
 copy(lbl_summ[3]->text,username);
 copy(lbl_summ[4]->text,"Timezone:");
 copy(lbl_summ[5]->text,tz_buf);
 for(i=0;i<6;i++)tui_widget_repaint(lbl_summ[i]);
}

static void simulate_install(void){
 cur_step=INSTALL;set_all_visible(INSTALL);
 copy(lbl_prog->text,"Installing OS64...");
 prog_bar->value=0;prog_bar->maximum=100;
 tui_request_redraw();tui_render();
 const char*msgs[]={
  "Installing bootloader...","Copying configuration files...",
  "Generating users...","Configuring networking...","Finalizing..."
 };
 int phases=5;
 for(int p=0;p<phases;p++){
  int start=p*20,end=(p+1)*20;
  copy(lbl_prog_status->text,msgs[p]);
  tui_widget_repaint(lbl_prog_status);
  for(int v=start;v<=end;v++){
   prog_bar->value=v;
   tui_widget_repaint(prog_bar);
   tui_render();
   unsigned long delay=0;
   while(delay<20000000ul)delay++;
  }
 }
 copy(lbl_prog_status->text,"Done.");
 tui_widget_repaint(lbl_prog_status);tui_render();
 cur_step=COMPLETE;
 copy(lbl_done->text,"Installation completed successfully.");
 copy(lbl_reboot_info->text,"The system is ready. Reboot to start using " OS64_NAME ".");
 set_all_visible(COMPLETE);
 tui_request_redraw();
}

static void on_back(struct tui_widget*w){
 (void)w;
 if(cur_step>0){cur_step--;set_all_visible(cur_step);update_step_indicator();if(cur_step==ACCOUNT)validate();}
}
static void on_next(struct tui_widget*w){
 (void)w;
 if(!can_advance()){validate();return;}
 if(cur_step==SUMMARY){simulate_install();return;}
 if(cur_step<SUMMARY){cur_step++;set_all_visible(cur_step);update_step_indicator();if(cur_step==NETWORK){lbl_dhcp->text[0]=0;copy(lbl_dhcp->text,use_dhcp?"Network : DHCP":"Network : Static");tui_request_redraw();}if(cur_step==SUMMARY)build_summary();}
}
static void on_reboot(struct tui_widget*w){
 (void)w;
 tui_shutdown();const os64_api_t*a=tui_api();a->dispatch("reboot","");app->running=0;
}

static void dhcp_toggle(struct tui_widget*w){
 if(!w)return;
 use_dhcp=!use_dhcp;
 copy(w->text,use_dhcp?"  DHCP  ":"  Static ");
 copy(lbl_dhcp->text,use_dhcp?"Network : DHCP":"Network : Static");
 int sv=use_dhcp?0:1;
 lbl_ip->visible=sv;txt_ip->visible=sv;
 lbl_nm->visible=sv;txt_nm->visible=sv;
 lbl_gw->visible=sv;txt_gw->visible=sv;
 lbl_dns->visible=sv;txt_dns->visible=sv;
 tui_request_redraw();
}

int _start(const os64_api_t*api,const char*args){
 (void)args;
 if(!tui_initialize(api)){api->write("setup: terminal must be at least 40x15\n");return 1;}
 tui_set_theme(&tui_theme_installer);
 app=tui_app_create("os64-setup");
 win=tui_window_create(0,0,72,24,"OS64 Setup Wizard",TUI_WINDOW_CENTER|TUI_WINDOW_DOUBLE_BORDER);

 lbl_step=tui_widget_create(TUI_LABEL,win,2,0,68,1,"");
 copy(lbl_step->text,"  >> Account    Timezone    Network    Summary  ");
 lbl_title=tui_widget_create(TUI_LABEL,win,2,2,68,1,"");
 copy(lbl_title->text,"Account Configuration");

 lbl_hostname=tui_label_create(win,2,4,"Hostname :");
 txt_hostname=tui_textbox_create(win,14,4,50,hostname,63,0);
 lbl_username=tui_label_create(win,2,5,"Username :");
 txt_username=tui_textbox_create(win,14,5,50,username,63,0);
 lbl_password=tui_label_create(win,2,6,"Password :");
 txt_password=tui_textbox_create(win,14,6,50,pass1,63,1);
 lbl_verify=tui_label_create(win,2,7,"Verify   :");
 txt_verify=tui_textbox_create(win,14,7,50,pass2,63,1);
 lbl_err=tui_widget_create(TUI_LABEL,win,2,9,68,1,"");

 lst_tz=tui_listbox_create(win,2,4,68,13,tz_names,tz_count);
 lst_tz->selected=tz_sel;

 lbl_dhcp=tui_widget_create(TUI_LABEL,win,2,4,68,1,"");
 copy(lbl_dhcp->text,"Network : DHCP");
 struct tui_widget*dhcp_btn=tui_button_create(win,14,4,12,"  DHCP  ");
 dhcp_btn->activate=dhcp_toggle;
 lbl_ip=tui_label_create(win,2,6,"IP Address:");
 txt_ip=tui_textbox_create(win,14,6,50,ip,31,0);
 lbl_nm=tui_label_create(win,2,7,"Netmask  :");
 txt_nm=tui_textbox_create(win,14,7,50,nm,31,0);
 lbl_gw=tui_label_create(win,2,8,"Gateway  :");
 txt_gw=tui_textbox_create(win,14,8,50,gw,31,0);
 lbl_dns=tui_label_create(win,2,9,"DNS      :");
 txt_dns=tui_textbox_create(win,14,9,50,dns_buf,31,0);
 lbl_net_err=tui_widget_create(TUI_LABEL,win,2,11,68,1,"");

 for(int i=0;i<6;i++){
  lbl_summ[i]=tui_widget_create(TUI_LABEL,win,i%2?22:2,(int)(4+i/2),i%2?48:18,1,"");
 }

 lbl_prog=tui_label_create(win,2,4,"");
 prog_bar=tui_widget_create(TUI_PROGRESS,win,2,6,68,1,"");
 prog_bar->maximum=100;
 lbl_prog_status=tui_label_create(win,2,8,"");

 lbl_done=tui_label_create(win,2,4,"");
 copy(lbl_done->text,"Installation completed successfully.");
 lbl_reboot_info=tui_label_create(win,2,6,"");
 copy(lbl_reboot_info->text,"The system is ready. Reboot to start using OS64.");
 btn_reboot=tui_button_create(win,24,9,20,"  Reboot  ");
 btn_reboot->activate=on_reboot;

 btn_back=tui_button_create(win,18,16,14,"  Back  ");
 btn_back->activate=on_back;
 btn_next=tui_button_create(win,36,16,14,"  Next  ");
 btn_next->activate=on_next;

 sbar=tui_widget_create(TUI_STATUS_BAR,win,1,20,70,1,
  "TAB Next  Shift+TAB Previous  Enter Select  F10 Finish     " OS64_NAME " " OS64_KERNEL_VERSION);

 set_all_visible(ACCOUNT);update_step_indicator();
 tui_widget_set_focus(txt_hostname);
 tui_request_redraw();

 while(app->running){
  tui_render();
  struct tui_event e;
  tui_next_event(&e);
  if(e.key==TUI_KEY_F10||e.key==27){
   if(cur_step!=INSTALL)break;
   continue;
  }
  if(e.key=='\t'){tui_dispatch_event(app,&e);continue;}
  if(e.key==TUI_KEY_BACKTAB){tui_dispatch_event(app,&e);continue;}
  if(e.key=='\n'&&cur_step==TIMEZONE&&lst_tz->focused){
   copy(tz_buf,tz_names[lst_tz->selected]);
   tz_sel=(int)lst_tz->selected;
   continue;
  }
  tui_dispatch_event(app,&e);
  if(cur_step==ACCOUNT||cur_step==NETWORK)validate();
  if(cur_step==ACCOUNT){
   if(btn_next->focused)validate();
  }
 }

 tui_shutdown();
 return 0;
}
