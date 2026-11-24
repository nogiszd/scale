// T-701 instruction stream for decoder coverage.
.text
s_mov_b32 s105, 0x12345678
s_mov_b32 s8, s105
s_mov_b32 s9, 0
s_add_u32 s19, s105, -16
s_add_u32 s20, 0x12345678, 0x12345678
s_movk_i32 s17, -32768
s_movk_i32 s21, 0x7fff

v_mov_b32_e32 v31, v255
v_mov_b32_e32 v32, s8
v_mov_b32_e32 v33, 0x87654321
v_mov_b32_e64 v34, v32
v_mov_b32_e64 v35, 0x3e22f983 // Inline 1/(2*pi).
v_mov_b32_e64 v36, 0x87654321
v_mov_b32_e64 v37, s8
v_add_f32_e32 v11, 0.5, v4
v_add_f32_e32 v11, -0.5, v4
v_add_f32_e32 v11, 1.25, v4
v_add_f32_e64 v12, 1.25, v7
v_add_f32_e64 v12, -1.0, v7
v_mul_f32_e32 v9, 4.0, v3
v_mul_f32_e32 v9, 1.0, v3
v_mul_f32_e64 v24, v128, -4.0
v_mul_f32_e64 v24, 2.0, v128
v_mul_f32_e64 v24, -2.0, v128
v_add_nc_u32_e32 v19, 64, v255
v_add_nc_u32_e32 v19, 0x87654321, v255
v_add_nc_u32_e64 v25, s8, v73
v_add_nc_u32_e64 v5, 0x87654321, 0x87654321
v_mul_lo_u32 v8, v10, s9

buffer_load_dword v19, v20, s[4:7], 64 offen offset:420
s_waitcnt vmcnt(44) expcnt(3) lgkmcnt(34)
buffer_store_dword v255, v253, s[100:103], s105 offen offset:4095
s_endpgm
