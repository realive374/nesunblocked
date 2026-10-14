; ===========================================================================
;  RetroTube 90 - VCR-1 firmware
;
;  Runs once per video frame (WAI waits for vsync), polls the front panel and
;  the tape deck, drives the transport, picks the screen mode and draws the
;  on-screen display. See docs/VCR1.md for the instruction set.
; ===========================================================================

; ---- I/O ports (address $FF00 + port) -------------------------------------
.equ P_BTN_HELD,    $00
.equ P_BTN_EDGE,    $01
.equ P_FRAME_LO,    $02
.equ P_POWER_LED,   $04
.equ P_DECK_STAT,   $10
.equ P_DECK_CMD,    $11
.equ P_DECK_H,      $12
.equ P_DECK_M,      $13
.equ P_DECK_S,      $14
.equ P_DECK_SPEED,  $16
.equ P_SCREEN,      $20
.equ P_OSD_X,       $21
.equ P_OSD_Y,       $22
.equ P_OSD_CHAR,    $23
.equ P_OSD_CLEAR,   $24
.equ P_OSD_COLOR,   $25
.equ P_GPU_FX,      $26

; ---- front panel buttons ----------------------------------------------------
.equ B_POWER,  $01
.equ B_PLAY,   $02
.equ B_STOP,   $04
.equ B_REW,    $08
.equ B_FF,     $10
.equ B_EJECT,  $20
.equ B_TRACK,  $40

; ---- deck status bits / commands -------------------------------------------
.equ S_TAPE,    $01
.equ S_MOTOR,   $02
.equ S_BOT,     $04
.equ S_EOT,     $08
.equ S_CHANGED, $10

.equ C_STOP,  0
.equ C_PLAY,  1
.equ C_PAUSE, 2
.equ C_REW,   3
.equ C_FF,    4
.equ C_EJECT, 5
.equ C_ACK,   6

; ---- GPU ---------------------------------------------------------------------
.equ SCR_OFF,    0
.equ SCR_STATIC, 1
.equ SCR_BLUE,   2
.equ SCR_TAPE,   3

.equ COL_GREEN, 0
.equ COL_WHITE, 1

.equ FX_TRACKING, $01

; ---- firmware modes (doubling as OSD label ids) ---------------------------------
.equ M_NOTAPE, 0
.equ M_STOP,   1
.equ M_PLAY,   2
.equ M_PAUSE,  3
.equ M_REW,    4
.equ M_FF,     5
.equ L_EJECT,  6
.equ L_TRACK,  7
.equ L_NOTAPE, 8

; ---- timings, in frames (29.97 per second) ---------------------------------------
.equ T_POWERON_STATIC, 45   ; 1.5 s of snow while the "tube warms up"
.equ T_TAPE_STATIC,    30   ; 1 s of snow when a tape goes in or out
.equ T_PLAY_STATIC,    15   ; 0.5 s burst when playback starts
.equ T_OSD,            90   ; transport labels stay up for 3 s
.equ T_OSD_SHORT,      45
.equ SHUTTLE_SPEED,    12   ; REW/FF speed in frames per vsync

; ---- work RAM ------------------------------------------------------------------
.equ V_POWER,    $8000
.equ V_MODE,     $8001
.equ V_LABEL,    $8002
.equ V_OSD_T,    $8003
.equ V_STATIC_T, $8004
.equ V_SCREEN,   $8005
.equ V_EDGE,     $8006
.equ V_STATUS,   $8007

; ===========================================================================
.org $0000
reset:
    LD A, 0
    ST [V_POWER], A
    ST [V_OSD_T], A
    ST [V_STATIC_T], A
    ST [V_MODE], A
    OUT P_SCREEN, SCR_OFF
    OUT P_OSD_CLEAR, 0
    OUT P_DECK_SPEED, SHUTTLE_SPEED
    CALL power_on               ; the TV/VCR combo switches on at reset

main_loop:
    WAI                         ; sleep until the next vsync
    IN A, P_BTN_EDGE
    ST [V_EDGE], A
    LD A, [V_POWER]
    CMP A, 0
    JZ standby
    CALL poll_deck
    CALL handle_buttons
    LD A, [V_POWER]             ; POWER may have just switched us off
    CMP A, 0
    JZ main_loop
    CALL update_screen
    CALL draw_osd
    JMP main_loop

standby:                        ; powered off: only the POWER button works
    LD A, [V_EDGE]
    AND A, B_POWER
    JZ main_loop
    CALL power_on
    JMP main_loop

; ---------------------------------------------------------------------------
power_on:
    LD A, 1
    ST [V_POWER], A
    OUT P_POWER_LED, 1
    LD A, T_POWERON_STATIC
    ST [V_STATIC_T], A
    LD A, 0
    ST [V_OSD_T], A
    OUT P_DECK_CMD, C_ACK
    IN A, P_DECK_STAT
    AND A, S_TAPE
    JZ po_notape
    LD A, M_STOP
    ST [V_MODE], A
    RET
po_notape:
    LD A, M_NOTAPE
    ST [V_MODE], A
    RET

