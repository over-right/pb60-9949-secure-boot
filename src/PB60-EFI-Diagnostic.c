/* PB60 preboot read-only variable diagnostic. No SetVariable, flash, TPM,
 * ResetSystem, ExitBootServices, protocol mutation, or OS disk discovery.
 * Only filesystem writes: a new numbered log on the loaded image's USB
 * volume, after verifying an explicit media marker. UEFI x64 Microsoft ABI.
 * Minimal ABI subset follows UEFI 2.10 chapters 4, 7, 8, 9 and 13. */
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long long U64;
typedef U64 N;
typedef N STATUS;
typedef void *HANDLE;
typedef U16 C16;
typedef struct { U32 a; U16 b,c; U8 d[8]; } GUID;
typedef struct { U64 sig; U32 revision,size,crc,reserved; } HEADER;
typedef struct { HEADER h; void *fn[44]; } BOOT;
typedef struct { HEADER h; void *fn[14]; } RUNTIME;
typedef struct { void *fn[9]; void *mode; } OUTPUT;
typedef struct { void *reset,*readkey; HANDLE wait; } INPUT;
typedef struct {
 HEADER h; C16 *vendor; U32 revision; HANDLE cinHandle; INPUT *cin;
 HANDLE coutHandle; OUTPUT *cout; HANDLE errHandle; OUTPUT *err;
 RUNTIME *rt; BOOT *bs; N count; void *config;
} SYSTEM;
typedef struct {
 U32 revision; HANDLE parent; SYSTEM *system; HANDLE device; void *path;
 void *reserved; U32 optionsSize; void *options,*base; U64 imageSize;
 U32 codeType,dataType; void *unload;
} LOADED;
typedef struct FILE FILE;
struct FILE {
 U64 revision;
 STATUS (*open)(FILE*,FILE**,C16*,U64,U64);
 STATUS (*close)(FILE*);
 void *deleteUnused;
 STATUS (*read)(FILE*,N*,void*);
 STATUS (*write)(FILE*,N*,void*);
 void *getPositionUnused,*setPositionUnused,*getInfoUnused,*setInfoUnused;
 STATUS (*flush)(FILE*);
};
typedef struct FS { U64 revision; STATUS (*root)(struct FS*,FILE**); } FS;
typedef STATUS (*HANDLE_PROTOCOL)(HANDLE,GUID*,void**);
typedef STATUS (*GET_VARIABLE)(C16*,GUID*,U32*,N*,void*);
typedef STATUS (*PRINT)(OUTPUT*,C16*);
typedef STATUS (*STALL)(N);
#define ERROR(x) ((STATUS)(x)|0x8000000000000000ULL)
#define BAD(x) (((x)>>63)!=0)
_Static_assert(sizeof(HEADER)==24,"header ABI");
_Static_assert(__builtin_offsetof(SYSTEM,rt)==88,"system ABI");
_Static_assert(__builtin_offsetof(SYSTEM,bs)==96,"boot ABI");
_Static_assert(__builtin_offsetof(LOADED,device)==24,"loaded ABI");
_Static_assert(__builtin_offsetof(FILE,flush)==80,"file ABI");
static GUID loadedGuid={0x5b1b31a1,0x9562,0x11d2,{0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}};
static GUID fsGuid={0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
static GUID pathGuid={0x09576e91,0x6d3f,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
static GUID globalGuid={0x8be4df61,0x93ca,0x11d2,{0xaa,0x0d,0,0xe0,0x98,0x03,0x2b,0x8c}};
static GUID setupGuid={0x7b59104a,0xc00d,0x4158,{0x87,0xff,0xf0,0x4d,0x63,0x96,0xa9,0x15}};
static GUID accessGuid={0xe770bb69,0xbcb4,0x4d04,{0x9e,0x97,0x23,0xff,0x94,0x56,0xfe,0xac}};
static char logbuf[12288];
static N used;
static void text(const char *s) { while(*s && used<sizeof(logbuf)-1) logbuf[used++]=*s++; }
static void hex(U64 x,N digits) { const char *h="0123456789ABCDEF"; while(digits) { digits--; if(used<sizeof(logbuf)-1)logbuf[used++]=h[(x>>(digits*4))&15]; } }
static void console(SYSTEM *s,const char *msg) { C16 buf[256]; N i=0; while(*msg && i<255)buf[i++]=(U8)*msg++; buf[i]=0; ((PRINT)s->cout->fn[1])(s->cout,buf); }
static void variable(SYSTEM *s,const char *label,C16 *name,GUID *g) {
 U8 data[256]; U32 attrs=0; N size=sizeof(data);
 STATUS status=((GET_VARIABLE)s->rt->fn[6])(name,g,&attrs,&size,data);
 text(label); text(": status=0x");hex(status,16);text(" size=0x");hex(size,16);
 if(!BAD(status)) { text(" attrs=0x");hex(attrs,8);text(" data=");for(N i=0;i<size && i<sizeof(data);i++)hex(data[i],2); }
 text("\r\n");
}
static int usbPath(U8 *p) {
 int usb=0; N walked=0;
 while(walked<4096) {
  N len=(N)p[2]|((N)p[3]<<8); if(len<4 || walked+len>4096)return 0;
  if(p[0]==3 && (p[1]==5 || p[1]==15 || p[1]==16))usb=1;
  if(p[0]==127 && p[1]==255)return usb;
  walked+=len;p+=len;
 }
 return 0;
}
/* SBAT included for normal shim compatibility; not a trust grant. */
__attribute__((section(".sbat"),used)) static const char sbat[]=
 "sbat,1,SBAT Version,sbat,1,https://github.com/rhboot/shim/blob/main/SBAT.md\n"
 "pb60diag,1,Local PB60 Diagnostic,pb60diag,1,https://uefi.org/specifications\n";
STATUS efi_main(HANDLE image,SYSTEM *s) {
 HANDLE_PROTOCOL hp=(HANDLE_PROTOCOL)s->bs->fn[16];
 LOADED *loaded=0; U8 *path=0; FS *fs=0; FILE *root=0,*marker=0,*file=0;
 STATUS status; const char expected[]="PB60_READ_ONLY_DIAGNOSTIC_V1\r\n";
 char markerbuf[64]; N size=sizeof(markerbuf); C16 filename[]=L"PB60EFI-0001.TXT";
 console(s,"PB60 read-only EFI diagnostic v1\r\n");
 if(BAD(hp(image,&loadedGuid,(void**)&loaded)) || !loaded ||
    BAD(hp(loaded->device,&pathGuid,(void**)&path)) || !path || !usbPath(path) ||
    BAD(hp(loaded->device,&fsGuid,(void**)&fs)) || !fs || BAD(fs->root(fs,&root))) {
  console(s,"STOP: loaded image is not on a confirmed USB filesystem. No log written.\r\n");return ERROR(3);
 }
 status=root->open(root,&marker,L"PB60_DIAG_MEDIA.ID",1,0);
 if(BAD(status)) { root->close(root);console(s,"STOP: diagnostic USB marker missing. No log written.\r\n");return status; }
 status=marker->read(marker,&size,markerbuf);marker->close(marker);
 int match=!BAD(status) && size==sizeof(expected)-1;
 for(N i=0;match && i<size;i++) if(markerbuf[i]!=expected[i])match=0;
 if(!match) { root->close(root);console(s,"STOP: diagnostic USB marker invalid. No log written.\r\n");return ERROR(2); }
 text("PB60 read-only preboot diagnostic v1\r\n");
 text("Only GetVariable is used. No variable writes, TPM access, flash access or reboot.\r\n");
 text("Log destination: loaded image USB device; marker verified.\r\nFirmware vendor: ");
 for(N i=0;s->vendor && s->vendor[i] && i<128;i++) { char c[2]={(s->vendor[i]<128)?(char)s->vendor[i]:'?',0};text(c); }
 text("\r\nFirmware revision=0x");hex(s->revision,8);text(" UEFI revision=0x");hex(s->h.revision,8);text("\r\n");
 variable(s,"SecureBoot",L"SecureBoot",&globalGuid);
 variable(s,"SetupMode",L"SetupMode",&globalGuid);
 variable(s,"VendorKeys",L"VendorKeys",&globalGuid);
 variable(s,"AuditMode",L"AuditMode",&globalGuid);
 variable(s,"DeployedMode",L"DeployedMode",&globalGuid);
 variable(s,"SecureBootSetup",L"SecureBootSetup",&setupGuid);
 variable(s,"SystemAccess",L"SystemAccess",&accessGuid);
 text("NOT_FOUND does not imply zero. SystemAccess may be absent outside BIOS Setup.\r\n");
 text("This probe cannot establish write acceptance or PK deletion permission.\r\n");
 text("DIAGNOSTIC_COMPLETE\r\n");
 for(N number=1;number<=9999;number++) {
  N n=number;for(int i=11;i>=8;i--){ filename[i]=(C16)('0'+n%10);n/=10; }
  status=root->open(root,&file,filename,1,0);
  if(!BAD(status)) { file->close(file);file=0;continue; }
  if(status!=ERROR(14))break;
  status=root->open(root,&file,filename,0x8000000000000003ULL,0);
  if(BAD(status))break;
  size=used;status=file->write(file,&size,logbuf);
  if(!BAD(status) && size==used)status=file->flush(file);else if(!BAD(status))status=ERROR(7);
  file->close(file);file=0;
  root->close(root);
  if(BAD(status)) { console(s,"ERROR: USB log write/flush failed. Do not rely on a partial log.\r\n");return status; }
  console(s,"SUCCESS: saved ");((PRINT)s->cout->fn[1])(s->cout,filename);
  console(s," to the USB. No firmware variables changed by this application.\r\n");
  console(s,"Returning to the boot manager after 15 seconds. No reboot requested.\r\n");
  ((STALL)s->bs->fn[28])(15000000);return 0;
 }
 root->close(root);console(s,"ERROR: could not create a new numbered USB log.\r\n");return BAD(status)?status:ERROR(9);
}
