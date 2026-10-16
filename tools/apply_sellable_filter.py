from pathlib import Path
import hashlib,json

def patch(s):
 assert '(all_cards||owned(id))' in s
 s=s.replace('quantity,all_cards,english','quantity,english').replace('(all_cards||owned(id))','sellable(id)>0')
 s=s.replace('  else if(p&PAD_BUTTON_R1){all_cards=!all_cards;rebuild();}\n','')
 for old in ['all_cards?(english?"All cards | 3 copies kept":"Toutes | 3 exemplaires gardes"):(english?"Owned | 3 copies kept":"Possedees | 3 exemplaires gardes")','all_cards?(english?"All cards (including missing)":"Toutes les cartes (meme non possedees)"):(english?"Owned cards | 3 copies protected":"Cartes possedees | reserve : 3 exemplaires")']:
  s=s.replace(old,'english?"Sellable | 3 copies kept":"Vendables | 3 exemplaires gardes"')
 s=s.replace('"No matching cards.":"Aucune carte correspondante."','"No sellable cards.":"Aucune carte a vendre."')
 s=s.replace(' | R1: all','').replace(' | R1:toutes','').replace(' | R1: owned/all','').replace(' | R1 : toutes','')
 assert 'all_cards' not in s
 return s

if __name__=='__main__':
 r=Path(__file__).resolve().parents[1]
 p=r/'SourceMod/Lekak/Collection/collection.c';p.write_text(patch(p.read_text()))
 p=r/'tools/build_native_ipa.py';s=p.read_text().replace("CFBundleShortVersionString='0.32',CFBundleVersion='32'","CFBundleShortVersionString='0.33',CFBundleVersion='33'").replace('Lekak_iPhone_32_','Lekak_iPhone_33_');p.write_text(s)
 p=r/'windows-port-inventory.json';j=json.loads(p.read_text());j['files']['SourceMod/Lekak/Collection/collection.c']=hashlib.sha256((r/'SourceMod/Lekak/Collection/collection.c').read_bytes()).hexdigest();p.write_text(json.dumps(j,indent=2)+'\n')
 print('v33: sellable-only collection filter applied')
