# Generate two files that define a bunch of symbols containing files
import os
import sys

def main():
    in_directory = sys.argv[1]
    asm_path = sys.argv[2]
    header_path = sys.argv[3]

    hdr = open(header_path, 'w')
    asm = open(asm_path, 'w')

    hdr.write("// AUTO GENERATED. DO NOT EDIT.\n")

    asm.write("// AUTO GENERATED. DO NOT EDIT.\n")
    asm.write(".data\n")


    for filename in os.listdir(in_directory):
        _,name = os.path.split(filename)
        no_ext = name.split(".")[0]

        hdr.write("extern const char __%s_start, __%s_end;\n" % (no_ext, no_ext))

        asm.write(".p2align 6\n")
        asm.write(".global __%s_start\n" % no_ext)
        asm.write(".global __%s_end\n" % no_ext)
        asm.write("__%s_start:\n" % no_ext)
        asm.write(".incbin \"%s\"\n" % filename)
        asm.write("__%s_end:\n" % no_ext)
        asm.write(".size __%s_start, __%s_end - __%s_start\n" % (no_ext, no_ext, no_ext))
        asm.write(".size __%s_end, 1\n" % no_ext)

if __name__== "__main__":
    main()
