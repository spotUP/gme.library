| emu2413_68k.s - hand-written 68020+ versions of the emu2413 hot paths.
|
| gmelib: one 49716 Hz core tick (update_output plus the mono sum of mix_output in
| emu2413.c) is a single routine here: update_ampm, update_short_noise, update_slots
| (envelope and phase of all 18 slots), the channel calc (calc_slot_mod/car/hat/snare/
| tom/cym, to_linear, lookup_exp_table) and update_noise, with state kept in registers
| and no calls but the rare commit_slot_update. The 16-tap resampler FIR is a second
| routine. All of it is bit-exact against the C in emu2413.c (which stays the
| host/reference path, selected by OPLL_ASM). Only 16x16 muls.w and one 32-bit divsl.l
| (32-bit dividend, native on the 060) once per 64 ticks, no 64-bit mul/div, so the code
| runs on 68020/030/040/060; loads are grouped ahead of their use so the 060 can pair
| them. Structure offsets are pinned by _Static_assert in emu2413.c.

| OPLL
OPLL_test_flag  = 102
OPLL_rhythm     = 108
OPLL_eg_counter = 110
OPLL_pm_phase   = 114
OPLL_am_phase   = 118
OPLL_lfo_am     = 122
OPLL_noise      = 124
OPLL_short_noise = 128
OPLL_slot       = 166
OPLL_mask       = 3330
OPLL_ch_out     = 3334
OPLL_mix_out    = 3362
OPLL_conv       = 3366
OPLL_fx_out_time = 3370
OPLL_fx_out_step = 3378
OPLL_fx_inp_step = 3386
OPLL_asm_slot   = 3398
OPLL_asm_pg_key = 3542
| OPLL_RateConv
RC_sinc         = 20
RC_buf          = 24
RC_fx_timer     = 28
RC_fx_ratio     = 36
RC_fx_bits      = 44
RC_pos          = 52
| OPLL_SLOT
SLOT_size       = 58
S_type          = 1
S_patch         = 2
S_out0          = 6
S_out1          = 10
S_wave          = 14
S_pg_phase      = 18
S_pg_out        = 22
S_pg_keep       = 26
S_fnum          = 30
S_blk           = 32
S_eg_state      = 33
S_tll           = 40
S_blk_fnum      = 28
S_volume        = 34
S_key_flag      = 38
S_sus_flag      = 39
S_rks           = 42
S_eg_rate_h     = 43
S_eg_rate_l     = 44
S_eg_shift      = 46
S_eg_out        = 50
S_upd           = 54
| OPLL_PATCH
P_TL            = 0
P_FB            = 4
P_EG            = 8
P_DR            = 20
P_RR            = 28
P_KR            = 32
P_KL            = 36
P_WS            = 48
DAMPER_RATE     = 12
P_ML            = 12
P_AR            = 16
P_SL            = 24
P_AM            = 40
P_PM            = 44
| enum __OPLL_EG_STATE and friends
ATTACK          = 0
DECAY           = 1
SUSTAIN         = 2
RELEASE         = 3
DAMP            = 4
UPDATE_EG       = 8
EG_MUTE         = 127
EG_MAX          = 123
| OPLL_MASK_*
M_HH            = 9
M_CYM           = 10
M_TOM           = 11
M_SD            = 12
M_BD            = 13

| TOLIN: d1 = to_linear(d0, a2, d1) = lookup_exp_table(h + att), sign-extended long.
|   in: d0.l = h (wave table entry, zero-extended), d1.l = am, a2 = slot, d4 = EG_MAX,
|   a4 = exp2_table. Clobbers d0, d2, d3. The shift count is at most 23 (h & 0x7fff
|   <= 4095, att <= 2032), so the 68k count mod 64 and the host's mod 32 agree.
	.macro	TOLIN
	move.l	S_eg_out(%a2),%d2
	cmp.l	%d4,%d2
	bhi	.Ltz\@
	add.w	S_tll(%a2),%d2
	moveq	#EG_MUTE,%d3
	add.l	%d1,%d2
	cmp.l	%d3,%d2
	bls	.Lta\@
	move.l	%d3,%d2
.Lta\@:
	lsl.w	#4,%d2
	moveq	#0,%d1
	add.w	%d2,%d0
	move.b	%d0,%d1
	move.w	%d0,%d2
	move.w	(%a4,%d1.l*2),%d1
	lsr.w	#8,%d2
	lsr.l	%d2,%d1
	add.w	%d0,%d0
	subx.l	%d3,%d3
	eor.l	%d3,%d1
	add.w	%d1,%d1
	ext.l	%d1
	bra	.Ltd\@
.Ltz\@:
	moveq	#0,%d1
.Ltd\@:
	.endm

| AMOF: d1 = slot->patch->AM ? lfo_am : 0 (AM is 0 or 1), a3 = patch. d7 = lfo_am.
	.macro	AMOF
	move.l	S_patch(%a2),%a3
	move.l	P_AM(%a3),%d1
	neg.l	%d1
	and.l	%d7,%d1
	.endm

| FMPAIR: d1 = calc_slot_car(ch, calc_slot_mod(ch)), a2 = modulator slot on entry,
| carrier slot on exit. A pair with both envelopes above EG_MAX outputs 0 and only
| shifts its output history.
	.macro	FMPAIR
	cmp.l	S_eg_out(%a2),%d4
	bcc	.Lfull\@
	cmp.l	SLOT_size+S_eg_out(%a2),%d4
	bcc	.Lfull\@
	move.l	S_out0(%a2),S_out1(%a2)
	moveq	#0,%d1
	move.l	SLOT_size+S_out0(%a2),SLOT_size+S_out1(%a2)
	move.l	%d1,S_out0(%a2)
	move.l	%d1,SLOT_size+S_out0(%a2)
	lea	SLOT_size(%a2),%a2
	bra	.Ldone\@
