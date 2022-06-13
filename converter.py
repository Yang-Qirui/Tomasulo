def convert_opcode(input: str):
    input = input.lower()
    if input == "lw":
        return "100011"  # 35
    elif input == "sw":
        return "101011"  # 43
    elif input == "add" or input == "sub" or input == "and":
        return "000000"  # 0
    elif input == "addi":
        return "001000"  # 8
    elif input == "andi":
        return "001100"  # 12
    elif input == "beqz":
        return "000100"  # 4
    elif input == "j":
        return "000010"  # 2
    elif input == "halt":
        return "000001"  # 1
    elif input == "noop":
        return "000011"  # 3
    else:
        raise Exception(f"Not support op {input}")


def convert(input: str):
    parse_comment = input.split(";")
    rm_space = parse_comment[0].rstrip()
    parse_command = rm_space.replace(" ", ",")
    parse_command = parse_command.split(",")
    if len(parse_command) == 5:
        """SIGN op x,y,z"""

    elif len(parse_command) == 4:
        """ op rs,rd,imm"""


if __name__ == "__main__":
    convert("addi r9,r9,1     ;set r9=1")
