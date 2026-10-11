/* Keep tiny-font numeric entries on the channel's original retail palette. */
static void (*nc_original_append)(Box *,int);
static void nc_append(Box *box,int code) {
 unsigned flags=(unsigned)box->bytes[0x34]|((unsigned)box->bytes[0x35]<<8);
 unsigned color=box->bytes[0x54];
 if((flags&0x180)==0x100 && (color==7 || (color>=9 && color<16))) {
  for(int i=0;i<16;i++) if(saved[i].box==box) {
   box->bytes[0x54]=saved[i].color;
   nc_original_append(box,code);
   box->bytes[0x54]=(u8)color;
   return;
  }
 }
 nc_original_append(box,code);
}
static int nc_install(const Host *api) {
 void *fn=api->symbol(api,"func_80036C14");
 return fn && api->hook(api,fn,(void *)nc_append,(void **)&nc_original_append);
}