.Lfull\@:
	AMOF
	move.l	P_FB(%a3),%d2
	moveq	#0,%d0
	tst.l	%d2
	beq	.Lnf\@
	moveq	#9,%d3
	move.l	S_out0(%a2),%d0
	sub.l	%d2,%d3
	add.l	S_out1(%a2),%d0
	asr.l	%d3,%d0
.Lnf\@:
	move.l	S_out0(%a2),S_out1(%a2)
	add.l	S_pg_out(%a2),%d0
	move.l	S_wave(%a2),%a3
	and.l	#0x3ff,%d0
	move.w	(%a3,%d0.l*2),%d0
	TOLIN
	move.l	%d1,S_out0(%a2)
	lea	SLOT_size(%a2),%a2
	moveq	#-2,%d0
	and.l	%d1,%d0
	AMOF
	add.l	S_pg_out(%a2),%d0
	move.l	S_wave(%a2),%a3
	and.l	#0x3ff,%d0
	move.l	S_out0(%a2),S_out1(%a2)
	move.w	(%a3,%d0.l*2),%d0
	TOLIN
	move.l	%d1,S_out0(%a2)
.Ldone\@:
	.endm

| NOISE9: d0 = update_noise(d0, 9). Nine steps of the Galois LFSR (n & 1 ? n ^= 0x800200;
| n >>= 1) in closed form: the feedback bits of the nine steps are the original bits 0..8
| (the bit fed back at 8 needs eight shifts to reach bit 0), each landing at bits j and
| 14 + j, so n9 = (n >> 9) ^ m ^ (m << 14) with m = n & 0x1ff. Clobbers d1, d2.
	.macro	NOISE9
	move.l	%d0,%d1
	lsr.l	#8,%d0
	and.l	#0x1ff,%d1
	lsr.l	#1,%d0
	moveq	#14,%d2
	eor.l	%d1,%d0
	lsl.l	%d2,%d1
	eor.l	%d1,%d0
	.endm

| SLOTF i: update_slots for slot i with a cached phase increment, a6 = opll, d4 = 9,
| d6 = 0x7ffff. Clobbers d0-d3/a0-a3/a5.
	.macro	SLOTF i
	tst.b	OPLL_asm_slot+8*\i+6(%a6)
	bne.s	.Lsf\@
	lea	OPLL_slot+SLOT_size*\i(%a6),%a2
	lea	OPLL_asm_slot+8*\i(%a6),%a5
	bsr	.Lslot_eg
.Lsf\@:
	move.l	OPLL_asm_slot+8*\i(%a6),%d1
	add.l	OPLL_slot+SLOT_size*\i+S_pg_phase(%a6),%d1
	and.l	%d6,%d1
	move.l	%d1,OPLL_slot+SLOT_size*\i+S_pg_phase(%a6)
	lsr.l	%d4,%d1
	move.l	%d1,OPLL_slot+SLOT_size*\i+S_pg_out(%a6)
	.endm

	.text

| int16_t opll_update_output_68k(OPLL *opll): update_output() in emu2413.c, which
| returns the mono mix_output sum of ch_out[0..13] (int16 wrap).
	.even
	.globl	_opll_update_output_68k
_opll_update_output_68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	move.l	48(%sp),%a0
	bsr	.Lupd
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

| .Lupd: the tick body, a0 = opll in, d0.l = sum out; clobbers d0-d7/a0-a6.
| (sp) holds opll during the tick (reloaded after .Lslots).
.Lupd:
	move.l	%a0,-(%sp)
| ---- update_ampm. lfo_am == am_table[(am_phase >> 6) % 210] holds on entry (calloc,
| OPLL_reset and every tick keep it), so it is reloaded only when am_phase >> 6 changes.
	moveq	#0,%d0
	move.b	OPLL_test_flag(%a0),%d0
	move.l	OPLL_am_phase(%a0),%d1
	btst	#1,%d0
	beq	.Lam_run
	clr.l	OPLL_pm_phase(%a0)
	moveq	#0,%d2
	bra	.Lam_set
.Lam_run:
	moveq	#1,%d2
	moveq	#1,%d3
	btst	#3,%d0
	beq	.Lam_inc
	moveq	#64,%d2
	move.l	#1024,%d3
.Lam_inc:
	add.l	%d3,OPLL_pm_phase(%a0)
	add.l	%d1,%d2
.Lam_set:
	move.l	%d2,OPLL_am_phase(%a0)
	eor.l	%d2,%d1
	and.l	#-64,%d1
	beq	.Lam_same
	asr.l	#6,%d2
	divsl.l	#210,%d3:%d2
	move.l	_opll_asm_tab+12,%a1
	move.b	(%a1,%d3.l),OPLL_lfo_am(%a0)
