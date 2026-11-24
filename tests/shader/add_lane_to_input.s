// T-704 synthetic ABI v0 example (docs/contracts/shader_abi.md).
.text
buffer_load_dword v2, v1, s[0:3], 0 offen
s_waitcnt vmcnt(0)
v_add_nc_u32_e32 v2, v0, v2
buffer_store_dword v2, v1, s[4:7], 0 offen
s_endpgm
