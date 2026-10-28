import re, time, serial, tkinter as tk

PORT = "/dev/ttyACM0"
ser = serial.Serial(PORT, 115200, timeout=1)
time.sleep(2)          
ser.read(4096)         

OPS = ["add", "sub", "and", "or", "xor", "sll", "srl", "sltu"]
SYMS = {"add": "+", "sub": "-", "and": "AND", "or": "OR", "xor": "XOR",
        "sll": "<<", "srl": ">>", "sltu": "<"}

def send_alu(a, b, op):
    ser.reset_input_buffer()
    for cmd in (f"alu a {a}", f"alu b {b}", f"alu op {op}", "alu run"):
        ser.write((cmd + "\n").encode())
        time.sleep(0.15)
    data = ser.read(4096).decode(errors="ignore")
    r = re.search(r"R = (\d+)", data)
    f = re.search(r"Z=(\d).*N=(\d).*OVF=(\d).*C=(\d)", data, re.S)
    if not r or not f:
        raise RuntimeError(
            "No se reconocio la respuesta de la ALU. Fijate que el dispositivo "
            "este parado en Ajustes -> Terminal Avz. Esto llego por serial:\n"
            + repr(data)
        )
    return int(r.group(1)), f.groups()

BG, FG, LIT, DIM, BORDER = "#0b0b0b", "#d9d9d9", "#39d353", "#1c1c1c", "#4a4a4a"
CELL = 28

def draw_register(canvas, x, y, label):
    canvas.create_text(x - 14, y + CELL / 2, text=label, fill=FG,
                        font=("Consolas", 12, "bold"), anchor="e")
    cells, texts = [], []
    for i in range(8):
        cx = x + i * CELL
        rect = canvas.create_rectangle(cx, y, cx + CELL - 2, y + CELL,
                                        fill=DIM, outline=BORDER)
        txt = canvas.create_text(cx + CELL / 2 - 1, y + CELL / 2, text="0",
                                  fill=FG, font=("Consolas", 12, "bold"))
        cells.append(rect)
        texts.append(txt)
    val_txt = canvas.create_text(x + 8 * CELL + 10, y + CELL / 2, text="= 0",
                                  fill=FG, font=("Consolas", 11), anchor="w")
    return cells, texts, val_txt

def set_register(canvas, reg, value):
    cells, texts, val_txt = reg
    for i in range(8):
        bit = (value >> (7 - i)) & 1
        canvas.itemconfig(texts[i], text=str(bit))
        canvas.itemconfig(cells[i], fill=LIT if bit else DIM)
    canvas.itemconfig(val_txt, text=f"= {value}  (0x{value:02X})")

root = tk.Tk()
root.title("ALU RISC-V")
root.configure(bg=BG)

ctrl = tk.Frame(root, bg=BG)
ctrl.pack(padx=10, pady=(10, 0), anchor="w")

tk.Label(ctrl, text="A", bg=BG, fg=FG).grid(row=0, column=0)
a_entry = tk.Entry(ctrl, width=5); a_entry.insert(0, "132"); a_entry.grid(row=0, column=1, padx=5)
tk.Label(ctrl, text="B", bg=BG, fg=FG).grid(row=0, column=2)
b_entry = tk.Entry(ctrl, width=5); b_entry.insert(0, "47"); b_entry.grid(row=0, column=3, padx=5)
tk.Label(ctrl, text="OP", bg=BG, fg=FG).grid(row=0, column=4)
op_var = tk.StringVar(value="add")
tk.OptionMenu(ctrl, op_var, *OPS).grid(row=0, column=5, padx=5)

canvas = tk.Canvas(root, width=440, height=260, bg=BG, highlightthickness=0)
canvas.pack(padx=10, pady=10)

reg_a = draw_register(canvas, 60, 20, "A")
reg_b = draw_register(canvas, 60, 70, "B")

alu_box = canvas.create_rectangle(140, 120, 300, 170, outline=FG, width=2)
op_label = canvas.create_text(220, 145, text="ALU", fill=FG, font=("Consolas", 14, "bold"))

reg_r = draw_register(canvas, 60, 190, "R")

flag_names = ["Z", "N", "OVF", "C"]
flag_boxes, flag_texts = [], []
fx = 60
for name in flag_names:
    canvas.create_text(fx + 12, 245, text=name, fill=FG, font=("Consolas", 10))
    box = canvas.create_rectangle(fx, 250, fx + 24, 258, fill=DIM, outline=BORDER)
    flag_boxes.append(box)
    fx += 50

def flash_alu(times_left, done):
    if times_left <= 0:
        canvas.itemconfig(alu_box, outline=FG)
        done()
        return
    canvas.itemconfig(alu_box, outline=LIT)
    root.after(120, lambda: (canvas.itemconfig(alu_box, outline=FG),
                              root.after(120, lambda: flash_alu(times_left - 1, done))))

def play():
    a, b, op = int(a_entry.get()), int(b_entry.get()), op_var.get()
    set_register(canvas, reg_a, a)
    set_register(canvas, reg_b, b)
    canvas.itemconfig(op_label, text=SYMS[op])

    def finish():
        r, (z, n, ovf, c) = send_alu(a, b, op)
        set_register(canvas, reg_r, r)
        for box, bit in zip(flag_boxes, (z, n, ovf, c)):
            canvas.itemconfig(box, fill=LIT if bit == "1" else DIM)

    flash_alu(3, finish)

tk.Button(root, text="Play", command=play).pack(pady=(0, 10))
root.mainloop()
