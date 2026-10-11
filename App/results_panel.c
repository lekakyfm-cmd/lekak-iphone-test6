#include "results_panel.h"
#include <stdatomic.h>
static atomic_int visible;
void LekakResults_Publish(int value){atomic_store(&visible,value!=0);}
int LekakResults_Visible(void){return atomic_load(&visible);}
