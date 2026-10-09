# Statically linked library code in REBEXE.EXE

Machine-readable list: `symbols/library_functions.csv` (462 entries; every function and hidden entry point in the library range).
Names follow MSVC linker-symbol style (C `rand` -> `_rand`, C `_exit` -> `__exit`); C++ members are `class_member` (e.g. `ostream_flush`), because the hybrid build links by plain symbol name.

## Address map (.text = 0x401000-0x6573ff)

| Range | What |
|---|---|
| 0x401000 - ~0x615345 | Game code (import-lib thunks for USER32 etc. are interleaved, e.g. 0x601090). The last game routine is a DirectDraw init at 0x615220 that Ghidra has not made a function. |
| 0x615350 - 0x6153ad | 15 import-lib thunks (DirectDrawCreate, Direct3DRMCreate, DirectSoundCreate, AVIFile*, GetFileVersionInfo*, ...). Not CRT. Names left as Ghidra set them. |
| **0x6153b0 - 0x61870f** | **LIBCI (old iostream), 107 entries.** First function is `ios::ios()` (0x6153b0), last is `istream::getint` (0x618510). |
| **0x618710 - 0x628e0f** | **LIBCMT (multithreaded static CRT) incl. C++ EH runtime, 355 entries.** Starts with the trnsctrl.obj EH helpers (`__JumpToContinuation` ... `__GetRangeOfTrysToCheck`), ends with `__strdup` (0x628dc0, 0x50 bytes). |
| 0x628e10 - 0x62908f | Game code again (small COMDAT-style functions: vector deleting dtors, ctors; call game functions at 0x4f5xxx/0x5xxxxx). 0x629080 is the `RtlUnwind` import thunk. |
| 0x629090 - ~0x657390 | `Unwind@` EH funclets (game and library). |
| ~0x657000 - 0x6573ff | Per-function `mov eax,<FuncInfo>; jmp __CxxFrameHandler` stubs (the `LAB_006570xx` handler targets). |

Library data is in .rdata/.data: iostream/CRT vftables 0x66e4dc-0x66e630, type descriptors 0x6ac970-0x6acc88, `_iob` 0x6acc88, CRT globals throughout .data (0x6a6000-0x6bfe4f).

Libraries present: LIBCI (iostream), LIBCMT 5.0-era (multithreaded static, `/MT`), plus the VC5 C++ EH runtime. No `_CxxThrowException`, no `__RTDynamicCast`/`__RTtypeid`: the game never throws and never uses dynamic_cast/typeid; EH is only used for unwinding (3820 handler stubs jump to `__CxxFrameHandler`). RTTI exists only for the iostream classes (ios, streambuf, strstreambuf, istream, ostream, istrstream, ostrstream, iostream, strstream, ofstream, ifstream, filebuf, type_info).

Confidence summary (462 entries): high 297, medium 134, low 31. By library: crt 305, iostream 107, eh 41, cpp_runtime 7, other 2. About 50 entries are code that Ghidra never turned into a function (vtable-only iostream methods, `__except_handler3`, `_purecall`, `_allmul`, `__cfltcvt` and friends reached through pointer tables, locale-init functions); they have a label and are marked "label only" in the CSV. They are not exported by `export_symbols.py` until a function is created there.

## Observations

