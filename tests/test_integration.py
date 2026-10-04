from pathlib import Path
import sys
from lupa import LuaRuntime
root=Path(__file__).resolve().parents[1]/'package-template'
for p in (root/'payload').rglob('*.lua'):
 lua=LuaRuntime();lua.execute('assert(load(...))',p.read_text(encoding='utf-8-sig'))
source=(root/'payload/Game/Binaries/Win64/ue4ss/Mods/AC8SourceInit/Scripts/main.lua').read_text()
for case in ('normal','visual-failure','source-failure'):
 lua=LuaRuntime();lua.globals().case=case
 lua.execute(r'''
 calls,prepares,starts=0,0,0
 function print()end
 package.preload.config=function()return {msl_visuals=true,acceptance_recording=false}end
 package.preload.source_init=function()return {apply=function()
  calls=calls+1;return {ok=case~='source-failure',verified=375,written=0,error='test',rollbackErrors={}}
 end}end
 package.preload.msl_visuals=function()return {start=function()starts=starts+1 end,prepare=function()
  prepares=prepares+1;if case=='visual-failure'then error('test mesh failure')end
 end}end
 function RegisterInitGameStatePreHook(fn)init=fn end
 function ExecuteInGameThreadAfterFrames(n,fn)assert(n==2);bootstrap=fn end
 function mission()init({get=function()return {IsValid=function()return true end,GetFullName=function()return 'Mode /Game/Maps/Ingame/Test'end}end})end
 ''')
 lua.execute(source)
 lua.execute(r'''
 assert(starts==1);mission();assert(calls==0);bootstrap();mission()
 if case=='source-failure'then assert(calls==1 and prepares==0)
 else assert(calls==2 and prepares==2)end
 ''')
print('PASS all packaged Lua syntax; source validation precedes visuals; cosmetic failure does not disable missile performance; source failure blocks dependent visual setup.')


