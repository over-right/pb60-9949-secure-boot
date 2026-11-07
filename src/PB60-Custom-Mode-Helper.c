/* Separate, explicitly confirmed Custom-mode candidate. Host tests must use
 * disposable virtual firmware only. No PK/KEK/db/dbx, SystemAccess, TPM,
 * reset, raw flash, or retry path. UEFI x64 Microsoft ABI. */
typedef unsigned char U8;
typedef unsigned short C16;
typedef unsigned int U32;
typedef unsigned long long N;
typedef N STATUS;
typedef void *HANDLE;
typedef struct { U32 a; C16 b,c; U8 d[8]; } GUID;
typedef struct { N sig; U32 revision,size,crc,reserved; } HEADER;
typedef struct { HEADER h; void *fn[44]; } BOOT;
typedef struct { HEADER h; void *fn[14]; } RUNTIME;
typedef struct { void *fn[9]; void *mode; } OUTPUT;
typedef struct { C16 scan,character; } KEY;
typedef struct INPUT { STATUS (*reset)(struct INPUT*,U8); STATUS (*read)(struct INPUT*,KEY*); HANDLE wait; } INPUT;
typedef struct {
 HEADER h; C16 *vendor; U32 revision; HANDLE cinHandle; INPUT *cin;
 HANDLE coutHandle; OUTPUT *cout; HANDLE errHandle; OUTPUT *err;
 RUNTIME *rt; BOOT *bs; N count; void *config;
} SYSTEM;
typedef struct { U32 revision; HANDLE parent; SYSTEM *system; HANDLE device; } LOADED;
typedef struct FILE FILE;
struct FILE {
 N revision; STATUS (*open)(FILE*,FILE**,C16*,N,N); STATUS (*close)(FILE*);
 void *deleteUnused; STATUS (*read)(FILE*,N*,void*); STATUS (*write)(FILE*,N*,void*);
 void *unused[4]; STATUS (*flush)(FILE*);
};
typedef struct FS { N revision; STATUS (*root)(struct FS*,FILE**); } FS;
typedef STATUS (*HANDLE_PROTOCOL)(HANDLE,GUID*,void**);
typedef STATUS (*GET_VARIABLE)(C16*,GUID*,U32*,N*,void*);
typedef STATUS (*SET_VARIABLE)(C16*,GUID*,U32,N,void*);
typedef STATUS (*PRINT)(OUTPUT*,C16*);
typedef STATUS (*WAIT)(N,HANDLE*,N*);
#define ERR(x) ((STATUS)(x)|0x8000000000000000ULL)
_Static_assert(__builtin_offsetof(SYSTEM,rt)==88 && __builtin_offsetof(SYSTEM,bs)==96,"system ABI");
_Static_assert(sizeof(HEADER)==24 && __builtin_offsetof(LOADED,device)==24 && __builtin_offsetof(FILE,flush)==80,"UEFI ABI");
static GUID loadedGuid={0x5b1b31a1,0x9562,0x11d2,{0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}};
static GUID fsGuid={0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
static GUID pathGuid={0x09576e91,0x6d3f,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
static GUID globalGuid={0x8be4df61,0x93ca,0x11d2,{0xaa,0x0d,0,0xe0,0x98,0x03,0x2b,0x8c}};
static GUID setupGuid={0x7b59104a,0xc00d,0x4158,{0x87,0xff,0xf0,0x4d,0x63,0x96,0xa9,0x15}};
static const U8 before[7]={1,0,0,0,0,0,0}, after[7]={1,1,0,0,0,0,0};
static const char markerValue[]="PB60_CUSTOM_MODE_V1\r\n";
static void say(SYSTEM *s,const char *msg) { C16 buf[192]; N i=0; while(*msg && i<191)buf[i++]=(U8)*msg++;buf[i]=0;((PRINT)s->cout->fn[1])(s->cout,buf); }
static int same(const void *a,const void *b,N size) { const U8 *x=a,*y=b; for(N i=0;i<size;i++)if(x[i]!=y[i])return 0;return 1; }
static int usb_path(const U8 *p) {
 int usb=0;
 for(N walked=0;walked<4096;) {
  N n=p[2]|((N)p[3]<<8);if(n<4 || walked+n>4096)return 0;
  if(p[0]==3 && (p[1]==5 || p[1]==15 || p[1]==16))usb=1;
  if(p[0]==127 && p[1]==255)return usb;
  walked+=n;p+=n;
 }
 return 0;
}
static int exact(SYSTEM *s,C16 *name,GUID *g,U32 attrs,const U8 *expect,N len) {
 U8 bytes[7];U32 got=0;N n=sizeof(bytes);
 return ((GET_VARIABLE)s->rt->fn[6])(name,g,&got,&n,bytes)==0 && n==len && got==attrs && same(bytes,expect,len);
}
static int boot_state(SYSTEM *s) {
 const U8 on=1, user=0;
 return exact(s,L"SecureBoot",&globalGuid,6,&on,1) && exact(s,L"SetupMode",&globalGuid,6,&user,1);
}
static int confirm(SYSTEM *s) {
 const C16 phrase[]=L"custom\r";
 if(!s->cin || !s->cin->wait || s->cin->reset(s->cin,0)!=0)return 0;
 say(s,"Type custom, then Enter. Letter case does not matter. Any other key cancels.\r\nPB60_CONFIRM_READY\r\n");
 for(N i=0;phrase[i];i++) {
  N index=0;KEY key;
  if(((WAIT)s->bs->fn[9])(1,&s->cin->wait,&index)!=0 || s->cin->read(s->cin,&key)!=0 || key.scan)return 0;
  C16 character=key.character;
  if(character>=L'A' && character<=L'Z')character+=L'a'-L'A';
  if(character!=phrase[i])return 0;
 }
 return 1;
}
static STATUS append(FILE *f,const char *msg) {
 N n=0;while(msg[n])n++;N done=n;
 STATUS st=f->write(f,&done,(void*)msg);if(st)return st;if(done!=n)return ERR(7);return f->flush(f);
}
static void hex16(char *out,N value,N width) { const char *h="0123456789ABCDEF";while(width){--width;out[width]=h[value&15];value>>=4;} }
__attribute__((section(".sbat"),used)) static const char sbat[]=
 "sbat,1,SBAT Version,sbat,1,https://github.com/rhboot/shim/blob/main/SBAT.md\n"
 "pb60custom,1,Local PB60 Custom Mode Probe,pb60custom,1,https://uefi.org/specifications\n";
STATUS efi_main(HANDLE image,SYSTEM *s) {
 HANDLE_PROTOCOL hp=(HANDLE_PROTOCOL)s->bs->fn[16];LOADED *li=0;U8 *path=0;FS *fs=0;FILE *root=0,*marker=0,*journal=0;
 N n=64;char m[64];STATUS st;
 say(s,"PB60 Custom-mode candidate. One persistent setting may change after confirmation.\r\n");
 if(hp(image,&loadedGuid,(void**)&li)||!li||hp(li->device,&pathGuid,(void**)&path)||!path||!usb_path(path)||hp(li->device,&fsGuid,(void**)&fs)||!fs||fs->root(fs,&root)||!root) {
  say(s,"PB60_STOP_MEDIA\r\n");return ERR(3);
 }
 if(root->open(root,&marker,L"PB60_CUSTOM.ID",1,0)!=0){root->close(root);say(s,"PB60_STOP_MEDIA\r\n");return ERR(3);}
 st=marker->read(marker,&n,m);marker->close(marker);
 if(st || n!=sizeof(markerValue)-1 || !same(m,markerValue,n)){root->close(root);say(s,"PB60_STOP_MEDIA\r\n");return ERR(3);}
 st=root->open(root,&journal,L"PB60_CUSTOM.TXT",1,0);
 if(st!=ERR(14)){if(!st)journal->close(journal);root->close(root);say(s,"PB60_STOP_EXISTING_OR_UNREADABLE_JOURNAL\r\n");return ERR(3);}
 if(!boot_state(s) || !exact(s,L"SecureBootSetup",&setupGuid,7,before,7)){root->close(root);say(s,"PB60_STOP_PREFLIGHT\r\n");return ERR(3);}
 if(!confirm(s)){root->close(root);say(s,"PB60_CANCELLED\r\n");return ERR(21);}
 if(!boot_state(s) || !exact(s,L"SecureBootSetup",&setupGuid,7,before,7)){root->close(root);say(s,"PB60_STOP_PREFLIGHT\r\n");return ERR(3);}
 st=root->open(root,&journal,L"PB60_CUSTOM.TXT",0x8000000000000003ULL,0);root->close(root);
 if(st || append(journal,"PB60 Custom-mode one-attempt journal\r\nPreflight: SecureBoot=01 SetupMode=00 attrs=00000007 before=01000000000000 requested=01010000000000\r\nConfirmation received. One write may have been attempted; if this is the last line, the outcome is unknown. Do not retry.\r\n")){
  if(!st)journal->close(journal);say(s,"PB60_STOP_JOURNAL_WRITE\r\n");return ERR(7);
 }
 if(!boot_state(s) || !exact(s,L"SecureBootSetup",&setupGuid,7,before,7)){
  append(journal,"Final guard rejected. No firmware write was attempted.\r\n");journal->close(journal);say(s,"PB60_STOP_PREFLIGHT\r\n");return ERR(3);
 }
 U8 request[7];for(N i=0;i<7;i++)request[i]=after[i];
 /* The only firmware-variable write in this program. */
 STATUS write=((SET_VARIABLE)s->rt->fn[8])(L"SecureBootSetup",&setupGuid,7,7,request);
 U8 readback[7];U32 attrs=0;N count=sizeof(readback);
 STATUS read=((GET_VARIABLE)s->rt->fn[6])(L"SecureBootSetup",&setupGuid,&attrs,&count,readback);
 char ws[]="SetVariable=0x0000000000000000\r\n",rs[]="GetVariable=0x0000000000000000\r\n", info[]="Readback attrs=0x00000000 size=0x0000000000000000\r\n", bytes[]="Readback data=00000000000000\r\n";
 hex16(ws+14,write,16);hex16(rs+14,read,16);hex16(info+17,attrs,8);hex16(info+33,count,16);
 int secure=boot_state(s);
 int success=!write&&!read&&attrs==7&&count==7&&same(readback,after,7)&&secure;
 const char *result=success?"PB60_WRITE_VERIFIED\r\n":"PB60_WRITE_NOT_VERIFIED_STOP\r\n";
 STATUS logerr=append(journal,ws);if(!logerr)logerr=append(journal,rs);if(!logerr)logerr=append(journal,info);
 if(!read&&count==7){for(N i=0;i<7;i++)hex16(bytes+14+2*i,readback[i],2);say(s,bytes);if(!logerr)logerr=append(journal,bytes);}
 if(!logerr)logerr=append(journal,secure?"SecureBoot=01 SetupMode=00 after attempt\r\n":"Secure Boot/User Mode check failed after attempt\r\n");
 if(!logerr)logerr=append(journal,result);
 STATUS close=journal->close(journal);
 say(s,ws);say(s,rs);say(s,info);
 if(logerr||close){say(s,"PB60_STOP_JOURNAL_WRITE_AFTER_ATTEMPT\r\n");return ERR(7);}
 say(s,result);
 return success?0:ERR(7);
}