* **Memory**: `operator_new` (0x618b70) -> `__nh_malloc(size,1)` -> `__heap_alloc` (0x6191e0). Returns NULL on failure rather than throwing; the game checks the result (hence the `if (p==0) icf_empty_function()` pattern). `_malloc` is `__nh_malloc(size,__newmode)`; the new-handler hook `_pnhHeap` is DAT_006be830, `__newmode` DAT_006be82c (0).
* **Heap**: VC5 small-block heap. `__heap_init` (0x61d960) does `HeapCreate(0,0x1000,0)` into `_crtheap` = DAT_006bed14, then `__sbh_alloc_new_region`. Blocks <= `__sbh_threshold` (DAT_006af194 = 0x1e0 = 480 bytes) come from 4 MB VirtualAlloc'd regions (static first region header at 0x6ad170, current region DAT_006af190, 16-byte paragraphs, 1 page per group); larger blocks go straight to `HeapAlloc`. `_free`/`_realloc`/`__msize` take `__lock(9)` (`_HEAP_LOCK`).
* **rand**: `_srand` (0x61a300) / `_rand` (0x61a310): `seed = seed*214013 + 2531011; return (seed>>16)&0x7fff`. The seed is NOT a global: it is `ptd->_holdrand` at offset +0x14 of the per-thread `_tiddata` (0x74 bytes, `_calloc`'d by `__mtinit` 0x61d3b0, pointer in TLS slot `__tlsindex` = DAT_006ad0a0, fetched by `__getptd` 0x61d430). `__initptd` (0x61d410) sets it to 1. Game call sites: rand 5, srand 2. Note: gameplay randomness does NOT come from `rand` — it comes from the game's own `RandomGenerator` (Lehmer, 0x5f5650–0x5f5810, `src/game/core/utility_classes.h`) via `GameRandom` (0x53e290); `rand`/`srand` only pick a seed when none is given.
* **errno**: `__errno` (0x61c250) = `&ptd->_terrno` (ptd+8); `___doserrno` (0x61c260) = ptd+0xc.
* **stdio**: `_iob` = 0x6acc88 (20 FILEs x 0x20; stdin 0x6acc88, stdout 0x6acca8, stderr 0x6accc8; iostream's `osfx` flushes those two for `ios::stdio`). `__initstdio` (0x61b460) builds `__piob` (DAT_006bee24) with `_nstream` = DAT_006bfe40 (512) pointers into `_iob`/heap FILEs. Low-level handles: `__pioinfo` = DAT_006bed20 (array of blocks of 32 x 0x24-byte ioinfo; handle at +0, flags at +4, CRITICAL_SECTION at +0xc), `_nhandle` = DAT_006bee20. Per-FILE locks via `__lock_file` (CRITICAL_SECTION at FILE+0x20 beyond the first 20).
* **printf family**: only `_sprintf` (19 game sites) is called directly; `__output` (0x61e470) is the shared formatter; float formatting via `__cfltcvt_tab` at 0x6af4e0 filled by `__cfltcvt_init` (0x619740). No scanf/strtod/atof/fprintf/printf are linked.
* **Locale/ctype**: `__mb_cur_max` DAT_006af3ac = 1, `_pctype` PTR_DAT_006af1a0, `__lc_handle[]` at 0x6be838 (ctype 0x6be840, codepage DAT_006be850), `_setlocale` has 1 game caller.
* **Process/startup**: `_WinMainCRTStartup` (0x619d80) -> `GetVersion`, `__heap_init`, `__mtinit`, `__ioinit`, `__setargv/__setenvp`, `__cinit` (0x61d5d0 = `_FPinit` + `_initterm`), then `WinMain` (0x4165b0), `exit` (0x61d600) = `_doexit`. `__onexit` table: begin DAT_006bed1c, end DAT_006bed18; `_atexit` has 147 game callers (static-object destructors).
* **Math**: the game almost never calls CRT math; sqrt/sin/cos are inline x87 (`fsqrt` 42x, `fsin` 19x, `fcos` 16x, `fyl2x` 4x, `fptan` 1x in game code). The only CRT math the game calls: `__ftol` (66), `_floor` (5; control word DAT_006acac0 = 0x173f = round-down), `_CIpow` (3), `_CIacos` (2). A Pentium FDIV-workaround block (`__adj_fdiv*`/`__adj_fprem*`, 0x62586b-0x62632d) is linked but never called by game code. `__fpmath` sets `_adjust_fdiv` (DAT_006be748).
* **Not present as out-of-line functions**: `strlen/strcpy/strcat/strcmp/memcpy(memmove only)/memset/memcmp/strstr/strrchr` - MSVC inlines them. `_memmove` (0x61be90, overlap-safe, also the memcpy implementation) has no game call sites. Only `_strncpy`, `_strncat`, `_strncmp`, `_strchr`, `__strnicmp`, `_stricmp` survive as calls.
* **ICF (identical COMDAT folding) is active** - important when reading call graphs:
  * `icf_empty_function` (0x619730): a 1-byte `ret`. The linker folded every empty no-arg function into it. The game "calls" it 6791 times (trivial destructors, scope exits) and ~5000 unwind funclets/379 vtable slots reference it. It was originally CRT-looking (sits between `__fpmath` and `__cfltcvt_init`) but is not meaningful.
  * `icf_return_zero` (0x6158b0): `xor eax,eax; ret`; doubles as `strstreambuf::sync` and `_matherr` and is the target of ~500 vtable/EH slots.
  * `fstreambase_close` (0x6168a0) is shared by `ifstream::close`/`ofstream::close`; `istream_operator_rs_long` (0x617610) is probably also `operator>>(int&)`; `FrameUnwindFilter` (0x618e40) is also `ArrayUnwindFilter`.
* **Ghidra mislabel**: the symbol `_abort` at 0x61d527 is a 5-byte `call _abort; ret` EH funclet belonging to `terminate()`; renamed `terminate_abort_funclet`. The real `_abort` is 0x624a60 (`_NMSG_WRITE(10)`, `raise(SIGABRT)`, `_exit(3)`). `entry` renamed `_WinMainCRTStartup`.
* **iostream usage**: the game uses strstream/ostrstream (formatting and parsing text: 54 `strstream_ctor`, 100 `operator<<(const char*)`, 95 `operator>>(long&)`), plus ifstream/ofstream for files (~12 constructor calls, 6 `close`). `ios::lock()`-style locking is (apparently) inlined into game code as calls to four 12-byte wrappers around Initialize/Delete/Enter/LeaveCriticalSection (`mtx_init/term/lock/unlock`, 0x615ec0-0x615ef0).
* Hidden entry points reached only via data tables (`__cfltcvt_tab`, vftables, scope tables): 0x620070/0x6200e0/0x620100/0x6204d0, 0x6170a0, 0x617160, 0x617eb0, etc.; also `__except_handler3` at 0x61a414 (SEH personality for every `__try`, signature "VC20XC00" just before it) which Ghidra did not make a function.

## Reconstruction of LIBCI (src/game/iostream)

The 95 functions of 0x6153b0-0x6184e0 that carry logic are reconstructed and hooked like game code (the compiler-made scalar
deleting destructors, `mtx_*`-style thunks excepted: `mtx_*` are reconstructed, the deleting destructors and the ICF-folded
`icf_return_zero` (also `strstreambuf::sync`) are not). `iostream/iostream.h` is the only declaration of the library; game code
includes it. The classes are layout structs plus free functions named as in `library_functions.csv`, not C++ classes, so that
objects keep the original layout and every virtual call (`streambuf` slots, the `ios` deleting destructor) goes through the
original vftables, which `symbols.csv` names `<class>::vftable` / `<class>::vbtable` (0x66e4dc-0x66e630).

Layout facts worth knowing (found while reconstructing):

* `streambuf` (0x4c bytes) lists the put area first: `base +0x10, ebuf +0x14, pbase +0x18, pptr +0x1c, epptr +0x20, eback +0x24,
  gptr +0x28, egptr +0x2c`, lock count +0x30, critical section +0x34. `strstreambuf` adds `dynamic +0x4c, next size +0x50,
  static +0x58, allocator +0x5c, deallocator +0x60` (size 0x64); `filebuf` adds `fd +0x4c, opened +0x50` (size 0x54). The
  game's inlined "bytes written to a strstream" test reads `pptr - pbase`.
* `ios` (0x50): `bp +4, state +8, delbuf +0x1c, tie +0x20, flags +0x24, precision +0x28, fill +0x2c, width +0x30, lock count
  +0x34, CRITICAL_SECTION +0x38`. `istream` is {vbptr, `_fGline`, `gcount`} (12 bytes), `ostream` {vbptr, floatused} (8).
* Destructor entry points take the address of the `ios` base in ECX; `istream_dtor` and `ostream_dtor` take the address
  just past their own part (istream + 0xc, ostream + 8), which for a strstream/iostream is *not* the ios (it is ios - 8).
* Original quirks kept: `getint`'s final length test can never fire (`BUG(original)`), `strstreambuf::seekoff` stores a
  stale pointer after growing the buffer, `ostream::operator<<(char)` calls `overflow` a second time after a failed
  `sputc`, the filebuf destructor takes the buffer lock and never releases it, `ios::ios` leaves `isfx/osfx` special words
  uninitialised.
* Data the library needs is named in `symbols.csv`: `g_iosInstanceCount` 0x6be740, `g_iosStaticLock` 0x6be708, `g_pioinfo` 0x6bed20
  (the CRT's handle table, read by `filebuf::sync` for the text-mode flag).

Verification: `tests/difftest_ext/iostream_variants.py` (poked entry cases for the text parsing and number formatting paths no
scenario reaches, 1,521 cases, unit and `--integration`) on top of captures over load_game, save_game_slot, mp_lobby_host/join and
new_game_rebel.

## CRT in the standalone build: reconstruct or use the host (M4)

The standalone program links the host's static UCRT. Only what can change a game result or a file byte is reconstructed
(`src/game/crt`, hooked and difftested like game code); the rest of the CRT symbols the reconstructed code references
(`tools/standalone_gaps.py --list crt`, 48 symbols) uses the host, with the reason below.

Reconstructed:

| Symbol | Where | Why / how |
|---|---|---|
| `_rand`, `_srand` 0x61a310, 0x61a300 | `crt/rand.cpp` | VC5 LCG; seed per thread, initially 1. The hybrid keeps it in the original's thread data block (`__getptd()+0x14`) so memory effects equal the original's; without `REB_HYBRID` it is a `thread_local`. |
| `_qsort` 0x61a0c0 (+ `__shortsort`, `__swap`) | `crt/qsort.cpp` | The VC5 algorithm (middle pivot moved to the front, explicit stack, selection sort up to 8 elements): the order of equal elements decides the neighbour lists of `SystemNeighbourTable::Build`. |
| `_floor` 0x619610 | `crt/floor.cpp` | FRNDINT under control word 0x173f (round down). Infinities/NaNs return unchanged; the original's `_matherr`/errno reporting for NaNs is not kept. |
| `__ftol` 0x619780 | `crt/x87_math.cpp` | The compiler's double-to-integer cast (FISTP with rounding forced to truncation). Only unreconstructed code calls it; clang emits an equivalent inline. |
| `_CIpow` 0x6197b0 | `crt/x87_math.cpp` | FYL2X/F2XM1/FSCALE at the caller's precision: `pow(10, 2)` is 100.00000000000004 here, so host `pow` would change volume slider results. Intel-syntax transliteration of 0x6197d2 and its helpers (the error paths excepted, below). |
| `_CIacos` 0x61a340 | `crt/x87_math.cpp` | atan2(sqrt((1+x)(1-x)), x) at x87 precision. |
| `_atoi` 0x619600 (`_atol` 0x619560) | `crt/atoi.cpp` | Wraps at 32 bits instead of clamping like `strtol`; C-locale white space and digits. |

Not reconstructed on purpose: the error paths of `_CIpow`/`_CIacos`/`_floor` (domain errors, overflow, NaN operands). The
original sets `errno` and calls `_matherr`; the reconstruction returns the same value and restores the control word but
touches neither. The game never reads either, and its inputs (slider positions, 0..1 cosines) do not get there.
`tests/difftest_ext/crt_variants.py` sweeps the cases that do not report errors (343 cases: pow, acos, ftol, floor, atoi,
qsort with ties) and `tools/difftest.py` now loads the captured x87 stack (`--fpu`, `--fpcw` override it); before that every
run started with an empty FPU.

Host CRT is fine:

| Symbol(s) | Reason |
|---|---|
| `_sprintf` | The game formats only `%s`, `%ld`, `%lu`, `%d`/`%x`-style integers (19 sites plus `ostream` number output); no floating-point conversion. |
| `_fopen _fread _fseek _ftell _fclose _fflush` | Binary reads of game files and plain seeks; no behaviour that differs between CRTs. |
| `__sopen __read __write __lseek __close` | Same flag values (`_O_BINARY`, `_O_TEXT`, sharing modes) and CR/LF translation, so file bytes are identical. The one dependency on CRT internals, `filebuf::sync` reading the handle-info table `__pioinfo` for the text-mode flag, is split by `REB_HYBRID` in `iostream/filebuf.cpp`: standalone uses `_setmode` and cannot see the CRT's end-of-file flag (the original adds a byte for it when a text read stopped on a lone CR; open item). |
| `_malloc _free` `operator_new operator_delete` | Pointer values differ from the VC5 small-block heap (the pointer-ordered containers risk in the plan), but nothing stores them. The standalone must define `operator new` as malloc-or-null (the code is built with `-fcheck-new`). Not hooked in the hybrid: it would split the heap between original and reconstructed callers. |
| `_atexit` | Same LIFO order. |
| `_strncpy _strncat _strncmp _strchr _memmove` | Defined byte semantics (padding, overlap) are identical. |
| `__strnicmp _stricmp` | Both lower-case through the locale's table; the game compares ASCII names. |
| `_toupper _isxdigit _isspace _ispunct _isprint`, `_setlocale` | Locale tables: the game calls `setlocale(LC_ALL, "")` and classifies keystrokes and ASCII text; both CRTs derive them from the same Windows locale. |
| `_strtol _strtoul`, `__errno` | Same clamping to LONG_MAX/MIN and `ERANGE`; `istream::operator>>` reads `errno` only after a conversion. |
| `_time` 0x61b120 | Seconds since 1970 (UTC); used only to seed the game's `RandomGenerator`, which is not reproducible anyway (the determinism harness fixes it). The standalone needs a `time(long*)` shim over `_time32`: the UCRT header makes `time` an inline wrapper over `_time64`. |
| `__chdir __chdrive` | `SetCurrentDirectory` and the `=X:` variables; one call each at start-up. |
| `__aulldiv` | Compiler runtime for 64-bit division; clang's own is the same arithmetic. |
| `_purecall`, `__except_handler3`, `icf_return_zero` | Never reached by reconstructed code (the build has no exceptions and no `__try`); the ICF stub is whatever lld folds. |

## Static initializers

`tools/gen_static_init.py` reads the tables `__cinit` (0x61d5d0) walks: the C initializers 0x6a7bdc-0x6a7bec (3 entries:
`__onexitinit`, `__initstdio`, `__CxxSetUnhandledExceptionFilter`) plus the FP hook `__fpmath` through the pointer at 0x6acacc, all
CRT-owned and left to the host, then the C++ table 0x6a6000-0x6a7bd8 (1,782 slots, the first one empty, 1,781 entries).
It writes `static_init_table.cpp` (`RunStaticInitializers()`): 111 calls of reconstructed initializers (by linker symbol),
1,670 initializers evaluated in Unicorn against the pristine EXE and baked as 1,673 byte-run stores into `reb_data_block`
(4,822 bytes), 0 unhandled. Two of the baked ones (0x54f7c0, 0x5d8930) also register an unreconstructed destructor with
`atexit` in the original; the table omits the registration (it would run at process exit only) and says so in a comment.
The sequence is validated in the emulator: the original's code for all 1,781 entries against the generated one (hooks for
the reconstructed ones, baked stores for the rest) leave identical bytes in .data/.bss and the emulated heap; flipping one
baked byte is detected (`--negative-control`). Initializers that allocate or call imports could not be baked; none of the
1,670 does.

## Top 40 library functions by game call sites

Counts are call/jump references from game code (addresses below 0x6153b0, plus the game COMDAT block 0x628e10-0x62908f). References from `Unwind@` funclets are not included (they are large for `operator_delete`: 2210, `icf_empty_function`: ~5000).

| # | Address | Name | Library | Game call sites |
|---|---|---|---|---|
| 1 | 0x00619730 | icf_empty_function | other | 6791 |
| 2 | 0x00618b70 | operator_new | cpp_runtime | 2759 |
| 3 | 0x00618b60 | operator_delete | cpp_runtime | 1503 |
| 4 | 0x00618c20 | _atexit | crt | 147 |
| 5 | 0x00616110 | ostream_operator_ls_cstr | iostream | 100 |
| 6 | 0x00617610 | istream_operator_rs_long | iostream | 95 |
| 7 | 0x00615440 | ios_dtor | iostream | 72 |
| 8 | 0x00619780 | __ftol | crt | 66 |
| 9 | 0x00615ea0 | strstream_dtor | iostream | 55 |
| 10 | 0x00615db0 | strstream_ctor | iostream | 54 |
| 11 | 0x00615f00 | ostream_operator_ls_char | iostream | 34 |
| 12 | 0x00618c80 | __ehvec_ctor | eh | 29 |
| 13 | 0x00618d20 | __ehvec_dtor | eh | 27 |
| 14 | 0x006173b0 | istream_read | iostream | 26 |
| 15 | 0x00615680 | strstreambuf_str | iostream | 23 |
| 16 | 0x00615ee0 | mtx_lock | iostream | 21 |
| 17 | 0x00615ef0 | mtx_unlock | iostream | 21 |
| 18 | 0x00615660 | strstreambuf_freeze | iostream | 20 |
| 19 | 0x00618ed0 | _sprintf | crt | 19 |
| 20 | 0x00617760 | istream_operator_rs_ulong | iostream | 16 |
| 21 | 0x006158b0 | icf_return_zero | other | 11 |
| 22 | 0x00616810 | ifstream_dtor | iostream | 9 |
| 23 | 0x00616620 | ifstream_ctor | iostream | 8 |
| 24 | 0x00616900 | ostream_seekp_pos | iostream | 7 |
| 25 | 0x00616cd0 | istream_seekg_off | iostream | 7 |
| 26 | 0x00617480 | ostream_operator_ls_ulong | iostream | 7 |
| 27 | 0x006168a0 | fstreambase_close | iostream | 6 |
| 28 | 0x00616e10 | ostream_operator_ls_long | iostream | 6 |
| 29 | 0x00617430 | ostream_write | iostream | 6 |
| 30 | 0x00619600 | _atoi | crt | 5 |
| 31 | 0x00619610 | _floor | crt | 5 |
| 32 | 0x0061a310 | _rand | crt | 5 |
| 33 | 0x0061a4f0 | _strncat | crt | 5 |
| 34 | 0x00616c30 | istream_seekg_pos | iostream | 4 |
| 35 | 0x00617900 | istream_operator_rs_short | iostream | 4 |
| 36 | 0x0061a080 | _strncmp | crt | 4 |
| 37 | 0x00615d90 | ostrstream_dtor | iostream | 3 |
| 38 | 0x006164b0 | ofstream_ctor | iostream | 3 |
| 39 | 0x00616590 | ofstream_dtor | iostream | 3 |
| 40 | 0x006197b0 | _CIpow | crt | 3 |

Everything else the game calls (1-2 sites each): `_strncpy` 3, `_stricmp` 2, `_CIacos` 2, `_srand` 2, `_isprint` 2, `_fseek` 2, `istream_operator_rs_ushort` 2, `istream_tellg` 2, `_fopen`/`_fread`/`_fclose`/`_ftell` 1 each, `_malloc` 1, `_free` 1, `_qsort` 1, `_setlocale` 1, `_time` 1 (0x61b120, earlier mislabelled `__ftime`), `__chdir` 1, `__chdrive` 1, `_toupper` 1, `_isspace` 1, `_ispunct` 1, `_strchr` 1, `__strnicmp` 1.

## Uncertain areas (low/medium confidence)

* x87 helper internals (0x61f4xx-0x6209xx, `__87_transcendental_core`, `__pow_*`, `__adj_fdiv*`) are named by role, not verified against a CRT build.
* Locale-init (`__init_time/numeric/monetary/ctype`, `GetLcidFrom*`, `__get_qualified_locale`) is identified from `__lc_handle[]` use and GetLocaleInfo patterns; names beyond that are best guesses.
* `istream_operator_rs_*` (long/int, ulong/uint, short, ushort) overloads: int vs long cannot be distinguished on x86; ICF may merge them.
* `ostream_seekp_*`, `istream_seekg_*`, `streambuf_snextc/sbumpc/stossc/sgetc` are inferred from vtable slot numbers and return conventions.
