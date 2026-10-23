# String triage

Strings below are pulled from non-code PE data sections. Their presence locates behavior clues but does not, on its own, prove call flow.

## game

### `Software\Sierra` (1)

- RVA `0x6140`: `Software\Sierra On-Line\Viper Racing`

### `DataDirectory` (1)

- RVA `0x6168`: `DataDirectory`

### `CDDirectory` (1)

- RVA `0x6178`: `CDDirectory`

### `race.bin` (1)

- RVA `0x6218`: `race.bin`

### `common.res` (1)

- RVA `0x61F0`: `common.res`

### `Data\` (2)

- RVA `0x626C`: `Data\`
- RVA `0x656C`: `%sdata\%s`

### `texture` (1)

- RVA `0x6308`: `Your video card does not support 3D texture mapping`

### `Can't find` (1)

- RVA `0x627C`: `Can't find data directory--Game may not be installed`

### `requires` (4)

- RVA `0x62B4`: `This game requires DirectX 5 or 6`
- RVA `0x6458`: `%s requires %d megabytes of memory (you have %d).`
- RVA `0x64C4`: `%s requires %d megabytes of virtual memory (you have %d).  Please free up disk space or close other applications.`
- RVA `0x65D8`: `This product requires DirectX 5.0 or better`

### `CD inserted` (1)

- RVA `0x6578`: `You must have the %s CD inserted to play.`

### `Viper Racing` (6)

- RVA `0x6140`: `Software\Sierra On-Line\Viper Racing`
- RVA `0x6194`: `MGI Viper Racing 1998`
- RVA `0x61AC`: `MGI Viper Racing 1998 Canary`
- RVA `0x61CC`: `Viper Racing Window`
- RVA `0x61E0`: `Viper Racing`
- RVA `0x61FC`: `Viper Racing`

## engine

### `Software\Sierra` (2)

- RVA `0xCF754`: `Software\Sierra On-Line\Viper Racing`
- RVA `0xD1CC4`: `Software\Sierra On-Line\Viper Racing`

### `DataDirectory` (2)

- RVA `0xCF77C`: `DataDirectory`
- RVA `0xD1CEC`: `DataDirectory`

### `CDDirectory` (2)

- RVA `0xCF78C`: `CDDirectory`
- RVA `0xD1CFC`: `CDDirectory`

### `common.res` (4)

- RVA `0xCF868`: `common.res`
- RVA `0xCF89C`: `common.res`
- RVA `0xE377C`: `common.res`
- RVA `0xE378C`: `common.res`

### `Direct3D` (1)

- RVA `0xDB9A4`: `Can't find IDirect3D2`

### `DirectDraw` (12)

- RVA `0xD9D28`: `Unable to initialize DirectDraw`
- RVA `0xDA95C`: `DDERR_NODIRECTDRAWSUPPORT`
- RVA `0xDACCC`: `DDERR_INVALIDDIRECTDRAWGUID`
- RVA `0xDACE8`: `DDERR_DIRECTDRAWALREADYCREATED`
- RVA `0xDAD08`: `DDERR_NODIRECTDRAWHW`
- RVA `0xDB8AC`: `Unable to create DirectDraw Object`
- RVA `0xDB8D0`: `Unable to get DirectDraw2 interface`
- RVA `0xDBAE8`: `can't query directdrawsurface3`
- RVA `0xE93A0`: `intro: Can't get DirectDraw stuff`
- RVA `0xE95F8`: `pPrimaryVidStream->QueryInterface(IID_IDirectDrawMediaStream, (void **)&pDDStream)  failed with HRESULT(0x%8.8X)`
- RVA `0x16DF4A`: `DirectDrawEnumerateA`
- RVA `0x16DF62`: `DirectDrawCreate`

### `DirectSound` (5)

- RVA `0xDDDE0`: `init_directsound(): Can't get primary buffer. (%s)`
- RVA `0xDDE14`: `DirectSoundCreate fails! (%s)`
- RVA `0xDE254`: `WaveBegin(): DirectSoundCreate fails! (%s)`
- RVA `0xDE364`: `Wave: Couldn't get DirectSound caps`
- RVA `0x16DF80`: `DirectSoundCreate`

### `DirectInput` (5)

- RVA `0xD2050`: `Unable to initialize DirectInput keyboard`
- RVA `0xD2164`: `DIERR_BETADIRECTINPUTVERSION`
- RVA `0xD22EC`: `DIERR_OLDDIRECTINPUTVERSION`
- RVA `0xD34B8`: `Couldn't get DirectInput2 joystick`
- RVA `0x16DFA0`: `DirectInputCreateA`

### `texture` (56)

