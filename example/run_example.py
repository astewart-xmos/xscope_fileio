#We assume that the Xscope FileIO Python library has been installed via pip beforehand and is available to import. Please see readme for instuctions.

import xscope_fileio

firmware_xe = "fileio_test.xe"
adapter_id = "L4Ss6YfM"
ref_text = b"Evolution is change in the heritable characteristics of biological populations over successive generations." + b"\x00";
with open("ref.bin", "wb") as ref_file:
    ref_file.write(ref_text)

xscope_fileio.run_on_target(adapter_id, firmware_xe, use_xsim=False)

with open("dut.bin", "rb") as dut_file:
    dut_text = dut_file.read()

with open("dut_mod.bin", "rb") as dut_mod_file:
    dut_mod_text = dut_mod_file.read()

ref_mod_text = ref_text[0:10] + b"IS" + ref_text[12:-1]

assert dut_text == ref_text
print(dut_mod_text, ref_mod_text)
assert dut_mod_text == ref_mod_text

print("PASS")