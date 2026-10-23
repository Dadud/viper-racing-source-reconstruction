loc_00401000:
00401000: cmp byte ptr [esp+4],0
00401005: mov eax,40620Ch
0040100A: jne short 00401011h
0040100C: mov eax,406218h
loc_00401011:
00401011: ret
00401012: lea esp,[esp]
00401019: lea esp,[esp]
loc_00401020:
00401020: lea eax,[esp+8]
00401024: mov ecx,[esp+4]
00401028: sub esp,100h
0040102E: lea edx,[esp]
00401032: push edi
00401033: mov byte ptr [esp+103h],0
0040103B: push eax
0040103C: push ecx
0040103D: push edx
0040103E: call 00402090h
00401043: lea edi,[esp+10h]
00401047: add esp,0Ch
0040104A: mov edx,406224h
0040104F: mov ecx,0FFFFFFFFh
00401054: sub eax,eax
00401056: repne scasb [edi]
00401058: dec edi
00401059: mov ecx,[edx]
0040105B: mov eax,[edx+4]
0040105E: mov [edi],ecx
00401060: mov cx,[edx+8]
00401064: mov [edi+4],eax
00401067: mov [edi+8],cx
0040106B: mov dl,[edx+0Ah]
0040106E: mov [edi+0Ah],dl
00401071: cmp byte ptr [esp+103h],0
00401079: jne short 00401090h
0040107B: lea eax,[esp+4]
0040107F: push eax
00401080: call 004010B0h
00401085: add esp,4
00401088: pop edi
00401089: add esp,100h
0040108F: ret
loc_00401090:
00401090: call 004010A0h
00401095: pop edi
00401096: add esp,100h
0040109C: ret
0040109D: lea ecx,[ecx]
loc_004010A0:
004010A0: push 1
004010A2: call 00402130h
004010A7: add esp,4
004010AA: ret
004010AB: add eax,0
loc_004010B0:
004010B0: push 10034h
004010B5: mov eax,[40501Ch]
004010BA: mov ecx,[esp+8]
004010BE: push eax
004010BF: push ecx
004010C0: push 0
004010C2: call dword ptr [40925Ch]
004010C8: sub eax,7
004010CB: cmp eax,1
004010CE: sbb al,al
004010D0: neg al
004010D2: mov [406030h],al
004010D7: ret
004010D8: lea esp,[esp]
004010DC: lea esp,[esp]
loc_004010E0:
004010E0: lea eax,[esp+8]
004010E4: mov ecx,[esp+4]
004010E8: sub esp,100h
004010EE: lea edx,[esp]
004010F2: push eax
004010F3: mov byte ptr [esp+103h],0
004010FB: push ecx
004010FC: push edx
004010FD: call 00402090h
00401102: add esp,0Ch
00401105: cmp byte ptr [esp+0FFh],0
0040110D: jne short 0040112Ah
0040110F: lea eax,[esp]
00401113: push eax
00401114: call 00401140h
00401119: mov byte ptr [406030h],1
00401120: add esp,4
00401123: add esp,100h
00401129: ret
loc_0040112A:
0040112A: call 004010A0h
0040112F: add esp,100h
00401135: ret
00401136: lea ecx,[ecx]
00401139: lea esp,[esp]
loc_00401140:
00401140: mov eax,[esp+4]
00401144: push 10010h
00401149: push 406230h
0040114E: push eax
0040114F: push 0
00401151: call dword ptr [40925Ch]
00401157: ret
00401158: lea esp,[esp]
0040115C: lea esp,[esp]
00401160: mov eax,[esp+8]
00401164: push esi
00401165: mov ecx,[esp+8]
00401169: push eax
0040116A: push ecx
0040116B: call dword ptr [409254h]
00401171: mov esi,eax
00401173: test esi,esi
00401175: je short 004011A1h
00401177: push 0FFFFFFF0h
00401179: push esi
0040117A: call dword ptr [409278h]
00401180: or eax,8000000h
00401185: push eax
00401186: push 0FFFFFFF0h
00401188: push esi
00401189: call dword ptr [409268h]
0040118F: push 1
00401191: push 0
00401193: push esi
00401194: call dword ptr [409258h]
0040119A: push esi
0040119B: call dword ptr [409260h]
loc_004011A1:
004011A1: pop esi
004011A2: ret
004011A3: lea ebx,[ebx]
004011A9: lea esp,[esp]
004011B0: mov eax,[esp+8]
004011B4: push esi
004011B5: mov ecx,[esp+8]
004011B9: push eax
004011BA: push ecx
004011BB: call dword ptr [409254h]
004011C1: mov esi,eax
004011C3: test esi,esi
004011C5: je short 004011FAh
004011C7: push 0FFFFFFF0h
004011C9: push esi
004011CA: call dword ptr [409278h]
004011D0: and eax,0F7FFFFFFh
004011D5: push eax
004011D6: push 0FFFFFFF0h
004011D8: push esi
004011D9: call dword ptr [409268h]
004011DF: push 1
004011E1: push esi
004011E2: call dword ptr [40926Ch]
004011E8: push 1
004011EA: push 0
004011EC: push esi
004011ED: call dword ptr [409258h]
004011F3: push esi
004011F4: call dword ptr [409260h]
loc_004011FA:
004011FA: pop esi
004011FB: ret
004011FC: lea esp,[esp]
loc_00401200:
00401200: sub esp,460h
00401206: mov ecx,0FFFFFFFFh
0040120B: sub eax,eax
0040120D: push ebx
0040120E: push esi
0040120F: push edi
00401210: mov edi,[esp+478h]
00401217: push ebp
00401218: repne scasb [edi]
0040121A: not ecx
0040121C: sub edi,ecx
0040121E: mov eax,ecx
00401220: shr ecx,2
00401223: mov esi,edi
00401225: lea edi,[esp+270h]
0040122C: rep movsd [edi],[esi]
0040122E: mov ecx,eax
00401230: and ecx,3
00401233: rep movsb [edi],[esi]
00401235: mov ecx,[esp+474h]
0040123C: mov eax,[40500Ch]
00401241: push eax
00401242: mov [4074F8h],ecx
00401248: push 1
0040124A: push 0
0040124C: push 0
0040124E: call dword ptr [4091B8h]
00401254: mov ebp,eax
00401256: test ebp,ebp
00401258: je 0040158Ah
0040125E: call dword ptr [4091B4h]
00401264: cmp eax,0B7h
00401269: je 0040158Ah
0040126F: call 00401600h
00401274: test al,al
00401276: jne 00401563h
0040127C: mov dword ptr [esp+68h],401640h
00401284: mov dword ptr [esp+6Ch],401670h
0040128C: lea eax,[esp+270h]
00401293: push 406240h
00401298: mov dword ptr [esp+74h],401690h
004012A0: mov dword ptr [esp+78h],4019C0h
004012A8: mov dword ptr [esp+7Ch],4017C0h
004012B0: mov dword ptr [esp+80h],4017B0h
004012BB: mov dword ptr [esp+84h],401730h
004012C6: push eax
004012C7: call 004015B0h
004012CC: add esp,8
004012CF: test al,al
004012D1: je short 004012D8h
004012D3: call 00401860h
loc_004012D8:
004012D8: lea eax,[esp+270h]
004012DF: push 40624Ch
004012E4: push eax
004012E5: call 004015B0h
004012EA: add esp,8
004012ED: test al,al
004012EF: je short 004012FBh
004012F1: push 0
004012F3: call 004019A0h
004012F8: add esp,4
loc_004012FB:
004012FB: lea eax,[esp+270h]
00401302: push 406254h
00401307: mov byte ptr [esp+170h],1
0040130F: push eax
00401310: call 004015B0h
00401315: add esp,8
00401318: test al,al
0040131A: jne short 00401343h
0040131C: lea esi,[esp+68h]
loc_00401320:
00401320: call dword ptr [esi]
00401322: cmp byte ptr [406030h],0
00401329: jne short 0040133Bh
0040132B: add esi,4
0040132E: lea eax,[esp+84h]
00401335: cmp esi,eax
00401337: jb short 00401320h
00401339: jmp short 00401343h
loc_0040133B:
0040133B: mov byte ptr [esp+16Ch],0
loc_00401343:
00401343: xor bl,bl
00401345: push 406260h
0040134A: lea eax,[esp+274h]
00401351: push eax
00401352: call 004015B0h
00401357: add esp,8
0040135A: test al,al
0040135C: je short 00401360h
0040135E: mov bl,1
loc_00401360:
00401360: cmp byte ptr [esp+16Ch],0
00401368: je 00401583h
0040136E: mov al,[406268h]
00401373: lea edi,[esp+16Dh]
0040137A: mov ecx,40h
0040137F: mov [esp+16Ch],al
00401386: xor eax,eax
00401388: rep stosd [edi]
0040138A: stosw [edi]
0040138C: stosb [edi]
0040138D: mov edi,40626Ch
00401392: mov ecx,0FFFFFFFFh
00401397: sub eax,eax
00401399: repne scasb [edi]
0040139B: not ecx
0040139D: sub edi,ecx
0040139F: mov edx,ecx
004013A1: shr ecx,2
004013A4: mov esi,edi
004013A6: lea edi,[esp+16Ch]
004013AD: rep movsd [edi],[esi]
004013AF: mov ecx,edx
004013B1: and ecx,3
004013B4: rep movsb [edi],[esi]
004013B6: lea eax,[esp+270h]
004013BD: push eax
004013BE: push ebx
004013BF: call 00401000h
004013C4: lea ecx,[esp+70h]
004013C8: add esp,4
004013CB: push eax
004013CC: xor esi,esi
004013CE: lea eax,[esp+174h]
004013D5: mov bl,1
004013D7: push eax
004013D8: push 406274h
004013DD: push ecx
004013DE: call 00402210h
004013E3: lea eax,[esp+180h]
004013EA: add esp,14h
004013ED: push eax
004013EE: call 00401880h
004013F3: add esp,4
004013F6: test al,al
004013F8: je short 00401403h
004013FA: lea esi,[esp+16Ch]
00401401: jmp short 00401412h
loc_00401403:
00401403: push 40627Ch
00401408: xor bl,bl
0040140A: call 004010E0h
0040140F: add esp,4
00401412: lea eax,[esp+14h]
00401416: lea edi,[esp+24h]
0040141A: mov ecx,11h
0040141F: mov dword ptr [eax],0
00401425: mov dword ptr [eax+4],0
0040142C: mov dword ptr [eax+8],0
00401433: mov dword ptr [eax+0Ch],0
0040143A: xor eax,eax
0040143C: rep stosd [edi]
0040143E: mov dword ptr [esp+24h],44h
00401446: mov dword ptr [esp+50h],1
0040144E: test bl,bl
00401450: je 00401583h
00401456: lea eax,[esp+14h]
0040145A: lea ecx,[esp+24h]
0040145E: push eax
0040145F: push ecx
00401460: lea edx,[esp+70h]
00401464: push esi
00401465: push 0
00401467: push 0
00401469: push 0
0040146B: push 0
0040146D: push 0
0040146F: push edx
00401470: push 0
00401472: call dword ptr [4091B0h]
00401478: test eax,eax
0040147A: je 0040154Fh
00401480: push 9C4h
00401485: call dword ptr [4091C4h]
0040148B: mov eax,[esp+14h]
0040148F: push 0FFFFFFFFh
00401491: push eax
00401492: call dword ptr [4091C0h]
00401498: lea ecx,[esp+10h]
0040149C: mov eax,[esp+14h]
004014A0: push ecx
004014A1: push eax
004014A2: call dword ptr [4091ACh]
004014A8: test eax,eax
004014AA: je 00401583h
004014B0: cmp dword ptr [esp+10h],0
004014B5: je 00401583h
004014BB: mov eax,[esp+10h]
004014BF: sub eax,2
004014C2: cmp eax,4
004014C5: ja short 004014CEh
004014C7: jmp dword ptr [eax*4+40159Ch]
loc_004014CE:
004014CE: mov eax,[esp+10h]
004014D2: mov ecx,[40501Ch]
004014D8: push eax
004014D9: push ecx
004014DA: push 4063BCh
004014DF: call 004010E0h
004014E4: add esp,0Ch
004014E7: call 00401870h
004014EC: push 5
004014EE: push 4063ECh
004014F3: call dword ptr [4091D8h]
004014F9: jmp 00401583h
004014FE: push 4062B4h
00401503: call 004010E0h
00401508: add esp,4
0040150B: jmp short 00401583h
0040150D: push 406308h
00401512: call 004010E0h
00401517: add esp,4
0040151A: jmp short 00401583h
0040151C: push 4062D8h
00401521: call 004010E0h
00401526: add esp,4
00401529: jmp short 00401583h
0040152B: push 40633Ch
00401530: call 004010E0h
00401535: add esp,4
00401538: jmp short 00401583h
0040153A: mov eax,[40501Ch]
0040153F: push eax
00401540: push 406384h
00401545: call 004010E0h
0040154A: add esp,8
0040154D: jmp short 00401583h
loc_0040154F:
0040154F: lea eax,[esp+68h]
00401553: push eax
00401554: push 406400h
00401559: call 004010E0h
0040155E: add esp,8
00401561: jmp short 00401583h
loc_00401563:
00401563: mov eax,[405014h]
00401568: mov ecx,[405010h]
0040156E: push eax
0040156F: push ecx
00401570: call dword ptr [409270h]
00401576: test eax,eax
00401578: je short 00401583h
0040157A: push 9
0040157C: push eax
0040157D: call dword ptr [409274h]
loc_00401583:
00401583: push ebp
00401584: call dword ptr [409224h]
loc_0040158A:
0040158A: xor eax,eax
0040158C: pop ebp
0040158D: pop edi
0040158E: pop esi
0040158F: pop ebx
00401590: add esp,460h
00401596: ret 10h
00401599: lea ecx,[ecx]
0040159C: (bad)
0040159E: inc eax
0040159F: add [1C004015h],cl
004015A5: adc eax,152B0040h
004015AA: inc eax
004015AB: add [edx],bh
004015AD: adc eax,448B0040h
004015B2: and al,4
004015B4: push esi
004015B5: push edi
004015B6: mov edi,[esp+10h]
004015BA: push edi
004015BB: push eax
004015BC: call 004023D0h
004015C1: add esp,8
004015C4: mov esi,eax
004015C6: test esi,esi
004015C8: je short 004015F2h
004015CA: mov ecx,0FFFFFFFFh
004015CF: sub eax,eax
004015D1: repne scasb [edi]
004015D3: not ecx
004015D5: sub eax,eax
004015D7: lea edx,[ecx+esi-1]
004015DB: mov edi,edx
004015DD: mov ecx,0FFFFFFFFh
004015E2: repne scasb [edi]
004015E4: not ecx
004015E6: dec ecx
004015E7: push ecx
004015E8: push edx
004015E9: push esi
004015EA: call 00402280h
004015EF: add esp,0Ch
loc_004015F2:
004015F2: pop edi
004015F3: cmp esi,1
004015F6: sbb al,al
004015F8: pop esi
004015F9: inc al
004015FB: ret
004015FC: lea esp,[esp]
loc_00401600:
00401600: push ebx
00401601: mov eax,[405008h]
00401606: push esi
00401607: xor bl,bl
00401609: push eax
0040160A: push 1
0040160C: push 0
0040160E: push 0
00401610: call dword ptr [4091B8h]
00401616: mov esi,eax
00401618: test esi,esi
0040161A: je short 00401632h
0040161C: call dword ptr [4091B4h]
00401622: cmp eax,0B7h
00401627: jne short 0040162Bh
00401629: mov bl,1
loc_0040162B:
0040162B: push esi
0040162C: call dword ptr [409224h]
loc_00401632:
00401632: mov al,bl
00401634: pop esi
00401635: pop ebx
00401636: ret
00401637: mov edi,edi
00401639: lea esp,[esp]
00401640: call dword ptr [4091BCh]
00401646: cmp eax,80000000h
0040164B: mov al,1
0040164D: adc al,0FFh
0040164F: je short 00401664h
00401651: mov eax,[40501Ch]
00401656: push eax
00401657: push 406418h
0040165C: call 004010E0h
00401661: add esp,8
loc_00401664:
00401664: ret
00401665: lea esp,[esp]
00401669: lea esp,[esp]
00401670: push 13h
00401672: call dword ptr [409264h]
00401678: test eax,eax
0040167A: jne short 0040168Fh
0040167C: mov eax,[40501Ch]
00401681: push eax
00401682: push 406438h
00401687: call 004010E0h
0040168C: add esp,8
loc_0040168F:
0040168F: ret
00401690: sub esp,20h
00401693: mov dword ptr [esp],20h
0040169B: lea eax,[esp]
0040169F: push eax
004016A0: call dword ptr [4091E0h]
004016A6: cmp dword ptr [esp+8],1700000h
004016AE: jae short 004016CFh
004016B0: mov eax,[esp+8]
004016B4: mov ecx,[40501Ch]
004016BA: shr eax,14h
004016BD: push eax
004016BE: push 17h
004016C0: push ecx
004016C1: push 406458h
004016C6: call 004010E0h
004016CB: add esp,30h
004016CE: ret
loc_004016CF:
004016CF: cmp dword ptr [esp+8],1F00000h
004016D7: jae short 004016F6h
004016D9: mov eax,[esp+8]
004016DD: mov ecx,[40501Ch]
004016E3: shr eax,14h
004016E6: push eax
004016E7: push ecx
004016E8: push 40648Ch
004016ED: call 00401020h
004016F2: add esp,2Ch
004016F5: ret
loc_004016F6:
004016F6: cmp dword ptr [esp+14h],2700000h
004016FE: jae short 0040171Eh
00401700: mov eax,[esp+14h]
00401704: mov ecx,[40501Ch]
0040170A: shr eax,14h
0040170D: push eax
0040170E: push 27h
00401710: push ecx
00401711: push 4064C4h
00401716: call 004010E0h
0040171B: add esp,10h
loc_0040171E:
0040171E: add esp,20h
00401721: ret
00401722: lea esp,[esp]
00401729: lea esp,[esp]
00401730: sub esp,14h
00401733: cmp byte ptr [406038h],0
0040173A: je short 0040179Eh
0040173C: lea eax,[esp]
00401740: push 3
00401742: push 406038h
00401747: push eax
00401748: call 00402410h
0040174D: lea eax,[esp+1Ch]
00401751: lea ecx,[esp+10h]
00401755: mov byte ptr [esp+0Fh],0
0040175A: lea edx,[esp+14h]
0040175E: add esp,0Ch
00401761: push eax
00401762: push ecx
00401763: lea eax,[esp+14h]
00401767: push edx
00401768: lea ecx,[esp+0Ch]
0040176C: push eax
0040176D: push ecx
0040176E: call dword ptr [4091DCh]
00401774: test eax,eax
00401776: je short 0040179Eh
00401778: mov eax,[esp+4]
0040177C: imul eax,[esp+8]
00401781: imul eax,[esp+0Ch]
00401786: cmp eax,1400000h
0040178B: jae short 0040179Eh
0040178D: shr eax,14h
00401790: push eax
00401791: push 406538h
00401796: call 00401020h
0040179B: add esp,8
loc_0040179E:
0040179E: add esp,14h
004017A1: ret
004017A2: lea esp,[esp]
004017A9: lea esp,[esp]
004017B0: ret
004017B1: lea esp,[esp]
004017B5: lea esp,[esp]
004017B9: lea esp,[esp]
004017C0: sub esp,24h
004017C3: push ebx
004017C4: push esi
004017C5: push edi
004017C6: xor esi,esi
004017C8: mov edi,[4091C8h]
004017CE: mov ebx,[405018h]
loc_004017D4:
004017D4: mov eax,[406568h]
004017D9: lea ecx,[esi+41h]
004017DC: mov [esp+0Ch],eax
004017E0: lea eax,[esp+0Ch]
004017E4: mov [esp+0Ch],cl
004017E8: push eax
004017E9: call edi
004017EB: cmp eax,5
004017EE: jne short 00401819h
004017F0: lea eax,[esp+0Ch]
004017F4: push ebx
004017F5: lea ecx,[esp+14h]
004017F9: push eax
004017FA: push 40656Ch
004017FF: push ecx
00401800: call 00402210h
00401805: lea ecx,[esp+20h]
00401809: add esp,10h
0040180C: push ecx
0040180D: call 00401840h
00401812: add esp,4
00401815: test al,al
00401817: jne short 00401832h
loc_00401819:
00401819: inc esi
0040181A: cmp esi,1Ah
0040181D: jl short 004017D4h
0040181F: mov eax,[40501Ch]
00401824: push eax
00401825: push 406578h
0040182A: call 004010E0h
0040182F: add esp,8
loc_00401832:
00401832: pop edi
00401833: pop esi
00401834: pop ebx
00401835: add esp,24h
00401838: ret
00401839: lea esp,[esp]
loc_00401840:
00401840: mov eax,[esp+4]
00401844: push eax
00401845: call dword ptr [4091CCh]
0040184B: lea ecx,[eax+1]
0040184E: cmp ecx,1
00401851: sbb cl,cl
00401853: inc cl
00401855: mov al,cl
00401857: ret
00401858: lea esp,[esp]
0040185C: lea esp,[esp]
loc_00401860:
00401860: ret
00401861: lea esp,[esp]
00401865: lea esp,[esp]
00401869: lea esp,[esp]
loc_00401870:
00401870: ret
00401871: lea esp,[esp]
00401875: lea esp,[esp]
00401879: lea esp,[esp]
loc_00401880:
00401880: sub esp,110h
00401886: mov ecx,[405000h]
0040188C: lea eax,[esp]
00401890: push edi
00401891: push eax
00401892: push 2001Fh
00401897: push 0
00401899: push ecx
0040189A: push 80000002h
0040189F: call dword ptr [4091A4h]
004018A5: test eax,eax
004018A7: jne 00401939h
004018AD: lea eax,[esp+8]
004018B1: mov edi,[esp+118h]
004018B8: lea ecx,[esp+0Ch]
004018BC: push eax
004018BD: push edi
004018BE: mov eax,[405004h]
004018C3: mov edx,[esp+0Ch]
004018C7: push ecx
004018C8: mov dword ptr [esp+14h],104h
004018D0: push 0
004018D2: push eax
004018D3: push edx
004018D4: call dword ptr [4091A0h]
004018DA: test eax,eax
004018DC: jne short 0040192Ch
004018DE: cmp byte ptr [edi],0
004018E1: je short 0040192Ch
004018E3: mov eax,[405018h]
004018E8: lea ecx,[esp+10h]
004018EC: push eax
004018ED: push edi
004018EE: push 4065A4h
004018F3: push ecx
004018F4: call 00402210h
004018F9: lea ecx,[esp+20h]
004018FD: add esp,10h
00401900: push ecx
00401901: call 00401840h
00401906: add esp,4
00401909: test al,al
0040190B: je short 00401922h
0040190D: mov eax,[esp+4]
00401911: push eax
00401912: call dword ptr [40919Ch]
00401918: mov al,1
0040191A: pop edi
0040191B: add esp,110h
00401921: ret
loc_00401922:
00401922: xor al,al
00401924: pop edi
00401925: add esp,110h
0040192B: ret
loc_0040192C:
0040192C: mov eax,[esp+4]
00401930: push eax
00401931: call dword ptr [40919Ch]
00401937: jmp short 00401940h
loc_00401939:
00401939: mov edi,[esp+118h]
00401940: push 104h
00401945: mov eax,[4074F8h]
0040194A: push edi
0040194B: push eax
0040194C: call dword ptr [4091D0h]
00401952: test eax,eax
00401954: je short 0040198Dh
00401956: push 5Ch
00401958: push edi
00401959: call 00402440h
0040195E: add esp,8
00401961: test eax,eax
00401963: je short 0040198Dh
00401965: mov edx,4065ACh
0040196A: mov ecx,0FFFFFFFFh
0040196F: mov byte ptr [eax],0
00401972: sub eax,eax
00401974: repne scasb [edi]
00401976: dec edi
00401977: mov eax,[edx]
00401979: mov [edi],eax
0040197B: mov al,1
0040197D: mov cx,[edx+4]
00401981: mov [edi+4],cx
00401985: pop edi
00401986: add esp,110h
0040198C: ret
loc_0040198D:
0040198D: xor al,al
0040198F: pop edi
00401990: add esp,110h
00401996: ret
00401997: mov edi,edi
00401999: lea esp,[esp]
loc_004019A0:
004019A0: mov al,[esp+4]
004019A4: mov [4065B8h],al
004019A9: ret
004019AA: lea ebx,[ebx]
004019B0: ret
004019B1: lea esp,[esp]
004019B5: lea esp,[esp]
004019B9: lea esp,[esp]
004019C0: mov eax,1210h
004019C5: call 00402470h
004019CA: push edi
004019CB: mov al,[4065BCh]
004019D0: lea edi,[esp+111h]
004019D7: mov ecx,40h
004019DC: mov [esp+110h],al
004019E3: xor eax,eax
004019E5: rep stosd [edi]
004019E7: stosw [edi]
004019E9: stosb [edi]
004019EA: mov cl,[4065C0h]
004019F0: lea edi,[esp+0Dh]
004019F4: mov [esp+0Ch],cl
004019F8: push 104h
004019FD: mov ecx,40h
00401A02: rep stosd [edi]
00401A04: stosw [edi]
00401A06: stosb [edi]
00401A07: lea ecx,[esp+10h]
00401A0B: push ecx
00401A0C: call dword ptr [4091D4h]
00401A12: lea ecx,[esp+0Ch]
00401A16: lea edx,[esp+110h]
00401A1D: push ecx
00401A1E: push 4065C4h
00401A23: push edx
00401A24: call 00402210h
00401A29: lea edi,[esp+220h]
00401A30: add esp,0Ch
00401A33: xor eax,eax
00401A35: mov ecx,400h
00401A3A: rep stosd [edi]
00401A3C: lea ecx,[esp+214h]
00401A43: lea edx,[esp+110h]
00401A4A: push ecx
00401A4B: push 1000h
00401A50: push eax
00401A51: push edx
00401A52: call 0040207Eh
00401A57: test eax,eax
00401A59: je short 00401A88h
00401A5B: lea eax,[esp+8]
00401A5F: lea ecx,[esp+4]
00401A63: lea edx,[esp+214h]
00401A6A: push eax
00401A6B: push ecx
00401A6C: push 4065D4h
00401A71: push edx
00401A72: call 00402078h
00401A77: test eax,eax
00401A79: je short 00401A88h
00401A7B: mov eax,[esp+4]
00401A7F: cmp dword ptr [eax+10h],40005h
00401A86: jae short 00401A95h
loc_00401A88:
00401A88: push 4065D8h
00401A8D: call 004010E0h
00401A92: add esp,4
loc_00401A95:
00401A95: pop edi
00401A96: add esp,1210h
00401A9C: ret
00401A9D: lea ecx,[ecx]
00401AA0: push esi
00401AA1: push 40h
00401AA3: mov esi,[esp+10h]
00401AA7: push esi
00401AA8: call 004024A0h
00401AAD: add esp,8
00401AB0: test eax,eax
00401AB2: je short 00401AD0h
00401AB4: mov ecx,[esp+8]
00401AB8: cmp eax,esi
00401ABA: jbe short 00401ACBh
loc_00401ABC:
00401ABC: mov dl,[esi]
00401ABE: inc esi
00401ABF: mov [ecx],dl
00401AC1: inc ecx
00401AC2: cmp eax,esi
00401AC4: ja short 00401ABCh
00401AC6: pop esi
00401AC7: mov byte ptr [ecx],0
00401ACA: ret
loc_00401ACB:
00401ACB: pop esi
00401ACC: mov byte ptr [ecx],0
00401ACF: ret
loc_00401AD0:
00401AD0: mov ecx,[esp+8]
00401AD4: pop esi
00401AD5: mov byte ptr [ecx],0
00401AD8: ret
00401AD9: lea esp,[esp]
00401AE0: mov eax,[esp+4]
00401AE4: push 40h
00401AE6: push eax
00401AE7: call 004024A0h
00401AEC: add esp,8
00401AEF: test eax,eax
00401AF1: je short 00401AF5h
00401AF3: inc eax
00401AF4: ret
loc_00401AF5:
00401AF5: xor eax,eax
00401AF7: ret
00401AF8: lea esp,[esp]
00401AFC: lea esp,[esp]
loc_00401B00:
00401B00: push ebx
00401B01: push esi
00401B02: xor bl,bl
00401B04: mov esi,[esp+10h]
00401B08: push esi
00401B09: call 0040203Ch
00401B0E: cmp eax,0FFFFFFFFh
00401B11: jne short 00401B2Fh
00401B13: push esi
00401B14: call 00402036h
00401B19: test eax,eax
00401B1B: je short 00401B3Ah
00401B1D: mov eax,[eax+0Ch]
00401B20: mov esi,[esp+0Ch]
00401B24: mov bl,1
00401B26: mov ecx,[eax]
00401B28: mov edx,[ecx]
00401B2A: mov [esi+4],edx
00401B2D: jmp short 00401B3Eh
loc_00401B2F:
00401B2F: mov esi,[esp+0Ch]
00401B33: mov bl,1
00401B35: mov [esi+4],eax
00401B38: jmp short 00401B3Eh
loc_00401B3A:
00401B3A: mov esi,[esp+0Ch]
00401B3E: push 19h
00401B40: call 00402030h
00401B45: mov [esi+2],ax
00401B49: mov al,bl
00401B4B: mov word ptr [esi],2
00401B50: pop esi
00401B51: pop ebx
00401B52: ret
00401B53: lea ebx,[ebx]
00401B59: lea esp,[esp]
00401B60: sub esp,1F0h
00401B66: lea eax,[esp+60h]
00401B6A: push ebx
00401B6B: push esi
00401B6C: xor ebx,ebx
00401B6E: push edi
00401B6F: push ebp
00401B70: push eax
00401B71: push 101h
00401B76: call 00402066h
00401B7B: test eax,eax
00401B7D: jne 00401D4Dh
00401B83: push ebx
00401B84: push 1
00401B86: push 2
00401B88: call 00402060h
00401B8D: cmp eax,0FFFFFFFFh
00401B90: mov ebp,eax
00401B92: je 00401D39h
00401B98: mov [esp+14h],ebx
00401B9C: push ebx
00401B9D: mov word ptr [esp+14h],2
00401BA4: call 00402030h
00401BA9: mov [esp+12h],ax
00401BAE: push 10h
00401BB0: lea eax,[esp+14h]
00401BB4: push eax
00401BB5: push ebp
00401BB6: call 0040205Ah
00401BBB: test eax,eax
00401BBD: jne 00401D19h
00401BC3: lea eax,[esp+20h]
00401BC7: mov esi,[esp+20Ch]
00401BCE: cmp esi,ebx
00401BD0: mov [eax],ebx
00401BD2: mov [eax+4],ebx
00401BD5: mov [eax+8],ebx
00401BD8: mov [eax+0Ch],ebx
00401BDB: je 00401CF7h
00401BE1: lea eax,[esp+20h]
00401BE5: push esi
00401BE6: push eax
00401BE7: call 00401B00h
00401BEC: add esp,8
00401BEF: test al,al
00401BF1: je 00401CDCh
00401BF7: lea eax,[esp+20h]
00401BFB: push 10h
00401BFD: push eax
00401BFE: push ebp
00401BFF: call 00402054h
00401C04: test eax,eax
00401C06: jne 00401CBBh
00401C0C: mov edx,[esp+208h]
00401C13: mov ecx,0FFFFFFFFh
00401C18: mov edi,edx
00401C1A: sub eax,eax
00401C1C: repne scasb [edi]
00401C1E: not ecx
00401C20: sub edi,ecx
00401C22: mov eax,ecx
00401C24: shr ecx,2
00401C27: mov esi,edi
00401C29: lea edi,[esp+30h]
00401C2D: rep movsd [edi],[esi]
00401C2F: mov ecx,eax
00401C31: and ecx,3
00401C34: rep movsb [edi],[esi]
00401C36: cmp [esp+30h],bl
00401C3A: je short 00401C9Dh
00401C3C: mov eax,[esp+218h]
00401C43: mov ecx,[esp+214h]
00401C4A: mov edx,[esp+210h]
00401C51: push eax
00401C52: mov eax,[esp+208h]
00401C59: push ecx
00401C5A: lea ecx,[esp+38h]
00401C5E: push edx
00401C5F: push eax
00401C60: push ecx
00401C61: push ebp
00401C62: call 00401D70h
00401C67: add esp,18h
00401C6A: test al,al
00401C6C: je short 00401C80h
00401C6E: mov bl,1
00401C70: push ebp
00401C71: call 00402048h
00401C76: call 00402042h
00401C7B: jmp 00401D5Ah
loc_00401C80:
00401C80: push 406604h
00401C85: call 004010E0h
00401C8A: add esp,4
00401C8D: push ebp
00401C8E: call 00402048h
00401C93: call 00402042h
00401C98: jmp 00401D5Ah
loc_00401C9D:
00401C9D: push edx
00401C9E: push 40661Ch
00401CA3: call 004010E0h
00401CA8: add esp,8
00401CAB: push ebp
00401CAC: call 00402048h
00401CB1: call 00402042h
00401CB6: jmp 00401D5Ah
loc_00401CBB:
00401CBB: call 0040204Eh
00401CC0: push eax
00401CC1: push esi
00401CC2: push 406638h
00401CC7: call 004010E0h
00401CCC: add esp,0Ch
00401CCF: push ebp
00401CD0: call 00402048h
00401CD5: call 00402042h
00401CDA: jmp short 00401D5Ah
loc_00401CDC:
00401CDC: push esi
00401CDD: push 406654h
00401CE2: call 004010E0h
00401CE7: add esp,8
00401CEA: push ebp
00401CEB: call 00402048h
00401CF0: call 00402042h
00401CF5: jmp short 00401D5Ah
loc_00401CF7:
00401CF7: mov edx,[esp+208h]
00401CFE: push edx
00401CFF: push 406670h
00401D04: call 004010E0h
00401D09: add esp,8
00401D0C: push ebp
00401D0D: call 00402048h
00401D12: call 00402042h
00401D17: jmp short 00401D5Ah
loc_00401D19:
00401D19: call 0040204Eh
00401D1E: push eax
00401D1F: push 406694h
00401D24: call 004010E0h
00401D29: add esp,8
00401D2C: push ebp
00401D2D: call 00402048h
00401D32: call 00402042h
00401D37: jmp short 00401D5Ah
loc_00401D39:
00401D39: push 4066ACh
00401D3E: call 004010E0h
00401D43: add esp,4
00401D46: call 00402042h
00401D4B: jmp short 00401D5Ah
loc_00401D4D:
00401D4D: push 4066C4h
00401D52: call 004010E0h
00401D57: add esp,4
00401D5A: mov al,bl
00401D5C: pop ebp
00401D5D: pop edi
00401D5E: pop esi
00401D5F: pop ebx
00401D60: add esp,1F0h
00401D66: ret
00401D67: mov edi,edi
00401D69: lea esp,[esp]
loc_00401D70:
00401D70: sub esp,104h
00401D76: lea eax,[esp+4]
00401D7A: push ebx
00401D7B: push esi
00401D7C: xor ebx,ebx
00401D7E: mov esi,[esp+110h]
00401D85: push edi
00401D86: mov [esp+0Eh],bl
00401D8A: push 100h
00401D8F: push eax
00401D90: push esi
00401D91: call 00401EF0h
00401D96: add esp,0Ch
00401D99: test al,al
00401D9B: je 00401EC2h
00401DA1: mov edi,[esp+118h]
loc_00401DA8:
00401DA8: lea eax,[esp+10h]
00401DAC: push eax
00401DAD: call 00401F20h
00401DB2: add esp,4
00401DB5: test al,al
00401DB7: jne 00401EC2h
00401DBD: mov byte ptr [esp+0Fh],0
00401DC2: cmp ebx,5
00401DC5: ja short 00401DCEh
00401DC7: jmp dword ptr [ebx*4+401ED0h]
loc_00401DCE:
00401DCE: push 406730h
00401DD3: call 004010E0h
00401DD8: add esp,4
00401DDB: jmp 00401E9Fh
00401DE0: push 4066D8h
00401DE5: push esi
00401DE6: call 00401FA0h
00401DEB: mov [esp+17h],al
00401DEF: add esp,8
00401DF2: jmp 00401E9Fh
00401DF7: mov eax,[esp+11Ch]
00401DFE: push eax
00401DFF: push 4066F4h
00401E04: push esi
00401E05: call 00401FD0h
00401E0A: mov [esp+1Bh],al
00401E0E: add esp,0Ch
00401E11: jmp 00401E9Fh
00401E16: push edi
00401E17: push 406704h
00401E1C: push esi
00401E1D: call 00401FD0h
00401E22: mov [esp+1Bh],al
00401E26: add esp,0Ch
00401E29: jmp short 00401E9Fh
00401E2B: push 406714h
00401E30: push esi
00401E31: call 00401FA0h
00401E36: mov [esp+17h],al
00401E3A: add esp,8
00401E3D: jmp short 00401E9Fh
00401E3F: mov eax,[esp+120h]
00401E46: push eax
00401E47: push 40671Ch
00401E4C: push esi
00401E4D: call 00401FD0h
00401E52: mov [esp+1Bh],al
00401E56: add esp,0Ch
00401E59: test al,al
00401E5B: je short 00401E7Ah
00401E5D: mov eax,[esp+128h]
00401E64: mov ecx,[esp+124h]
00401E6B: push eax
00401E6C: push ecx
00401E6D: push esi
00401E6E: call 00401F50h
00401E73: mov [esp+1Bh],al
00401E77: add esp,0Ch
loc_00401E7A:
00401E7A: cmp byte ptr [esp+0Fh],0
00401E7F: je short 00401EC2h
00401E81: push 40672Ch
00401E86: push esi
00401E87: call 00401FA0h
00401E8C: mov [esp+17h],al
00401E90: add esp,8
00401E93: jmp short 00401E9Fh
00401E95: mov byte ptr [esp+0Eh],1
00401E9A: mov byte ptr [esp+0Fh],0
00401E9F: cmp byte ptr [esp+0Fh],0
00401EA4: je short 00401EC2h
00401EA6: inc ebx
00401EA7: push 100h
00401EAC: lea eax,[esp+14h]
00401EB0: push eax
00401EB1: push esi
00401EB2: call 00401EF0h
00401EB7: add esp,0Ch
00401EBA: test al,al
00401EBC: jne 00401DA8h
loc_00401EC2:
00401EC2: mov al,[esp+0Eh]
00401EC6: pop edi
00401EC7: pop esi
00401EC8: pop ebx
00401EC9: add esp,104h
00401ECF: ret
00401ED0: loopne 00401EEFh
00401ED2: inc eax
00401ED3: add bh,dh
00401ED5: sbb eax,1E160040h
00401EDA: inc eax
00401EDB: add [ebx],ch
00401EDD: push ds
00401EDE: inc eax
00401EDF: add [edi],bh
00401EE1: push ds
00401EE2: inc eax
00401EE3: add [ebp-72FFBFE2h],dl
00401EE9: and al,0
00401EEC: lea esp,[esp]
loc_00401EF0:
00401EF0: mov eax,[esp+0Ch]
00401EF4: push esi
00401EF5: mov esi,[esp+0Ch]
00401EF9: push 0
00401EFB: mov ecx,[esp+0Ch]
00401EFF: push eax
00401F00: push esi
00401F01: push ecx
00401F02: call 0040206Ch
00401F07: test eax,eax
00401F09: jle short 00401F13h
00401F0B: mov byte ptr [eax+esi],0
00401F0F: mov al,1
00401F11: pop esi
00401F12: ret
loc_00401F13:
00401F13: xor al,al
00401F15: pop esi
00401F16: ret
00401F17: mov edi,edi
00401F19: lea esp,[esp]
loc_00401F20:
00401F20: push esi
00401F21: mov esi,[esp+8]
00401F25: cmp byte ptr [esi],35h
00401F28: jne short 00401F38h
00401F2A: push esi
00401F2B: push 406744h
00401F30: call 004010E0h
00401F35: add esp,8
loc_00401F38:
00401F38: mov al,[esi]
00401F3A: pop esi
00401F3B: sub al,35h
00401F3D: cmp al,1
00401F3F: sbb al,al
00401F41: neg al
00401F43: ret
00401F44: add eax,0
00401F49: lea esp,[esp]
loc_00401F50:
00401F50: push ebx
00401F51: push esi
00401F52: push edi
00401F53: xor ebx,ebx
00401F55: mov edi,[esp+18h]
00401F59: push ebp
00401F5A: cmp edi,ebx
00401F5C: jle short 00401F8Bh
00401F5E: mov esi,[esp+18h]
00401F62: mov ebp,[4091C4h]
loc_00401F68:
00401F68: push 0
00401F6A: mov eax,edi
00401F6C: sub eax,ebx
00401F6E: lea ecx,[esi+ebx]
00401F71: mov edx,[esp+18h]
00401F75: push eax
00401F76: push ecx
00401F77: push edx
00401F78: call 00402072h
00401F7D: test eax,eax
00401F7F: jle short 00401F8Bh
00401F81: add ebx,eax
00401F83: push 64h
00401F85: call ebp
00401F87: cmp ebx,edi
00401F89: jl short 00401F68h
loc_00401F8B:
00401F8B: sub ebx,edi
00401F8D: pop ebp
00401F8E: pop edi
00401F8F: cmp ebx,1
00401F92: sbb al,al
00401F94: pop esi
00401F95: neg al
00401F97: pop ebx
00401F98: ret
00401F99: lea esp,[esp]
loc_00401FA0:
00401FA0: mov edx,[esp+8]
00401FA4: push edi
00401FA5: mov edi,edx
00401FA7: mov ecx,0FFFFFFFFh
00401FAC: sub eax,eax
00401FAE: repne scasb [edi]
00401FB0: not ecx
00401FB2: dec ecx
00401FB3: mov eax,[esp+8]
00401FB7: push ecx
00401FB8: push edx
00401FB9: push eax
00401FBA: call 00401F50h
00401FBF: add esp,0Ch
00401FC2: pop edi
00401FC3: ret
00401FC4: add eax,0
00401FC9: lea esp,[esp]
loc_00401FD0:
00401FD0: lea eax,[esp+0Ch]
00401FD4: mov ecx,[esp+8]
00401FD8: sub esp,400h
00401FDE: lea edx,[esp]
00401FE2: push eax
00401FE3: mov byte ptr [esp+403h],0
00401FEB: push ecx
00401FEC: push edx
00401FED: call 00402090h
00401FF2: add esp,0Ch
00401FF5: cmp byte ptr [esp+3FFh],0
00401FFD: jne short 00402018h
00401FFF: lea eax,[esp]
00402003: mov ecx,[esp+404h]
0040200A: push eax
0040200B: push ecx
0040200C: call 00401FA0h
00402011: add esp,408h
00402017: ret
loc_00402018:
00402018: push 406754h
0040201D: call 004010E0h
00402022: add esp,4
00402025: xor al,al
00402027: add esp,400h
0040202D: ret
0040202E: mov edi,edi
loc_00402030:
00402030: jmp dword ptr [4092B0h]
loc_00402036:
00402036: jmp dword ptr [4092ACh]
loc_0040203C:
0040203C: jmp dword ptr [4092A8h]
loc_00402042:
00402042: jmp dword ptr [4092A4h]
loc_00402048:
00402048: jmp dword ptr [4092A0h]
loc_0040204E:
0040204E: jmp dword ptr [40929Ch]
loc_00402054:
00402054: jmp dword ptr [4092B4h]
loc_0040205A:
0040205A: jmp dword ptr [4092B8h]
loc_00402060:
00402060: jmp dword ptr [409298h]
loc_00402066:
00402066: jmp dword ptr [409294h]
loc_0040206C:
0040206C: jmp dword ptr [409290h]
loc_00402072:
00402072: jmp dword ptr [40928Ch]
loc_00402078:
00402078: jmp dword ptr [409284h]
loc_0040207E:
0040207E: jmp dword ptr [409280h]
00402084: int3
00402085: int3
00402086: int3
00402087: int3
00402088: int3
00402089: int3
0040208A: int3
0040208B: int3
0040208C: int3
0040208D: int3
0040208E: int3
0040208F: int3
loc_00402090:
00402090: mov eax,[esp+4]
00402094: sub esp,20h
00402097: mov ecx,[esp+2Ch]
0040209B: push esi
0040209C: mov edx,[esp+2Ch]
004020A0: push ecx
004020A1: mov dword ptr [esp+14h],42h
004020A9: mov [esp+10h],eax
004020AD: push edx
004020AE: mov [esp+0Ch],eax
004020B2: lea eax,[esp+0Ch]
004020B6: mov dword ptr [esp+10h],7FFFFFFFh
004020BE: push eax
004020BF: call 004027D0h
004020C4: add esp,0Ch
004020C7: mov esi,eax
004020C9: dec dword ptr [esp+8]
004020CD: js short 004020E1h
004020CF: mov eax,[esp+4]
004020D3: mov byte ptr [eax],0
004020D6: mov eax,esi
004020D8: inc dword ptr [esp+4]
004020DC: pop esi
004020DD: add esp,20h
004020E0: ret
loc_004020E1:
004020E1: lea eax,[esp+4]
004020E5: push eax
004020E6: push 0
004020E8: call 00402680h
004020ED: add esp,8
004020F0: mov eax,esi
004020F2: pop esi
004020F3: add esp,20h
004020F6: ret
004020F7: int3
004020F8: int3
004020F9: int3
004020FA: int3
004020FB: int3
004020FC: int3
004020FD: int3
004020FE: int3
004020FF: int3
loc_00402100:
00402100: mov eax,[408728h]
00402105: test eax,eax
00402107: je short 0040210Bh
00402109: call eax
loc_0040210B:
0040210B: push 406010h
00402110: push 406008h
00402115: call 004021F0h
0040211A: add esp,8
0040211D: push 406004h
00402122: push 406000h
00402127: call 004021F0h
0040212C: add esp,8
0040212F: ret
loc_00402130:
00402130: mov eax,[esp+4]
00402134: push 0
00402136: push 0
00402138: push eax
00402139: call 00402170h
0040213E: add esp,0Ch
00402141: ret
00402142: int3
00402143: int3
00402144: int3
00402145: int3
00402146: int3
00402147: int3
00402148: int3
00402149: int3
0040214A: int3
0040214B: int3
0040214C: int3
0040214D: int3
0040214E: int3
0040214F: int3
loc_00402150:
00402150: mov eax,[esp+4]
00402154: push 0
00402156: push 1
00402158: push eax
00402159: call 00402170h
0040215E: add esp,0Ch
00402161: ret
00402162: int3
00402163: int3
00402164: int3
00402165: int3
00402166: int3
00402167: int3
00402168: int3
00402169: int3
0040216A: int3
0040216B: int3
0040216C: int3
0040216D: int3
0040216E: int3
0040216F: int3
loc_00402170:
00402170: mov dword ptr [4067B4h],1
0040217A: cmp dword ptr [esp+8],0
0040217F: push ebx
00402180: push esi
00402181: mov ebx,[esp+14h]
00402185: mov [4067B0h],bl
0040218B: jne short 004021CCh
0040218D: cmp dword ptr [40872Ch],0
00402194: je short 004021BAh
00402196: mov esi,[408724h]
0040219C: sub esi,4
0040219F: cmp esi,[40872Ch]
004021A5: jb short 004021BAh
loc_004021A7:
004021A7: mov eax,[esi]
004021A9: test eax,eax
004021AB: je short 004021AFh
004021AD: call eax
loc_004021AF:
004021AF: sub esi,4
004021B2: cmp esi,[40872Ch]
004021B8: jae short 004021A7h
loc_004021BA:
004021BA: push 40601Ch
004021BF: push 406014h
004021C4: call 004021F0h
004021C9: add esp,8
loc_004021CC:
004021CC: push 406024h
004021D1: push 406020h
004021D6: call 004021F0h
004021DB: add esp,8
004021DE: test ebx,ebx
004021E0: jne short 004021EDh
004021E2: mov eax,[esp+0Ch]
004021E6: push eax
004021E7: call dword ptr [4091E4h]
loc_004021ED:
004021ED: pop esi
004021EE: pop ebx
004021EF: ret
loc_004021F0:
004021F0: push esi
004021F1: push edi
004021F2: mov esi,[esp+10h]
004021F6: mov edi,[esp+0Ch]
004021FA: cmp esi,edi
004021FC: jbe short 0040220Dh
loc_004021FE:
004021FE: mov eax,[edi]
00402200: test eax,eax
00402202: je short 00402206h
00402204: call eax
loc_00402206:
00402206: add edi,4
00402209: cmp esi,edi
0040220B: ja short 004021FEh
loc_0040220D:
0040220D: pop edi
0040220E: pop esi
0040220F: ret
loc_00402210:
00402210: mov eax,[esp+4]
00402214: sub esp,20h
00402217: lea ecx,[esp+2Ch]
0040221B: push esi
0040221C: mov dword ptr [esp+10h],42h
00402224: mov [esp+0Ch],eax
00402228: push ecx
00402229: mov [esp+8],eax
0040222D: lea eax,[esp+8]
00402231: mov dword ptr [esp+0Ch],7FFFFFFFh
00402239: mov edx,[esp+30h]
0040223D: push edx
0040223E: push eax
0040223F: call 004027D0h
00402244: add esp,0Ch
00402247: mov esi,eax
00402249: dec dword ptr [esp+8]
0040224D: js short 00402261h
0040224F: mov eax,[esp+4]
00402253: mov byte ptr [eax],0
00402256: mov eax,esi
00402258: inc dword ptr [esp+4]
0040225C: pop esi
0040225D: add esp,20h
00402260: ret
loc_00402261:
00402261: lea eax,[esp+4]
00402265: push eax
00402266: push 0
00402268: call 00402680h
0040226D: add esp,8
00402270: mov eax,esi
00402272: pop esi
00402273: add esp,20h
00402276: ret
00402277: int3
00402278: int3
00402279: int3
0040227A: int3
0040227B: int3
0040227C: int3
0040227D: int3
0040227E: int3
0040227F: int3
loc_00402280:
00402280: push ebp
00402281: mov ebp,esp
00402283: push edi
00402284: push esi
00402285: mov esi,[ebp+0Ch]
00402288: mov edi,[ebp+8]
0040228B: mov ecx,[ebp+10h]
0040228E: cmp edi,esi
00402290: jbe short 0040229Eh
00402292: mov eax,esi
00402294: add eax,ecx
00402296: cmp edi,eax
00402298: jb 0040232Ch
loc_0040229E:
0040229E: test edi,3
004022A4: jne short 004022F8h
004022A6: mov edx,ecx
004022A8: and edx,3
004022AB: shr ecx,2
004022AE: rep movsd [edi],[esi]
004022B0: jmp dword ptr [edx*4+4022B8h]
004022B7: nop
004022B8: (bad)
004022BC: in al,dx
004022BD: and al,[eax]
004022C0: fsub st,qword ptr [edx]
004022C2: inc eax
004022C3: add al,cl
004022C5: and al,[eax]
004022C8: mov ax,[esi]
004022CB: mov [edi],ax
004022CE: mov al,[esi+2]
004022D1: mov [edi+2],al
004022D4: mov eax,[ebp+8]
004022D7: pop esi
004022D8: pop edi
004022D9: leave
004022DA: ret
004022DB: nop
004022DC: mov ax,[esi]
004022DF: mov [edi],ax
004022E2: mov eax,[ebp+8]
004022E5: pop esi
004022E6: pop edi
004022E7: leave
004022E8: ret
004022E9: mov eax,eax
004022EC: mov al,[esi]
004022EE: mov [edi],al
004022F0: mov eax,[ebp+8]
004022F3: pop esi
004022F4: pop edi
004022F5: leave
004022F6: ret
004022F7: nop
loc_004022F8:
004022F8: cmp ecx,0Ch
004022FB: jbe short 00402320h
004022FD: mov edx,edi
004022FF: neg edx
00402301: and edx,3
00402304: sub ecx,edx
00402306: mov eax,ecx
00402308: mov ecx,edx
0040230A: rep movsb [edi],[esi]
0040230C: mov ecx,eax
0040230E: and eax,3
00402311: shr ecx,2
00402314: rep movsd [edi],[esi]
00402316: jmp dword ptr [eax*4+4022B8h]
0040231D: mov eax,eax
loc_00402320:
00402320: rep movsb [edi],[esi]
00402322: mov eax,[ebp+8]
00402325: pop esi
00402326: pop edi
00402327: leave
00402328: ret
00402329: mov eax,eax
loc_0040232C:
0040232C: std
0040232D: add esi,ecx
0040232F: add edi,ecx
00402331: test edi,3
00402337: jne short 00402398h
00402339: mov edx,ecx
0040233B: and edx,3
0040233E: sub esi,4
00402341: sub edi,4
00402344: shr ecx,2
00402347: rep movsd [edi],[esi]
00402349: jmp dword ptr [edx*4+402350h]
00402350: mov fs,[ebx]
00402352: inc eax
00402353: add [eax+78004023h],cl
00402359: and eax,[eax]
0040235C: pushad
0040235D: and eax,[eax]
00402360: mov ax,[esi+2]
00402364: mov [edi+2],ax
00402368: mov al,[esi+1]
0040236B: mov [edi+1],al
0040236E: cld
0040236F: mov eax,[ebp+8]
00402372: pop esi
00402373: pop edi
00402374: leave
00402375: ret
00402376: mov eax,eax
00402378: mov ax,[esi+2]
0040237C: mov [edi+2],ax
00402380: cld
00402381: mov eax,[ebp+8]
00402384: pop esi
00402385: pop edi
00402386: leave
00402387: ret
00402388: mov al,[esi+3]
0040238B: mov [edi+3],al
0040238E: cld
0040238F: mov eax,[ebp+8]
00402392: pop esi
00402393: pop edi
00402394: leave
00402395: ret
00402396: mov eax,eax
loc_00402398:
00402398: dec esi
00402399: dec edi
0040239A: cmp ecx,0Ch
0040239D: jbe short 004023C4h
0040239F: neg edx
004023A1: and edx,3
004023A4: sub ecx,edx
004023A6: mov eax,ecx
004023A8: mov ecx,edx
004023AA: rep movsb [edi],[esi]
004023AC: mov ecx,eax
004023AE: and eax,3
004023B1: sub esi,3
004023B4: sub edi,3
004023B7: shr ecx,2
004023BA: rep movsd [edi],[esi]
004023BC: jmp dword ptr [eax*4+402350h]
004023C3: nop
loc_004023C4:
004023C4: rep movsb [edi],[esi]
004023C6: cld
004023C7: mov eax,[ebp+8]
004023CA: pop esi
004023CB: pop edi
004023CC: leave
004023CD: ret
004023CE: int3
004023CF: int3
loc_004023D0:
004023D0: push ebp
004023D1: mov ebp,esp
004023D3: push esi
004023D4: push edi
004023D5: push ebx
004023D6: mov esi,[ebp+8]
004023D9: mov ecx,esi
004023DB: mov edi,[ebp+0Ch]
004023DE: mov edx,edi
004023E0: mov al,[edi]
004023E2: or al,al
004023E4: je short 004023FBh
004023E6: mov ah,[esi]
004023E8: or ah,ah
004023EA: je short 004023FFh
004023EC: cmp al,ah
004023EE: je short 004023F7h
004023F0: mov edi,edx
004023F2: inc ecx
004023F3: mov esi,ecx
004023F5: jmp short 004023E0h
loc_004023F7:
004023F7: inc edi
004023F8: inc esi
004023F9: jmp short 004023E0h
loc_004023FB:
004023FB: mov eax,ecx
004023FD: jmp short 00402401h
loc_004023FF:
004023FF: xor eax,eax
00402401: pop ebx
00402402: pop edi
00402403: pop esi
00402404: leave
00402405: ret
00402406: int3
00402407: int3
00402408: int3
00402409: int3
0040240A: int3
0040240B: int3
0040240C: int3
0040240D: int3
0040240E: int3
0040240F: int3
loc_00402410:
00402410: push ebp
00402411: mov ebp,esp
00402413: push edi
00402414: push esi
00402415: mov edi,[ebp+8]
00402418: mov esi,[ebp+0Ch]
0040241B: mov edx,edi
0040241D: mov ecx,[ebp+10h]
00402420: jecxz 0040242Eh
loc_00402422:
00402422: lodsb [esi]
00402423: or al,al
00402425: je short 0040242Ah
00402427: stosb [edi]
00402428: loop 00402422h
loc_0040242A:
0040242A: xor al,al
0040242C: rep stosb [edi]
loc_0040242E:
0040242E: mov eax,edx
00402430: pop esi
00402431: pop edi
00402432: leave
00402433: ret
00402434: int3
00402435: int3
00402436: int3
00402437: int3
00402438: int3
00402439: int3
0040243A: int3
0040243B: int3
0040243C: int3
0040243D: int3
0040243E: int3
0040243F: int3
loc_00402440:
00402440: push ebp
00402441: mov ebp,esp
00402443: push edi
00402444: mov edi,[ebp+8]
00402447: xor eax,eax
00402449: or ecx,0FFFFFFFFh
0040244C: repne scasb [edi]
0040244E: inc ecx
0040244F: neg ecx
00402451: dec edi
00402452: mov al,[ebp+0Ch]
00402455: std
00402456: repne scasb [edi]
00402458: inc edi
00402459: cmp [edi],al
0040245B: je short 00402461h
0040245D: xor eax,eax
0040245F: jmp short 00402463h
loc_00402461:
00402461: mov eax,edi
00402463: cld
00402464: pop edi
00402465: leave
00402466: ret
00402467: int3
00402468: int3
00402469: int3
0040246A: int3
0040246B: int3
0040246C: int3
0040246D: int3
0040246E: int3
0040246F: int3
loc_00402470:
00402470: push ecx
00402471: cmp eax,1000h
00402476: lea ecx,[esp+8]
0040247A: jb short 00402490h
loc_0040247C:
0040247C: sub ecx,1000h
00402482: sub eax,1000h
00402487: test [ecx],eax
00402489: cmp eax,1000h
0040248E: jae short 0040247Ch
loc_00402490:
00402490: sub ecx,eax
00402492: mov eax,esp
00402494: test [ecx],eax
00402496: mov esp,ecx
00402498: mov ecx,[eax]
0040249A: mov eax,[eax+4]
0040249D: push eax
0040249E: ret
0040249F: int3
loc_004024A0:
004024A0: mov eax,[esp+4]
004024A4: mov cl,[esp+8]
004024A8: mov dl,[eax]
004024AA: cmp cl,dl
004024AC: je short 004024B7h
004024AE: or dl,dl
004024B0: je short 004024B5h
004024B2: inc eax
004024B3: jmp short 004024A8h
loc_004024B5:
004024B5: xor eax,eax
loc_004024B7:
004024B7: ret
004024B8: int3
004024B9: int3
004024BA: int3
004024BB: int3
004024BC: int3
004024BD: int3
004024BE: int3
004024BF: int3
004024C0: mov eax,fs:[0]
004024C6: push ebp
004024C7: mov ebp,esp
004024C9: push 0FFFFFFFFh
004024CB: push 405020h
004024D0: push 403F70h
004024D5: push eax
004024D6: mov fs:[0],esp
004024DD: sub esp,60h
004024E0: push ebx
004024E1: push esi
004024E2: push edi
004024E3: mov [ebp-18h],esp
004024E6: call dword ptr [4091BCh]
004024EC: mov [40677Ch],eax
004024F1: xor eax,eax
004024F3: mov al,[40677Dh]
004024F8: mov [406788h],eax
004024FD: mov eax,[40677Ch]
00402502: shr dword ptr [40677Ch],10h
00402509: and eax,0FFh
0040250E: mov [406784h],eax
00402513: shl eax,8
00402516: add eax,[406788h]
0040251C: mov [406780h],eax
00402521: call 00403F50h
00402526: mov dword ptr [ebp-4],0
0040252D: call 00403D70h
00402532: call 00403D60h
00402537: call dword ptr [40922Ch]
0040253D: mov [408720h],eax
00402542: call 00403900h
00402547: mov [4067B8h],eax
0040254C: test eax,eax
0040254E: je short 00402559h
00402550: cmp dword ptr [408720h],0
00402557: jne short 00402563h
loc_00402559:
00402559: push 0FFFFFFFFh
0040255B: call 00402130h
00402560: add esp,4
loc_00402563:
00402563: call 00403680h
00402568: call 004035A0h
0040256D: call 00402100h
00402572: mov esi,[408720h]
00402578: mov al,[esi]
0040257A: cmp al,22h
0040257C: je short 0040258Ah
0040257E: cmp al,20h
00402580: jbe short 004025B7h
loc_00402582:
00402582: inc esi
00402583: cmp byte ptr [esi],20h
00402586: ja short 00402582h
00402588: jmp short 004025B7h
loc_0040258A:
0040258A: inc esi
0040258B: cmp byte ptr [esi],22h
0040258E: je short 004025B6h
00402590: mov bl,[ebp-28h]
loc_00402593:
00402593: mov bl,[esi]
00402595: test bl,bl
00402597: je short 004025B1h
00402599: xor eax,eax
0040259B: mov al,bl
0040259D: push eax
0040259E: call 00403540h
004025A3: add esp,4
004025A6: test eax,eax
004025A8: je short 004025ABh
004025AA: inc esi
loc_004025AB:
004025AB: inc esi
004025AC: cmp byte ptr [esi],22h
004025AF: jne short 00402593h
loc_004025B1:
004025B1: cmp byte ptr [esi],22h
004025B4: jne short 004025B7h
loc_004025B6:
004025B6: inc esi
loc_004025B7:
004025B7: cmp byte ptr [esi],0
004025BA: je short 004025C7h
loc_004025BC:
004025BC: cmp byte ptr [esi],20h
004025BF: ja short 004025C7h
004025C1: inc esi
004025C2: cmp byte ptr [esi],0
004025C5: jne short 004025BCh
loc_004025C7:
004025C7: mov dword ptr [ebp-44h],0
004025CE: lea eax,[ebp-70h]
004025D1: push eax
004025D2: call dword ptr [40924Ch]
004025D8: test byte ptr [ebp-44h],1
004025DC: mov eax,0Ah
004025E1: je short 004025EBh
004025E3: mov eax,[ebp-40h]
004025E6: and eax,0FFFFh
loc_004025EB:
004025EB: push eax
004025EC: push esi
004025ED: push 0
004025EF: push 0
004025F1: call dword ptr [409248h]
004025F7: push eax
004025F8: call 00401200h
004025FD: push eax
004025FE: call 00402130h
00402603: jmp short 0040262Ch
00402605: mov eax,[ebp-14h]
00402608: mov eax,[eax]
0040260A: mov eax,[eax]
0040260C: mov [ebp-20h],eax
0040260F: mov eax,[ebp-14h]
00402612: push eax
00402613: mov eax,[ebp-20h]
00402616: push eax
00402617: call 004033B0h
0040261C: add esp,8
0040261F: ret
00402620: mov esp,[ebp-18h]
00402623: mov eax,[ebp-20h]
00402626: push eax
00402627: call 00402150h
0040262C: add esp,4
0040262F: mov dword ptr [ebp-4],0FFFFFFFFh
00402636: mov eax,[ebp-10h]
00402639: pop edi
0040263A: mov fs:[0],eax
00402640: pop esi
00402641: pop ebx
00402642: mov esp,ebp
00402644: pop ebp
00402645: ret
00402646: int3
00402647: int3
00402648: int3
00402649: int3
0040264A: int3
0040264B: int3
0040264C: int3
0040264D: int3
0040264E: int3
0040264F: int3
loc_00402650:
00402650: cmp dword ptr [4067C4h],1
00402657: jne short 0040265Eh
00402659: call 00404050h
loc_0040265E:
0040265E: mov eax,[esp+4]
00402662: push eax
00402663: call 00404090h
00402668: add esp,4
0040266B: push 0FFh
00402670: call dword ptr [4067C0h]
00402676: add esp,4
00402679: ret
0040267A: int3
0040267B: int3
0040267C: int3
0040267D: int3
0040267E: int3
0040267F: int3
loc_00402680:
00402680: push ebx
00402681: push esi
00402682: mov esi,[esp+10h]
00402686: push edi
00402687: push ebp
00402688: mov edi,[esi+10h]
0040268B: mov eax,[esi+0Ch]
0040268E: test al,82h
00402690: je 004027B8h
00402696: test al,40h
00402698: jne 004027B8h
0040269E: test al,1
004026A0: je short 004026BDh
004026A2: mov dword ptr [esi+4],0
004026A9: mov eax,[esi+0Ch]
004026AC: test al,10h
004026AE: je 00402734h
004026B4: mov eax,[esi+8]
004026B7: mov [esi],eax
004026B9: and dword ptr [esi+0Ch],0FFFFFFFEh
loc_004026BD:
004026BD: mov eax,[esi+0Ch]
004026C0: xor ebp,ebp
004026C2: or eax,2
004026C5: mov [esi+0Ch],eax
004026C8: and eax,0FFFFFFEFh
004026CB: mov [esi+0Ch],eax
004026CE: mov dword ptr [esi+4],0
004026D5: test dword ptr [esi+0Ch],10Ch
004026DC: jne short 00402704h
004026DE: cmp esi,406E30h
004026E4: je short 004026EEh
004026E6: cmp esi,406E50h
004026EC: jne short 004026FBh
loc_004026EE:
004026EE: push edi
004026EF: call 004045C0h
004026F4: add esp,4
004026F7: test eax,eax
004026F9: jne short 00402704h
loc_004026FB:
004026FB: push esi
004026FC: call 00404570h
00402701: add esp,4
loc_00402704:
00402704: test dword ptr [esi+0Ch],108h
0040270B: je short 00402782h
0040270D: mov eax,[esi+8]
00402710: mov ebx,[esi]
00402712: sub ebx,eax
00402714: inc eax
00402715: mov [esi],eax
00402717: mov eax,[esi+18h]
0040271A: dec eax
0040271B: test ebx,ebx
0040271D: mov [esi+4],eax
00402720: jle short 00402744h
00402722: push ebx
00402723: mov eax,[esi+8]
00402726: push eax
00402727: push edi
00402728: call 00404280h
0040272D: add esp,0Ch
00402730: mov ebp,eax
00402732: jmp short 00402777h
loc_00402734:
00402734: or eax,20h
00402737: pop ebp
00402738: pop edi
00402739: mov [esi+0Ch],eax
0040273C: mov eax,0FFFFFFFFh
00402741: pop esi
00402742: pop ebx
00402743: ret
loc_00402744:
00402744: mov eax,406AB0h
00402749: cmp edi,0FFFFFFFFh
0040274C: je short 00402764h
0040274E: mov eax,edi
00402750: mov ecx,edi
00402752: and eax,0FFFFFFE7h
00402755: and ecx,1Fh
00402758: sar eax,3
0040275B: mov edx,[eax+408620h]
00402761: lea eax,[edx+ecx*8]
loc_00402764:
00402764: test byte ptr [eax+4],20h
00402768: je short 00402777h
0040276A: push 2
0040276C: push 0
0040276E: push edi
0040276F: call 004044B0h
00402774: add esp,0Ch
loc_00402777:
00402777: mov al,[esp+14h]
0040277B: mov ecx,[esi+8]
0040277E: mov [ecx],al
00402780: jmp short 00402798h
loc_00402782:
00402782: mov ebx,1
00402787: lea eax,[esp+14h]
0040278B: push ebx
0040278C: push eax
0040278D: push edi
0040278E: call 00404280h
00402793: add esp,0Ch
00402796: mov ebp,eax
00402798: cmp ebx,ebp
0040279A: je short 004027AAh
0040279C: mov eax,0FFFFFFFFh
004027A1: pop ebp
004027A2: or dword ptr [esi+0Ch],20h
004027A6: pop edi
004027A7: pop esi
004027A8: pop ebx
004027A9: ret
loc_004027AA:
004027AA: mov eax,[esp+14h]
004027AE: pop ebp
004027AF: and eax,0FFh
004027B4: pop edi
004027B5: pop esi
004027B6: pop ebx
004027B7: ret
loc_004027B8:
004027B8: or eax,20h
004027BB: pop ebp
004027BC: pop edi
004027BD: mov [esi+0Ch],eax
004027C0: mov eax,0FFFFFFFFh
004027C5: pop esi
004027C6: pop ebx
004027C7: ret
004027C8: int3
004027C9: int3
004027CA: int3
004027CB: int3
004027CC: int3
004027CD: int3
004027CE: int3
004027CF: int3
loc_004027D0:
004027D0: sub esp,248h
004027D6: mov eax,[esp+250h]
004027DD: push ebx
004027DE: inc dword ptr [esp+254h]
004027E5: push esi
004027E6: mov dword ptr [esp+20h],0
004027EE: push edi
004027EF: mov bl,[eax]
004027F1: push ebp
004027F2: test bl,bl
004027F4: mov dword ptr [esp+40h],0
004027FC: je 00402A27h
00402802: mov esi,[esp+58h]
00402806: mov edi,[esp+58h]
loc_0040280A:
0040280A: cmp dword ptr [esp+28h],0
0040280F: jl 00402A27h
00402815: cmp bl,20h
00402818: jl short 0040282Fh
0040281A: cmp bl,78h
0040281D: jg short 0040282Fh
0040281F: movsx ecx,bl
00402822: xor eax,eax
00402824: mov al,[ecx+405010h]
0040282A: and eax,0Fh
0040282D: jmp short 00402831h
loc_0040282F:
0040282F: xor eax,eax
00402831: mov ecx,[esp+40h]
00402835: mov al,[ecx+eax*8+405030h]
0040283C: sar al,4
0040283F: movsx ecx,al
00402842: cmp ecx,7
00402845: mov eax,ecx
00402847: mov [esp+40h],ecx
0040284B: ja 00402A0Fh
00402851: jmp dword ptr [eax*4+4030A0h]
00402858: mov dword ptr [esp+4Ch],0
00402860: mov dword ptr [esp+48h],0
00402868: mov dword ptr [esp+34h],0
00402870: mov dword ptr [esp+38h],0
00402878: mov dword ptr [esp+1Ch],0FFFFFFFFh
00402880: xor esi,esi
00402882: mov [esp+3Ch],esi
00402886: jmp 00402A0Fh
0040288B: movsx eax,bl
0040288E: sub eax,20h
00402891: cmp eax,10h
00402894: ja 00402A0Fh
0040289A: xor ecx,ecx
0040289C: mov cl,[eax+4030D8h]
004028A2: jmp dword ptr [ecx*4+4030C0h]
004028A9: or esi,2
004028AC: jmp 00402A0Fh
004028B1: or esi,80h
004028B7: jmp 00402A0Fh
004028BC: or esi,1
004028BF: jmp 00402A0Fh
004028C4: or esi,4
004028C7: jmp 00402A0Fh
004028CC: or esi,8
004028CF: jmp 00402A0Fh
004028D4: cmp bl,2Ah
004028D7: jne short 00402903h
004028D9: lea eax,[esp+264h]
004028E0: push eax
004028E1: call 00403280h
004028E6: mov [esp+38h],eax
004028EA: add esp,4
004028ED: test eax,eax
004028EF: jge 00402A0Fh
004028F5: neg eax
004028F7: or esi,4
004028FA: mov [esp+34h],eax
004028FE: jmp 00402A0Fh
loc_00402903:
00402903: mov eax,[esp+34h]
00402907: movsx ecx,bl
0040290A: lea edx,[eax+eax*4]
0040290D: lea ecx,[ecx+edx*2-30h]
00402911: mov [esp+34h],ecx
00402915: jmp 00402A0Fh
0040291A: mov dword ptr [esp+1Ch],0
00402922: jmp 00402A0Fh
00402927: cmp bl,2Ah
0040292A: jne short 00402955h
0040292C: lea eax,[esp+264h]
00402933: push eax
00402934: call 00403280h
00402939: mov [esp+20h],eax
0040293D: add esp,4
00402940: test eax,eax
00402942: jge 00402A0Fh
00402948: mov dword ptr [esp+1Ch],0FFFFFFFFh
00402950: jmp 00402A0Fh
loc_00402955:
00402955: mov eax,[esp+1Ch]
00402959: movsx ecx,bl
0040295C: lea edx,[eax+eax*4]
0040295F: lea ecx,[ecx+edx*2-30h]
00402963: mov [esp+1Ch],ecx
00402967: jmp 00402A0Fh
0040296C: movsx ecx,bl
0040296F: sub ecx,49h
00402972: cmp ecx,2Eh
00402975: ja 00402A0Fh
0040297B: xor eax,eax
0040297D: mov al,[ecx+403100h]
00402983: jmp dword ptr [eax*4+4030ECh]
0040298A: mov eax,[esp+260h]
00402991: cmp byte ptr [eax],36h
00402994: jne short 004029ACh
00402996: cmp byte ptr [eax+1],34h
0040299A: jne short 004029ACh
0040299C: add dword ptr [esp+260h],2
004029A4: or esi,8000h
004029AA: jmp short 00402A0Fh
loc_004029AC:
004029AC: mov dword ptr [esp+40h],0
004029B4: xor eax,eax
004029B6: mov ecx,[4070B0h]
004029BC: mov al,bl
004029BE: mov dword ptr [esp+3Ch],0
004029C6: test byte ptr [ecx+eax*2+1],80h
004029CB: je short 004029F6h
004029CD: lea eax,[esp+28h]
004029D1: mov ecx,[esp+25Ch]
004029D8: movsx edx,bl
004029DB: push eax
004029DC: push ecx
004029DD: push edx
004029DE: call 004031B0h
004029E3: mov ecx,[esp+26Ch]
004029EA: add esp,0Ch
004029ED: mov bl,[ecx]
004029EF: inc dword ptr [esp+260h]
loc_004029F6:
004029F6: lea eax,[esp+28h]
004029FA: mov ecx,[esp+25Ch]
00402A01: movsx edx,bl
00402A04: push eax
00402A05: push ecx
00402A06: push edx
00402A07: call 004031B0h
00402A0C: add esp,0Ch
loc_00402A0F:
00402A0F: mov eax,[esp+260h]
00402A16: inc dword ptr [esp+260h]
00402A1D: mov bl,[eax]
00402A1F: test bl,bl
00402A21: jne 0040280Ah
loc_00402A27:
00402A27: mov eax,[esp+28h]
00402A2B: pop ebp
00402A2C: pop edi
00402A2D: pop esi
00402A2E: pop ebx
00402A2F: add esp,248h
00402A35: ret
00402A36: or esi,20h
00402A39: jmp short 00402A0Fh
00402A3B: or esi,10h
00402A3E: jmp short 00402A0Fh
00402A40: or esi,800h
00402A46: jmp short 00402A0Fh
00402A48: movsx ecx,bl
00402A4B: sub ecx,43h
00402A4E: cmp ecx,35h
00402A51: ja 00402F44h
00402A57: xor eax,eax
00402A59: mov al,[ecx+403174h]
00402A5F: jmp dword ptr [eax*4+403130h]
00402A66: test esi,830h
00402A6C: jne short 00402A74h
00402A6E: or esi,800h
loc_00402A74:
00402A74: test esi,810h
00402A7A: lea eax,[esp+264h]
00402A81: push eax
00402A82: je 00402EFDh
00402A88: call 004032B0h
00402A8D: add esp,4
00402A90: push eax
00402A91: lea eax,[esp+5Ch]
00402A95: push eax
00402A96: call 004046E0h
00402A9B: add esp,8
00402A9E: mov edi,eax
00402AA0: test edi,edi
00402AA2: jge 00402F0Eh
00402AA8: mov dword ptr [esp+48h],1
00402AB0: jmp 00402F0Eh
00402AB5: mov dword ptr [esp+4Ch],1
00402ABD: add bl,20h
00402AC0: or esi,40h
00402AC3: lea eax,[esp+58h]
00402AC7: cmp dword ptr [esp+1Ch],0
00402ACC: mov [esp+18h],eax
00402AD0: jge 00402E4Eh
00402AD6: mov dword ptr [esp+1Ch],6
00402ADE: jmp 00402E5Dh
00402AE3: test esi,830h
00402AE9: jne short 00402AF1h
00402AEB: or esi,800h
loc_00402AF1:
00402AF1: cmp dword ptr [esp+1Ch],0FFFFFFFFh
00402AF6: mov ebx,7FFFFFFFh
00402AFB: je short 00402B01h
00402AFD: mov ebx,[esp+1Ch]
loc_00402B01:
00402B01: lea eax,[esp+264h]
00402B08: push eax
00402B09: call 00403280h
00402B0E: mov [esp+1Ch],eax
00402B12: add esp,4
00402B15: test esi,810h
00402B1B: je 00402F18h
00402B21: test eax,eax
00402B23: jne short 00402B2Eh
00402B25: mov eax,[4067E8h]
00402B2A: mov [esp+18h],eax
loc_00402B2E:
00402B2E: mov ebp,[esp+18h]
00402B32: xor edi,edi
00402B34: mov dword ptr [esp+3Ch],1
00402B3C: test ebx,ebx
00402B3E: jle 00402F44h
loc_00402B44:
00402B44: mov ax,[ebp]
00402B48: test ax,ax
00402B4B: je 00402F44h
00402B51: push eax
00402B52: lea eax,[esp+14h]
00402B56: push eax
00402B57: call 004046E0h
00402B5C: add esp,8
00402B5F: test eax,eax
00402B61: je 00402F44h
00402B67: add edi,eax
00402B69: add ebp,2
00402B6C: cmp edi,ebx
00402B6E: jl short 00402B44h
00402B70: jmp 00402F44h
00402B75: lea eax,[esp+264h]
00402B7C: push eax
00402B7D: call 00403280h
00402B82: add esp,4
00402B85: test eax,eax
00402B87: je short 00402BC3h
00402B89: mov ecx,[eax+4]
00402B8C: test ecx,ecx
00402B8E: je short 00402BC3h
00402B90: test esi,800h
00402B96: je short 00402BAFh
00402B98: mov dword ptr [esp+3Ch],1
00402BA0: movsx edi,word ptr [eax]
00402BA3: shr edi,1
00402BA6: mov [esp+18h],ecx
00402BAA: jmp 00402F44h
loc_00402BAF:
00402BAF: mov dword ptr [esp+3Ch],0
00402BB7: movsx edi,word ptr [eax]
00402BBA: mov [esp+18h],ecx
00402BBE: jmp 00402F44h
loc_00402BC3:
00402BC3: mov eax,[4067E4h]
00402BC8: mov ecx,0FFFFFFFFh
00402BCD: mov edi,eax
00402BCF: mov [esp+18h],eax
00402BD3: sub eax,eax
00402BD5: repne scasb [edi]
00402BD7: not ecx
00402BD9: lea edi,[ecx-1]
00402BDC: jmp 00402F44h
00402BE1: or esi,40h
00402BE4: mov edi,0Ah
00402BE9: jmp short 00402C64h
00402BEB: lea eax,[esp+264h]
00402BF2: push eax
00402BF3: call 00403280h
00402BF8: mov ecx,[esp+2Ch]
00402BFC: add esp,4
00402BFF: test esi,20h
00402C05: je short 00402C0Ch
00402C07: mov [eax],cx
00402C0A: jmp short 00402C0Eh
loc_00402C0C:
00402C0C: mov [eax],ecx
00402C0E: mov dword ptr [esp+48h],1
00402C16: jmp 00402F44h
00402C1B: test esi,80h
00402C21: mov edi,8
00402C26: je short 00402C64h
00402C28: or esi,200h
00402C2E: jmp short 00402C64h
00402C30: mov dword ptr [esp+1Ch],8
00402C38: mov dword ptr [esp+44h],7
00402C40: test esi,80h
00402C46: mov edi,10h
00402C4B: je short 00402C64h
00402C4D: mov byte ptr [esp+12h],30h
00402C52: mov dword ptr [esp+38h],2
00402C5A: mov eax,[esp+44h]
00402C5E: add al,51h
00402C60: mov [esp+13h],al
loc_00402C64:
00402C64: test esi,8000h
00402C6A: je short 00402C9Ah
00402C6C: lea eax,[esp+264h]
00402C73: push eax
00402C74: call 00403290h
00402C79: mov [esp+24h],eax
00402C7D: mov [esp+28h],edx
00402C81: add esp,4
00402C84: jmp 00402D0Ch
00402C89: mov edi,0Ah
00402C8E: jmp short 00402C64h
00402C90: mov dword ptr [esp+44h],27h
00402C98: jmp short 00402C40h
loc_00402C9A:
00402C9A: test esi,20h
00402CA0: je short 00402CD5h
00402CA2: test esi,40h
00402CA8: lea eax,[esp+264h]
00402CAF: push eax
00402CB0: je short 00402CC8h
00402CB2: call 00403280h
00402CB7: movsx eax,ax
00402CBA: mov [esp+24h],eax
00402CBE: add esp,4
00402CC1: cdq
00402CC2: mov [esp+24h],edx
00402CC6: jmp short 00402D0Ch
loc_00402CC8:
00402CC8: call 00403280h
00402CCD: movzx eax,ax
00402CD0: add esp,4
00402CD3: jmp short 00402D00h
loc_00402CD5:
00402CD5: test esi,40h
00402CDB: lea eax,[esp+264h]
00402CE2: push eax
00402CE3: je short 00402CF8h
00402CE5: call 00403280h
00402CEA: mov [esp+24h],eax
00402CEE: add esp,4
00402CF1: cdq
00402CF2: mov [esp+24h],edx
00402CF6: jmp short 00402D0Ch
loc_00402CF8:
00402CF8: call 00403280h
00402CFD: add esp,4
00402D00: mov [esp+20h],eax
00402D04: mov dword ptr [esp+24h],0
00402D0C: test esi,40h
00402D12: je short 00402D43h
00402D14: cmp dword ptr [esp+24h],0
00402D19: jg short 00402D43h
00402D1B: jl short 00402D24h
00402D1D: cmp dword ptr [esp+20h],0
00402D22: jae short 00402D43h
loc_00402D24:
00402D24: mov eax,[esp+20h]
00402D28: mov ecx,[esp+24h]
00402D2C: neg eax
00402D2E: adc ecx,0
00402D31: mov [esp+2Ch],eax
00402D35: neg ecx
00402D37: or esi,100h
00402D3D: mov [esp+30h],ecx
00402D41: jmp short 00402D53h
loc_00402D43:
00402D43: mov eax,[esp+20h]
00402D47: mov ecx,[esp+24h]
00402D4B: mov [esp+2Ch],eax
00402D4F: mov [esp+30h],ecx
00402D53: test esi,8000h
00402D59: jne short 00402D65h
00402D5B: and dword ptr [esp+2Ch],0FFFFFFFFh
00402D60: and dword ptr [esp+30h],0
loc_00402D65:
00402D65: cmp dword ptr [esp+1Ch],0
00402D6A: jge short 00402D76h
00402D6C: mov dword ptr [esp+1Ch],1
00402D74: jmp short 00402D79h
loc_00402D76:
00402D76: and esi,0FFFFFFF7h
00402D79: cmp dword ptr [esp+30h],0
00402D7E: jne short 00402D8Fh
00402D80: cmp dword ptr [esp+2Ch],0
00402D85: jne short 00402D8Fh
00402D87: mov dword ptr [esp+38h],0
loc_00402D8F:
00402D8F: lea eax,[esp+257h]
00402D96: mov [esp+18h],eax
00402D9A: mov eax,[esp+1Ch]
00402D9E: dec dword ptr [esp+1Ch]
00402DA2: test eax,eax
00402DA4: jg short 00402DB4h
00402DA6: cmp dword ptr [esp+30h],0
00402DAB: jne short 00402DB4h
00402DAD: cmp dword ptr [esp+2Ch],0
00402DB2: je short 00402E11h
loc_00402DB4:
00402DB4: mov [esp+20h],edi
00402DB8: mov eax,edi
00402DBA: cdq
00402DBB: mov eax,[esp+2Ch]
00402DBF: mov ecx,[esp+30h]
00402DC3: mov [esp+24h],edx
00402DC7: mov edx,[esp+20h]
00402DCB: mov ebx,[esp+24h]
00402DCF: push ebx
00402DD0: push edx
00402DD1: push ecx
00402DD2: push eax
00402DD3: call 004047F0h
00402DD8: lea ebx,[eax+30h]
00402DDB: mov ecx,[esp+30h]
00402DDF: mov eax,[esp+2Ch]
00402DE3: mov edx,[esp+20h]
00402DE7: mov ebp,[esp+24h]
00402DEB: push ebp
00402DEC: push edx
00402DED: push ecx
00402DEE: push eax
00402DEF: call 00404780h
00402DF4: mov [esp+2Ch],eax
00402DF8: cmp ebx,39h
00402DFB: mov [esp+30h],edx
00402DFF: jle short 00402E05h
00402E01: add ebx,[esp+44h]
loc_00402E05:
00402E05: mov eax,[esp+18h]
00402E09: dec dword ptr [esp+18h]
00402E0D: mov [eax],bl
00402E0F: jmp short 00402D9Ah
loc_00402E11:
00402E11: lea edi,[esp+257h]
00402E18: sub edi,[esp+18h]
00402E1C: inc dword ptr [esp+18h]
00402E20: test esi,200h
00402E26: je 00402F44h
00402E2C: mov eax,[esp+18h]
00402E30: cmp byte ptr [eax],30h
00402E33: jne short 00402E3Dh
00402E35: test edi,edi
00402E37: jne 00402F44h
loc_00402E3D:
00402E3D: inc edi
00402E3E: dec dword ptr [esp+18h]
00402E42: mov eax,[esp+18h]
00402E46: mov byte ptr [eax],30h
00402E49: jmp 00402F44h
loc_00402E4E:
00402E4E: jne short 00402E5Dh
00402E50: cmp bl,67h
00402E53: jne short 00402E5Dh
00402E55: mov dword ptr [esp+1Ch],1
loc_00402E5D:
00402E5D: add dword ptr [esp+264h],8
00402E65: mov eax,[esp+264h]
00402E6C: mov ecx,[eax-8]
00402E6F: mov edx,[eax-4]
00402E72: sub eax,8
00402E75: mov [esp+50h],ecx
00402E79: mov eax,[esp+4Ch]
00402E7D: mov ecx,[esp+1Ch]
00402E81: mov [esp+54h],edx
00402E85: push eax
00402E86: lea eax,[esp+5Ch]
00402E8A: push ecx
00402E8B: movsx edx,bl
00402E8E: lea ecx,[esp+58h]
00402E92: push edx
00402E93: push eax
00402E94: mov edi,esi
00402E96: push ecx
00402E97: call dword ptr [407098h]
00402E9D: add esp,14h
00402EA0: and edi,80h
00402EA6: je short 00402EBDh
00402EA8: cmp dword ptr [esp+1Ch],0
00402EAD: jne short 00402EBDh
00402EAF: lea eax,[esp+58h]
00402EB3: push eax
00402EB4: call dword ptr [4070A4h]
00402EBA: add esp,4
loc_00402EBD:
00402EBD: cmp bl,67h
00402EC0: jne short 00402ED4h
00402EC2: test edi,edi
00402EC4: jne short 00402ED4h
00402EC6: lea eax,[esp+58h]
00402ECA: push eax
00402ECB: call dword ptr [40709Ch]
00402ED1: add esp,4
loc_00402ED4:
00402ED4: cmp byte ptr [esp+58h],2Dh
00402ED9: jne short 00402EE9h
00402EDB: or esi,100h
00402EE1: lea eax,[esp+59h]
00402EE5: mov [esp+18h],eax
loc_00402EE9:
00402EE9: mov edi,[esp+18h]
00402EED: mov ecx,0FFFFFFFFh
00402EF2: sub eax,eax
00402EF4: repne scasb [edi]
00402EF6: not ecx
00402EF8: lea edi,[ecx-1]
00402EFB: jmp short 00402F44h
loc_00402EFD:
00402EFD: mov edi,1
00402F02: call 00403280h
00402F07: mov [esp+5Ch],al
00402F0B: add esp,4
loc_00402F0E:
00402F0E: lea eax,[esp+58h]
00402F12: mov [esp+18h],eax
00402F16: jmp short 00402F44h
loc_00402F18:
00402F18: cmp dword ptr [esp+18h],0
00402F1D: jne short 00402F28h
00402F1F: mov eax,[4067E4h]
00402F24: mov [esp+18h],eax
loc_00402F28:
00402F28: mov edi,[esp+18h]
00402F2C: mov eax,ebx
00402F2E: dec ebx
00402F2F: test eax,eax
00402F31: je short 00402F40h
loc_00402F33:
00402F33: cmp byte ptr [edi],0
00402F36: je short 00402F40h
00402F38: inc edi
00402F39: mov eax,ebx
00402F3B: dec ebx
00402F3C: test eax,eax
00402F3E: jne short 00402F33h
loc_00402F40:
00402F40: sub edi,[esp+18h]
loc_00402F44:
00402F44: cmp dword ptr [esp+48h],0
00402F49: jne 00402A0Fh
00402F4F: test esi,40h
00402F55: je short 00402F8Ah
00402F57: test esi,100h
00402F5D: je short 00402F66h
00402F5F: mov byte ptr [esp+12h],2Dh
00402F64: jmp short 00402F82h
loc_00402F66:
00402F66: test esi,1
00402F6C: je short 00402F75h
00402F6E: mov byte ptr [esp+12h],2Bh
00402F73: jmp short 00402F82h
loc_00402F75:
00402F75: test esi,2
00402F7B: je short 00402F8Ah
00402F7D: mov byte ptr [esp+12h],20h
00402F82: mov dword ptr [esp+38h],1
loc_00402F8A:
00402F8A: mov eax,[esp+34h]
00402F8E: sub eax,edi
00402F90: sub eax,[esp+38h]
00402F94: test esi,0Ch
00402F9A: mov [esp+20h],eax
00402F9E: jne short 00402FBCh
00402FA0: lea eax,[esp+28h]
00402FA4: mov ecx,[esp+25Ch]
00402FAB: mov edx,[esp+20h]
00402FAF: push eax
00402FB0: push ecx
00402FB1: push edx
00402FB2: push 20h
00402FB4: call 00403200h
00402FB9: add esp,10h
loc_00402FBC:
00402FBC: lea eax,[esp+28h]
00402FC0: mov ecx,[esp+25Ch]
00402FC7: mov edx,[esp+38h]
00402FCB: push eax
00402FCC: lea eax,[esp+16h]
00402FD0: push ecx
00402FD1: push edx
00402FD2: push eax
00402FD3: call 00403240h
00402FD8: add esp,10h
00402FDB: test esi,8
00402FE1: je short 00403007h
00402FE3: test esi,4
00402FE9: jne short 00403007h
00402FEB: lea eax,[esp+28h]
00402FEF: mov ecx,[esp+25Ch]
00402FF6: mov edx,[esp+20h]
00402FFA: push eax
00402FFB: push ecx
00402FFC: push edx
00402FFD: push 30h
00402FFF: call 00403200h
00403004: add esp,10h
loc_00403007:
00403007: cmp dword ptr [esp+3Ch],0
0040300C: je short 00403057h
0040300E: test edi,edi
00403010: jle short 00403057h
00403012: mov ebp,[esp+18h]
00403016: lea ebx,[edi-1]
loc_00403019:
00403019: mov eax,ebp
0040301B: lea ecx,[esp+14h]
0040301F: mov ax,[eax]
00403022: add ebp,2
00403025: push eax
00403026: push ecx
00403027: call 004046E0h
0040302C: add esp,8
0040302F: test eax,eax
00403031: jle short 00403072h
00403033: lea ecx,[esp+28h]
00403037: mov edx,[esp+25Ch]
0040303E: push ecx
0040303F: push edx
00403040: push eax
00403041: lea eax,[esp+20h]
00403045: push eax
00403046: call 00403240h
0040304B: add esp,10h
0040304E: mov ecx,ebx
00403050: dec ebx
00403051: test ecx,ecx
00403053: jne short 00403019h
00403055: jmp short 00403072h
loc_00403057:
00403057: lea eax,[esp+28h]
0040305B: mov ecx,[esp+25Ch]
00403062: mov edx,[esp+18h]
00403066: push eax
00403067: push ecx
00403068: push edi
00403069: push edx
0040306A: call 00403240h
0040306F: add esp,10h
loc_00403072:
00403072: test esi,4
00403078: je 00402A0Fh
0040307E: lea eax,[esp+28h]
00403082: mov ecx,[esp+25Ch]
00403089: mov edx,[esp+20h]
0040308D: push eax
0040308E: push ecx
0040308F: push edx
00403090: push 20h
00403092: call 00403200h
00403097: add esp,10h
0040309A: jmp 00402A0Fh
0040309F: nop
004030A0: mov ah,29h
004030A2: inc eax
004030A3: add [eax+28h],bl
004030A6: inc eax
004030A7: add [ebx-2BFFBFD8h],cl
004030AD: sub [eax],al
004030B0: sbb ch,[ecx]
004030B2: inc eax
004030B3: add [edi],ah
004030B5: sub [eax],eax
004030B8: insb [edi],dx
004030B9: sub [eax],eax
004030BC: dec eax
004030BD: sub al,[eax]
004030C0: test eax,0B1004028h
004030C5: sub [eax],al
004030C8: mov esp,0C4004028h
004030CD: sub [eax],al
004030D0: int3
004030D1: sub [eax],al
004030D4: cvtpi2ps xmm0,[eax]
004030D8: add [5050105h],al
004030DE: add eax,5050505h
004030E3: add al,[4050503h]
004030E9: lea ecx,[ecx]
004030EC: mov ch,[ecx]
004030EE: inc eax
004030EF: add [esi],dh
004030F1: sub al,[eax]
004030F4: cmp ebp,[edx]
004030F6: inc eax
004030F7: add [eax+2Ah],al
004030FA: inc eax
004030FB: add [edi],cl
004030FD: sub al,[eax]
00403100: add [esp+eax],al
00403103: add al,4
00403105: add al,4
00403107: add al,4
00403109: add al,4
0040310B: add al,4
0040310D: add al,4
0040310F: add al,4
00403111: add al,4
00403113: add al,4
00403115: add al,4
00403117: add al,4
00403119: add al,4
0040311B: add al,4
0040311D: add al,4
0040311F: add [esp+eax],eax
00403122: add al,2
00403124: add al,4
00403126: add al,4
00403128: add al,4
0040312A: add al,4
0040312C: add al,4
0040312E: add edx,[eax+402A66h]
00403134: mov ch,2Ah
00403136: inc eax
00403137: add [ebp-1CFFBFD6h],dh
0040313D: sub al,[eax]
00403140: cmp [eax+eax*2],ch
00403143: add [ebp+2Bh],dh
00403146: inc eax
00403147: add [edx+ebp+40h],dh
0040314B: add cl,ah
0040314D: sub eax,[eax]
00403150: shr byte ptr [edx],40h
00403153: add cl,ah
00403155: sub eax,[eax]
00403158: jmp short 00403185h
0040315A: inc eax
0040315B: add [ebx],bl
0040315D: sub al,40h
0040315F: add [eax],dh
00403161: sub al,40h
00403163: add cl,dh
00403165: sub al,[eax]
00403168: mov [eax+eax*2],ebp
0040316B: add [eax+4400402Ch],dl
00403171: das
00403172: inc eax
00403173: add [eax],al
00403175: adc [ecx],al
00403177: adc [edx],al
00403179: adc [eax],dl
0040317B: adc [eax],dl
0040317D: adc [eax],dl
0040317F: adc [eax],dl
00403181: adc [eax],dl
00403183: adc [ebx],al
00403185: adc [eax],dl
00403187: adc [eax],dl
00403189: add al,10h
0040318B: add eax,10101010h
00403190: adc [eax],dl
00403192: adc [eax],dl
00403194: push es
00403195: pop es
00403196: or [eax],cl
00403198: or [eax],dl
0040319A: or [eax],edx
0040319C: adc [eax],dl
0040319E: adc [edx],cl
004031A0: or ecx,[eax+edx]
004031A3: adc [10100E10h],cl
004031A9: bswap esp
004031AB: int3
004031AC: int3
004031AD: int3
004031AE: int3
004031AF: int3
loc_004031B0:
004031B0: mov edx,[esp+8]
004031B4: mov eax,[edx+4]
004031B7: dec eax
004031B8: mov [edx+4],eax
004031BB: js short 004031D0h
004031BD: mov eax,[edx]
004031BF: mov cl,[esp+4]
004031C3: mov [eax],cl
004031C5: mov ecx,[edx]
004031C7: xor eax,eax
004031C9: mov al,[ecx]
004031CB: inc ecx
004031CC: mov [edx],ecx
004031CE: jmp short 004031DEh
loc_004031D0:
004031D0: mov ecx,[esp+4]
004031D4: push edx
004031D5: push ecx
004031D6: call 00402680h
004031DB: add esp,8
004031DE: cmp eax,0FFFFFFFFh
004031E1: mov eax,[esp+0Ch]
004031E5: jne short 004031EEh
004031E7: mov dword ptr [eax],0FFFFFFFFh
004031ED: ret
loc_004031EE:
004031EE: inc dword ptr [eax]
004031F0: ret
004031F1: int3
004031F2: int3
004031F3: int3
004031F4: int3
004031F5: int3
004031F6: int3
004031F7: int3
004031F8: int3
004031F9: int3
004031FA: int3
004031FB: int3
004031FC: int3
004031FD: int3
004031FE: int3
004031FF: int3
loc_00403200:
00403200: push ebx
00403201: push esi
00403202: mov esi,[esp+0Ch]
00403206: push edi
00403207: mov edi,[esp+14h]
0040320B: push ebp
0040320C: mov ebx,[esp+1Ch]
00403210: mov ebp,[esp+20h]
loc_00403214:
00403214: mov eax,edi
00403216: dec edi
00403217: test eax,eax
00403219: jle short 0040322Ch
0040321B: push ebp
0040321C: push ebx
0040321D: push esi
0040321E: call 004031B0h
00403223: add esp,0Ch
00403226: cmp dword ptr [ebp],0FFFFFFFFh
0040322A: jne short 00403214h
loc_0040322C:
0040322C: pop ebp
0040322D: pop edi
0040322E: pop esi
0040322F: pop ebx
00403230: ret
00403231: int3
00403232: int3
00403233: int3
00403234: int3
00403235: int3
00403236: int3
00403237: int3
00403238: int3
00403239: int3
0040323A: int3
0040323B: int3
0040323C: int3
0040323D: int3
0040323E: int3
0040323F: int3
loc_00403240:
00403240: push ebx
00403241: push esi
00403242: mov esi,[esp+0Ch]
00403246: push edi
00403247: mov edi,[esp+14h]
0040324B: push ebp
0040324C: mov ebx,[esp+1Ch]
00403250: mov ebp,[esp+20h]
loc_00403254:
00403254: mov eax,edi
00403256: dec edi
00403257: test eax,eax
00403259: jle short 00403272h
0040325B: mov eax,esi
0040325D: push ebp
0040325E: inc esi
0040325F: push ebx
00403260: movsx ecx,byte ptr [eax]
00403263: push ecx
00403264: call 004031B0h
00403269: add esp,0Ch
0040326C: cmp dword ptr [ebp],0FFFFFFFFh
00403270: jne short 00403254h
loc_00403272:
00403272: pop ebp
00403273: pop edi
00403274: pop esi
00403275: pop ebx
00403276: ret
00403277: int3
00403278: int3
00403279: int3
0040327A: int3
0040327B: int3
0040327C: int3
0040327D: int3
0040327E: int3
0040327F: int3
loc_00403280:
00403280: mov eax,[esp+4]
00403284: mov ecx,[eax]
00403286: add ecx,4
00403289: mov [eax],ecx
0040328B: mov eax,[ecx-4]
0040328E: ret
0040328F: int3
loc_00403290:
00403290: mov eax,[esp+4]
00403294: mov ecx,[eax]
00403296: add ecx,8
00403299: mov [eax],ecx
0040329B: mov eax,[ecx-8]
0040329E: mov edx,[ecx-4]
004032A1: sub ecx,8
004032A4: ret
004032A5: int3
004032A6: int3
004032A7: int3
004032A8: int3
004032A9: int3
004032AA: int3
004032AB: int3
004032AC: int3
004032AD: int3
004032AE: int3
004032AF: int3
loc_004032B0:
004032B0: mov eax,[esp+4]
004032B4: mov ecx,[eax]
004032B6: add ecx,4
004032B9: mov [eax],ecx
004032BB: mov ax,[ecx-4]
004032BF: ret
loc_004032C0:
004032C0: push ebp
004032C1: mov ebp,esp
004032C3: push ebx
004032C4: push esi
004032C5: push edi
004032C6: push ebp
004032C7: push 0
004032C9: push 0
004032CB: push 4032D8h
004032D0: push dword ptr [ebp+8]
004032D3: call 00404F82h
004032D8: pop ebp
004032D9: pop edi
004032DA: pop esi
004032DB: pop ebx
004032DC: mov esp,ebp
004032DE: pop ebp
004032DF: ret
004032E0: mov ecx,[esp+4]
004032E4: test dword ptr [ecx+4],6
004032EB: mov eax,1
004032F0: je short 00403301h
004032F2: mov eax,[esp+8]
004032F6: mov edx,[esp+10h]
004032FA: mov [edx],eax
004032FC: mov eax,3
loc_00403301:
00403301: ret
loc_00403302:
00403302: push ebx
00403303: push esi
00403304: push edi
00403305: mov eax,[esp+10h]
00403309: push eax
0040330A: push 0FFFFFFFEh
0040330C: push 4032E0h
00403311: push dword ptr fs:[0]
00403318: mov fs:[0],esp
0040331F: mov eax,[esp+20h]
00403323: mov ebx,[eax+8]
00403326: mov esi,[eax+0Ch]
00403329: cmp esi,0FFFFFFFFh
0040332C: je short 0040335Ch
0040332E: cmp esi,[esp+24h]
00403332: je short 0040335Ch
00403334: lea esi,[esi+esi*2]
00403337: mov ecx,[ebx+esi*4]
0040333A: mov [esp+8],ecx
0040333E: mov [eax+0Ch],ecx
00403341: cmp dword ptr [ebx+esi*4+4],0
00403346: jne short 0040335Ah
00403348: push 101h
0040334D: mov eax,[ebx+esi*4+8]
00403351: call 00403396h
00403356: call dword ptr [ebx+esi*4+8]
loc_0040335A:
0040335A: jmp short 0040331Fh
loc_0040335C:
0040335C: pop dword ptr fs:[0]
00403363: add esp,0Ch
00403366: pop edi
00403367: pop esi
00403368: pop ebx
00403369: ret
0040336A: xor eax,eax
0040336C: mov ecx,fs:[0]
00403373: cmp dword ptr [ecx+4],4032E0h
0040337A: jne short 0040338Ch
0040337C: mov edx,[ecx+0Ch]
0040337F: mov edx,[edx+0Ch]
00403382: cmp [ecx+8],edx
00403385: jne short 0040338Ch
00403387: mov eax,1
loc_0040338C:
0040338C: ret
0040338D: push ebx
0040338E: push ecx
0040338F: mov ebx,4067ECh
00403394: jmp short 004033A0h
loc_00403396:
00403396: push ebx
00403397: push ecx
00403398: mov ebx,4067ECh
0040339D: mov ecx,[ebp+8]
004033A0: mov [ebx+8],ecx
004033A3: mov [ebx+4],eax
004033A6: mov [ebx+0Ch],ebp
004033A9: pop ecx
004033AA: pop ebx
004033AB: ret 4
004033AE: int3
004033AF: int3
loc_004033B0:
004033B0: mov eax,[esp+4]
004033B4: push esi
004033B5: push edi
004033B6: push eax
004033B7: call 00403510h
004033BC: add esp,4
004033BF: test eax,eax
004033C1: je 00403501h
004033C7: mov edx,[eax+8]
004033CA: test edx,edx
004033CC: je 00403501h
004033D2: cmp edx,5
004033D5: jne short 004033E6h
004033D7: mov dword ptr [eax+8],0
004033DE: mov eax,1
004033E3: pop edi
004033E4: pop esi
004033E5: ret
loc_004033E6:
004033E6: cmp edx,1
004033E9: jne short 004033F3h
004033EB: mov eax,0FFFFFFFFh
004033F0: pop edi
004033F1: pop esi
004033F2: ret
loc_004033F3:
004033F3: mov esi,[406888h]
004033F9: mov ecx,[esp+10h]
004033FD: mov [406888h],ecx
00403403: cmp dword ptr [eax+4],8
00403407: jne 004034E3h
0040340D: mov ecx,[40687Ch]
00403413: add ecx,[406878h]
00403419: cmp [406878h],ecx
0040341F: jge short 00403443h
00403421: mov ecx,[406878h]
00403427: lea ecx,[ecx+ecx*2]
0040342A: lea edi,[ecx*4+406808h]
00403431: mov ecx,[40687Ch]
loc_00403437:
00403437: mov dword ptr [edi],0
0040343D: add edi,0Ch
00403440: dec ecx
00403441: jne short 00403437h
loc_00403443:
00403443: mov edi,[406884h]
00403449: mov eax,[eax]
0040344B: cmp eax,0C000008Eh
00403450: jne short 0040345Eh
00403452: mov dword ptr [406884h],83h
0040345C: jmp short 004034CEh
loc_0040345E:
0040345E: cmp eax,0C0000090h
00403463: jne short 00403471h
00403465: mov dword ptr [406884h],81h
0040346F: jmp short 004034CEh
loc_00403471:
00403471: cmp eax,0C0000091h
00403476: jne short 00403484h
00403478: mov dword ptr [406884h],84h
00403482: jmp short 004034CEh
loc_00403484:
00403484: cmp eax,0C0000093h
00403489: jne short 00403497h
0040348B: mov dword ptr [406884h],85h
00403495: jmp short 004034CEh
loc_00403497:
00403497: cmp eax,0C000008Dh
0040349C: jne short 004034AAh
0040349E: mov dword ptr [406884h],82h
004034A8: jmp short 004034CEh
loc_004034AA:
004034AA: cmp eax,0C000008Fh
004034AF: jne short 004034BDh
004034B1: mov dword ptr [406884h],86h
004034BB: jmp short 004034CEh
loc_004034BD:
004034BD: cmp eax,0C0000092h
004034C2: jne short 004034CEh
004034C4: mov dword ptr [406884h],8Ah
loc_004034CE:
004034CE: mov eax,[406884h]
004034D3: push eax
004034D4: push 8
004034D6: call edx
004034D8: add esp,8
004034DB: mov [406884h],edi
004034E1: jmp short 004034F3h
loc_004034E3:
004034E3: mov dword ptr [eax+8],0
004034EA: mov eax,[eax+4]
004034ED: push eax
004034EE: call edx
004034F0: add esp,4
004034F3: mov eax,0FFFFFFFFh
004034F8: pop edi
004034F9: mov [406888h],esi
004034FF: pop esi
00403500: ret
loc_00403501:
00403501: mov ecx,[esp+10h]
00403505: push ecx
00403506: call dword ptr [409230h]
0040350C: pop edi
0040350D: pop esi
0040350E: ret
0040350F: int3
loc_00403510:
00403510: mov edx,406800h
00403515: mov ecx,[esp+4]
loc_00403519:
00403519: cmp [edx],ecx
0040351B: je short 00403533h
0040351D: add edx,0Ch
00403520: mov eax,[406880h]
00403525: lea eax,[eax+eax*2]
00403528: lea eax,[eax*4+406800h]
0040352F: cmp eax,edx
00403531: ja short 00403519h
loc_00403533:
00403533: mov eax,[edx]
00403535: sub eax,ecx
00403537: cmp eax,1
0040353A: sbb eax,eax
0040353C: and eax,edx
0040353E: ret
0040353F: int3
loc_00403540:
00403540: mov eax,[esp+4]
00403544: push 4
00403546: push 0
00403548: push eax
00403549: call 00403560h
0040354E: add esp,0Ch
00403551: ret
00403552: int3
00403553: int3
00403554: int3
00403555: int3
00403556: int3
00403557: int3
00403558: int3
00403559: int3
0040355A: int3
0040355B: int3
0040355C: int3
0040355D: int3
0040355E: int3
0040355F: int3
loc_00403560:
00403560: xor edx,edx
00403562: xor ecx,ecx
00403564: mov dl,[esp+4]
00403568: mov cl,[edx+406899h]
0040356E: test [esp+0Ch],ecx
00403572: jne short 00403594h
00403574: mov eax,[esp+8]
00403578: mov ecx,0
0040357D: test eax,eax
0040357F: je short 0040358Dh
00403581: xor ecx,ecx
00403583: mov cx,[edx*2+4070BAh]
0040358B: and ecx,eax
loc_0040358D:
0040358D: test ecx,ecx
0040358F: jne short 00403594h
00403591: xor eax,eax
00403593: ret
loc_00403594:
00403594: mov eax,1
00403599: ret
0040359A: int3
0040359B: int3
0040359C: int3
0040359D: int3
0040359E: int3
0040359F: int3
loc_004035A0:
004035A0: sub esp,4
004035A3: mov edx,[4067B8h]
004035A9: push ebx
004035AA: push esi
004035AB: push edi
004035AC: xor esi,esi
004035AE: push ebp
004035AF: cmp byte ptr [edx],0
004035B2: je short 004035CEh
loc_004035B4:
004035B4: cmp byte ptr [edx],3Dh
004035B7: je short 004035BAh
004035B9: inc esi
loc_004035BA:
004035BA: mov edi,edx
004035BC: mov ecx,0FFFFFFFFh
004035C1: sub eax,eax
004035C3: repne scasb [edi]
004035C5: not ecx
004035C7: add edx,ecx
004035C9: cmp byte ptr [edx],0
004035CC: jne short 004035B4h
loc_004035CE:
004035CE: lea eax,[esi*4+4]
004035D5: push eax
004035D6: call 00404890h
004035DB: mov [406798h],eax
004035E0: add esp,4
004035E3: mov ebx,eax
004035E5: test ebx,ebx
004035E7: jne short 004035F3h
004035E9: push 9
004035EB: call 00402650h
004035F0: add esp,4
loc_004035F3:
004035F3: mov ebp,[4067B8h]
004035F9: mov eax,ebp
004035FB: cmp byte ptr [ebp],0
004035FF: je short 0040365Fh
loc_00403601:
00403601: mov edi,ebp
00403603: mov ecx,0FFFFFFFFh
00403608: sub eax,eax
0040360A: repne scasb [edi]
0040360C: not ecx
0040360E: mov [esp+10h],ecx
00403612: cmp byte ptr [ebp],3Dh
00403616: je short 00403655h
00403618: push ecx
00403619: call 00404890h
0040361E: add esp,4
00403621: mov [ebx],eax
00403623: test eax,eax
00403625: jne short 00403631h
00403627: push 9
00403629: call 00402650h
0040362E: add esp,4
loc_00403631:
00403631: mov edi,ebp
00403633: mov ecx,0FFFFFFFFh
00403638: sub eax,eax
0040363A: repne scasb [edi]
0040363C: not ecx
0040363E: sub edi,ecx
00403640: mov eax,ecx
00403642: shr ecx,2
00403645: mov esi,edi
00403647: mov edi,[ebx]
00403649: add ebx,4
0040364C: rep movsd [edi],[esi]
0040364E: mov ecx,eax
00403650: and ecx,3
00403653: rep movsb [edi],[esi]
loc_00403655:
00403655: add ebp,[esp+10h]
00403659: cmp byte ptr [ebp],0
0040365D: jne short 00403601h
loc_0040365F:
0040365F: mov eax,[4067B8h]
00403664: push eax
00403665: call 00404870h
0040366A: add esp,4
0040366D: mov dword ptr [ebx],0
00403673: pop ebp
00403674: pop edi
00403675: pop esi
00403676: pop ebx
00403677: add esp,4
0040367A: ret
0040367B: int3
0040367C: int3
0040367D: int3
0040367E: int3
0040367F: int3
loc_00403680:
00403680: sub esp,8
00403683: push esi
00403684: push edi
00403685: push 104h
0040368A: mov esi,407500h
0040368F: push esi
00403690: push 0
00403692: call dword ptr [4091D0h]
00403698: mov eax,[408720h]
0040369D: mov [4067A8h],esi
004036A3: cmp byte ptr [eax],0
004036A6: je short 004036AEh
004036A8: mov esi,[408720h]
loc_004036AE:
004036AE: lea eax,[esp+0Ch]
004036B2: lea ecx,[esp+8]
004036B6: push eax
004036B7: push ecx
004036B8: push 0
004036BA: push 0
004036BC: push esi
004036BD: call 00403720h
004036C2: mov eax,[esp+1Ch]
004036C6: add esp,14h
004036C9: shl eax,2
004036CC: add eax,[esp+0Ch]
004036D0: push eax
004036D1: call 00404890h
004036D6: add esp,4
004036D9: mov edi,eax
004036DB: test edi,edi
004036DD: jne short 004036E9h
004036DF: push 8
004036E1: call 00402650h
004036E6: add esp,4
loc_004036E9:
004036E9: lea eax,[esp+0Ch]
004036ED: lea ecx,[esp+8]
004036F1: mov edx,[esp+8]
004036F5: push eax
004036F6: push ecx
004036F7: lea eax,[edi+edx*4]
004036FA: push eax
004036FB: push edi
004036FC: push esi
004036FD: call 00403720h
00403702: mov eax,[esp+1Ch]
00403706: add esp,14h
00403709: dec eax
0040370A: mov [406790h],edi
00403710: pop edi
00403711: mov [40678Ch],eax
00403716: pop esi
00403717: add esp,8
0040371A: ret
0040371B: int3
0040371C: int3
0040371D: int3
0040371E: int3
0040371F: int3
loc_00403720:
00403720: mov ecx,[esp+14h]
00403724: push ebx
00403725: mov edx,[esp+14h]
00403729: push esi
0040372A: mov esi,[esp+0Ch]
0040372E: push edi
0040372F: mov eax,[esp+18h]
00403733: push ebp
00403734: cmp dword ptr [esp+18h],0
00403739: mov dword ptr [ecx],0
0040373F: mov dword ptr [edx],1
00403745: je short 00403752h
00403747: mov edx,[esp+18h]
0040374B: add dword ptr [esp+18h],4
00403750: mov [edx],eax
loc_00403752:
00403752: cmp byte ptr [esi],22h
00403755: je short 0040379Dh
loc_00403757:
00403757: inc dword ptr [ecx]
00403759: test eax,eax
0040375B: je short 00403762h
0040375D: mov dl,[esi]
0040375F: mov [eax],dl
00403761: inc eax
loc_00403762:
00403762: mov dl,[esi]
00403764: inc esi
00403765: xor ebx,ebx
00403767: mov bl,dl
00403769: test byte ptr [ebx+406899h],4
00403770: je short 0040377Eh
00403772: inc dword ptr [ecx]
00403774: test eax,eax
00403776: je short 0040377Dh
00403778: mov bl,[esi]
0040377A: mov [eax],bl
0040377C: inc eax
loc_0040377D:
0040377D: inc esi
loc_0040377E:
0040377E: cmp dl,20h
00403781: je short 0040378Ch
00403783: test dl,dl
00403785: je short 00403790h
00403787: cmp dl,9
0040378A: jne short 00403757h
loc_0040378C:
0040378C: test dl,dl
0040378E: jne short 00403793h
loc_00403790:
00403790: dec esi
00403791: jmp short 004037E3h
loc_00403793:
00403793: test eax,eax
00403795: je short 004037E3h
00403797: mov byte ptr [eax-1],0
0040379B: jmp short 004037E3h
loc_0040379D:
0040379D: inc esi
0040379E: cmp byte ptr [esi],22h
004037A1: je short 004037D3h
loc_004037A3:
004037A3: mov bl,[esi]
004037A5: test bl,bl
004037A7: je short 004037D3h
004037A9: xor edx,edx
004037AB: mov dl,bl
004037AD: test byte ptr [edx+406899h],4
004037B4: je short 004037C2h
004037B6: inc dword ptr [ecx]
004037B8: test eax,eax
004037BA: je short 004037C2h
004037BC: mov dl,[esi]
004037BE: inc esi
004037BF: mov [eax],dl
004037C1: inc eax
loc_004037C2:
004037C2: inc dword ptr [ecx]
004037C4: test eax,eax
004037C6: je short 004037CDh
004037C8: mov dl,[esi]
004037CA: mov [eax],dl
004037CC: inc eax
loc_004037CD:
004037CD: inc esi
004037CE: cmp byte ptr [esi],22h
004037D1: jne short 004037A3h
loc_004037D3:
004037D3: inc dword ptr [ecx]
004037D5: test eax,eax
004037D7: je short 004037DDh
004037D9: mov byte ptr [eax],0
004037DC: inc eax
loc_004037DD:
004037DD: cmp byte ptr [esi],22h
004037E0: jne short 004037E3h
004037E2: inc esi
loc_004037E3:
004037E3: xor edi,edi
004037E5: cmp byte ptr [esi],0
004037E8: je 004038D6h
004037EE: mov dl,[esi]
004037F0: cmp dl,20h
004037F3: je short 004037FAh
004037F5: cmp dl,9
004037F8: jne short 004037FDh
loc_004037FA:
004037FA: inc esi
004037FB: jmp short 004037EEh
loc_004037FD:
004037FD: cmp byte ptr [esi],0
00403800: je 004038D6h
00403806: cmp dword ptr [esp+18h],0
0040380B: je short 00403818h
0040380D: mov edx,[esp+18h]
00403811: add dword ptr [esp+18h],4
00403816: mov [edx],eax
loc_00403818:
00403818: mov edx,[esp+20h]
0040381C: inc dword ptr [edx]
0040381E: mov ebx,1
00403823: xor ebp,ebp
00403825: cmp byte ptr [esi],5Ch
00403828: jne short 00403831h
loc_0040382A:
0040382A: inc esi
0040382B: inc ebp
0040382C: cmp byte ptr [esi],5Ch
0040382F: je short 0040382Ah
loc_00403831:
00403831: cmp byte ptr [esi],22h
00403834: jne short 0040385Ah
00403836: test ebp,1
0040383C: jne short 00403857h
0040383E: test edi,edi
00403840: je short 0040384Eh
00403842: lea edx,[esi+1]
00403845: cmp byte ptr [edx],22h
00403848: jne short 0040384Eh
0040384A: mov esi,edx
0040384C: jmp short 00403850h
loc_0040384E:
0040384E: xor ebx,ebx
00403850: cmp edi,1
00403853: sbb edi,edi
00403855: neg edi
loc_00403857:
00403857: shr ebp,1
loc_0040385A:
0040385A: mov edx,ebp
0040385C: dec ebp
0040385D: test edx,edx
0040385F: je short 00403872h
loc_00403861:
00403861: test eax,eax
00403863: je short 00403869h
00403865: mov byte ptr [eax],5Ch
00403868: inc eax
loc_00403869:
00403869: mov edx,ebp
0040386B: inc dword ptr [ecx]
0040386D: dec ebp
0040386E: test edx,edx
00403870: jne short 00403861h
loc_00403872:
00403872: mov dl,[esi]
00403874: test dl,dl
00403876: je short 004038C7h
00403878: test edi,edi
0040387A: jne short 00403886h
0040387C: cmp dl,20h
0040387F: je short 004038C7h
00403881: cmp dl,9
00403884: je short 004038C7h
loc_00403886:
00403886: test ebx,ebx
00403888: je short 004038C1h
0040388A: test eax,eax
0040388C: je short 004038AFh
0040388E: xor ebx,ebx
00403890: mov bl,dl
00403892: test byte ptr [ebx+406899h],4
00403899: je short 004038A1h
0040389B: mov [eax],dl
0040389D: inc esi
0040389E: inc eax
0040389F: inc dword ptr [ecx]
loc_004038A1:
004038A1: mov dl,[esi]
004038A3: inc eax
004038A4: inc esi
004038A5: mov [eax-1],dl
004038A8: inc dword ptr [ecx]
004038AA: jmp 0040381Eh
loc_004038AF:
004038AF: xor ebx,ebx
004038B1: mov bl,dl
004038B3: test byte ptr [ebx+406899h],4
004038BA: je short 004038BFh
004038BC: inc esi
004038BD: inc dword ptr [ecx]
loc_004038BF:
004038BF: inc dword ptr [ecx]
loc_004038C1:
004038C1: inc esi
004038C2: jmp 0040381Eh
loc_004038C7:
004038C7: test eax,eax
004038C9: je short 004038CFh
004038CB: mov byte ptr [eax],0
004038CE: inc eax
loc_004038CF:
004038CF: inc dword ptr [ecx]
004038D1: jmp 004037E5h
loc_004038D6:
004038D6: cmp dword ptr [esp+18h],0
004038DB: je short 004038E7h
004038DD: mov edx,[esp+18h]
004038E1: mov dword ptr [edx],0
loc_004038E7:
004038E7: mov edx,[esp+20h]
004038EB: pop ebp
004038EC: pop edi
004038ED: pop esi
004038EE: pop ebx
004038EF: inc dword ptr [edx]
004038F1: ret
004038F2: int3
004038F3: int3
004038F4: int3
004038F5: int3
004038F6: int3
004038F7: int3
004038F8: int3
004038F9: int3
004038FA: int3
004038FB: int3
004038FC: int3
004038FD: int3
004038FE: int3
004038FF: int3
loc_00403900:
00403900: sub esp,4
00403903: cmp dword ptr [406890h],0
0040390A: push ebx
0040390B: push esi
0040390C: push edi
0040390D: push ebp
0040390E: jne short 00403950h
00403910: mov esi,[4091F0h]
00403916: call esi
00403918: mov edi,eax
0040391A: test edi,edi
0040391C: je short 0040392Eh
0040391E: mov dword ptr [406890h],1
00403928: mov ebx,[esp+10h]
0040392C: jmp short 0040395Eh
loc_0040392E:
0040392E: call dword ptr [409200h]
00403934: mov ebx,eax
00403936: test ebx,ebx
00403938: je short 00403946h
0040393A: mov dword ptr [406890h],2
00403944: jmp short 0040395Eh
loc_00403946:
00403946: xor eax,eax
00403948: pop ebp
00403949: pop edi
0040394A: pop esi
0040394B: pop ebx
0040394C: add esp,4
0040394F: ret
loc_00403950:
00403950: mov edi,[esp+10h]
00403954: mov ebx,[esp+10h]
00403958: mov esi,[4091F0h]
0040395E: cmp dword ptr [406890h],1
00403965: jne 00403A0Dh
0040396B: test edi,edi
0040396D: jne short 00403981h
0040396F: call esi
00403971: mov edi,eax
00403973: test edi,edi
00403975: jne short 00403981h
00403977: xor eax,eax
00403979: pop ebp
0040397A: pop edi
0040397B: pop esi
0040397C: pop ebx
0040397D: add esp,4
00403980: ret
loc_00403981:
00403981: cmp word ptr [edi],0
00403985: mov esi,edi
00403987: je short 0040399Bh
loc_00403989:
00403989: add esi,2
0040398C: cmp word ptr [esi],0
00403990: jne short 00403989h
00403992: add esi,2
00403995: cmp word ptr [esi],0
00403999: jne short 00403989h
loc_0040399B:
0040399B: sub esi,edi
0040399D: push 0
0040399F: sar esi,1
004039A2: push 0
004039A4: inc esi
004039A5: push 0
004039A7: push 0
004039A9: push esi
004039AA: push edi
004039AB: push 0
004039AD: push 0
004039AF: call dword ptr [4091F8h]
004039B5: mov ebp,eax
004039B7: test ebp,ebp
004039B9: je short 004039FCh
004039BB: push ebp
004039BC: call 00404890h
004039C1: add esp,4
004039C4: mov ebx,eax
004039C6: test ebx,ebx
004039C8: je short 004039FCh
004039CA: push 0
004039CC: push 0
004039CE: push ebp
004039CF: push ebx
004039D0: push esi
004039D1: push edi
004039D2: push 0
004039D4: push 0
004039D6: call dword ptr [4091F8h]
004039DC: test eax,eax
004039DE: jne short 004039EBh
004039E0: push ebx
004039E1: call 00404870h
004039E6: add esp,4
004039E9: xor ebx,ebx
loc_004039EB:
004039EB: push edi
004039EC: call dword ptr [4091F4h]
004039F2: mov eax,ebx
004039F4: pop ebp
004039F5: pop edi
004039F6: pop esi
004039F7: pop ebx
004039F8: add esp,4
004039FB: ret
loc_004039FC:
004039FC: push edi
004039FD: call dword ptr [4091F4h]
00403A03: xor eax,eax
00403A05: pop ebp
00403A06: pop edi
00403A07: pop esi
00403A08: pop ebx
00403A09: add esp,4
00403A0C: ret
loc_00403A0D:
00403A0D: cmp dword ptr [406890h],2
00403A14: jne short 00403A91h
00403A16: test ebx,ebx
00403A18: jne short 00403A30h
00403A1A: call dword ptr [409200h]
00403A20: mov ebx,eax
00403A22: test ebx,ebx
00403A24: jne short 00403A30h
00403A26: xor eax,eax
00403A28: pop ebp
00403A29: pop edi
00403A2A: pop esi
00403A2B: pop ebx
00403A2C: add esp,4
00403A2F: ret
loc_00403A30:
00403A30: mov ebp,ebx
00403A32: cmp byte ptr [ebx],0
00403A35: je short 00403A45h
loc_00403A37:
00403A37: inc ebp
00403A38: cmp byte ptr [ebp],0
00403A3C: jne short 00403A37h
00403A3E: inc ebp
00403A3F: cmp byte ptr [ebp],0
00403A43: jne short 00403A37h
loc_00403A45:
00403A45: sub ebp,ebx
00403A47: inc ebp
00403A48: push ebp
00403A49: call 00404890h
00403A4E: mov [esp+14h],eax
00403A52: add esp,4
00403A55: test eax,eax
00403A57: jne short 00403A6Ah
00403A59: push ebx
00403A5A: call dword ptr [4091FCh]
00403A60: xor eax,eax
00403A62: pop ebp
00403A63: pop edi
00403A64: pop esi
00403A65: pop ebx
00403A66: add esp,4
00403A69: ret
loc_00403A6A:
00403A6A: mov edi,[esp+10h]
00403A6E: mov esi,ebx
00403A70: mov ecx,ebp
00403A72: shr ecx,2
00403A75: rep movsd [edi],[esi]
00403A77: mov ecx,ebp
00403A79: push ebx
00403A7A: and ecx,3
00403A7D: rep movsb [edi],[esi]
00403A7F: call dword ptr [4091FCh]
00403A85: mov eax,[esp+10h]
00403A89: pop ebp
00403A8A: pop edi
00403A8B: pop esi
00403A8C: pop ebx
00403A8D: add esp,4
00403A90: ret
loc_00403A91:
00403A91: xor eax,eax
00403A93: pop ebp
00403A94: pop edi
00403A95: pop esi
00403A96: pop ebx
00403A97: add esp,4
00403A9A: ret
00403A9B: int3
00403A9C: int3
00403A9D: int3
00403A9E: int3
00403A9F: int3
loc_00403AA0:
00403AA0: mov eax,[esp+4]
00403AA4: sub esp,18h
00403AA7: push ebx
00403AA8: push esi
00403AA9: push edi
00403AAA: push ebp
00403AAB: push eax
00403AAC: call 00403C80h
00403AB1: add esp,4
00403AB4: mov ebp,eax
00403AB6: cmp [40699Ch],ebp
00403ABC: jne short 00403AC8h
00403ABE: xor eax,eax
00403AC0: pop ebp
00403AC1: pop edi
00403AC2: pop esi
00403AC3: pop ebx
00403AC4: add esp,18h
00403AC7: ret
loc_00403AC8:
00403AC8: test ebp,ebp
00403ACA: jne short 00403ADBh
00403ACC: call 00403D30h
00403AD1: xor eax,eax
00403AD3: pop ebp
00403AD4: pop edi
00403AD5: pop esi
00403AD6: pop ebx
00403AD7: add esp,18h
00403ADA: ret
loc_00403ADB:
00403ADB: mov dword ptr [esp+10h],0
00403AE3: mov eax,4069C0h
loc_00403AE8:
00403AE8: cmp [eax],ebp
00403AEA: je 00403B8Bh
00403AF0: add eax,30h
00403AF3: inc dword ptr [esp+10h]
00403AF7: cmp eax,406AB0h
00403AFC: jb short 00403AE8h
00403AFE: lea eax,[esp+14h]
00403B02: push eax
00403B03: push ebp
00403B04: call dword ptr [4091E8h]
00403B0A: cmp eax,1
00403B0D: jne 00403C56h
00403B13: mov edi,406898h
00403B18: xor eax,eax
00403B1A: mov ecx,40h
00403B1F: rep stosd [edi]
00403B21: stosb [edi]
00403B22: cmp dword ptr [esp+14h],1
00403B27: jbe 00403C33h
00403B2D: lea esi,[esp+1Ah]
00403B31: cmp [esp+1Ah],al
00403B35: je short 00403B63h
loc_00403B37:
00403B37: mov cl,[esi+1]
00403B3A: test cl,cl
00403B3C: je short 00403B63h
00403B3E: xor eax,eax
00403B40: xor edx,edx
00403B42: mov al,[esi]
00403B44: mov dl,cl
00403B46: cmp edx,eax
00403B48: jb short 00403B5Bh
loc_00403B4A:
00403B4A: or byte ptr [eax+406899h],4
00403B51: inc eax
00403B52: xor ecx,ecx
00403B54: mov cl,[esi+1]
00403B57: cmp ecx,eax
00403B59: jae short 00403B4Ah
loc_00403B5B:
00403B5B: add esi,2
00403B5E: cmp byte ptr [esi],0
00403B61: jne short 00403B37h
loc_00403B63:
00403B63: mov eax,1
loc_00403B68:
00403B68: or byte ptr [eax+406899h],8
00403B6F: inc eax
00403B70: cmp eax,0FFh
00403B75: jb short 00403B68h
00403B77: push ebp
00403B78: mov [40699Ch],ebp
00403B7E: call 00403CD0h
00403B83: add esp,4
00403B86: jmp 00403C3Ah
loc_00403B8B:
00403B8B: mov edi,406898h
00403B90: xor eax,eax
00403B92: mov ecx,40h
00403B97: rep stosd [edi]
00403B99: stosb [edi]
00403B9A: mov ecx,[esp+10h]
00403B9E: lea edx,[ecx+ecx*2]
00403BA1: lea edi,[edx*2]
loc_00403BA8:
00403BA8: lea ecx,[eax+edi]
00403BAB: lea esi,[ecx*8+4069D0h]
00403BB2: cmp byte ptr [esi],0
00403BB5: je short 00403BE8h
loc_00403BB7:
00403BB7: mov cl,[esi+1]
00403BBA: test cl,cl
00403BBC: je short 00403BE8h
00403BBE: xor edx,edx
00403BC0: xor ebx,ebx
00403BC2: mov dl,[esi]
00403BC4: mov bl,cl
00403BC6: cmp ebx,edx
00403BC8: jb short 00403BE0h
00403BCA: mov cl,[eax+4069B8h]
loc_00403BD0:
00403BD0: or [edx+406899h],cl
00403BD6: inc edx
00403BD7: xor ebx,ebx
00403BD9: mov bl,[esi+1]
00403BDC: cmp ebx,edx
00403BDE: jae short 00403BD0h
loc_00403BE0:
00403BE0: add esi,2
00403BE3: cmp byte ptr [esi],0
00403BE6: jne short 00403BB7h
loc_00403BE8:
00403BE8: inc eax
00403BE9: cmp eax,4
00403BEC: jb short 00403BA8h
00403BEE: push ebp
00403BEF: mov [40699Ch],ebp
00403BF5: call 00403CD0h
00403BFA: mov [4069A0h],eax
00403BFF: mov eax,[esp+14h]
00403C03: shl eax,4
00403C06: mov ebx,[eax+eax*2+4069C8h]
00403C0D: add esp,4
00403C10: mov edx,4069A8h
00403C15: lea ecx,[eax+eax*2+4069C4h]
00403C1C: pop ebp
00403C1D: pop edi
00403C1E: pop esi
00403C1F: mov eax,[ecx]
00403C21: mov ecx,[ecx+8]
00403C24: mov [edx],eax
00403C26: xor eax,eax
00403C28: mov [edx+4],ebx
00403C2B: pop ebx
00403C2C: mov [edx+8],ecx
00403C2F: add esp,18h
00403C32: ret
loc_00403C33:
00403C33: xor eax,eax
00403C35: mov [40699Ch],eax
00403C3A: mov ecx,4069A8h
00403C3F: pop ebp
00403C40: pop edi
00403C41: mov [4069A0h],eax
00403C46: xor eax,eax
00403C48: pop esi
00403C49: pop ebx
00403C4A: mov [ecx],eax
00403C4C: mov [ecx+4],eax
00403C4F: mov [ecx+8],eax
00403C52: add esp,18h
00403C55: ret
loc_00403C56:
00403C56: cmp dword ptr [4069B4h],0
00403C5D: je short 00403C6Eh
00403C5F: call 00403D30h
00403C64: xor eax,eax
00403C66: pop ebp
00403C67: pop edi
00403C68: pop esi
00403C69: pop ebx
00403C6A: add esp,18h
00403C6D: ret
loc_00403C6E:
00403C6E: mov eax,0FFFFFFFFh
00403C73: pop ebp
00403C74: pop edi
00403C75: pop esi
00403C76: pop ebx
00403C77: add esp,18h
00403C7A: ret
00403C7B: int3
00403C7C: int3
00403C7D: int3
00403C7E: int3
00403C7F: int3
loc_00403C80:
00403C80: mov dword ptr [4069B4h],0
00403C8A: mov eax,[esp+4]
00403C8E: cmp eax,0FFFFFFFEh
00403C91: jne short 00403CA3h
00403C93: mov dword ptr [4069B4h],1
00403C9D: jmp dword ptr [409240h]
loc_00403CA3:
00403CA3: cmp eax,0FFFFFFFDh
00403CA6: jne short 00403CB8h
00403CA8: mov dword ptr [4069B4h],1
00403CB2: jmp dword ptr [409244h]
loc_00403CB8:
00403CB8: cmp eax,0FFFFFFFCh
00403CBB: jne short 00403CCCh
00403CBD: mov dword ptr [4069B4h],1
00403CC7: mov eax,[4072D8h]
loc_00403CCC:
00403CCC: ret
00403CCD: int3
00403CCE: int3
00403CCF: int3
loc_00403CD0:
00403CD0: mov eax,[esp+4]
00403CD4: sub eax,3A4h
00403CD9: cmp eax,12h
00403CDC: ja short 00403CEDh
00403CDE: xor ecx,ecx
00403CE0: mov cl,[eax+403D1Ch]
00403CE6: jmp dword ptr [ecx*4+403D08h]
loc_00403CED:
00403CED: xor eax,eax
00403CEF: ret
00403CF0: mov eax,411h
00403CF5: ret
00403CF6: mov eax,804h
00403CFB: ret
00403CFC: mov eax,412h
00403D01: ret
00403D02: mov eax,404h
00403D07: ret
00403D08: (bad)
00403D0B: add dh,dh
00403D0D: cmp al,40h
00403D0F: add ah,bh
00403D11: cmp al,40h
00403D13: add [edx],al
00403D15: cmp eax,3CED0040h
00403D1A: inc eax
00403D1B: add [eax],al
00403D1D: add al,4
00403D1F: add al,1
00403D21: add al,4
00403D23: add al,4
00403D25: add al,4
00403D27: add al,4
00403D29: add al,4
00403D2B: add al,4
00403D2D: add al,[ebx]
00403D2F: int3
loc_00403D30:
00403D30: push edi
00403D31: xor eax,eax
00403D33: mov edi,406898h
00403D38: mov ecx,40h
00403D3D: rep stosd [edi]
00403D3F: stosb [edi]
00403D40: mov [4069A8h],eax
00403D45: mov ecx,4069A8h
00403D4A: pop edi
00403D4B: mov [40699Ch],eax
00403D50: mov [4069A0h],eax
00403D55: mov [ecx+4],eax
00403D58: mov [ecx+8],eax
00403D5B: ret
00403D5C: int3
00403D5D: int3
00403D5E: int3
00403D5F: int3
loc_00403D60:
00403D60: push 0FFFFFFFDh
00403D62: call 00403AA0h
00403D67: add esp,4
00403D6A: ret
00403D6B: int3
00403D6C: int3
00403D6D: int3
00403D6E: int3
00403D6F: int3
loc_00403D70:
00403D70: sub esp,44h
00403D73: push ebx
00403D74: push esi
00403D75: push edi
00403D76: push ebp
00403D77: push 100h
00403D7C: call 00404890h
00403D81: add esp,4
00403D84: mov esi,eax
00403D86: test esi,esi
00403D88: jne short 00403D94h
00403D8A: push 1Bh
00403D8C: call 00402650h
00403D91: add esp,4
loc_00403D94:
00403D94: lea eax,[esi+100h]
00403D9A: mov [408620h],esi
00403DA0: mov dword ptr [408614h],20h
00403DAA: cmp eax,esi
00403DAC: jbe short 00403DD3h
00403DAE: xor ecx,ecx
00403DB0: mov edx,0Ah
loc_00403DB5:
00403DB5: mov [esi+4],cl
00403DB8: add esi,8
00403DBB: mov dword ptr [esi-8],0FFFFFFFFh
00403DC2: mov [esi-3],dl
00403DC5: mov eax,[408620h]
00403DCA: add eax,100h
00403DCF: cmp eax,esi
00403DD1: ja short 00403DB5h
loc_00403DD3:
00403DD3: lea eax,[esp+10h]
00403DD7: push eax
00403DD8: call dword ptr [40924Ch]
00403DDE: cmp word ptr [esp+42h],0
00403DE4: je 00403EB8h
00403DEA: cmp dword ptr [esp+44h],0
00403DEF: je 00403EB8h
00403DF5: mov eax,[esp+44h]
00403DF9: mov edi,[eax]
00403DFB: lea ebx,[eax+4]
00403DFE: cmp edi,800h
00403E04: lea esi,[edi+ebx]
00403E07: jl short 00403E0Eh
00403E09: mov edi,800h
loc_00403E0E:
00403E0E: cmp edi,[408614h]
00403E14: jle short 00403E73h
00403E16: mov ebp,408624h
loc_00403E1B:
00403E1B: push 100h
00403E20: call 00404890h
00403E25: add esp,4
00403E28: test eax,eax
00403E2A: je short 00403E6Dh
00403E2C: lea ecx,[eax+100h]
00403E32: mov [ebp],eax
00403E35: add dword ptr [408614h],20h
00403E3C: cmp ecx,eax
00403E3E: jbe short 00403E60h
00403E40: xor ecx,ecx
loc_00403E42:
00403E42: mov [eax+4],cl
00403E45: add eax,8
00403E48: mov dword ptr [eax-8],0FFFFFFFFh
00403E4F: mov byte ptr [eax-3],0Ah
00403E53: mov edx,[ebp]
00403E56: add edx,100h
00403E5C: cmp edx,eax
00403E5E: ja short 00403E42h
loc_00403E60:
00403E60: add ebp,4
00403E63: cmp edi,[408614h]
00403E69: jg short 00403E1Bh
00403E6B: jmp short 00403E73h
loc_00403E6D:
00403E6D: mov edi,[408614h]
loc_00403E73:
00403E73: xor ebp,ebp
00403E75: test edi,edi
00403E77: jle short 00403EB8h
loc_00403E79:
00403E79: mov eax,[esi]
00403E7B: cmp eax,0FFFFFFFFh
00403E7E: je short 00403EAFh
00403E80: test byte ptr [ebx],1
00403E83: je short 00403EAFh
00403E85: push eax
00403E86: call dword ptr [409238h]
00403E8C: test eax,eax
00403E8E: je short 00403EAFh
00403E90: mov eax,ebp
00403E92: mov ecx,ebp
00403E94: and eax,0FFFFFFE7h
00403E97: and ecx,1Fh
00403E9A: sar eax,3
00403E9D: mov edx,[eax+408620h]
00403EA3: mov eax,[esi]
00403EA5: lea ecx,[edx+ecx*8]
00403EA8: mov [ecx],eax
00403EAA: mov dl,[ebx]
00403EAC: mov [ecx+4],dl
loc_00403EAF:
00403EAF: inc ebp
00403EB0: inc ebx
00403EB1: add esi,4
00403EB4: cmp ebp,edi
00403EB6: jl short 00403E79h
loc_00403EB8:
00403EB8: xor esi,esi
00403EBA: mov edi,[409234h]
loc_00403EC0:
00403EC0: lea ebx,[esi*8]
00403EC7: add ebx,[408620h]
00403ECD: cmp dword ptr [ebx],0FFFFFFFFh
00403ED0: jne short 00403F25h
00403ED2: mov eax,0FFFFFFF6h
00403ED7: test esi,esi
00403ED9: mov byte ptr [ebx+4],81h
00403EDD: je short 00403EEDh
00403EDF: lea eax,[esi-1]
00403EE2: cmp eax,1
00403EE5: mov eax,0FFFFFFF5h
00403EEA: adc eax,0FFFFFFFFh
loc_00403EED:
00403EED: push eax
00403EEE: call edi
00403EF0: cmp eax,0FFFFFFFFh
00403EF3: mov ebp,eax
00403EF5: je short 00403F1Fh
00403EF7: push ebp
00403EF8: call dword ptr [409238h]
00403EFE: test eax,eax
00403F00: je short 00403F1Fh
00403F02: and eax,0FFh
00403F07: mov [ebx],ebp
00403F09: cmp eax,2
00403F0C: jne short 00403F14h
00403F0E: or byte ptr [ebx+4],40h
00403F12: jmp short 00403F29h
loc_00403F14:
00403F14: cmp eax,3
00403F17: jne short 00403F29h
00403F19: or byte ptr [ebx+4],8
00403F1D: jmp short 00403F29h
loc_00403F1F:
00403F1F: or byte ptr [ebx+4],40h
00403F23: jmp short 00403F29h
loc_00403F25:
00403F25: or byte ptr [ebx+4],80h
loc_00403F29:
00403F29: inc esi
00403F2A: cmp esi,3
00403F2D: jl short 00403EC0h
00403F2F: mov eax,[408614h]
00403F34: push eax
00403F35: call dword ptr [40923Ch]
00403F3B: pop ebp
00403F3C: pop edi
00403F3D: pop esi
00403F3E: pop ebx
00403F3F: add esp,44h
00403F42: ret
00403F43: int3
00403F44: int3
00403F45: int3
00403F46: int3
00403F47: int3
00403F48: int3
00403F49: int3
00403F4A: int3
00403F4B: int3
00403F4C: int3
00403F4D: int3
00403F4E: int3
00403F4F: int3
loc_00403F50:
00403F50: push 0
00403F52: push 1000h
00403F57: push 1
00403F59: call dword ptr [4091ECh]
00403F5F: mov [408610h],eax
00403F64: ret
00403F65: int3
00403F66: int3
00403F67: int3
00403F68: push esi
00403F69: inc ebx
00403F6A: xor dh,[eax]
00403F6C: pop eax
00403F6D: inc ebx
00403F6E: xor [eax],dh
00403F70: push ebp
00403F71: mov ebp,esp
00403F73: sub esp,8
00403F76: push ebx
00403F77: push esi
00403F78: push edi
00403F79: push ebp
00403F7A: cld
00403F7B: mov ebx,[ebp+0Ch]
00403F7E: mov eax,[ebp+8]
00403F81: test dword ptr [eax+4],6
00403F88: jne 00404010h
00403F8E: mov [ebp-8],eax
00403F91: mov eax,[ebp+10h]
00403F94: mov [ebp-4],eax
00403F97: lea eax,[ebp-8]
00403F9A: mov [ebx-4],eax
00403F9D: mov esi,[ebx+0Ch]
00403FA0: mov edi,[ebx+8]
00403FA3: cmp esi,0FFFFFFFFh
00403FA6: je short 00404009h
00403FA8: lea ecx,[esi+esi*2]
00403FAB: cmp dword ptr [edi+ecx*4+4],0
00403FB0: je short 00403FF7h
00403FB2: push esi
00403FB3: push ebp
00403FB4: lea ebp,[ebx+10h]
00403FB7: call dword ptr [edi+ecx*4+4]
00403FBB: pop ebp
00403FBC: pop esi
00403FBD: mov ebx,[ebp+0Ch]
00403FC0: or eax,eax
00403FC2: je short 00403FF7h
00403FC4: js short 00404002h
00403FC6: mov edi,[ebx+8]
00403FC9: push ebx
00403FCA: call 004032C0h
00403FCF: add esp,4
00403FD2: lea ebp,[ebx+10h]
00403FD5: push esi
00403FD6: push ebx
00403FD7: call 00403302h
00403FDC: add esp,8
00403FDF: lea ecx,[esi+esi*2]
00403FE2: push 1
00403FE4: mov eax,[edi+ecx*4+8]
00403FE8: call 00403396h
00403FED: mov eax,[edi+ecx*4]
00403FF0: mov [ebx+0Ch],eax
00403FF3: call dword ptr [edi+ecx*4+8]
loc_00403FF7:
00403FF7: mov edi,[ebx+8]
00403FFA: lea ecx,[esi+esi*2]
00403FFD: mov esi,[edi+ecx*4]
00404000: jmp short 00403FA3h
loc_00404002:
00404002: mov eax,0
00404007: jmp short 00404025h
loc_00404009:
00404009: mov eax,1
0040400E: jmp short 00404025h
loc_00404010:
00404010: push ebp
00404011: lea ebp,[ebx+10h]
00404014: push 0FFFFFFFFh
00404016: push ebx
00404017: call 00403302h
0040401C: add esp,8
0040401F: pop ebp
00404020: mov eax,1
00404025: pop ebp
00404026: pop edi
00404027: pop esi
00404028: pop ebx
00404029: mov esp,ebp
0040402B: pop ebp
0040402C: ret
0040402D: push ebp
0040402E: mov ecx,[esp+8]
00404032: mov ebp,[ecx]
00404034: mov eax,[ecx+1Ch]
00404037: push eax
00404038: mov eax,[ecx+18h]
0040403B: push eax
0040403C: call 00403302h
00404041: add esp,8
00404044: pop ebp
00404045: ret 4
00404048: int3
00404049: int3
0040404A: int3
0040404B: int3
0040404C: int3
0040404D: int3
0040404E: int3
0040404F: int3
loc_00404050:
00404050: mov eax,[4067C4h]
00404055: cmp eax,1
00404058: je short 00404067h
0040405A: test eax,eax
0040405C: jne short 0040408Ch
0040405E: cmp dword ptr [4067C8h],1
00404065: jne short 0040408Ch
loc_00404067:
00404067: push 0FCh
0040406C: call 00404090h
00404071: add esp,4
00404074: mov eax,[406DA8h]
00404079: test eax,eax
0040407B: je short 0040407Fh
0040407D: call eax
loc_0040407F:
0040407F: push 0FFh
00404084: call 00404090h
00404089: add esp,4
loc_0040408C:
0040408C: ret
0040408D: int3
0040408E: int3
0040408F: int3
loc_00404090:
00404090: sub esp,1A8h
00404096: xor eax,eax
00404098: mov ecx,406D20h
0040409D: push ebx
0040409E: mov edx,[esp+1B0h]
004040A5: push esi
004040A6: push edi
004040A7: push ebp
loc_004040A8:
004040A8: cmp [ecx],edx
004040AA: je short 004040B8h
004040AC: add ecx,8
004040AF: inc eax
004040B0: cmp ecx,406DA8h
004040B6: jb short 004040A8h
loc_004040B8:
004040B8: cmp [eax*8+406D20h],edx
004040BF: lea ebx,[eax*8]
004040C6: jne 00404273h
004040CC: cmp dword ptr [4067C4h],1
004040D3: je 00404238h
004040D9: cmp dword ptr [4067C4h],0
004040E0: jne short 004040EFh
004040E2: cmp dword ptr [4067C8h],1
004040E9: je 00404238h
loc_004040EF:
004040EF: cmp edx,0FCh
004040F5: je 00404273h
004040FB: lea eax,[esp+0B4h]
00404102: push 104h
00404107: push eax
00404108: mov ebp,[4091D0h]
0040410E: push 0
00404110: call ebp
00404112: test eax,eax
00404114: jne short 0040412Ch
00404116: mov esi,406DF8h
0040411B: lea edi,[esp+0B4h]
00404122: mov ecx,5
00404127: rep movsd [edi],[esi]
00404129: movsw [edi],[esi]
0040412B: movsb [edi],[esi]
loc_0040412C:
0040412C: lea ebp,[esp+0B4h]
00404133: lea edi,[esp+0B4h]
0040413A: mov ecx,0FFFFFFFFh
0040413F: sub eax,eax
00404141: repne scasb [edi]
00404143: not ecx
00404145: cmp ecx,3Ch
00404148: jbe short 00404170h
0040414A: lea edi,[esp+0B4h]
00404151: mov ecx,0FFFFFFFFh
00404156: sub eax,eax
00404158: push 3
0040415A: repne scasb [edi]
0040415C: not ecx
0040415E: lea ebp,[esp+ecx+7Ch]
00404162: push 406DF4h
00404167: push ebp
00404168: call 00402410h
0040416D: add esp,0Ch
loc_00404170:
00404170: mov esi,406DD8h
00404175: lea edi,[esp+14h]
00404179: mov ecx,6
0040417E: rep movsd [edi],[esi]
00404180: movsw [edi],[esi]
00404182: mov edi,ebp
00404184: mov ecx,0FFFFFFFFh
00404189: sub eax,eax
0040418B: repne scasb [edi]
0040418D: not ecx
0040418F: sub edi,ecx
00404191: mov edx,ecx
00404193: mov esi,edi
00404195: mov ecx,0FFFFFFFFh
0040419A: lea edi,[esp+14h]
0040419E: sub eax,eax
004041A0: repne scasb [edi]
004041A2: dec edi
004041A3: mov ecx,edx
004041A5: shr ecx,2
004041A8: rep movsd [edi],[esi]
004041AA: mov ecx,edx
004041AC: and ecx,3
004041AF: rep movsb [edi],[esi]
004041B1: mov edi,406DD4h
004041B6: mov ecx,0FFFFFFFFh
004041BB: sub eax,eax
004041BD: repne scasb [edi]
004041BF: not ecx
004041C1: sub edi,ecx
004041C3: mov edx,ecx
004041C5: mov esi,edi
004041C7: mov ecx,0FFFFFFFFh
004041CC: lea edi,[esp+14h]
004041D0: sub eax,eax
004041D2: repne scasb [edi]
004041D4: dec edi
004041D5: mov ecx,edx
004041D7: shr ecx,2
004041DA: rep movsd [edi],[esi]
004041DC: mov ecx,edx
004041DE: and ecx,3
004041E1: rep movsb [edi],[esi]
004041E3: mov edi,[ebx+406D24h]
004041E9: mov ecx,0FFFFFFFFh
004041EE: sub eax,eax
004041F0: repne scasb [edi]
004041F2: not ecx
004041F4: sub edi,ecx
004041F6: mov esi,edi
004041F8: mov edx,ecx
004041FA: lea edi,[esp+14h]
004041FE: mov ecx,0FFFFFFFFh
00404203: sub eax,eax
00404205: repne scasb [edi]
00404207: dec edi
00404208: mov ecx,edx
0040420A: shr ecx,2
0040420D: rep movsd [edi],[esi]
0040420F: mov ecx,edx
00404211: push 12010h
00404216: and ecx,3
00404219: push 406DACh
0040421E: rep movsb [edi],[esi]
00404220: lea eax,[esp+1Ch]
00404224: push eax
00404225: call 00404910h
0040422A: add esp,0Ch
0040422D: pop ebp
0040422E: pop edi
0040422F: pop esi
00404230: pop ebx
00404231: add esp,1A8h
00404237: ret
loc_00404238:
00404238: mov eax,[408620h]
0040423D: mov esi,[eax+10h]
00404240: cmp esi,0FFFFFFFFh
00404243: jne short 0040424Fh
00404245: push 0FFFFFFF4h
00404247: call dword ptr [409234h]
0040424D: mov esi,eax
loc_0040424F:
0040424F: mov edx,[ebx+406D24h]
00404255: push 0
00404257: lea eax,[esp+14h]
0040425B: mov edi,edx
0040425D: push eax
0040425E: mov ecx,0FFFFFFFFh
00404263: sub eax,eax
00404265: repne scasb [edi]
00404267: not ecx
00404269: dec ecx
0040426A: push ecx
0040426B: push edx
0040426C: push esi
0040426D: call dword ptr [409228h]
loc_00404273:
00404273: pop ebp
00404274: pop edi
00404275: pop esi
00404276: pop ebx
00404277: add esp,1A8h
0040427D: ret
0040427E: int3
0040427F: int3
loc_00404280:
00404280: mov edx,[esp+4]
00404284: sub esp,418h
0040428A: cmp [408614h],edx
00404290: push ebx
00404291: push esi
00404292: push edi
00404293: push ebp
00404294: jbe 00404487h
0040429A: mov eax,edx
0040429C: and eax,0FFFFFFE7h
0040429F: sar eax,3
004042A2: add eax,408620h
004042A7: mov [esp+18h],eax
004042AB: mov eax,edx
004042AD: and eax,1Fh
004042B0: mov ecx,[esp+18h]
004042B4: shl eax,3
004042B7: mov ebx,[ecx]
004042B9: mov ebp,eax
004042BB: mov [esp+1Ch],eax
004042BF: mov al,[ebx+ebp+4]
004042C3: test al,1
004042C5: je 00404487h
004042CB: xor esi,esi
004042CD: mov ebp,[esp+434h]
004042D4: mov [esp+20h],esi
004042D8: cmp ebp,esi
004042DA: jne short 004042E9h
004042DC: xor eax,eax
004042DE: pop ebp
004042DF: pop edi
004042E0: pop esi
004042E1: pop ebx
004042E2: add esp,418h
004042E8: ret
loc_004042E9:
004042E9: test al,20h
004042EB: je short 004042FAh
004042ED: push 2
004042EF: push 0
004042F1: push edx
004042F2: call 004044B0h
004042F7: add esp,0Ch
loc_004042FA:
004042FA: mov eax,[esp+18h]
004042FE: mov ecx,[esp+1Ch]
00404302: add ecx,[eax]
00404304: test byte ptr [ecx+4],80h
00404308: je 00404396h
0040430E: mov dword ptr [esp+10h],0
00404316: mov edi,[esp+430h]
loc_0040431D:
0040431D: mov eax,edi
0040431F: sub eax,[esp+430h]
00404326: cmp eax,ebp
00404328: jae 004043CFh
0040432E: lea ebx,[esp+24h]
loc_00404332:
00404332: mov eax,edi
00404334: sub eax,[esp+430h]
0040433B: cmp eax,ebp
0040433D: jae short 0040435Dh
0040433F: mov al,[edi]
00404341: inc edi
00404342: cmp al,0Ah
00404344: jne short 0040434Bh
00404346: mov byte ptr [ebx],0Dh
00404349: inc esi
0040434A: inc ebx
loc_0040434B:
0040434B: mov [ebx],al
0040434D: inc ebx
0040434E: mov eax,ebx
00404350: lea ecx,[esp+24h]
00404354: sub eax,ecx
00404356: cmp eax,400h
0040435B: jl short 00404332h
loc_0040435D:
0040435D: lea eax,[esp+24h]
00404361: push 0
00404363: sub ebx,eax
00404365: lea ecx,[esp+28h]
00404369: lea eax,[esp+18h]
0040436D: mov edx,[esp+1Ch]
00404371: push eax
00404372: push ebx
00404373: push ecx
00404374: mov eax,[edx]
00404376: mov ecx,[esp+2Ch]
0040437A: mov edx,[eax+ecx]
0040437D: push edx
0040437E: call dword ptr [409228h]
00404384: test eax,eax
00404386: je short 004043C5h
00404388: mov eax,[esp+14h]
0040438C: add [esp+20h],eax
00404390: cmp eax,ebx
00404392: jge short 0040431Dh
00404394: jmp short 004043CFh
loc_00404396:
00404396: lea eax,[esp+14h]
0040439A: push 0
0040439C: mov edx,[esp+434h]
004043A3: push eax
004043A4: push ebp
004043A5: mov ecx,[ecx]
004043A7: push edx
004043A8: push ecx
004043A9: call dword ptr [409228h]
004043AF: test eax,eax
004043B1: je short 004043C5h
004043B3: mov dword ptr [esp+10h],0
004043BB: mov eax,[esp+14h]
004043BF: mov [esp+20h],eax
004043C3: jmp short 004043CFh
loc_004043C5:
004043C5: call dword ptr [4091B4h]
004043CB: mov [esp+10h],eax
loc_004043CF:
004043CF: cmp dword ptr [esp+20h],0
004043D4: jne 00404476h
004043DA: cmp dword ptr [esp+10h],0
004043DF: je short 00404428h
004043E1: cmp dword ptr [esp+10h],5
004043E6: jne short 0040440Bh
004043E8: mov dword ptr [406770h],9
004043F2: mov eax,[esp+10h]
004043F6: pop ebp
004043F7: pop edi
004043F8: mov [406774h],eax
004043FD: mov eax,0FFFFFFFFh
00404402: pop esi
00404403: pop ebx
00404404: add esp,418h
0040440A: ret
loc_0040440B:
0040440B: mov eax,[esp+10h]
0040440F: push eax
00404410: call 004049B0h
00404415: add esp,4
00404418: mov eax,0FFFFFFFFh
0040441D: pop ebp
0040441E: pop edi
0040441F: pop esi
00404420: pop ebx
00404421: add esp,418h
00404427: ret
loc_00404428:
00404428: mov eax,[esp+18h]
0040442C: mov ecx,[esp+1Ch]
00404430: mov edx,[eax]
00404432: test byte ptr [edx+ecx+4],40h
00404437: je short 00404452h
00404439: mov eax,[esp+430h]
00404440: cmp byte ptr [eax],1Ah
00404443: jne short 00404452h
00404445: xor eax,eax
00404447: pop ebp
00404448: pop edi
00404449: pop esi
0040444A: pop ebx
0040444B: add esp,418h
00404451: ret
loc_00404452:
00404452: mov eax,0FFFFFFFFh
00404457: pop ebp
00404458: mov dword ptr [406770h],1Ch
00404462: mov dword ptr [406774h],0
0040446C: pop edi
0040446D: pop esi
0040446E: pop ebx
0040446F: add esp,418h
00404475: ret
loc_00404476:
00404476: mov eax,[esp+20h]
0040447A: pop ebp
0040447B: sub eax,esi
0040447D: pop edi
0040447E: pop esi
0040447F: pop ebx
00404480: add esp,418h
00404486: ret
loc_00404487:
00404487: mov eax,0FFFFFFFFh
0040448C: pop ebp
0040448D: mov dword ptr [406770h],9
00404497: mov dword ptr [406774h],0
004044A1: pop edi
004044A2: pop esi
004044A3: pop ebx
004044A4: add esp,418h
004044AA: ret
004044AB: int3
004044AC: int3
004044AD: int3
004044AE: int3
004044AF: int3
loc_004044B0:
004044B0: mov ecx,[esp+4]
004044B4: push ebx
004044B5: push esi
004044B6: cmp ecx,[408614h]
004044BC: push edi
004044BD: jae 0040454Fh
004044C3: mov eax,ecx
004044C5: and eax,0FFFFFFE7h
004044C8: sar eax,3
004044CB: lea edi,[eax+408620h]
004044D1: mov eax,ecx
004044D3: and eax,1Fh
004044D6: mov edx,[edi]
004044D8: lea esi,[eax*8]
004044DF: test byte ptr [edx+esi+4],1
004044E4: je short 0040454Fh
004044E6: push ecx
004044E7: call 00404AB0h
004044EC: add esp,4
004044EF: cmp eax,0FFFFFFFFh
004044F2: jne short 00404507h
004044F4: mov eax,0FFFFFFFFh
004044F9: pop edi
004044FA: mov dword ptr [406770h],9
00404504: pop esi
00404505: pop ebx
00404506: ret
loc_00404507:
00404507: mov ecx,[esp+18h]
0040450B: mov edx,[esp+14h]
0040450F: push ecx
00404510: push 0
00404512: push edx
00404513: push eax
00404514: call dword ptr [40921Ch]
0040451A: cmp eax,0FFFFFFFFh
0040451D: mov ebx,eax
0040451F: mov eax,0
00404524: jne short 0040452Ch
00404526: call dword ptr [4091B4h]
loc_0040452C:
0040452C: test eax,eax
0040452E: je short 00404542h
00404530: push eax
00404531: call 004049B0h
00404536: add esp,4
00404539: mov eax,0FFFFFFFFh
0040453E: pop edi
0040453F: pop esi
00404540: pop ebx
00404541: ret
loc_00404542:
00404542: mov eax,[edi]
00404544: pop edi
00404545: and byte ptr [eax+esi+4],0FDh
0040454A: mov eax,ebx
0040454C: pop esi
0040454D: pop ebx
0040454E: ret
loc_0040454F:
0040454F: mov eax,0FFFFFFFFh
00404554: pop edi
00404555: mov dword ptr [406770h],9
0040455F: mov dword ptr [406774h],0
00404569: pop esi
0040456A: pop ebx
0040456B: ret
0040456C: int3
0040456D: int3
0040456E: int3
0040456F: int3
loc_00404570:
00404570: push esi
00404571: inc dword ptr [407090h]
00404577: mov esi,[esp+8]
0040457B: push 1000h
00404580: call 00404890h
00404585: add esp,4
00404588: mov [esi+8],eax
0040458B: test eax,eax
0040458D: je short 0040459Ch
0040458F: or dword ptr [esi+0Ch],8
00404593: mov dword ptr [esi+18h],1000h
0040459A: jmp short 004045ADh
loc_0040459C:
0040459C: or dword ptr [esi+0Ch],4
004045A0: lea eax,[esi+14h]
004045A3: mov [esi+8],eax
004045A6: mov dword ptr [esi+18h],2
004045AD: mov eax,[esi+8]
004045B0: mov [esi],eax
004045B2: mov dword ptr [esi+4],0
004045B9: pop esi
004045BA: ret
004045BB: int3
004045BC: int3
004045BD: int3
004045BE: int3
004045BF: int3
loc_004045C0:
004045C0: mov edx,[esp+4]
004045C4: cmp [408614h],edx
004045CA: ja short 004045CFh
004045CC: xor eax,eax
004045CE: ret
loc_004045CF:
004045CF: mov eax,edx
004045D1: and edx,1Fh
004045D4: and eax,0FFFFFFE7h
004045D7: sar eax,3
004045DA: mov ecx,[eax+408620h]
004045E0: xor eax,eax
004045E2: mov al,[ecx+edx*8+4]
004045E6: and eax,40h
004045E9: ret
004045EA: int3
004045EB: int3
004045EC: int3
004045ED: int3
004045EE: int3
004045EF: int3
004045F0: cmp dword ptr [407608h],0
004045F7: push esi
004045F8: jne short 00404606h
004045FA: mov dword ptr [407608h],200h
00404604: jmp short 00404619h
loc_00404606:
00404606: cmp dword ptr [407608h],14h
0040460D: jge short 00404619h
0040460F: mov dword ptr [407608h],14h
loc_00404619:
00404619: push 4
0040461B: mov eax,[407608h]
00404620: push eax
00404621: call 00404B00h
00404626: add esp,8
00404629: mov [40760Ch],eax
0040462E: test eax,eax
00404630: jne short 0040465Bh
00404632: mov dword ptr [407608h],14h
0040463C: push 4
0040463E: push 14h
00404640: call 00404B00h
00404645: add esp,8
00404648: mov [40760Ch],eax
0040464D: test eax,eax
0040464F: jne short 0040465Bh
00404651: push 1Ah
00404653: call 00402650h
00404658: add esp,4
loc_0040465B:
0040465B: mov ecx,406E10h
00404660: xor eax,eax
loc_00404662:
00404662: mov edx,[40760Ch]
00404668: add eax,4
0040466B: mov [edx+eax-4],ecx
0040466F: add ecx,20h
00404672: cmp eax,50h
00404675: jl short 00404662h
00404677: xor esi,esi
00404679: mov edx,406E20h
loc_0040467E:
0040467E: mov eax,esi
00404680: mov ecx,esi
00404682: and eax,0FFFFFFE7h
00404685: and ecx,1Fh
00404688: sar eax,3
0040468B: mov eax,[eax+408620h]
00404691: mov eax,[eax+ecx*8]
00404694: cmp eax,0FFFFFFFFh
00404697: je short 0040469Dh
00404699: test eax,eax
0040469B: jne short 004046A3h
loc_0040469D:
0040469D: mov dword ptr [edx],0FFFFFFFFh
loc_004046A3:
004046A3: add edx,20h
004046A6: inc esi
004046A7: cmp edx,406E80h
004046AD: jb short 0040467Eh
004046AF: pop esi
004046B0: ret
004046B1: int3
004046B2: int3
004046B3: int3
004046B4: int3
004046B5: int3
004046B6: int3
004046B7: int3
004046B8: int3
004046B9: int3
004046BA: int3
004046BB: int3
004046BC: int3
004046BD: int3
004046BE: int3
004046BF: int3
004046C0: call 00404CA0h
004046C5: cmp byte ptr [4067B0h],0
004046CC: je short 004046D3h
004046CE: jmp 00404B60h
loc_004046D3:
004046D3: ret
004046D4: int3
004046D5: int3
004046D6: int3
004046D7: int3
004046D8: int3
004046D9: int3
004046DA: int3
004046DB: int3
004046DC: int3
004046DD: int3
004046DE: int3
004046DF: int3
loc_004046E0:
004046E0: mov ecx,[esp+4]
004046E4: sub esp,4
004046E7: test ecx,ecx
004046E9: jne short 004046F1h
004046EB: xor eax,eax
004046ED: add esp,4
004046F0: ret
loc_004046F1:
004046F1: cmp dword ptr [4072C8h],0
004046F8: jne short 00404725h
004046FA: cmp word ptr [esp+0Ch],0FFh
00404701: jbe short 00404716h
00404703: mov eax,0FFFFFFFFh
00404708: add esp,4
0040470B: mov dword ptr [406770h],2Ah
00404715: ret
loc_00404716:
00404716: mov al,[esp+0Ch]
0040471A: mov [ecx],al
0040471C: mov eax,1
00404721: add esp,4
00404724: ret
loc_00404725:
00404725: lea eax,[esp]
00404729: mov edx,[407490h]
0040472F: mov dword ptr [esp],0
00404737: push eax
00404738: push 0
0040473A: push edx
0040473B: mov eax,[4072D8h]
00404740: push ecx
00404741: push 1
00404743: lea ecx,[esp+20h]
00404747: push ecx
00404748: push 220h
0040474D: push eax
0040474E: call dword ptr [4091F8h]
00404754: test eax,eax
00404756: je short 0040475Fh
00404758: cmp dword ptr [esp],0
0040475D: je short 0040476Eh
loc_0040475F:
0040475F: mov dword ptr [406770h],2Ah
00404769: mov eax,0FFFFFFFFh
loc_0040476E:
0040476E: add esp,4
00404771: ret
00404772: int3
00404773: int3
00404774: int3
00404775: int3
00404776: int3
00404777: int3
00404778: int3
00404779: int3
0040477A: int3
0040477B: int3
0040477C: int3
0040477D: int3
0040477E: int3
0040477F: int3
loc_00404780:
00404780: push ebx
00404781: push esi
00404782: mov eax,[esp+18h]
00404786: or eax,eax
00404788: jne short 004047A2h
0040478A: mov ecx,[esp+14h]
0040478E: mov eax,[esp+10h]
00404792: xor edx,edx
00404794: div ecx
00404796: mov ebx,eax
00404798: mov eax,[esp+0Ch]
0040479C: div ecx
0040479E: mov edx,ebx
004047A0: jmp short 004047E3h
loc_004047A2:
004047A2: mov ecx,eax
004047A4: mov ebx,[esp+14h]
004047A8: mov edx,[esp+10h]
004047AC: mov eax,[esp+0Ch]
loc_004047B0:
004047B0: shr ecx,1
004047B2: rcr ebx,1
004047B4: shr edx,1
004047B6: rcr eax,1
004047B8: or ecx,ecx
004047BA: jne short 004047B0h
004047BC: div ebx
004047BE: mov esi,eax
004047C0: mul dword ptr [esp+18h]
004047C4: mov ecx,eax
004047C6: mov eax,[esp+14h]
004047CA: mul esi
004047CC: add edx,ecx
004047CE: jb short 004047DEh
004047D0: cmp edx,[esp+10h]
004047D4: ja short 004047DEh
004047D6: jb short 004047DFh
004047D8: cmp eax,[esp+0Ch]
004047DC: jbe short 004047DFh
loc_004047DE:
004047DE: dec esi
loc_004047DF:
004047DF: xor edx,edx
004047E1: mov eax,esi
004047E3: pop esi
004047E4: pop ebx
004047E5: ret 10h
004047E8: int3
004047E9: int3
004047EA: int3
004047EB: int3
004047EC: int3
004047ED: int3
004047EE: int3
004047EF: int3
loc_004047F0:
004047F0: push ebx
004047F1: mov eax,[esp+14h]
004047F5: or eax,eax
004047F7: jne short 00404811h
004047F9: mov ecx,[esp+10h]
004047FD: mov eax,[esp+0Ch]
00404801: xor edx,edx
00404803: div ecx
00404805: mov eax,[esp+8]
00404809: div ecx
0040480B: mov eax,edx
0040480D: xor edx,edx
0040480F: jmp short 00404861h
loc_00404811:
00404811: mov ecx,eax
00404813: mov ebx,[esp+10h]
00404817: mov edx,[esp+0Ch]
0040481B: mov eax,[esp+8]
loc_0040481F:
0040481F: shr ecx,1
00404821: rcr ebx,1
00404823: shr edx,1
00404825: rcr eax,1
00404827: or ecx,ecx
00404829: jne short 0040481Fh
0040482B: div ebx
0040482D: mov ecx,eax
0040482F: mul dword ptr [esp+14h]
00404833: xchg ecx,eax
00404834: mul dword ptr [esp+10h]
00404838: add edx,ecx
0040483A: jb short 0040484Ah
0040483C: cmp edx,[esp+0Ch]
00404840: ja short 0040484Ah
00404842: jb short 00404852h
00404844: cmp eax,[esp+8]
00404848: jbe short 00404852h
loc_0040484A:
0040484A: sub eax,[esp+10h]
0040484E: sbb edx,[esp+14h]
loc_00404852:
00404852: sub eax,[esp+8]
00404856: sbb edx,[esp+0Ch]
0040485A: neg edx
0040485C: neg eax
0040485E: sbb edx,0
00404861: pop ebx
00404862: ret 10h
00404865: int3
00404866: int3
00404867: int3
00404868: int3
00404869: int3
0040486A: int3
0040486B: int3
0040486C: int3
0040486D: int3
0040486E: int3
0040486F: int3
loc_00404870:
00404870: mov eax,[esp+4]
00404874: test eax,eax
00404876: je short 00404887h
00404878: push eax
00404879: push 0
0040487B: mov eax,[408610h]
00404880: push eax
00404881: call dword ptr [409218h]
loc_00404887:
00404887: ret
00404888: int3
00404889: int3
0040488A: int3
0040488B: int3
0040488C: int3
0040488D: int3
0040488E: int3
0040488F: int3
loc_00404890:
00404890: mov eax,[40749Ch]
00404895: mov ecx,[esp+4]
00404899: push eax
0040489A: push ecx
0040489B: call 004048B0h
004048A0: add esp,8
004048A3: ret
004048A4: int3
004048A5: int3
004048A6: int3
004048A7: int3
004048A8: int3
004048A9: int3
004048AA: int3
004048AB: int3
004048AC: int3
004048AD: int3
004048AE: int3
004048AF: int3
loc_004048B0:
004048B0: push esi
004048B1: push edi
004048B2: mov esi,[esp+0Ch]
004048B6: cmp esi,0FFFFFFE0h
004048B9: jbe short 004048C0h
004048BB: xor eax,eax
004048BD: pop edi
004048BE: pop esi
004048BF: ret
loc_004048C0:
004048C0: test esi,esi
004048C2: jne short 004048C9h
004048C4: mov esi,1
loc_004048C9:
004048C9: mov edi,[esp+10h]
loc_004048CD:
004048CD: push esi
004048CE: call 004048F0h
004048D3: add esp,4
004048D6: test eax,eax
004048D8: jne short 004048EDh
004048DA: test edi,edi
004048DC: je short 004048EDh
004048DE: push esi
004048DF: call 00404D30h
004048E4: add esp,4
004048E7: test eax,eax
004048E9: jne short 004048CDh
004048EB: xor eax,eax
loc_004048ED:
004048ED: pop edi
004048EE: pop esi
004048EF: ret
loc_004048F0:
004048F0: mov eax,[esp+4]
004048F4: mov ecx,[408610h]
004048FA: push eax
004048FB: push 0
004048FD: push ecx
004048FE: call dword ptr [409214h]
00404904: ret
00404905: int3
00404906: int3
00404907: int3
00404908: int3
00404909: int3
0040490A: int3
0040490B: int3
0040490C: int3
0040490D: int3
0040490E: int3
0040490F: int3
loc_00404910:
00404910: push ebx
00404911: push esi
00404912: push edi
00404913: xor esi,esi
00404915: cmp [4072DCh],esi
0040491B: jne short 0040495Fh
0040491D: push 407318h
00404922: call dword ptr [40920Ch]
00404928: mov ebx,eax
0040492A: test ebx,ebx
0040492C: je short 0040499Ch
0040492E: push 40730Ch
00404933: mov edi,[409210h]
00404939: push ebx
0040493A: call edi
0040493C: mov [4072DCh],eax
00404941: test eax,eax
00404943: je short 0040499Ch
00404945: push 4072FCh
0040494A: push ebx
0040494B: call edi
0040494D: push 4072E8h
00404952: mov [4072E0h],eax
00404957: push ebx
00404958: call edi
0040495A: mov [4072E4h],eax
loc_0040495F:
0040495F: mov eax,[4072E0h]
00404964: test eax,eax
00404966: je short 0040496Ch
00404968: call eax
0040496A: mov esi,eax
loc_0040496C:
0040496C: test esi,esi
0040496E: je short 00404982h
00404970: cmp dword ptr [4072E4h],0
00404977: je short 00404982h
00404979: push esi
0040497A: call dword ptr [4072E4h]
00404980: mov esi,eax
loc_00404982:
00404982: mov eax,[esp+18h]
00404986: mov ecx,[esp+14h]
0040498A: mov edx,[esp+10h]
0040498E: push eax
0040498F: push ecx
00404990: push edx
00404991: push esi
00404992: call dword ptr [4072DCh]
00404998: pop edi
00404999: pop esi
0040499A: pop ebx
0040499B: ret
loc_0040499C:
0040499C: xor eax,eax
0040499E: pop edi
0040499F: pop esi
004049A0: pop ebx
004049A1: ret
004049A2: int3
004049A3: int3
004049A4: int3
004049A5: int3
004049A6: int3
004049A7: int3
004049A8: int3
004049A9: int3
004049AA: int3
004049AB: int3
004049AC: int3
004049AD: int3
004049AE: int3
004049AF: int3
loc_004049B0:
004049B0: mov eax,[esp+4]
004049B4: xor ecx,ecx
004049B6: mov edx,407328h
004049BB: mov [406774h],eax
loc_004049C0:
004049C0: cmp [edx],eax
004049C2: je short 004049E5h
004049C4: add edx,8
004049C7: inc ecx
004049C8: cmp edx,407490h
004049CE: jb short 004049C0h
004049D0: cmp eax,13h
004049D3: jb short 004049F2h
004049D5: cmp eax,24h
004049D8: ja short 004049F2h
004049DA: mov dword ptr [406770h],0Dh
004049E4: ret
loc_004049E5:
004049E5: mov eax,[ecx*8+40732Ch]
004049EC: mov [406770h],eax
004049F1: ret
loc_004049F2:
004049F2: cmp eax,0BCh
004049F7: jb short 00404A0Bh
004049F9: cmp eax,0CAh
004049FE: ja short 00404A0Bh
00404A00: mov dword ptr [406770h],8
00404A0A: ret
loc_00404A0B:
00404A0B: mov dword ptr [406770h],16h
00404A15: ret
00404A16: int3
00404A17: int3
00404A18: int3
00404A19: int3
00404A1A: int3
00404A1B: int3
00404A1C: int3
00404A1D: int3
00404A1E: int3
00404A1F: int3
loc_00404A20:
00404A20: mov ecx,[esp+4]
00404A24: push esi
00404A25: push edi
00404A26: cmp [408614h],ecx
00404A2C: jbe short 00404A94h
00404A2E: mov eax,ecx
00404A30: and eax,0FFFFFFE7h
00404A33: sar eax,3
00404A36: lea edi,[eax+408620h]
00404A3C: mov eax,ecx
00404A3E: and eax,1Fh
00404A41: lea esi,[eax*8]
00404A48: mov eax,[edi]
00404A4A: add eax,esi
00404A4C: test byte ptr [eax+4],1
00404A50: je short 00404A94h
00404A52: cmp dword ptr [eax],0FFFFFFFFh
00404A55: je short 00404A94h
00404A57: cmp dword ptr [4067C8h],1
00404A5E: jne short 00404A86h
00404A60: test ecx,ecx
00404A62: je short 00404A70h
00404A64: cmp ecx,1
00404A67: je short 00404A76h
00404A69: cmp ecx,2
00404A6C: je short 00404A7Ch
00404A6E: jmp short 00404A86h
loc_00404A70:
00404A70: push 0
00404A72: push 0FFFFFFF6h
00404A74: jmp short 00404A80h
loc_00404A76:
00404A76: push 0
00404A78: push 0FFFFFFF5h
00404A7A: jmp short 00404A80h
loc_00404A7C:
00404A7C: push 0
00404A7E: push 0FFFFFFF4h
00404A80: call dword ptr [409208h]
loc_00404A86:
00404A86: mov eax,[edi]
00404A88: pop edi
00404A89: mov dword ptr [eax+esi],0FFFFFFFFh
00404A90: xor eax,eax
00404A92: pop esi
00404A93: ret
loc_00404A94:
00404A94: mov eax,0FFFFFFFFh
00404A99: pop edi
00404A9A: mov dword ptr [406770h],9
00404AA4: mov dword ptr [406774h],0
00404AAE: pop esi
00404AAF: ret
loc_00404AB0:
00404AB0: mov ecx,[esp+4]
00404AB4: cmp [408614h],ecx
00404ABA: jbe short 00404AD9h
00404ABC: mov eax,ecx
00404ABE: and ecx,1Fh
00404AC1: and eax,0FFFFFFE7h
00404AC4: sar eax,3
00404AC7: mov edx,[eax+408620h]
00404ACD: lea eax,[edx+ecx*8]
00404AD0: test byte ptr [eax+4],1
00404AD4: je short 00404AD9h
00404AD6: mov eax,[eax]
00404AD8: ret
loc_00404AD9:
00404AD9: mov dword ptr [406770h],9
00404AE3: mov dword ptr [406774h],0
00404AED: mov eax,0FFFFFFFFh
00404AF2: ret
00404AF3: int3
00404AF4: int3
00404AF5: int3
00404AF6: int3
00404AF7: int3
00404AF8: int3
00404AF9: int3
00404AFA: int3
00404AFB: int3
00404AFC: int3
00404AFD: int3
00404AFE: int3
00404AFF: int3
loc_00404B00:
00404B00: push esi
00404B01: push edi
00404B02: mov edi,[esp+0Ch]
00404B06: imul edi,[esp+10h]
00404B0B: test edi,edi
00404B0D: jne short 00404B14h
00404B0F: mov edi,1
loc_00404B14:
00404B14: mov esi,[409214h]
00404B1A: mov eax,[408610h]
loc_00404B1F:
00404B1F: cmp edi,0FFFFFFE0h
00404B22: jbe short 00404B28h
00404B24: xor eax,eax
00404B26: jmp short 00404B2Eh
loc_00404B28:
00404B28: push edi
00404B29: push 8
00404B2B: push eax
00404B2C: call esi
00404B2E: test eax,eax
00404B30: jne short 00404B4Fh
00404B32: cmp dword ptr [40749Ch],0
00404B39: je short 00404B4Fh
00404B3B: push edi
00404B3C: call 00404D30h
00404B41: add esp,4
00404B44: test eax,eax
00404B46: mov eax,[408610h]
00404B4B: jne short 00404B1Fh
00404B4D: xor eax,eax
loc_00404B4F:
00404B4F: pop edi
00404B50: pop esi
00404B51: ret
00404B52: int3
00404B53: int3
00404B54: int3
00404B55: int3
00404B56: int3
00404B57: int3
00404B58: int3
00404B59: int3
00404B5A: int3
00404B5B: int3
00404B5C: int3
00404B5D: int3
00404B5E: int3
00404B5F: int3
00404B60: push ebx
00404B61: push esi
00404B62: push edi
00404B63: xor esi,esi
00404B65: push ebp
00404B66: mov edi,3
00404B6B: cmp [407608h],edi
00404B71: jle short 00404BCCh
00404B73: mov ebx,0Ch
00404B78: mov ebp,83h
loc_00404B7D:
00404B7D: mov eax,[40760Ch]
00404B82: mov eax,[eax+ebx]
00404B85: test eax,eax
00404B87: je short 00404BC0h
00404B89: test [eax+0Ch],ebp
00404B8C: je short 00404B9Dh
00404B8E: push eax
00404B8F: call 00404D60h
00404B94: add esp,4
00404B97: cmp eax,0FFFFFFFFh
00404B9A: je short 00404B9Dh
00404B9C: inc esi
loc_00404B9D:
00404B9D: cmp ebx,50h
00404BA0: jl short 00404BC0h
00404BA2: mov eax,[40760Ch]
00404BA7: mov ecx,[eax+ebx]
00404BAA: push ecx
00404BAB: call 00404870h
00404BB0: add esp,4
00404BB3: mov ecx,[40760Ch]
00404BB9: mov dword ptr [ecx+ebx],0
loc_00404BC0:
00404BC0: add ebx,4
00404BC3: inc edi
00404BC4: cmp [407608h],edi
00404BCA: jg short 00404B7Dh
loc_00404BCC:
00404BCC: mov eax,esi
00404BCE: pop ebp
00404BCF: pop edi
00404BD0: pop esi
00404BD1: pop ebx
00404BD2: ret
00404BD3: int3
00404BD4: int3
00404BD5: int3
00404BD6: int3
00404BD7: int3
00404BD8: int3
00404BD9: int3
00404BDA: int3
00404BDB: int3
00404BDC: int3
00404BDD: int3
00404BDE: int3
00404BDF: int3
loc_00404BE0:
00404BE0: push esi
00404BE1: mov esi,[esp+8]
00404BE5: test esi,esi
00404BE7: jne short 00404BF5h
00404BE9: push 0
00404BEB: call 00404CB0h
00404BF0: add esp,4
00404BF3: pop esi
00404BF4: ret
loc_00404BF5:
00404BF5: push esi
00404BF6: call 00404C30h
00404BFB: add esp,4
00404BFE: test eax,eax
00404C00: je short 00404C09h
00404C02: mov eax,0FFFFFFFFh
00404C07: pop esi
00404C08: ret
loc_00404C09:
00404C09: test byte ptr [esi+0Dh],40h
00404C0D: je short 00404C28h
00404C0F: mov eax,[esi+10h]
00404C12: push eax
00404C13: call 00404DD0h
00404C18: add esp,4
00404C1B: cmp eax,1
00404C1E: mov eax,0
00404C23: pop esi
00404C24: adc eax,0FFFFFFFFh
00404C27: ret
loc_00404C28:
00404C28: xor eax,eax
00404C2A: pop esi
00404C2B: ret
00404C2C: int3
00404C2D: int3
00404C2E: int3
00404C2F: int3
loc_00404C30:
00404C30: push ebx
00404C31: push esi
00404C32: mov esi,[esp+0Ch]
00404C36: push edi
00404C37: xor edi,edi
00404C39: mov eax,[esi+0Ch]
00404C3C: mov ecx,eax
00404C3E: and cl,3
00404C41: cmp cl,2
00404C44: jne short 00404C82h
00404C46: test eax,108h
00404C4B: je short 00404C82h
00404C4D: mov eax,[esi+8]
00404C50: mov ebx,[esi]
00404C52: sub ebx,eax
00404C54: test ebx,ebx
00404C56: jle short 00404C82h
00404C58: push ebx
00404C59: push eax
00404C5A: mov eax,[esi+10h]
00404C5D: push eax
00404C5E: call 00404280h
00404C63: add esp,0Ch
00404C66: cmp eax,ebx
00404C68: jne short 00404C79h
00404C6A: mov eax,[esi+0Ch]
00404C6D: test al,80h
00404C6F: je short 00404C82h
00404C71: and eax,0FFFFFFFDh
00404C74: mov [esi+0Ch],eax
00404C77: jmp short 00404C82h
loc_00404C79:
00404C79: or dword ptr [esi+0Ch],20h
00404C7D: mov edi,0FFFFFFFFh
loc_00404C82:
00404C82: mov eax,[esi+8]
00404C85: mov [esi],eax
00404C87: mov eax,edi
00404C89: mov dword ptr [esi+4],0
00404C90: pop edi
00404C91: pop esi
00404C92: pop ebx
00404C93: ret
00404C94: int3
00404C95: int3
00404C96: int3
00404C97: int3
00404C98: int3
00404C99: int3
00404C9A: int3
00404C9B: int3
00404C9C: int3
00404C9D: int3
00404C9E: int3
00404C9F: int3
loc_00404CA0:
00404CA0: push 1
00404CA2: call 00404CB0h
00404CA7: add esp,4
00404CAA: ret
00404CAB: int3
00404CAC: int3
00404CAD: int3
00404CAE: int3
00404CAF: int3
loc_00404CB0:
00404CB0: push ebx
00404CB1: push esi
00404CB2: push edi
00404CB3: xor ebx,ebx
00404CB5: push ebp
00404CB6: xor esi,esi
00404CB8: xor ebp,ebp
00404CBA: mov edi,[esp+14h]
loc_00404CBE:
00404CBE: mov eax,[40760Ch]
00404CC3: mov ecx,[eax+esi]
00404CC6: test ecx,ecx
00404CC8: je short 00404D02h
00404CCA: mov eax,[ecx+0Ch]
00404CCD: test al,83h
00404CCF: je short 00404D02h
00404CD1: cmp edi,1
00404CD4: jne short 00404CE7h
00404CD6: push ecx
00404CD7: call 00404BE0h
00404CDC: add esp,4
00404CDF: cmp eax,0FFFFFFFFh
00404CE2: je short 00404D02h
00404CE4: inc ebx
00404CE5: jmp short 00404D02h
loc_00404CE7:
00404CE7: test edi,edi
00404CE9: jne short 00404D02h
00404CEB: test al,2
00404CED: je short 00404D02h
00404CEF: push ecx
00404CF0: call 00404BE0h
00404CF5: add esp,4
00404CF8: cmp eax,0FFFFFFFFh
00404CFB: jne short 00404D02h
00404CFD: mov ebp,0FFFFFFFFh
loc_00404D02:
00404D02: add esi,4
00404D05: cmp esi,800h
00404D0B: jl short 00404CBEh
00404D0D: mov eax,ebx
00404D0F: cmp edi,1
00404D12: je short 00404D16h
00404D14: mov eax,ebp
loc_00404D16:
00404D16: pop ebp
00404D17: pop edi
00404D18: pop esi
00404D19: pop ebx
00404D1A: ret
00404D1B: int3
00404D1C: int3
00404D1D: int3
00404D1E: int3
00404D1F: int3
00404D20: push 2
00404D22: call 00402650h
00404D27: add esp,4
00404D2A: ret
00404D2B: int3
00404D2C: int3
00404D2D: int3
00404D2E: int3
00404D2F: int3
loc_00404D30:
00404D30: mov ecx,[407604h]
00404D36: test ecx,ecx
00404D38: je short 00404D4Eh
00404D3A: mov eax,[esp+4]
00404D3E: push eax
00404D3F: call ecx
00404D41: add esp,4
00404D44: test eax,eax
00404D46: je short 00404D4Eh
00404D48: mov eax,1
00404D4D: ret
loc_00404D4E:
00404D4E: xor eax,eax
00404D50: ret
00404D51: int3
00404D52: int3
00404D53: int3
00404D54: int3
00404D55: int3
00404D56: int3
00404D57: int3
00404D58: int3
00404D59: int3
00404D5A: int3
00404D5B: int3
00404D5C: int3
00404D5D: int3
00404D5E: int3
00404D5F: int3
loc_00404D60:
00404D60: push esi
00404D61: push edi
00404D62: mov edi,0FFFFFFFFh
00404D67: mov esi,[esp+0Ch]
00404D6B: mov eax,[esi+0Ch]
00404D6E: test al,40h
00404D70: je short 00404D7Eh
00404D72: mov dword ptr [esi+0Ch],0
00404D79: mov eax,edi
00404D7B: pop edi
00404D7C: pop esi
00404D7D: ret
loc_00404D7E:
00404D7E: test al,83h
00404D80: je short 00404DC4h
00404D82: push esi
00404D83: call 00404C30h
00404D88: add esp,4
00404D8B: mov edi,eax
00404D8D: push esi
00404D8E: call 00404F10h
00404D93: add esp,4
00404D96: mov eax,[esi+10h]
00404D99: push eax
00404D9A: call 00404E40h
00404D9F: add esp,4
00404DA2: test eax,eax
00404DA4: jge short 00404DADh
00404DA6: mov edi,0FFFFFFFFh
00404DAB: jmp short 00404DC4h
loc_00404DAD:
00404DAD: mov eax,[esi+1Ch]
00404DB0: test eax,eax
00404DB2: je short 00404DC4h
00404DB4: push eax
00404DB5: call 00404870h
00404DBA: mov dword ptr [esi+1Ch],0
00404DC1: add esp,4
loc_00404DC4:
00404DC4: mov dword ptr [esi+0Ch],0
00404DCB: mov eax,edi
00404DCD: pop edi
00404DCE: pop esi
00404DCF: ret
loc_00404DD0:
00404DD0: mov edx,[esp+4]
00404DD4: cmp edx,[408614h]
00404DDA: jae short 00404E2Fh
00404DDC: mov eax,edx
00404DDE: mov ecx,edx
00404DE0: and eax,0FFFFFFE7h
00404DE3: and ecx,1Fh
00404DE6: sar eax,3
00404DE9: mov eax,[eax+408620h]
00404DEF: mov al,[eax+ecx*8+4]
00404DF3: and al,1
00404DF5: je short 00404E2Fh
00404DF7: push edx
00404DF8: call 00404AB0h
00404DFD: add esp,4
00404E00: push eax
00404E01: call dword ptr [409220h]
00404E07: test eax,eax
00404E09: mov eax,0
00404E0E: jne short 00404E16h
00404E10: call dword ptr [4091B4h]
loc_00404E16:
00404E16: test eax,eax
00404E18: je short 00404E3Eh
00404E1A: mov dword ptr [406770h],9
00404E24: mov [406774h],eax
00404E29: mov eax,0FFFFFFFFh
00404E2E: ret
loc_00404E2F:
00404E2F: mov dword ptr [406770h],9
00404E39: mov eax,0FFFFFFFFh
loc_00404E3E:
00404E3E: ret
00404E3F: int3
loc_00404E40:
00404E40: push ebx
00404E41: push esi
00404E42: push edi
00404E43: push ebp
00404E44: mov edi,[esp+14h]
00404E48: cmp [408614h],edi
00404E4E: jbe 00404EE9h
00404E54: mov eax,edi
00404E56: and eax,0FFFFFFE7h
00404E59: sar eax,3
00404E5C: lea ebx,[eax+408620h]
00404E62: mov eax,edi
00404E64: and eax,1Fh
00404E67: mov ecx,[ebx]
00404E69: lea esi,[eax*8]
00404E70: test byte ptr [ecx+esi+4],1
00404E75: je short 00404EE9h
00404E77: cmp edi,1
00404E7A: je short 00404E81h
00404E7C: cmp edi,2
00404E7F: jne short 00404E9Bh
loc_00404E81:
00404E81: push 1
00404E83: call 00404AB0h
00404E88: add esp,4
00404E8B: mov ebp,eax
00404E8D: push 2
00404E8F: call 00404AB0h
00404E94: add esp,4
00404E97: cmp ebp,eax
00404E99: je short 00404EB9h
loc_00404E9B:
00404E9B: push edi
00404E9C: call 00404AB0h
00404EA1: add esp,4
00404EA4: push eax
00404EA5: call dword ptr [409224h]
00404EAB: test eax,eax
00404EAD: jne short 00404EB9h
00404EAF: call dword ptr [4091B4h]
00404EB5: mov ebp,eax
00404EB7: jmp short 00404EBBh
loc_00404EB9:
00404EB9: xor ebp,ebp
00404EBB: push edi
00404EBC: call 00404A20h
00404EC1: add esp,4
00404EC4: test ebp,ebp
00404EC6: je short 00404EDBh
00404EC8: push ebp
00404EC9: call 004049B0h
00404ECE: add esp,4
00404ED1: mov eax,0FFFFFFFFh
00404ED6: pop ebp
00404ED7: pop edi
00404ED8: pop esi
00404ED9: pop ebx
00404EDA: ret
loc_00404EDB:
00404EDB: mov eax,[ebx]
00404EDD: pop ebp
00404EDE: pop edi
00404EDF: mov byte ptr [eax+esi+4],0
00404EE4: xor eax,eax
00404EE6: pop esi
00404EE7: pop ebx
00404EE8: ret
loc_00404EE9:
00404EE9: mov eax,0FFFFFFFFh
00404EEE: pop ebp
00404EEF: mov dword ptr [406770h],9
00404EF9: mov dword ptr [406774h],0
00404F03: pop edi
00404F04: pop esi
00404F05: pop ebx
00404F06: ret
00404F07: int3
00404F08: int3
00404F09: int3
00404F0A: int3
00404F0B: int3
00404F0C: int3
00404F0D: int3
00404F0E: int3
00404F0F: int3
loc_00404F10:
00404F10: push esi
00404F11: mov esi,[esp+8]
00404F15: mov eax,[esi+0Ch]
00404F18: test al,83h
00404F1A: je short 00404F47h
00404F1C: test al,8
00404F1E: je short 00404F47h
00404F20: mov eax,[esi+8]
00404F23: push eax
00404F24: call 00404870h
00404F29: add esp,4
00404F2C: mov dword ptr [esi],0
00404F32: and dword ptr [esi+0Ch],0FFFFFBF7h
00404F39: mov dword ptr [esi+8],0
00404F40: mov dword ptr [esi+4],0
loc_00404F47:
00404F47: pop esi
00404F48: ret
00404F49: int3
00404F4A: int3
00404F4B: int3
00404F4C: int3
00404F4D: int3
00404F4E: int3
00404F4F: int3
00404F50: push ebp
00404F51: mov ebp,esp
00404F53: push edi
00404F54: push esi
00404F55: mov edi,[ebp+8]
00404F58: mov edx,edi
00404F5A: xor eax,eax
00404F5C: or ecx,0FFFFFFFFh
00404F5F: repne scasb [edi]
00404F61: dec edi
00404F62: mov esi,edi
00404F64: mov edi,[ebp+0Ch]
00404F67: push edi
00404F68: mov ecx,[ebp+10h]
00404F6B: repne scasb [edi]
00404F6D: jne short 00404F70h
00404F6F: inc ecx
loc_00404F70:
00404F70: sub ecx,[ebp+10h]
00404F73: neg ecx
00404F75: mov edi,esi
00404F77: pop esi
00404F78: rep movsb [edi],[esi]
00404F7A: stosb [edi]
00404F7B: mov eax,edx
00404F7D: pop esi
00404F7E: pop edi
00404F7F: leave
00404F80: ret
00404F81: int3
loc_00404F82:
00404F82: jmp dword ptr [409204h]
00404F88: add [eax],al
00404F8A: add [eax],al
00404F8C: add [eax],al
00404F8E: add [eax],al
00404F90: add [eax],al
00404F92: add [eax],al
00404F94: add [eax],al
00404F96: add [eax],al
00404F98: add [eax],al
00404F9A: add [eax],al
00404F9C: add [eax],al
00404F9E: add [eax],al
00404FA0: add [eax],al
00404FA2: add [eax],al
00404FA4: add [eax],al
00404FA6: add [eax],al
00404FA8: add [eax],al
00404FAA: add [eax],al
00404FAC: add [eax],al
00404FAE: add [eax],al
00404FB0: add [eax],al
00404FB2: add [eax],al
00404FB4: add [eax],al
00404FB6: add [eax],al
00404FB8: add [eax],al
00404FBA: add [eax],al
00404FBC: add [eax],al
00404FBE: add [eax],al
00404FC0: add [eax],al
00404FC2: add [eax],al
00404FC4: add [eax],al
00404FC6: add [eax],al
00404FC8: add [eax],al
00404FCA: add [eax],al
00404FCC: add [eax],al
00404FCE: add [eax],al
00404FD0: add [eax],al
00404FD2: add [eax],al
00404FD4: add [eax],al
00404FD6: add [eax],al
00404FD8: add [eax],al
00404FDA: add [eax],al
00404FDC: add [eax],al
00404FDE: add [eax],al
00404FE0: add [eax],al
00404FE2: add [eax],al
00404FE4: add [eax],al
00404FE6: add [eax],al
00404FE8: add [eax],al
00404FEA: add [eax],al
00404FEC: add [eax],al
00404FEE: add [eax],al
00404FF0: add [eax],al
00404FF2: add [eax],al
00404FF4: add [eax],al
00404FF6: add [eax],al
00404FF8: add [eax],al
00404FFA: add [eax],al
00404FFC: add [eax],al
00404FFE: add [eax],al
