import argparse
import os

op_list = ['lw', 'sw', 'add', 'addi', 'sub',
           'and', 'andi', 'beqz', 'j', 'halt', 'noop']
TYPE_I = 0
TYPE_R = 1
TYPE_J = 2
sign_dict = {}
line_list = []


def convert_opcode(input: str):
    input = input.lower()
    if input == "lw":
        return ("100011", TYPE_I)  # 35
    elif input == "sw":
        return ("101011", TYPE_I)  # 43
    elif input == "add" or input == "sub" or input == "and":
        return ("000000", TYPE_R)  # 0
    elif input == "addi":
        return ("001000", TYPE_I)  # 8
    elif input == "andi":
        return ("001100", TYPE_I)  # 12
    elif input == "beqz":
        return ("000100", TYPE_I)  # 4
    elif input == "j":
        return ("000010", TYPE_J)  # 2
    elif input == "halt":
        return ("000001", TYPE_J)  # 1
    elif input == "noop":
        return ("000011", TYPE_J)  # 3
    else:
        raise Exception(f"Not support op {input}")


def get_func(input: str):
    if input == "add":
        return 32
    elif input == "sub":
        return 34
    elif input == "and":
        return 36
    else:
        raise Exception(f"{input} is not a proper R type command")


def is_imm(input: str):
    try:
        int(input)
        return True
    except:
        pass
    return False


def init_sign_dict(input: str, lineno):
    input = input.rstrip().lstrip()
    parse_comment = input.split(";")
    rm_space = parse_comment[0].rstrip()
    parse_command = rm_space.replace(" ", ",")
    parse_command = parse_command.split(",")
    header = parse_command[0]
    if header not in op_list:
        """header is a signal definition"""
        if header in sign_dict:
            raise Exception(
                f"{header} already in sign_dict. Duplicate definition")
        sign_dict[header] = lineno
        if len(parse_command) > 1:
            parse_command = parse_command[1:]
            line_list.append(parse_command)
        else:
            """signal is in a line alone"""
            pass
    else:
        line_list.append(parse_command)


def convert(parse_command, lineno):
    op = parse_command[0]
    # print(parse_command)
    opcode, optype = convert_opcode(op)

    if op == "j":
        line = sign_dict[parse_command[-1]]
        bias = line - (lineno + 1)
        bias_str = bin(bias & 0x3ffffff).replace("0b", "").zfill(26)
        res_str = opcode + bias_str
        return res_str
    else:
        # print('p', parse_command)
        if optype == TYPE_I:
            if op != 'beqz':
                rs = int(parse_command[2].replace('r', ''))
                rs_bin = bin(rs & 0x1f).replace("0b", "").zfill(5)
                rd = int(parse_command[1].replace('r', ''))
                imm = int(parse_command[3])
                rd_bin = bin(rd & 0x1f).replace("0b", "").zfill(5)
                imm_bin = bin(imm & 0xffff).replace("0b", "").zfill(16)
                return opcode + rs_bin + rd_bin + imm_bin
            else:
                rs = int(parse_command[1].replace('r', ''))
                rs_bin = bin(rs & 0x1f).replace("0b", "").zfill(5)
                rd_bin = "00000"
                bias = sign_dict[parse_command[2]] - (lineno + 1)
                bias_bin = bin(bias & 0xffff).replace("0b", "").zfill(16)
                return opcode + rs_bin + rd_bin + bias_bin
        elif optype == TYPE_R:
            func = get_func(op)
            rs1 = int(parse_command[2].replace('r', ''))
            rs2 = int(parse_command[3].replace('r', ''))
            rd = int(parse_command[1].replace('r', ''))
            rs1_bin = bin(rs1 & 0x1f).replace("0b", "").zfill(5)
            rs2_bin = bin(rs2 & 0x1f).replace("0b", "").zfill(5)
            rd_bin = bin(rd & 0x1f).replace("0b", "").zfill(5)
            func_bin = bin(func & 0x7ff).replace("0b", "").zfill(11)
            return opcode + rs1_bin + rs2_bin + rd_bin + func_bin
        if optype == TYPE_J:
            return opcode.ljust(32, '0')


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-file', help='path of input file')
    args = parser.parse_args()
    file = args.file
    with open(file, 'r') as input:
        basename = os.path.basename(file).split('.')[0]
        with open(f'./convert/{basename}-converted.txt', 'w') as output:
            line = input.readline()
            linecnt = 0
            while line:
                init_sign_dict(line, linecnt)
                line = input.readline()
                linecnt += 1
            # print(line_list)
            for i in range(len(line_list)):
                bin_str = convert(line_list[i], i)
                if bin_str[0] == '1':
                    bin_str = bin_str.replace('1', '2')
                    bin_str = bin_str.replace('0', '1')
                    bin_str = bin_str.replace('2', '0')
                    # print(bin_str)
                    converted = str((int(bin_str, 2)) + 1) + "\n"
                    output.write(f"-{converted}")
                else:
                    # print(bin_str)
                    converted = str(int(bin_str, 2)) + "\n"
                    output.write(converted)
