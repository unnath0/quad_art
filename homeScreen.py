import tkinter as tk
from tkinter import filedialog
import subprocess

win_width = 500
win_height = 400

def render_text(canvas, x, y, text, font, size, color="black"):
    canvas.create_text(x, y, text=text, font=(font, size), fill=color)

def on_click(event):
    fx = event.x / (win_width / 2) - 1.0
    fy = 1.0 - event.y / (win_height / 2)

    if -0.2 <= fx <= 0.2 and -0.05 <= fy <= 0.05:
        file_path = filedialog.askopenfilename(
            title="Select an image",
            filetypes=(("JPEG files", "*.jpg"), ("PNG files", "*.png")),
            initialdir=".",
            parent=root  # Ensure dialog is attached to the root window
        )
        if file_path:
            print(f"Selected file is {file_path}")
            command = f"python quad_art_generate.py \"{file_path}\""
            subprocess.call(command, shell=True)

root = tk.Tk()
root.title("Quad Art")

canvas = tk.Canvas(root, width=win_width, height=win_height, bg="white")
canvas.pack()

# Display project title
render_text(canvas, win_width/2, 50, "Quad Art", "Arial", 24, color="blue")

# Display team members (centered)
render_text(canvas, win_width/2, 100, "Team Members", "Arial", 12, color="gray")
render_text(canvas, win_width/2, 130, "Abhijnan S", "Arial", 12)
render_text(canvas, win_width/2, 150, "Anand S P", "Arial", 12)

# Upload button
def draw_upload_button():
    upload_btn = canvas.create_rectangle(win_width/2 - 100, 200, win_width/2 + 100, 240, fill="#4CAF50", outline="")
    render_text(canvas, win_width/2, 220, "Upload an image", "Arial", 14, color="white")
    canvas.tag_bind(upload_btn, "<Button-1>", on_click)

draw_upload_button()

root.mainloop()