.Lam_same:
| ---- update_short_noise (pg_out of HH and CYM before this tick's update_slots)
	move.l	OPLL_slot+14*SLOT_size+S_pg_out(%a0),%d0
	move.l	OPLL_slot+17*SLOT_size+S_pg_out(%a0),%d1
	move.l	%d0,%d2
	move.l	%d1,%d3
	lsr.l	#5,%d2
	lsr.l	#2,%d3
	eor.l	%d0,%d2
	move.l	%d3,%d4
	eor.l	%d0,%d3
	eor.l	%d1,%d4
	or.l	%d4,%d3
	lsr.l	#1,%d3
	or.l	%d3,%d2
	lsr.l	#2,%d2
	moveq	#1,%d0
	and.l	%d0,%d2
	move.b	%d2,OPLL_short_noise(%a0)
| ---- update_slots
	bsr	.Lslots
| ---- channels. a0 = opll, a1 = ch_out, a2 = slot, a4 = exp2_table, d7 = lfo_am,
|   d6 = loop, d5 = mask (shifted), d4 = EG_MAX
	move.l	(%sp),%a0
	moveq	#0,%d7
	move.l	_opll_asm_tab+16,%a4
	move.b	OPLL_lfo_am(%a0),%d7
	lea	OPLL_slot(%a0),%a2
	lea	OPLL_ch_out(%a0),%a1
	move.l	OPLL_mask(%a0),%d5
	moveq	#EG_MAX,%d4
	moveq	#8,%d6
	tst.b	OPLL_rhythm(%a0)
	beq	.Lch
	moveq	#5,%d6
.Lch:
	lsr.l	#1,%d5
	bcs	.Lch_skip
	FMPAIR
	neg.l	%d1
	lea	SLOT_size(%a2),%a2
	asr.l	#1,%d1
	move.w	%d1,(%a1)+
	dbra	%d6,.Lch
	bra	.Lnoise
.Lch_skip:
	lea	2*SLOT_size(%a2),%a2
	addq.l	#2,%a1
	dbra	%d6,.Lch
| ---- update_noise 14 + 2 + 2: d6 = n9 (bit 5 of n9 is bit 0 of n14), noise = n18
.Lnoise:
	move.l	OPLL_noise(%a0),%d0
	NOISE9
	move.l	%d0,%d6
	NOISE9
	move.l	%d0,OPLL_noise(%a0)
	tst.b	OPLL_rhythm(%a0)
	beq	.Lmix
| ---- rhythm: BD (a2 = slot 12), HH 14, SD 15, TOM 16, CYM 17
	lea	OPLL_ch_out(%a0),%a1
	move.l	OPLL_mask(%a0),%d5
	btst	#M_BD,%d5
	bne	.Lr_hh
	FMPAIR
	move.w	%d1,9*2(%a1)
.Lr_hh:
	btst	#M_HH,%d5
	bne	.Lr_sd
	lea	OPLL_slot+14*SLOT_size(%a0),%a2
	moveq	#0,%d0
	move.l	S_wave(%a2),%a3
	tst.b	OPLL_short_noise(%a0)
	beq	.Lhh0
	btst	#5,%d6
	beq	.Lhh10
	move.w	0x2d0*2(%a3),%d0
	bra	.Lhhc
.Lhh10:
	move.w	0x234*2(%a3),%d0
	bra	.Lhhc
.Lhh0:
	btst	#5,%d6
	beq	.Lhh00
	move.w	0x34*2(%a3),%d0
	bra	.Lhhc
.Lhh00:
	move.w	0xd0*2(%a3),%d0
.Lhhc:
	moveq	#0,%d1
	TOLIN
	move.w	%d1,10*2(%a1)
.Lr_sd:
	btst	#M_SD,%d5
	bne	.Lr_tom
	lea	OPLL_slot+15*SLOT_size(%a0),%a2
	moveq	#0,%d0
	move.l	S_wave(%a2),%a3
	btst	#0,S_pg_out+2(%a2)
	beq	.Lsd0
	btst	#5,%d6
	beq	.Lsd10
	move.w	0x300*2(%a3),%d0
	bra	.Lsdc
.Lsd10:
	move.w	0x200*2(%a3),%d0
	bra	.Lsdc
.Lsd0:
	btst	#5,%d6
	beq	.Lsd00
	move.w	(%a3),%d0
	bra	.Lsdc
.Lsd00:
	move.w	0x100*2(%a3),%d0
.Lsdc:
	moveq	#0,%d1
	TOLIN
	move.w	%d1,11*2(%a1)
.Lr_tom:
	btst	#M_TOM,%d5
	bne	.Lr_cym
	lea	OPLL_slot+16*SLOT_size(%a0),%a2
	move.l	S_pg_out(%a2),%d0
	move.l	S_wave(%a2),%a3
	moveq	#0,%d1
	move.w	(%a3,%d0.l*2),%d0
	TOLIN
	move.w	%d1,12*2(%a1)
.Lr_cym:
	btst	#M_CYM,%d5
	bne	.Lmix
	lea	OPLL_slot+17*SLOT_size(%a0),%a2
	moveq	#0,%d0
	move.l	S_wave(%a2),%a3
	tst.b	OPLL_short_noise(%a0)
	beq	.Lcy0
	move.w	0x300*2(%a3),%d0
	bra	.Lcyc
.Lcy0:
	move.w	0x100*2(%a3),%d0
.Lcyc:
	moveq	#0,%d1
	TOLIN
	move.w	%d1,13*2(%a1)
| ---- mix_output sum
.Lmix:
	lea	OPLL_ch_out(%a0),%a1
	move.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1)+,%d0
	add.w	(%a1),%d0
	ext.l	%d0
	addq.l	#4,%sp
	rts