power_off:
    LD A, 0
    ST [V_POWER], A
    OUT P_POWER_LED, 0
    OUT P_DECK_CMD, C_STOP
    OUT P_OSD_CLEAR, 0
    OUT P_SCREEN, SCR_OFF
    RET

; ---------------------------------------------------------------------------
; poll_deck: notices tape insert/eject, auto-stop at the beginning of the tape
; while rewinding and auto-rewind at the end of the tape.
poll_deck:
    IN A, P_DECK_STAT
    ST [V_STATUS], A
    AND A, S_CHANGED
    JZ pd_nochange
    OUT P_DECK_CMD, C_ACK
    LD A, T_TAPE_STATIC         ; switching tapes: a second of snow
    CALL set_static_min
    LD A, [V_STATUS]
    AND A, S_TAPE
    JZ pd_notape
    LD A, M_STOP                ; new tape threaded and sitting stopped
    ST [V_MODE], A
    RET
pd_nochange:
    LD A, [V_STATUS]
    AND A, S_TAPE
    JNZ pd_tape
pd_notape:
    LD A, M_NOTAPE
    ST [V_MODE], A
    RET
pd_tape:
    LD B, [V_MODE]
    CMP B, M_REW
    JNZ pd_not_rew
    LD A, [V_STATUS]
    AND A, S_BOT
    JZ pd_done
    LD A, M_STOP                ; fully rewound
    CALL set_mode
    RET
pd_not_rew:
    CMP B, M_PLAY
    JZ pd_check_eot
    CMP B, M_FF
    JNZ pd_done
pd_check_eot:
    LD A, [V_STATUS]
    AND A, S_EOT
    JZ pd_done
    LD A, M_REW                 ; end of tape: auto-rewind
    CALL set_mode
pd_done:
    RET

; ---------------------------------------------------------------------------
handle_buttons:
    LD B, [V_EDGE]
    LD A, B
    AND A, B_POWER
    JZ hb_not_power
    CALL power_off
    RET
hb_not_power:
    LD A, [V_MODE]
    CMP A, M_NOTAPE
    JZ hb_no_tape               ; transport buttons need a tape
    LD A, B
    AND A, B_EJECT
    JNZ hb_eject
    LD A, B
    AND A, B_PLAY
    JNZ hb_play
    LD A, B
    AND A, B_STOP
    JNZ hb_stop
    LD A, B
    AND A, B_REW
    JNZ hb_rew
    LD A, B
    AND A, B_FF
    JNZ hb_ff
    LD A, B
    AND A, B_TRACK
    JNZ hb_track
hb_done:
    RET

hb_no_tape:                     ; a transport button without a tape: say so
    LD A, B
    AND A, B_PLAY + B_STOP + B_REW + B_FF + B_EJECT
    JZ hb_done
    LD A, L_NOTAPE
    ST [V_LABEL], A
    LD A, T_OSD_SHORT
    ST [V_OSD_T], A
    RET

hb_eject:
    OUT P_DECK_CMD, C_EJECT     ; poll_deck sees the change next frame
    LD A, L_EJECT
    ST [V_LABEL], A
    LD A, T_OSD_SHORT
    ST [V_OSD_T], A
    RET

hb_play:                        ; PLAY toggles play / pause
    LD A, [V_MODE]
    CMP A, M_PLAY
    JNZ hbp_start
    LD A, M_PAUSE
    CALL set_mode
    RET
hbp_start:
    CMP A, M_STOP
    JNZ hbp_resume
    LD A, T_PLAY_STATIC         ; starting from STOP: short burst of snow
    CALL set_static_min
hbp_resume:
    LD A, M_PLAY
    CALL set_mode
    RET

hb_stop:
    LD A, M_STOP
    CALL set_mode
    RET

hb_rew:                         ; second press of REW goes back to PLAY
    LD A, [V_MODE]
    CMP A, M_REW
    JZ hbp_resume
    LD A, M_REW
    CALL set_mode
    RET

hb_ff:                          ; second press of FF goes back to PLAY
    LD A, [V_MODE]
    CMP A, M_FF
    JZ hbp_resume
    LD A, M_FF
    CALL set_mode
    RET

hb_track:
    OUT P_GPU_FX, FX_TRACKING
    LD A, L_TRACK
    ST [V_LABEL], A
    LD A, T_OSD_SHORT
    ST [V_OSD_T], A
    RET

; ---------------------------------------------------------------------------
; set_mode: A = M_STOP..M_FF. Commands the deck and shows the matching label.
set_mode:
    ST [V_MODE], A
    ST [V_LABEL], A
    LD B, T_OSD
    ST [V_OSD_T], B
    CMP A, M_STOP
    JNZ sm_play
    OUT P_DECK_CMD, C_STOP
    RET
sm_play:
    CMP A, M_PLAY
    JNZ sm_pause
    OUT P_DECK_CMD, C_PLAY
    RET
sm_pause:
    CMP A, M_PAUSE
    JNZ sm_rew
    OUT P_DECK_CMD, C_PAUSE
    RET
