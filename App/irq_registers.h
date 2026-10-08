#ifndef LEKAK_IRQ_REGISTERS_H
#define LEKAK_IRQ_REGISTERS_H
#include <stdint.h>
/* Register-only model. No device edges, timing, CP0 or exception delivery.
 * Undefined upper bits read as zero. Reset starts with no requests/mask. */
typedef struct LekakIrqRegisters { uint32_t status,mask,reads,writes; } LekakIrqRegisters;
static inline int LekakIrq_Index(uint32_t address,unsigned bytes) {
 uint32_t physical=address&0x1fffffffu;
 if(bytes!=2&&bytes!=4)return -1;
 if(physical==0x1f801070u)return 0;
 if(physical==0x1f801074u)return 1;
 return -1;
}
static inline int LekakIrq_Read(LekakIrqRegisters *irq,uint32_t address,unsigned bytes,uint32_t *value){
 int index=LekakIrq_Index(address,bytes);if(index<0)return 0;
 *value=index?irq->mask:irq->status;irq->reads++;return 1;
}
static inline int LekakIrq_Write(LekakIrqRegisters *irq,uint32_t address,unsigned bytes,uint32_t value){
 int index=LekakIrq_Index(address,bytes);if(index<0)return 0;
 if(index)irq->mask=value&0x7ffu;
 else irq->status&=value&0x7ffu; /* Zero acknowledges; one leaves unchanged. */
 irq->writes++;return 1;
}
#endif