| .Lslots: update_slots() in emu2413.c, a0 = opll; clobbers d0-d7/a0-a6.
|   d7 = eg_counter as uint16, d6 = loop, d5 = test_flag, d4 = pm column
|   a2 = slot, a3 = patch, a4 = eg_step_tables, a5 = pm_table, a6 = ml_table
|   in calc_envelope: d0 = eg_out, d1 = eg_shift, d2 = eg_rate_h,
|   d3 = counter & mask (zero whenever an envelope step was taken), a0 = step row
	.even
.Lslots:
	addq.l	#1,OPLL_eg_counter(%a0)
	moveq	#0,%d7
	move.w	OPLL_eg_counter+2(%a0),%d7
	moveq	#0,%d5
	move.b	OPLL_test_flag(%a0),%d5
	move.l	OPLL_pm_phase(%a0),%d4
	moveq	#10,%d0
	lsr.l	%d0,%d4
	moveq	#7,%d0
	and.l	%d0,%d4
| Phase increment cache (opll->asm_slot[i][0]): valid when asm_pg_key == pm column + 1.
| Any public OPLL_* mutator zeroes the key (opll_asm_dirty in emu2413.c), test bit 2
| (phase reset) never validates it. d5 bit 31 set = recompute and store the increments.
	moveq	#1,%d0
	add.l	%d4,%d0
	btst	#2,%d5
	beq.s	.Lkey
	moveq	#0,%d0
.Lkey:
	cmp.l	OPLL_asm_pg_key(%a0),%d0
	beq.s	.Lkey_ok
	bset	#31,%d5
.Lkey_ok:
	tst.l	%d0
	bne.s	.Lkey_st
	bset	#31,%d5
.Lkey_st:
	move.l	%d0,OPLL_asm_pg_key(%a0)
	lea	OPLL_asm_slot(%a0),%a5
	lea	_opll_asm_tab+4,%a1
	move.l	(%a1)+,%a4
	move.l	(%a1),%a6
	lea	OPLL_slot(%a0),%a2
	moveq	#17,%d6

| Frozen slot (asm_slot[i][1] byte 6, set in .Lslot_eg): SUSTAIN or RELEASE with test
| bit 0 clear and eg_out == EG_MUTE or eg_rate_h == 0. Nothing but a public OPLL_* mutator
| (which clears the cache) can change that, so its commit and envelope are no-ops.
| With valid phase increments (d5 bit 31 clear) the 18 slots run unrolled (SLOTF), a6 = opll.
	tst.l	%d5
	bmi	.Lslot
	move.l	%a0,%a6
	moveq	#9,%d4
	move.l	#0x7ffff,%d6
	SLOTF	0
	SLOTF	1
	SLOTF	2
	SLOTF	3
	SLOTF	4
	SLOTF	5
	SLOTF	6
	SLOTF	7
	SLOTF	8
	SLOTF	9
	SLOTF	10
	SLOTF	11
	SLOTF	12
	SLOTF	13
	SLOTF	14
	SLOTF	15
	SLOTF	16
	SLOTF	17
	rts
| generic loop: recompute and store the phase increments
.Lslot:
	tst.b	6(%a5)
	bne.s	.Lphase
	bsr	.Lslot_eg
| ---- calc_phase ----
.Lphase:
	tst.l	%d5
	bmi.s	.Lph_slow
	move.l	(%a5),%d1
	add.l	S_pg_phase(%a2),%d1
	and.l	#0x7ffff,%d1
	move.l	%d1,S_pg_phase(%a2)
	moveq	#9,%d0
	lsr.l	%d0,%d1
	move.l	%d1,S_pg_out(%a2)
	addq.l	#8,%a5
	lea	SLOT_size(%a2),%a2
	dbra	%d6,.Lslot
	rts
.Lph_slow:
	move.l	S_patch(%a2),%a3	| (a frozen slot skips the load at .Lenv)
	moveq	#0,%d0
	move.w	S_fnum(%a2),%d0
	move.l	%d0,%d1
	and.l	#0x1ff,%d1
	add.l	%d1,%d1
	tst.l	P_PM(%a3)
	beq	.Lnopm
	lsr.l	#3,%d0			| pm_table[(fnum >> 6) & 7][pm column]
	and.l	#0x38,%d0
	add.l	%d4,%d0
	move.l	_opll_asm_tab,%a0
	move.b	(%a0,%d0.l),%d0
	ext.w	%d0
	add.w	%d0,%d1
.Lnopm:
	move.l	P_ML(%a3),%d0
	move.l	(%a6,%d0.l*4),%d0
	moveq	#0,%d2
	move.b	S_blk(%a2),%d2
	muls.w	%d0,%d1			| same low 32 bits as the C uint32 product
	lsl.l	%d2,%d1
	lsr.l	#2,%d1
	move.l	%d1,(%a5)
	btst	#2,%d5
	beq	.Lph_add
	clr.l	S_pg_phase(%a2)
.Lph_add:
	add.l	S_pg_phase(%a2),%d1
	and.l	#0x7ffff,%d1
	move.l	%d1,S_pg_phase(%a2)
	moveq	#9,%d0
	lsr.l	%d0,%d1
	move.l	%d1,S_pg_out(%a2)
	addq.l	#8,%a5
	lea	SLOT_size(%a2),%a2
	dbra	%d6,.Lslot
	| (callers restore registers)
	rts

| .Lslot_eg: commit (if requested) and calc_envelope of the slot a2, a5 = its asm_slot
| entry; d7 = eg_counter, d5 = test flag, a4 = eg_step_tables. Clobbers d0-d3/a0/a1/a3.
	.even
.Lslot_eg:
	tst.l	S_upd(%a2)
	beq	.Lenv
