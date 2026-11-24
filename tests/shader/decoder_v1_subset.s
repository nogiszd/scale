// Wave32 ABI v1 decoder coverage. Branch targets include a backward jump.
.text
entry:
v_cmp_eq_u32_e32 vcc_lo, s105, v255
v_cmp_eq_u32_e32 vcc_lo, v255, v1
v_cmp_eq_u32_e32 vcc_lo, 64, v2
v_cmp_eq_u32_e32 vcc_lo, 0.5, v4
v_cmp_eq_u32_e32 vcc_lo, 0x12345678, v3
v_cmp_lt_u32_e32 vcc_lo, s8, v0
v_cmp_lt_u32_e32 vcc_lo, v0, v255
v_cmp_lt_u32_e32 vcc_lo, -16, v0
v_cmp_lt_u32_e32 vcc_lo, -4.0, v4
v_cmp_lt_u32_e32 vcc_lo, 0xfedcba98, v0

s_and_saveexec_b32 s9, s8
s_and_saveexec_b32 s105, vcc_lo
s_and_saveexec_b32 s9, -1
s_and_saveexec_b32 s9, 0.5
s_and_saveexec_b32 s9, 0x12345678
s_mov_b32 exec_lo, s105
s_mov_b32 exec_lo, vcc_lo
s_mov_b32 exec_lo, -1
s_mov_b32 exec_lo, -4.0
s_mov_b32 exec_lo, 0x87654321

s_cbranch_scc0 join
s_cbranch_scc1 join
s_cbranch_execz join
s_branch join
s_mov_b32 s8, 0x12345678 // Branch displacement counts both dwords.
join:
s_branch end
end:
s_endpgm
s_branch entry
