import pathlib

bin_path = pathlib.Path("model.tflite")
out_path = pathlib.Path("model_data.h")
var_name = "model_tflite"

data = bin_path.read_bytes()

with out_path.open("w") as f:
    f.write("#pragma once\n\n")
    f.write(f"const unsigned char {var_name}[] = {{\n  ")
    for i, b in enumerate(data):
        f.write(f"0x{b:02x}, ")
        if (i + 1) % 12 == 0:
            f.write("\n  ")
    f.write("\n};\n")
    f.write(f"const unsigned int {var_name}_len = {len(data)};\n")

print(f"Written {out_path} with {len(data)} bytes.")
