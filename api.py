from copy import copy
from ctypes import Structure, c_int, c_int
import ctypes
import os
import sys

ROB_SIZE = 16
RES_STATION_SIZE = 6
MEM_SIZE = 10000
REG_SIZE = 32
BTB_SIZE = 8


class resStation(Structure):
    _fields_ = [
        ("instr", c_int),
        ("busy", c_int),
        ("Vj", c_int),
        ("Vk", c_int),
        ("Qj", c_int),
        ("Qk", c_int),
        ("exTimeLeft", c_int),
        ("reorderNum", c_int)
    ]


class reorderEntry(Structure):
    _fields_ = [
        ("busy", c_int),
        ("instr", c_int),
        ("execUnit", c_int),
        ("instrStatus", c_int),
        ("result", c_int),
        ("storeAddress", c_int)
    ]


class regResultEntry(Structure):
    _fields_ = [
        ("valid", c_int),
        ("reorderNum", c_int)
    ]


class btbEntry(Structure):
    _fields_ = [
        ("valid", c_int),
        ("branchPC", c_int),
        ("branchTarget", c_int),
        ("branchPred", c_int)
    ]


class machineState(Structure):
    _fields_ = [
        ("pc", c_int),
        ("cycles", c_int),
        ("reservation", resStation*RES_STATION_SIZE),
        ("reorderBuf", reorderEntry*ROB_SIZE),
        ("regResult", regResultEntry*REG_SIZE),
        ("btFuf", btbEntry*BTB_SIZE),
        ("memory", c_int*MEM_SIZE),
        ("regFile", c_int*REG_SIZE),
        ("headRB", c_int),
        ("tailRB", c_int),
        ("clear", c_int),
        ("memorySize", c_int),
        ("halt", c_int)
    ]


states = []
os.add_dll_directory(os.getcwd())
template = ctypes.cdll.LoadLibrary("template.dll")
c_run_one_tick = template.run_one_tick
c_init = template.init
c_init.restype = machineState
c_run_one_tick.argtypes = [machineState]
c_run_one_tick.restype = machineState


def init_state(machine_code_path):
    # print("init")
    init = c_init(machine_code_path.encode())
    # print("finish")
    states.append(init)


def run_one_tick():
    # print("run one")
    present = states[-1]
    new_state = c_run_one_tick(present)
    states.append(new_state)
    # print("run finish")


def get_present():
    return states[-1]


def states_pop():
    if len(states) > 0:
        states.pop()


def states_len():
    return len(states)

def states_clear():
    states.clear()

def test():
    init_state(
        "D:\Personal\Grade3 bot\Computer System\Tomasulo\convert\input1-converted.txt")
    state = states[-1]
    while state.halt != 1:
        run_one_tick()
        state = states[-1]


if __name__ == "__main__":
    test()
