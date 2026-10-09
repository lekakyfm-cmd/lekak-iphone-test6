"""Exercise the production manifest reader, password resolver and shop lookup."""
import csv,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT/'EngineSDK'
def main():
 mod=json.loads((ROOT/'Resources/Lekak/mod.json').read_text())
 special=json.loads((ROOT/'tools/shop-special-passwords.json').read_text())
 retail={int(r['id']):r for r in csv.DictReader((SDK/'notes/card-catalog.csv').open())}
 expected=[];identities={};own={};nextid=723
 def price(atk,kind):return 500 if kind in ['Magic','Trap','Ritual','Equip'] else min(999999,500*2**max(0,(int(atk)-1500+499)//500))
 for id,row in retail.items():expected.append((id,price(row['attack'],row['type'])))
 for c in mod['cards']:
  if 'copy' not in c:continue
  id=nextid;nextid+=1;row=retail[c['copy']];key=mod['id']+':'+c['id']+':1';identities[key]=id
  expected.append((id,price(c.get('attack',row['attack']),c.get('type',row['type']))))
  if c.get('password'):own[id]=int(c['password'],16)
 assert len(expected)==1240 and len(special)==23
 text=(SDK/'src/pc/cards/tables.c').read_text();section=text[text.index('#define SHOP_PASSWORD 1'):text.index('/* --- building')]
 start=section.index('typedef struct { unsigned password; int id; } ShopPassword;');end=section.index('int Tables_PasswordShop(',start);section=section[:start]+section[end:]
 shop=(SDK/'src/overlays/password/shop.c').read_text();lookup=shop[shop.index('s32 Password_LookupCardID(void)'):shop.index('void Password_UpdateShopScreen(void)')]
 driver='''#include <assert.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <strings.h>\n#include <stdarg.h>\n#include "pc/mods/json.h"\n#include "pc/cards/passwords.h"\n#define STARCHIP_MAX 999999L
typedef int s32;int gCard_nCount=1240;unsigned char gPassword_abDigits[8];
static unsigned own[1241];static unsigned rawpass[723];
static int errors;
int Cards_Valid(int id){return id>0&&id<=1240;}
int Cards_OwnPassword(int id,unsigned *p){if(!own[id])return 0;*p=own[id];return 1;}
static int same_letters(const char *a,const char *b){return !strcasecmp(a,b);}
void Mods_Note(const char *mod,const char *fmt,...){(void)mod;errors++;va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);}
int Memories_DiscFileStart(const char *p){(void)p;return 0;}
int Memories_DiscReadSectors(int lba,int n,void *out){(void)lba;memset(out,0,n*2048);for(int id=1;id<=722;id++){((unsigned *)out)[id*2]=999999;((unsigned *)out)[id*2+1]=rawpass[id];}return n;}
int Cards_Named(const char *key){char *end;long n=strtol(key,&end,10);if(!*end&&Cards_Valid(n))return n;
'''
 for key,id in identities.items():driver+='if(!strcmp(key,'+json.dumps(key)+'))return '+str(id)+';\n'
 driver+='return 0;}\n'+section+'\n'+lookup+'\nint main(int argc,char **argv){assert(argc==2);char error[200];JsonDocument *doc=Json_ParseFile(argv[1],error,sizeof(error));assert(doc);read_passwords("Lekakmod",Json_Member(Json_Root(doc),"passwords"));assert(!errors);\n'
 for id,row in retail.items():driver+=f'rawpass[{id}]=0x{row["password"] if row["password"].isdigit() else "FFFFFFFE"}u;\n'
 for id,p in own.items():driver+=f'own[{id}]=0x{p:08X}u;\n'
 for id,cost in expected:driver+=f'assert(Cards_PasswordPrice({id})=={cost});\n'
 for key,v in special.items():
  driver+=f'{{const char *s="{v["password"]}";for(int i=0;i<8;i++)gPassword_abDigits[i]=s[i]-\'0\';assert(Password_LookupCardID()=={key});assert(Cards_Password({key})==0x{v["password"]}u);}}\n'
 driver+='for(int id=1;id<=1240;id++)assert(Cards_PasswordPrice(id)<=64000);puts("1240 live-policy prices and 23 password-screen lookups (00000005..00000027) passed.");Json_Free(doc);return 0;}\n'
 with tempfile.TemporaryDirectory(prefix='lekak-shop-') as folder:
  p=Path(folder);(p/'test.c').write_text(driver)
  command=['cc','-std=gnu11','-O2','-DMEMORIES_PC','-I'+str(SDK/'src'),'-I'+str(SDK/'src/pc/cards'),str(p/'test.c'),str(SDK/'src/pc/mods/json.c'),str(ROOT/'tools/shop-password-policy.c')]
  policy=p/'card-policy.c';policy.write_text('''#include "pc/cards/passwords.h"\nunsigned Cards_Password(int id){return CardPassword_Resolve(id);}\nunsigned Cards_PasswordPrice(int id){return CardPassword_ResolvePrice(id);}\n''')
  subprocess.run(command+[str(policy),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test'),str(ROOT/'Resources/Lekak/mod.json')],check=True)
if __name__=='__main__':main()