| A slot whose last commit returned at parameter rate 0 keeps its requests, and the
| re-commit is idempotent until its inputs change: asm_slot[i][1] is cleared by any
| public OPLL_* mutator and by every eg_state change below.
	tst.b	7(%a5)
	bne	.Lenv
	bsr	.Lcommit

| ---- calc_envelope ----
.Lenv:
	move.l	S_patch(%a2),%a3
| Fast path: a slot in SUSTAIN or RELEASE (test bit 0 clear) keeps eg_out and eg_state
| when no envelope step applies this tick: eg_out == EG_MUTE (the decay step clamps at
| EG_MUTE), eg_rate_h == 0 (step 0), or eg_counter & ((1 << eg_shift) - 1) != 0. These
| states have no transition, so the full path would store eg_out unchanged.
	move.b	S_eg_state(%a2),%d1
	subq.b	#SUSTAIN,%d1
	cmp.b	#RELEASE-SUSTAIN,%d1
	bhi	.Lenv_full
	btst	#0,%d5
	bne	.Lenv_full
	moveq	#EG_MUTE,%d0
	cmp.l	S_eg_out(%a2),%d0
	beq.s	.Lfrz
	tst.b	S_eg_rate_h(%a2)
	beq.s	.Lfrz
	move.l	S_eg_shift(%a2),%d1
	move.w	.Lmasktab(%pc,%d1.l*2),%d3
	and.w	%d7,%d3
	bne	.Leg_ret
	bra	.Lenv_full
.Lfrz:
	st	6(%a5)
	rts
| (1 << eg_shift) - 1; eg_shift is 0..13 (commit_slot_update)
.Lmasktab:
	.word	0,1,3,7,15,31,63,127,255,511,1023,2047,4095,8191,16383,32767
.Lenv_full:
	moveq	#0,%d3
	move.b	S_eg_rate_l(%a2),%d3
	move.l	S_eg_out(%a2),%d0
	lea	(%a4,%d3.l*8),%a0
	move.l	S_eg_shift(%a2),%d1
	moveq	#0,%d2
	moveq	#1,%d3
	move.b	S_eg_rate_h(%a2),%d2
	lsl.l	%d1,%d3
	subq.l	#1,%d3
	and.l	%d7,%d3
	tst.b	S_eg_state(%a2)
	bne	.Lnotatt

| state ATTACK
	tst.l	%d0
	beq	.Lstate
	tst.b	%d2
	beq	.Lstate
	moveq	#-4,%d1
	and.l	%d3,%d1
	bne	.Lstate
	cmp.b	#12,%d2
	bcs	.Latt_def
	cmp.b	#15,%d2
	beq	.Lstate			| rate 15: step 0
	moveq	#0xc,%d3		| rates 12..14: s = (16 - rate) - tab[(c & 0xc) >> 1]
	and.l	%d7,%d3
	lsr.l	#1,%d3
	moveq	#16,%d1
	sub.l	%d2,%d1
	move.b	(%a0,%d3.l),%d3
	sub.l	%d3,%d1
	bra	.Latt_apply
.Latt_def:
	move.l	S_eg_shift(%a2),%d1	| d1 was the ~3 test above
	move.l	%d7,%d3			| tab[(c >> shift) & 7] ? 4 : 0
	lsr.l	%d1,%d3
	moveq	#7,%d1
	and.l	%d1,%d3
	tst.b	(%a0,%d3.l)
	beq	.Lstate
	moveq	#4,%d1
.Latt_apply:
	move.l	%d0,%d3			| eg_out = max(0, eg_out - (eg_out >> s) - 1)
	lsr.l	%d1,%d3
	sub.l	%d3,%d0
	subq.l	#1,%d0
	bpl	.Lstate
	moveq	#0,%d0
	bra	.Lstate

| states other than ATTACK
.Lnotatt:
	tst.b	%d2
	beq	.Lstate
	tst.l	%d3
	bne	.Lstate
	cmp.b	#13,%d2
	bcs	.Ldec_def
	beq	.Ldec13
	cmp.b	#15,%d2
	beq	.Ldec15
	moveq	#0xc,%d3		| 14: tab[(c & 0xc) >> 1] + 1
	and.l	%d7,%d3
	lsr.l	#1,%d3
	move.b	(%a0,%d3.l),%d3
	addq.l	#1,%d3
	bra	.Ldec_add
.Ldec13:
	moveq	#0xc,%d3		| 13: tab[((c & 0xc) >> 1) | (c & 1)]
	and.l	%d7,%d3
	lsr.l	#1,%d3
	moveq	#1,%d1
	and.l	%d7,%d1
	or.l	%d1,%d3
	move.b	(%a0,%d3.l),%d3
	bra	.Ldec_add
.Ldec15:
	moveq	#2,%d3
	bra	.Ldec_add
.Ldec_def:
	move.l	%d7,%d3			| tab[(c >> shift) & 7]
	lsr.l	%d1,%d3
	moveq	#7,%d1
	and.l	%d1,%d3
	move.b	(%a0,%d3.l),%d3
.Ldec_add:
	add.l	%d3,%d0			| eg_out = min(EG_MUTE, eg_out + step)
	moveq	#EG_MUTE,%d3
	cmp.l	%d3,%d0
	bls	.Ldec_ok
	move.l	%d3,%d0
.Ldec_ok:
	moveq	#0,%d3

| state transitions
.Lstate:
	move.b	S_eg_state(%a2),%d1
	beq	.Lst_att
	subq.b	#1,%d1
	beq	.Lst_dec
	subq.b	#3,%d1
	bne	.Lst_done
