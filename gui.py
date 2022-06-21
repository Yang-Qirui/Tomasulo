from time import sleep
from tkinter import *
from tkinter import filedialog, messagebox
from tkinter.ttk import Treeview
import tkinter as tk

from converter import convert_all
from api import machineState, states_clear, states_pop, run_one_tick, init_state, get_present, states_len

RES_STATION_SIZE = 6
ROB_SIZE = 16
REG_SIZE = 32
MEM_SIZE = 10000

converted_path = ''
pause_signal = False

INSTR_STATUS = ['ISSUE', 'EXECUTE', 'WRITE RESULT', 'COMMIT']


def draw(state: machineState, res: Treeview, rob: Treeview, reg: Treeview, mem: Treeview):
    for i in range(RES_STATION_SIZE):
        item = state.reservation[i]
        res.delete(i)
        if item.busy:
            res.insert('', i, iid=i, values=[
                       i, item.instr, item.busy, item.Vj, item.Vk, item.Qj, item.Qk, item.exTimeLeft, item.reorderNum])
        else:
            res.insert('', i, iid=i, values=[i])
    for i in range(ROB_SIZE):
        item = state.reorderBuf[i]
        rob.delete(i)
        if item.busy:
            rob.insert('', i, iid=i, values=[
                       i, item.busy, item.instr, item.execUnit, INSTR_STATUS[item.instrStatus], item.result, item.storeAddress])
        else:
            rob.insert('', i, iid=i, values=[i])
    for i in range(REG_SIZE):
        value = state.regFile[i]
        valid = state.regResult[i].valid
        robNum = state.regResult[i].reorderNum
        reg.set(i, column='value', value=value)
        reg.set(i, column='valid', value=valid)
        reg.set(i, column='reorderNum', value=robNum)
    for i in range(state.memorySize):
        value = state.memory[i]
        mem.set(i, column='value', value=value)


def run_all(root: Tk, res, rob, reg, mem):
    global converted_path, pause_signal
    if states_len() == 0:
        init_state(converted_path)
    state = get_present()
    while state.halt != 1:
        if pause_signal:
            pause_signal = False
            return
        run_one_tick()
        draw(state, res, rob, reg, mem)
        state = get_present()
        root.update()
        sleep(0.15)
    for item in rob.get_children():
        rob.delete(item)
    for item in res.get_children():
        res.delete(item)
    messagebox.showinfo("Program finished!", "Halt!")


def pause():
    global pause_signal
    pause_signal = True


def run_one_step(root: Tk, res, rob, reg, mem):
    global converted_path
    if states_len() == 0:
        init_state(converted_path)
    state = get_present()
    run_one_tick()
    draw(state, res, rob, reg, mem)
    if state.halt == 1:
        for item in rob.get_children():
            rob.delete(item)
        for item in res.get_children():
            res.delete(item)
        messagebox.showinfo("Program finished!", "Halt!")
    root.update()


def backward(root: Tk, res, rob, reg, mem):
    states_pop()
    if states_len() > 0:
        state = get_present()
        draw(state, res, rob, reg, mem)
        root.update()
    else:
        messagebox.showinfo("Program restart", "Already at the beginning!")


def restart(root, res, rob, reg, mem, asm):
    for item in res.get_children():
        res.delete(item)
    for i in range(RES_STATION_SIZE):
        res.insert("", END, iid=i, values=[i])

    for item in rob.get_children():
        rob.delete(item)
    for i in range(ROB_SIZE):
        rob.insert("", END, iid=i, values=[i])

    for item in reg.get_children():
        reg.delete(item)
    for i in range(REG_SIZE):
        reg.insert("", END, iid=i, values=[f"r[{i}]", 0])

    for item in mem.get_children():
        mem.delete(item)
    for i in range(MEM_SIZE):
        mem.insert("", END, iid=i, values=[f"mem[{i}]", 0])

    for item in asm.get_children():
        asm.delete(item)

    root.update()
    states_clear()


def select_asm_file(asm: Treeview):
    for item in asm.get_children():
        asm.delete(item)
    asm_file = filedialog.askopenfilename()
    global converted_path
    lines, converted_path = convert_all(asm_file)
    for line in lines:
        line = line.split(";")[0]
        print(line)
        asm.insert('', END, values=[line])