- RVA `0xD9D8C`: `texture mem: %d of %d`
- RVA `0xD9F44`: `!!!! %d meg card with only %d texture ram`
- RVA `0xD9F70`: `vid: using AGP textures`
- RVA `0xDA2E4`: `D3DERR_TEXTURE_NO_SUPPORT`
- RVA `0xDA300`: `D3DERR_TEXTURE_CREATE_FAILED`
- RVA `0xDA320`: `D3DERR_TEXTURE_DESTROY_FAILED`
- RVA `0xDA340`: `D3DERR_TEXTURE_LOCK_FAILED`
- RVA `0xDA35C`: `D3DERR_TEXTURE_UNLOCK_FAILED`
- RVA `0xDA37C`: `D3DERR_TEXTURE_LOAD_FAILED`
- RVA `0xDA398`: `D3DERR_TEXTURE_SWAP_FAILED`
- RVA `0xDA3B4`: `D3DERR_TEXTURE_LOCKED`
- RVA `0xDA3CC`: `D3DERR_TEXTURE_NOT_LOCKED`
- RVA `0xDA3E8`: `D3DERR_TEXTURE_GETSURF_FAILED`
- RVA `0xDA4FC`: `D3DERR_TEXTURE_BADSIZE`
- RVA `0xDA514`: `D3DERR_INVALIDRAMPTEXTURE`
- RVA `0xDAA60`: `DDERR_NOTEXTUREHW`
- RVA `0xDB0CC`: `freeing unreleased texture elements: %s`
- RVA `0xDB0F4`: `already there: can't create texture: %s`
- RVA `0xDB11C`: `not last texture: can't destroy texture: %s`
- RVA `0xDB148`: `releasing no texture?`
- RVA `0xDB160`: `out of texture space, de-rezing!`
- RVA `0xDB184`: `out of texture cache entries`
- RVA `0xDBA5C`: `EnumTextureFormats`
- RVA `0xDBB4C`: `dtexture`
- RVA `0xDBC14`: `query texture interface`
- RVA `0xDBC2C`: `texture get handle`
- RVA `0xDBC40`: `texture load`
- RVA `0xDBC50`: `set texture handle`
- RVA `0xDBC64`: `set texture handle`
- RVA `0xDBC78`: `texture address u`
- RVA `0xDBC8C`: `texture address v`
- RVA `0xDBCD4`: `sys texture pool`
- RVA `0xDBCE8`: `sys texture pool`
- RVA `0xDBCFC`: `vid texture pool`
- RVA `0xDBD10`: `vid texture pool`

### `Can't find` (21)

- RVA `0xD0654`: `Can't find car %s`
- RVA `0xD1E4C`: `Game not installed properly--can't find registry information`
- RVA `0xD2DBC`: `trace: Can't find 0x%x in mapfile`
- RVA `0xD3334`: `BGUnHook(): Can't find func matching 0x%x`
- RVA `0xD364C`: `ResourceSetMaximize: Can't find %s`
- RVA `0xD3670`: `ResourceSetMinimize: Can't find %s`
- RVA `0xD3BA8`: `Can't find string table %s`
- RVA `0xD498C`: `Can't find message size for type %08x`
- RVA `0xD56C8`: `Can't find tire %s`
- RVA `0xDA03C`: `can't find model to destroy`
- RVA `0xDB9A4`: `Can't find IDirect3D2`
- RVA `0xDCA58`: `Can't find %s... using default.ccs`
- RVA `0xDCA7C`: `Can't find any defaults--using generic`
- RVA `0xDCC6C`: `can't find idx of %d in array[%d].`
- RVA `0xDCD04`: `EventUnInstallHandler: Can't find handler!`
- RVA `0xDD688`: `Can't find %s...`
- RVA `0xDD718`: `Can't find %s`
- RVA `0xDD9C4`: `SoundManager::Remove(): Can't find sound (%s).`
- RVA `0xE20D4`: `trkview: Can't find track %s in order table!`
- RVA `0xE3ED4`: `SessionMgr::WithdrawService - Can't find service 0x%x`
- RVA `0xE7654`: `Can't find upgrade %s`

### `requires` (1)

- RVA `0xE76DC`: `Upgrade:RequiresPrefix`

### `Viper Racing` (11)

- RVA `0xCF754`: `Software\Sierra On-Line\Viper Racing`
- RVA `0xCF7A8`: `MGI Viper Racing 1998`
- RVA `0xCF7C0`: `MGI Viper Racing 1998 Canary`
- RVA `0xCF7E0`: `Viper Racing Window`
- RVA `0xCF7F4`: `Viper Racing`
- RVA `0xD1CC4`: `Software\Sierra On-Line\Viper Racing`
- RVA `0xD1D18`: `MGI Viper Racing 1998`
- RVA `0xD1D30`: `MGI Viper Racing 1998 Canary`
- RVA `0xD1D50`: `Viper Racing Window`
- RVA `0xD1D64`: `Viper Racing`
- RVA `0xE36F8`: `Viper Racing Dedicated Server`