| DAMP: to ATTACK (start_envelope) at EG_MAX, synchronised to the counter
	moveq	#EG_MAX,%d1
	cmp.l	%d1,%d0
	bcs	.Lst_done
	tst.l	%d3
	bne	.Lst_done
	moveq	#0,%d1
	move.b	S_rks(%a2),%d1
	lsr.b	#2,%d1
	add.l	P_AR(%a3),%d1
	moveq	#15,%d2
	cmp.l	%d2,%d1
	bcs	.Lse_att
	move.b	#DECAY,S_eg_state(%a2)
	moveq	#0,%d0
	bra	.Lse_upd
.Lse_att:
	move.b	#ATTACK,S_eg_state(%a2)
.Lse_upd:
	or.b	#UPDATE_EG,S_upd+3(%a2)
	clr.b	7(%a5)
	btst	#0,S_type(%a2)
	beq	.Lst_done
	tst.b	S_pg_keep(%a2)
	bne	.Lse_bud
	clr.l	S_pg_phase(%a2)
.Lse_bud:
	cmp.b	#1,S_type(%a2)		| type 1: buddy is the modulator before it
	bne	.Lst_done
	tst.b	S_pg_keep-SLOT_size(%a2)
	bne	.Lst_done
	clr.l	S_pg_phase-SLOT_size(%a2)
	bra	.Lst_done
.Lst_att:
	tst.l	%d0
	bne	.Lst_done
	move.b	#DECAY,S_eg_state(%a2)
	or.b	#UPDATE_EG,S_upd+3(%a2)
	clr.b	7(%a5)
	bra	.Lst_done
.Lst_dec:
	move.l	%d0,%d1
	lsr.l	#3,%d1
	cmp.l	P_SL(%a3),%d1
	bne	.Lst_done
	move.b	#SUSTAIN,S_eg_state(%a2)
	or.b	#UPDATE_EG,S_upd+3(%a2)
	clr.b	7(%a5)
.Lst_done:
	btst	#0,%d5
	beq	.Lst_store
	moveq	#0,%d0
.Lst_store:
	move.l	%d0,S_eg_out(%a2)
.Leg_ret:
	rts

| .Lcommit: commit_slot_update() of emu2413.c, a2 = slot; clobbers d0-d3/a0/a1/a3.
| Not rare: when get_parameter_rate() is 0 the C returns before clearing
| update_requests, so such a slot (a keyed-off modulator, an EG sustain) re-commits
| on every tick. The tables come from opll_asm_tab[5..7].
	.even
.Lcommit:
	move.l	S_upd(%a2),%d0
	move.l	S_patch(%a2),%a3
	lea	_opll_asm_tab+20,%a1
	btst	#0,%d0
	beq.s	.Lcm_tll
	move.l	(%a1),%a0
	move.l	P_WS(%a3),%d1
	move.l	(%a0,%d1.l*4),S_wave(%a2)
.Lcm_tll:
	btst	#1,%d0
	beq.s	.Lcm_rks
	move.l	4(%a1),%a0
	moveq	#0,%d1
	move.w	S_blk_fnum(%a2),%d1
	lsr.l	#5,%d1
	lsl.l	#6,%d1
	btst	#0,S_type(%a2)
	bne.s	.Lcm_vol
	add.l	P_TL(%a3),%d1
	bra.s	.Lcm_kl
.Lcm_vol:
	add.l	S_volume(%a2),%d1
.Lcm_kl:
	lsl.l	#2,%d1
	add.l	P_KL(%a3),%d1
	move.w	2(%a0,%d1.l*4),S_tll(%a2)
.Lcm_rks:
	btst	#2,%d0
	beq.s	.Lcm_eg
	move.l	8(%a1),%a0
	moveq	#0,%d1
	move.w	S_blk_fnum(%a2),%d1
	lsr.l	#8,%d1
	add.l	%d1,%d1
	add.l	P_KR(%a3),%d1
	move.b	3(%a0,%d1.l*4),S_rks(%a2)
.Lcm_eg:
	and.b	#12,%d0
	beq	.Lcm_clear
| get_parameter_rate() -> d1
	btst	#0,S_type(%a2)
	bne.s	.Lcm_st
	tst.b	S_key_flag(%a2)
	beq	.Lcm_zero
.Lcm_st:
	move.b	S_eg_state(%a2),%d2
	bne.s	.Lcm_ndec
	move.l	P_AR(%a3),%d1
	bra.s	.Lcm_rate
.Lcm_ndec:
	subq.b	#DECAY,%d2
	bne.s	.Lcm_nsus
	move.l	P_DR(%a3),%d1
	bra.s	.Lcm_rate
.Lcm_nsus:
	subq.b	#SUSTAIN-DECAY,%d2
	bne.s	.Lcm_nrel
	tst.l	P_EG(%a3)
	bne	.Lcm_zero
	move.l	P_RR(%a3),%d1
	bra.s	.Lcm_rate
.Lcm_nrel:
	subq.b	#RELEASE-SUSTAIN,%d2
	bne.s	.Lcm_ndamp
	moveq	#5,%d1
	tst.b	S_sus_flag(%a2)
	bne.s	.Lcm_rate
	moveq	#7,%d1
	tst.l	P_EG(%a3)
	beq.s	.Lcm_rate
	move.l	P_RR(%a3),%d1
	bra.s	.Lcm_rate
