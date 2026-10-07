#include "src/custom_keys.h"
#include <cassert>
#include <cstdio>
int main(){using namespace custom_keys;
 auto defaults_word=pack(defaults);bool held_keys[256]{};auto held=[&](int key){return held_keys[key];};
 assert(valid(defaults)&&get(defaults_word,Action::FreeLook)=='C'&&get(defaults_word,Action::Zoom)==2);
 held_keys['W']=true;assert(manual_mask(defaults_word,held)==5);held_keys['W']=false;held_keys['A']=true;assert(manual_mask(defaults_word,held)==4);held_keys['A']=false;held_keys['Q']=true;assert(manual_mask(defaults_word,held)==2);held_keys['Q']=false;
 auto remap=defaults;remap[0]='V';remap[1]=5;remap[2]='I';remap[3]='K';remap[4]='J';remap[5]='L';remap[6]='U';remap[7]='O';assert(valid(remap));auto word=pack(remap);
 held_keys['C']=true;held_keys[2]=true;held_keys['W']=true;assert(!down(word,Action::FreeLook,held)&&!down(word,Action::Zoom,held)&&manual_mask(word,held)==0);
 held_keys['V']=true;held_keys[5]=true;held_keys['I']=true;assert(down(word,Action::FreeLook,held)&&down(word,Action::Zoom,held)&&manual_mask(word,held)==5);
 auto invalid=remap;invalid[1]=invalid[0];assert(!valid(invalid));invalid[1]=112;assert(!valid(invalid));invalid[1]=256;assert(!valid(invalid));
 remap[0]=0;remap[1]=0;assert(valid(remap));assert(!down(pack(remap),Action::FreeLook,held));
 puts("PASS default/remapped hold actions, player manual axis leases, old keys ignored, reserved/conflicting rejection and unbound actions");}
