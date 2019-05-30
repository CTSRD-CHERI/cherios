
#

import sys

def main():
    print ("# DO NOT EDIT. AUTO-GENERATED.")
    print(".text")
    type = "UNKNOWN"
    should_include = True

    for line in sys.stdin:
        parts = line.split()
        if(len(parts) == 1):
            parts2 = line.split(".")
            type = (parts2[len(parts2)-2]).lower()
            if(type == "c" or type == "cpp" or type == "cxx" or type == "s"):
                should_include = True
            else:
                should_include = False
        elif (len(parts) == 3):
            if(parts[1] == "T" and should_include):
                sym = parts[2]
                print ("clcbi $c1, %captab20(" + sym + ")($c25)")

if __name__== "__main__":
    main()