.Lcm_ndamp:
	subq.b	#DAMP-RELEASE,%d2
	bne	.Lcm_zero
	moveq	#DAMPER_RATE,%d1
.Lcm_rate:
	tst.l	%d1
	beq	.Lcm_zero
| eg_rate_h = min(15, p_rate + (rks >> 2)) (>= 1 here), eg_rate_l = rks & 3
	moveq	#0,%d2
	move.b	S_rks(%a2),%d2
	moveq	#3,%d3
	and.l	%d2,%d3
	lsr.l	#2,%d2
	add.l	%d1,%d2
	moveq	#15,%d1
	cmp.l	%d1,%d2
	bls.s	.Lcm_h
	move.l	%d1,%d2
.Lcm_h:
	move.b	%d2,S_eg_rate_h(%a2)
	move.b	%d3,S_eg_rate_l(%a2)
| eg_shift: ATTACK 13 - h for h < 12, others 13 - h for h < 13, else 0
	moveq	#12,%d1
	tst.b	S_eg_state(%a2)
	beq.s	.Lcm_att
	moveq	#13,%d1
.Lcm_att:
	moveq	#0,%d3
	cmp.l	%d1,%d2
	bcc.s	.Lcm_sh
	moveq	#13,%d3
	sub.l	%d2,%d3
.Lcm_sh:
	move.l	%d3,S_eg_shift(%a2)
.Lcm_clear:
	clr.l	S_upd(%a2)
	rts
| p_rate == 0: rates and shift cleared, update_requests kept (as the C returns early)
.Lcm_zero:
	move.b	#1,7(%a5)
	clr.l	S_eg_shift(%a2)
	clr.b	S_eg_rate_h(%a2)
	clr.b	S_eg_rate_l(%a2)
	rts

| int32_t opll_fir16_68k(const int16_t *w, const int16_t *t, int32_t fi, int32_t c)
| The fixed-point 16-tap sum of OPLL_RateConv_getData (LW 16, SINC_RESO 256):
|   k = 0..7   w[k] * t[fi + 256 (7 - k)]
|   k = 8..14  w[k] * t[256 (k - 7) - c]
|   k = 15     w[15] * t[min(2047, 2048 - c)]
	.even
	.globl	_opll_fir16_68k
_opll_fir16_68k:
	movem.l	%d2-%d5/%a2,-(%sp)
	movem.l	24(%sp),%a0-%a1
	movem.l	32(%sp),%d0-%d1
	bsr	.Lfir
	movem.l	(%sp)+,%d2-%d5/%a2
	rts

| .Lfir: a0 = window, a1 = sinc_table, d0 = fi, d1 = c; d0.l = sum out.
| Fully unrolled, two accumulators (d2, d4) and two temps (d3, d5) so each muls.w
| has an independent load ahead of it and the adds pair on the 060.
| Clobbers d1-d5/a0/a2.
	.even
.Lfir:
	lea	(%a1,%d0.l*2),%a2
	neg.l	%d1
	move.w	3584(%a2),%d2
	move.w	3072(%a2),%d4
	muls.w	(%a0)+,%d2
	muls.w	(%a0)+,%d4
	move.w	2560(%a2),%d3
	move.w	2048(%a2),%d5
	muls.w	(%a0)+,%d3
	muls.w	(%a0)+,%d5
	add.l	%d3,%d2
	add.l	%d5,%d4
	move.w	1536(%a2),%d3
	move.w	1024(%a2),%d5
	muls.w	(%a0)+,%d3
	muls.w	(%a0)+,%d5
	add.l	%d3,%d2
	add.l	%d5,%d4
	move.w	512(%a2),%d3
	move.w	0(%a2),%d5
	muls.w	(%a0)+,%d3
	muls.w	(%a0)+,%d5
	add.l	%d3,%d2
	add.l	%d5,%d4
	lea	(%a1,%d1.l*2),%a2
	move.w	512(%a2),%d3
	move.w	1024(%a2),%d5
	muls.w	(%a0)+,%d3
	muls.w	(%a0)+,%d5
	add.l	%d3,%d2
	add.l	%d5,%d4
	move.w	1536(%a2),%d3
	move.w	2048(%a2),%d5
	muls.w	(%a0)+,%d3
	muls.w	(%a0)+,%d5
	add.l	%d3,%d2
	add.l	%d5,%d4
	move.w	2560(%a2),%d3
	move.w	3072(%a2),%d5
	muls.w	(%a0)+,%d3
	muls.w	(%a0)+,%d5
	add.l	%d3,%d2
	add.l	%d5,%d4
	move.w	3584(%a2),%d3
	tst.l	%d1
	beq.s	.Lfir_c0
	move.w	4096(%a2),%d5
	bra.s	.Lfir_last
.Lfir_c0:
	move.w	4094(%a1),%d5
.Lfir_last:
	muls.w	(%a0)+,%d3
	muls.w	(%a0),%d5
	add.l	%d3,%d2
	add.l	%d5,%d4
	move.l	%d2,%d0
	add.l	%d4,%d0
	rts

| .Lround53: fx_round53() of emu2413.c on d0:d1 (hi:lo, value < 2^62): round to 53
| significant bits, ties to even. sh = bit length of r >> 53 is at most 9, so mask, half
| and the rounding increment fit in the low long. Clobbers d4-d6.
	.even