sm_rew:
    CMP A, M_REW
    JNZ sm_ff
    OUT P_DECK_CMD, C_REW
    RET
sm_ff:
    OUT P_DECK_CMD, C_FF
    RET

; set_static_min: A = frames. Extends the static timer to at least A frames.
set_static_min:
    LD B, [V_STATIC_T]
    CMP B, A
    JNC ssm_done
    ST [V_STATIC_T], A
ssm_done:
    RET

; ---------------------------------------------------------------------------
update_screen:
    LD A, [V_STATIC_T]
    CMP A, 0
    JZ us_mode
    DEC A
    ST [V_STATIC_T], A
    LD A, SCR_STATIC
    JMP us_set
us_mode:
    LD A, [V_MODE]
    CMP A, M_NOTAPE
    JNZ us_stop
    LD A, SCR_STATIC            ; no tape: the TV shows snow
    JMP us_set
us_stop:
    CMP A, M_STOP
    JNZ us_tape
    LD A, SCR_BLUE              ; stopped: VCR blue screen
    JMP us_set
us_tape:
    LD A, SCR_TAPE
us_set:
    ST [V_SCREEN], A
    OUT P_SCREEN, A
    RET

; ---------------------------------------------------------------------------
draw_osd:
    OUT P_OSD_CLEAR, 0
    OUT P_OSD_COLOR, COL_GREEN
    LD A, [V_SCREEN]
    CMP A, SCR_BLUE
    JNZ do_label
    OUT P_OSD_COLOR, COL_WHITE
    OUT P_OSD_X, 15
    OUT P_OSD_Y, 14
    LDX str_nosignal
    CALL print
    OUT P_OSD_COLOR, COL_GREEN
do_label:
    LD A, [V_OSD_T]
    CMP A, 0
    JZ do_persist
    DEC A
    ST [V_OSD_T], A
    LD A, [V_LABEL]
    JMP do_show
do_persist:                     ; PAUSE / REW / FF labels stay on screen
    LD A, [V_MODE]
    CMP A, M_PAUSE
    JZ do_show
    CMP A, M_REW
    JZ do_show
    CMP A, M_FF
    JZ do_show
    RET
do_show:
    OUT P_OSD_X, 2
    OUT P_OSD_Y, 2
    CALL label_ptr
    CALL print
    LD A, [V_MODE]
    CMP A, M_NOTAPE
    JZ do_end
    CALL draw_counter
do_end:
    RET

; label_ptr: A = label id -> X = string
label_ptr:
    CMP A, M_STOP
    JNZ lp_play
    LDX str_stop
    RET
lp_play:
    CMP A, M_PLAY
    JNZ lp_pause
    LDX str_play
    RET
lp_pause:
    CMP A, M_PAUSE
    JNZ lp_rew
    LDX str_pause
    RET
lp_rew:
    CMP A, M_REW
    JNZ lp_ff
    LDX str_rew
    RET
lp_ff:
    CMP A, M_FF
    JNZ lp_eject
    LDX str_ff
    RET
lp_eject:
    CMP A, L_EJECT
    JNZ lp_track
    LDX str_eject
    RET
lp_track:
    CMP A, L_TRACK
    JNZ lp_notape
    LDX str_track
    RET
lp_notape:
    CMP A, L_NOTAPE
    JNZ lp_none
    LDX str_notape
    RET
lp_none:
    LDX str_blank
    RET

; draw_counter: tape counter "H:MM:SS" in the bottom-right corner
draw_counter:
    OUT P_OSD_X, 31
    OUT P_OSD_Y, 26
    IN A, P_DECK_H
    CMP A, 10
    JC dc_hours
    LD A, 9
dc_hours:
    ADD A, '0'
    OUT P_OSD_CHAR, A
    OUT P_OSD_CHAR, ':'
    IN A, P_DECK_M
    CALL print_2d
    OUT P_OSD_CHAR, ':'
    IN A, P_DECK_S
    CALL print_2d
    RET

; print: writes the zero-terminated string at X to the OSD
print:
    LD A, [X]
    JZ pr_done
    OUT P_OSD_CHAR, A
    INX
    JMP print
pr_done:
    RET

; print_2d: A = 0..99, printed as two decimal digits (uses B)
print_2d:
    LD B, 0
p2_tens:
    CMP A, 10
    JC p2_out
    SUB A, 10
    INC B
    JMP p2_tens
p2_out:
    PUSH A
    LD A, B
    ADD A, '0'
    OUT P_OSD_CHAR, A
    POP A
    ADD A, '0'
    OUT P_OSD_CHAR, A
    RET

; ---------------------------------------------------------------------------
str_play:     .db "PLAY \x10", 0
str_pause:    .db "PAUSE \x13", 0
str_stop:     .db "STOP \x12", 0
str_rew:      .db "REW \x11\x11", 0
str_ff:       .db "FF \x10\x10", 0
str_eject:    .db "EJECT \x14", 0
str_track:    .db "TRACKING", 0
str_notape:   .db "NO TAPE", 0
str_nosignal: .db "NO SIGNAL", 0
str_blank:    .db 0
