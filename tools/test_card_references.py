#!/usr/bin/env python3
"""Check all added-card bases and exercise production CardRules_Equip."""
import csv,hashlib,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def digest(v):return hashlib.sha256(json.dumps(v,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()).hexdigest()
def main():
 mod=json.loads((ROOT/'Resources/Lekak/mod.json').read_text())
 before=json.loads((ROOT/'tools/card-reference-baseline.json').read_text())
 retail={int(c['id']):c for c in csv.DictReader((ROOT/'EngineSDK/notes/card-catalog.csv').open())}
 fields={'copy','type','attack','defense','attribute','level','stars'}
 assert digest({k:v for k,v in mod.items() if k!='cards'})==before['other_sections_sha256']
 assert len(mod['cards'])==len(before['cards'])
 cards=[]
 for c,prev in zip(mod['cards'],before['cards']):
  if 'copy' not in c:assert digest(c)==prev['sha256'];continue
  assert c['id']==prev['id']
  row=retail[c['copy']];assert c['type']==row['type'],c['id']
  visible={k:c.get(k,int(row[k]) if k in ['attack','defense','level'] else row[k]) for k in ['type','attack','defense','attribute','level']}
  visible['stars']=c.get('stars',[row['guardian_star_1'],row['guardian_star_2']])
  assert visible==prev['visible'],c['id']
  assert digest({k:v for k,v in c.items() if k not in fields})==prev['other_fields_sha256'],c['id']
  cards.append(c)
 assert len(cards)==518
 equips=list(csv.DictReader((ROOT/'EngineSDK/notes/research/fusion-and-drop-tables/equips.csv').open()))
 pairs={(int(e['equip_id']),int(e['monster_id'])) for e in equips}
 generic={'Spellcaster':323,'Warrior':301,'Dragon':315,'Machine':325,'Zombie':322,'Fiend':303,'Beast':308,'Beast-Warrior':308,'Rock':324,'Plant':310,'Pyro':654,'Insect':306,'Dinosaur':326,'Reptile':326,'Sea Serpent':328,'Aqua':328,'Fish':328,'Fairy':307,'Winged Beast':327,'Thunder':657}
 # Confirm the actual catalogue ids of each representative equipment.
 for c in cards:assert (generic[c['type']],c['copy']) in pairs,(c['id'],c['type'],generic[c['type']])
 production=(ROOT/'EngineSDK/src/game/duel_card_checks.c').read_text()
 start=production.index('int CardRules_Equip(int a, int b)')
 function=production[start:production.index('\ns32 Duel_CheckEquip(s32 arg0, s32 arg1)',start)]
 types={t:i for i,t in enumerate(sorted({v['type'] for v in retail.values()}))}
 fixture='''#include <assert.h>\n#include <stdio.h>\n#define CARD_TYPE_EQUIP 23\nstatic int base[1241],kind[1241],retail_kind[723];
static int Cards_Valid(int id){return id>0&&id<=1240;}
static int Cards_BaseId(int id){return base[id];}
static int Cards_EffectId(int id){return base[id];}
static int Cards_Type(int id){return kind[id];}
static int Cards_RetailType(int id){return retail_kind[id];}
static int Cards_KindChanged(int id){(void)id;return 0;}
/* No explicit equip overrides in this manifest; retail compatibility decides. */
static int Tables_Equip(int a,int b){(void)a;(void)b;return -1;}
static const int pairs[][2]={'''+','.join('{%d,%d}'%p for p in sorted(pairs))+'''};
static int Duel_CheckEquipRetail(int a,int b){for(unsigned i=0;i<sizeof(pairs)/sizeof(*pairs);i++)if(pairs[i][0]==a&&pairs[i][1]==b)return 1;return 0;}
'''+function+'\nint main(void){\n'
 for id,row in retail.items():fixture+=f'base[{id}]={id};kind[{id}]=retail_kind[{id}]={23 if row["type"]=="Equip" else types[row["type"]]};\n'
 for id,c in enumerate(cards,723):
  fixture+=f'base[{id}]={c["copy"]};kind[{id}]=retail_kind[{c["copy"]}];assert(CardRules_Equip({generic[c["type"]]},{id})=={id});\n'
  if c['type']=='Spellcaster':fixture+=f'assert(CardRules_Equip(323,{id})=={id});assert(CardRules_Equip(301,{id})==0);\n'
 fixture+='puts("518 reference types, unchanged visible stats/data, representative equips and 37 Spellcaster book/sword checks passed");return 0;}\n'
 with tempfile.TemporaryDirectory(prefix='lekak-equip-') as folder:
  p=Path(folder);(p/'test.c').write_text(fixture)
  subprocess.run([os.environ.get('CC','cc'),'-std=gnu11','-O2','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
if __name__=='__main__':main()