.Lround53:
	move.l	%d0,%d4
	moveq	#21,%d5
	lsr.l	%d5,%d4
	beq.s	.Lr53_ret
	bfffo	%d4{#0:#32},%d5
	moveq	#32,%d4
	sub.l	%d5,%d4
	moveq	#1,%d6
	lsl.l	%d4,%d6
	move.l	%d6,%d5
	subq.l	#1,%d5
	and.l	%d1,%d5
	sub.l	%d5,%d1
	lsr.l	#1,%d6
	cmp.l	%d6,%d5
	bhi.s	.Lr53_up
	bne.s	.Lr53_ret
	btst	%d4,%d1
	beq.s	.Lr53_ret
.Lr53_up:
	add.l	%d6,%d6
	add.l	%d6,%d1
	moveq	#0,%d6
	addx.l	%d6,%d0
.Lr53_ret:
	rts

| int16_t opll_calc_68k(OPLL *opll): OPLL_calc() of emu2413.c for the fixed-point case
| with a rate converter (opll->fx_ok, opll->conv, conv->fx_ok; the caller checks), fused:
| the tick loop (64-bit fx_out_time compare/add, fx_round53), update_output (.Lupd),
| putData into the doubled ring of channel 0, then getData: the 64-bit fx_timer step, the
| exactness margin test and the FIR (.Lfir). Samples inside the margin take the C double
| loop, opll_rateconv_getdata_dbl(conv), as the C does. (sp) = opll throughout.
	.even
	.globl	_opll_calc_68k
_opll_calc_68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	move.l	48(%sp),%a0
	move.l	%a0,-(%sp)
	movem.l	OPLL_fx_out_time(%a0),%d0-%d1
.Lc_loop:
	movem.l	OPLL_fx_out_step(%a0),%d2-%d3
	cmp.l	%d0,%d2
	bhi.s	.Lc_tick
	bcs.s	.Lc_done
	cmp.l	%d1,%d3
	bls.s	.Lc_done
.Lc_tick:
	movem.l	OPLL_fx_inp_step(%a0),%d2-%d3
	add.l	%d3,%d1
	addx.l	%d2,%d0
	bsr	.Lround53
	movem.l	%d0-%d1,OPLL_fx_out_time(%a0)
	bsr	.Lupd
| putData(conv, 0, d0): buf[pos] = buf[pos + 16] = d0, pos = (pos + 1) & 15
	move.l	(%sp),%a0
	move.l	OPLL_conv(%a0),%a1
	move.l	RC_buf(%a1),%a2
	move.l	RC_pos(%a1),%d1
	move.l	(%a2),%a2
	moveq	#15,%d2
	move.w	%d0,(%a2,%d1.l*2)
	move.w	%d0,32(%a2,%d1.l*2)
	addq.l	#1,%d1
	and.l	%d2,%d1
	move.l	%d1,RC_pos(%a1)
	movem.l	OPLL_fx_out_time(%a0),%d0-%d1
	bra.s	.Lc_loop
.Lc_done:
	sub.l	%d3,%d1
	subx.l	%d2,%d0
	bsr	.Lround53
	movem.l	%d0-%d1,OPLL_fx_out_time(%a0)
| getData: t = round53(fx_timer + fx_ratio) & (2^fb - 1), fb = fx_bits in 46..52, so the
| mask and the shifts below act on the high long only. s = lb - 32 = fb - 40 (6..12).
	move.l	OPLL_conv(%a0),%a1
	movem.l	RC_fx_timer(%a1),%d0-%d1
	movem.l	RC_fx_ratio(%a1),%d2-%d3
	add.l	%d3,%d1
	addx.l	%d2,%d0
	bsr	.Lround53
	move.l	RC_fx_bits(%a1),%d5
	moveq	#1,%d2
	sub.l	#32,%d5
	lsl.l	%d5,%d2
	subq.l	#1,%d2
	and.l	%d2,%d0
	movem.l	%d0-%d1,RC_fx_timer(%a1)
	subq.l	#8,%d5
	move.l	%d0,%d6
	lsr.l	%d5,%d6
	moveq	#1,%d7
	lsl.l	%d5,%d7
	subq.l	#1,%d7
	move.l	%d0,%d2
	and.l	%d7,%d2
	move.l	%d2,%d4
	or.l	%d1,%d4
	beq.s	.Lg_c0
| low != 0: fast only if margin <= low <= 2^lb - margin, margin = 2^(s - 6)
	subq.l	#6,%d5
	moveq	#1,%d3
	lsl.l	%d5,%d3
	tst.l	%d2
	bne.s	.Lg_top
	cmp.l	%d3,%d1
	bcs.s	.Lg_slow
.Lg_top:
	cmp.l	%d7,%d2
	bne.s	.Lg_c1
	neg.l	%d3
	cmp.l	%d3,%d1
	bhi.s	.Lg_slow
.Lg_c1:
	move.l	%d6,%d1
	addq.l	#1,%d1
	bra.s	.Lg_fir
.Lg_c0:
	move.l	%d6,%d1
.Lg_fir:
	move.l	%d6,%d0
	move.l	RC_buf(%a1),%a0
	move.l	RC_pos(%a1),%d2
	move.l	(%a0),%a0
	lea	(%a0,%d2.l*2),%a0
	move.l	RC_sinc(%a1),%a1
	bsr	.Lfir
	asr.l	#8,%d0
	asr.l	#4,%d0
.Lg_out:
	move.l	(%sp)+,%a0
	move.w	%d0,OPLL_mix_out(%a0)
	ext.l	%d0
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts
.Lg_slow:
	move.l	%a1,-(%sp)
	jsr	_opll_rateconv_getdata_dbl
	addq.l	#4,%sp
	bra.s	.Lg_out