def init_res_station(root):
    res_station = Treeview(root, show="headings")
    res_station["columns"] = ("No.", "instr", "busy", "Vj",
                              "Vk", "Qj", "Qk", "exTimeLeft", "reorderNum")
    for item in res_station["columns"]:
        res_station.column(item, width=100)
        res_station.heading(item, text=item)
    for i in range(RES_STATION_SIZE):
        res_station.insert("", END, iid=i, values=[i])
    return res_station


def init_rob(root):
    rob = Treeview(root, show="headings")
    rob["columns"] = ("No.", "busy", "instr", "execUnit",
                      "instrStatus", "result", "storeAddress")
    for item in rob["columns"]:
        rob.column(item, width=100)
        rob.heading(item, text=item)
    for i in range(ROB_SIZE):
        rob.insert("", END, iid=i, values=[i])
    return rob


def init_register(root):
    regs = Treeview(root, show="headings", height=16)
    regs["columns"] = ("regNum", "value", "valid", "reorderNum")
    for item in regs["columns"]:
        regs.column(item, width=100)
        regs.heading(item, text=item)
    for i in range(REG_SIZE):
        regs.insert("", END, iid=i, values=[f"r[{i}]", 0])
    return regs


def init_memory(root):
    mems = Treeview(root, show="headings", height=16)
    mems["columns"] = ("memNum", "value")
    for item in mems["columns"]:
        mems.column(item, width=100)
        mems.heading(item, text=item)
    for i in range(MEM_SIZE):
        mems.insert("", END, iid=i, values=[f"mem[{i}]", 0])
    return mems


def init_asm(root):
    asm = Treeview(root, show="headings")
    asm["columns"] = ("asmCode")
    asm.column('asmCode', width=200)
    asm.heading('asmCode', text='asmCode')
    return asm


def main():
    root = Tk()
    root.resizable(width=0, height=0)

    Label(root).grid(row=1)

    res_station = init_res_station(root)
    res_station.grid(column=0, row=2, sticky=tk.N,
                     columnspan=2, padx=15, pady=10)

    Label(root).grid(column=3)

    regs = init_register(root)
    reg_scroll = Scrollbar(root, orient="vertical", command=regs.yview)
    regs.configure(yscrollcommand=reg_scroll.set)
    regs.grid(column=0, row=4, sticky=tk.W+tk.E, padx=15, pady=10)
    reg_scroll.grid(column=0, row=4, sticky=tk.NE+tk.SE, padx=15, pady=10)

    mems = init_memory(root)
    mem_scroll = Scrollbar(root, orient="vertical", command=mems.yview)
    mems.configure(yscrollcommand=mem_scroll.set)
    mems.grid(column=1, row=4, sticky=tk.W+tk.E, padx=15, pady=10)
    mem_scroll.grid(column=1, row=4, sticky=tk.NE+tk.SE, padx=15, pady=10)

    rob = init_rob(root)
    rob.grid(column=2, row=2, rowspan=4, sticky=tk.N+tk.S, padx=15, pady=10)

    asm = init_asm(root)
    asm_yscroll = Scrollbar(root, orient="vertical", command=asm.yview)
    asm.configure(yscrollcommand=asm_yscroll.set)
    asm.grid(column=3, row=2, rowspan=4, sticky=tk.N+tk.S, padx=15, pady=10)
    asm_yscroll.grid(column=3, row=2, rowspan=4,
                     sticky=tk.NE+tk.SE, padx=15, pady=10)

    Label(root).grid(row=0)
    Button(root, text="choose", command=lambda: select_asm_file(
        asm)).place(relx=0.4, rely=0)
    Button(root, text="run_all", command=lambda: run_all(
        root, res_station, rob, regs, mems)).place(relx=0.43, rely=0)
    Button(root, text="pause", command=lambda: pause()).place(
        relx=0.46, rely=0)
    Button(root, text="run_one_step", command=lambda: run_one_step(
        root, res_station, rob, regs, mems)).place(relx=0.49, rely=0)
    Button(root, text="backward_one_step", command=lambda: backward(
        root, res_station, rob, regs, mems)).place(relx=0.54, rely=0)
    Button(root, text="clear", command=lambda: restart(
        root, res_station, rob, regs, mems, asm)).place(relx=0.61, rely=0)
    Label(root).grid(row=6)
    root.mainloop()


if __name__ == "__main__":
    main()
