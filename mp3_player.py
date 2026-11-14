import os
import sys
import json
import time
import threading
import tkinter as tk
import tkinter.ttk as ttk
from tkinter import filedialog, messagebox
import pygame
import wave

try:
    from tinytag import TinyTag
    HAS_TINYTAG = True
except ImportError:
    HAS_TINYTAG = False

# ============================================================
# NOIR MUSIC — тёмный современный музыкальный плеер
# ============================================================
APP_NAME = "МАЛЫШ"
APP_DIR = os.path.join(
    os.environ.get("APPDATA", os.path.expanduser("~")),
    "NoirMusic"
)
PLAYLIST_FILE = os.path.join(APP_DIR, "playlist.json")
EXTENSIONS = {".mp3", ".wav", ".ogg", ".flac"}
MIN_SIZE = 30_000
WAV_MIN_SIZE = 250_000
EXCLUDED_FOLDERS = {
    "windows", "program files", "program files (x86)",
    "appdata", "programdata"
}
SYSTEM_WAV_PREFIXES = (
    "chimes", "ding", "notify", "tada", "windows", "ringout"
)

# ============================================================
# ПАЛИТРА
# ============================================================
BG = "#09090b"
PANEL = "#111114"
PANEL_ALT = "#17171c"
PANEL_HOVER = "#202027"
BORDER = "#25252d"
ACCENT = "#C2A5FF"
ACCENT_HOVER = "#D8C5FF"
ACCENT_DARK = "#8058D0"
TEXT = "#FFFFFF"
MUTED = "#B8B8C5"
DIM = "#858590"
GREEN = "#8CE8B5"

# ============================================================
# ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
# ============================================================
def format_time(seconds):
    seconds = max(0, int(seconds or 0))
    minutes, secs = divmod(seconds, 60)
    hours, minutes = divmod(minutes, 60)
    return (
        f"{hours:02d}:{minutes:02d}:{secs:02d}"
        if hours else f"{minutes:02d}:{secs:02d}"
    )

def _get_app_dir():
    """Возвращает директорию, где лежит приложение (для поиска иконки)."""
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))

def _get_system_dirs():
    win = os.environ.get("SYSTEMROOT", r"C:\Windows")
    paths = [
        os.path.join(win, x)
        for x in ("System32", "SysWOW64", "Resources", "Media")
    ]
    return [
        os.path.normcase(os.path.abspath(p))
        for p in paths
    ]

SYSTEM_DIRS = _get_system_dirs()

def _is_system(path):
    p = os.path.normcase(os.path.abspath(path))
    for system_dir in SYSTEM_DIRS:
        try:
            if os.path.commonpath([p, system_dir]) == system_dir:
                return True
        except ValueError:
            pass
    return False

def _is_valid_audio(path):
    if _is_system(path):
        return False
    ext = os.path.splitext(path)[1].lower()
    if ext not in EXTENSIONS:
        return False
    if os.path.basename(path).lower().startswith(SYSTEM_WAV_PREFIXES):
        return False
    try:
        size = os.path.getsize(path)
    except OSError:
        return False
    if size < MIN_SIZE:
        return False
    if ext == ".wav":
        try:
            with wave.open(path, "rb") as wf:
                rate = wf.getframerate()
                if rate and (
                    wf.getnframes() / float(rate) < 1.5
                    or size < WAV_MIN_SIZE
                ):
                    return False
        except Exception:
            return False
    return True

def get_track_duration(path):
    if HAS_TINYTAG:
        try:
            tag = TinyTag.get(path)
            if tag and tag.duration and tag.duration > 0:
                return float(tag.duration)
        except Exception:
            pass
    return 0.0

# ============================================================
# ОСНОВНОЙ КЛАСС ПЛЕЕРА
# ============================================================
class MusicPlayer:
    def __init__(self, root):
        self.root = root
        self.root.title(APP_NAME)
        self.root.geometry("1120x720")
        self.root.minsize(1120, 720)
        self.root.maxsize(1120, 720)
        self.root.resizable(False, False)
        self.root.configure(bg=BG)
        
        self._set_icon()
        
        pygame.mixer.init()
        pygame.mixer.music.set_volume(0.75)
        
        self.playlist = []
        self.current = -1
        self.is_playing = False
        self.is_paused = False
        self.duration = 0.0
        self.position = 0.0
        self.last_sync_time = 0.0
        self.scan_thread = None
        self._seeking = False
        self._filtered_indices = []
        self._search_after = None
        self._scan_cancelled = False
        self.volume_value = 0.75
        
        self._style = ttk.Style(self.root)
        self._style.theme_use("clam")
        self._style.layout(
            "Noir.Vertical.TScrollbar",
            [("Vertical.Scrollbar.trough", {
                "children": [
                    ("Vertical.Scrollbar.thumb", {
                        "expand": "1",
                        "sticky": "ns"
                    })
                ],
                "sticky": "ns"
            })]
        )
        self._style.configure(
            "Noir.Vertical.TScrollbar",
            troughcolor=PANEL,
            background="#3d3450",
            lightcolor="#3d3450",
            darkcolor="#3d3450",
            bordercolor=PANEL,
            width=10,
            gripcount=0
        )
        self._style.map(
            "Noir.Vertical.TScrollbar",
            background=[
                ("pressed", ACCENT_DARK),
                ("active", ACCENT_DARK)
            ]
        )
        
        self._build_ui()
        self._load_playlist()
        self._refresh_list()
        self._tick()
        self.root.protocol("WM_DELETE_WINDOW", self.close)
    
    def _set_icon(self):
        """Ищет icon.ico рядом с exe/скриптом и устанавливает его."""
        app_dir = _get_app_dir()
        icon_path = os.path.join(app_dir, "icon.ico")
        
        try:
            if os.path.isfile(icon_path):
                self.root.iconbitmap(default=icon_path)
                try:
                    self.root.iconbitmap(icon_path)
                except Exception:
                    pass
        except Exception:
            pass
        
        try:
            if sys.platform == "win32":
                import ctypes
                ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(
                    "NoirMusic.Player.1.0"
                )
        except Exception:
            pass
    
    def _label(self, parent, text, color=TEXT, size=10, weight="normal", **kwargs):
        return tk.Label(
            parent,
            text=text,
            bg=kwargs.pop("bg", PANEL),
            fg=color,
            font=("Segoe UI", size, weight),
            **kwargs
        )
    
    def _button(self, parent, text, command, primary=False, width=None):
        bg = ACCENT_DARK if primary else PANEL_ALT
        fg = "#ffffff" if primary else TEXT
        active_bg = ACCENT if primary else PANEL_HOVER
        btn = tk.Button(
            parent,
            text=text,
            command=command,
            bg=bg,
            fg=fg,
            activebackground=active_bg,
            activeforeground="#ffffff",
            relief="flat",
            bd=0,
            cursor="hand2",
            font=("Segoe UI", 9, "bold"),
            padx=15,
            pady=9,
            width=width,
            highlightthickness=0
        )
        return btn
    
    def _build_ui(self):
        header = tk.Frame(self.root, bg=BG, padx=34, pady=22)
        header.pack(fill="x")
        
        brand = tk.Frame(header, bg=BG)
        brand.pack(side="left")
        self._label(brand, "МАЛЫШ", ACCENT, 21, "bold", bg=BG).pack(side="left")
        self._label(
            header,
            "ВАШЕ ЛИЧНОЕ МУЗЫКАЛЬНОЕ ПРОСТРАНСТВО",
            MUTED,
            8,
            "bold",
            bg=BG
        ).pack(side="left", padx=22, pady=(8, 0))
        
        self.header_status = self._label(header, "●  ГОТОВ", GREEN, 9, "bold", bg=BG)
        self.header_status.pack(side="right", pady=7)
        
        body = tk.Frame(self.root, bg=BG, padx=28, pady=4)
        body.pack(fill="both", expand=True)
        body.grid_columnconfigure(0, weight=3, minsize=440)
        body.grid_columnconfigure(1, weight=2, minsize=330)
        body.grid_rowconfigure(0, weight=1)
        
        player = tk.Frame(
            body,
            bg=PANEL,
            highlightthickness=1,
            highlightbackground=BORDER
        )
        player.grid(row=0, column=0, sticky="nsew", padx=(0, 12))
        player.grid_columnconfigure(0, weight=1)
        
        top = tk.Frame(player, bg=PANEL, padx=28, pady=24)
        top.grid(row=0, column=0, sticky="ew")
        self._label(top, "СЕЙЧАС ИГРАЕТ", MUTED, 8, "bold").pack(anchor="w")
        
        visual = tk.Canvas(
            player,
            height=205,
            bg="#0d0d12",
            highlightthickness=0,
            bd=0
        )
        visual.grid(row=1, column=0, sticky="ew", padx=28)
        self.visual = visual
        visual.bind("<Configure>", self._draw_visual)
        
        track_info = tk.Frame(player, bg=PANEL, height=92)
        track_info.grid(row=2, column=0, sticky="ew", padx=30, pady=(24, 3))
        track_info.grid_propagate(False)
        track_info.grid_columnconfigure(0, weight=1)
        track_info.grid_rowconfigure(0, weight=1)
        
        self.lbl_title = self._label(
            track_info,
            "Выберите трек",
            TEXT,
            20,
            "bold",
            wraplength=520,
            justify="left",
            anchor="nw"
        )
        self.lbl_title.config(height=2)
        self.lbl_title.grid(row=0, column=0, sticky="nw")
        
        self.lbl_artist = self._label(track_info, "Ваша библиотека ждёт", MUTED, 10)
        self.lbl_artist.grid(row=1, column=0, sticky="sw")
        
        timeline = tk.Frame(player, bg=PANEL, padx=30)
        timeline.grid(row=3, column=0, sticky="ew", pady=(28, 0))
        timeline.grid_columnconfigure(0, weight=1)
        
        times = tk.Frame(timeline, bg=PANEL)
        times.grid(row=0, column=0, sticky="ew")
        
        self.lbl_current = self._label(times, "00:00", MUTED, 9, "normal")
        self.lbl_current.pack(side="left")
        
        self.lbl_total = self._label(times, "00:00", MUTED, 9, "normal")
        self.lbl_total.config(width=8, anchor="e")
        self.lbl_total.pack(side="right")
        
        self.timeline = tk.Canvas(
            timeline,
            height=28,
            bg=PANEL,
            highlightthickness=0,
            cursor="hand2"
        )
        self.timeline.grid(row=1, column=0, sticky="ew", pady=(5, 0))
        self.timeline.bind("<Configure>", lambda _e: self._draw_timeline())
        self.timeline.bind("<Button-1>", self._timeline_press)
        self.timeline.bind("<B1-Motion>", self._timeline_drag)
        self.timeline.bind("<ButtonRelease-1>", self._timeline_release)
        
        controls = tk.Frame(player, bg=PANEL, pady=23)
        controls.grid(row=4, column=0)
        
        self._button(controls, "⏮", self.prev_track).pack(side="left", padx=8)
        
        self.btn_play = tk.Button(
            controls,
            text="▶",
            command=self.toggle_play,
            bg=ACCENT,
            fg="#17131f",
            activebackground=ACCENT_HOVER,
            activeforeground="#17131f",
            relief="flat",
            bd=0,
            cursor="hand2",
            font=("Segoe UI", 17, "bold"),
            width=4,
            height=1,
            padx=8,
            pady=5,
            highlightthickness=0
        )
        self.btn_play.pack(side="left", padx=12)
        
        self._button(controls, "⏭", self.next_track).pack(side="left", padx=8)
        self._button(controls, "■", self.stop).pack(side="left", padx=8)
        
        volume_row = tk.Frame(player, bg=PANEL, padx=30, pady=15)
        volume_row.grid(row=5, column=0, sticky="ew")
        
        self._label(volume_row, "ГРОМКОСТЬ", MUTED, 8, "bold").pack(side="left")
        self._label(volume_row, "−", MUTED, 12, "bold").pack(side="left", padx=(20, 7))
        
        self.volume_canvas = tk.Canvas(
            volume_row,
            height=22,
            width=150,
            bg=PANEL,
            highlightthickness=0,
            cursor="hand2"
        )
        self.volume_canvas.pack(side="left", fill="x", expand=True)
        self.volume_canvas.bind("<Configure>", lambda _e: self._draw_volume())
        self.volume_canvas.bind("<Button-1>", self._volume_click)
        self.volume_canvas.bind("<B1-Motion>", self._volume_click)
        
        self.lbl_volume = self._label(volume_row, "75%", TEXT, 9, "bold")
        self.lbl_volume.pack(side="left", padx=(10, 0))
        
        library = tk.Frame(
            body,
            bg=PANEL,
            highlightthickness=1,
            highlightbackground=BORDER
        )
        library.grid(row=0, column=1, sticky="nsew", padx=(12, 0))
        library.grid_columnconfigure(0, weight=1)
        library.grid_rowconfigure(3, weight=1)
        
        lib_head = tk.Frame(library, bg=PANEL, padx=22, pady=22, height=70)
        lib_head.grid(row=0, column=0, sticky="ew")
        lib_head.grid_propagate(False)
        lib_head.grid_columnconfigure(0, weight=1)
        
        self._label(lib_head, "Ваша библиотека", TEXT, 16, "bold").pack(side="left")
        
        self.lbl_count = self._label(lib_head, "0 ТРЕКОВ", MUTED, 8, "bold")
        self.lbl_count.config(width=10, anchor="e")
        self.lbl_count.pack(side="right", pady=6)
        
        search_wrap = tk.Frame(library, bg=PANEL_ALT, padx=12, pady=2)
        search_wrap.grid(row=1, column=0, sticky="ew", padx=20, pady=(0, 12))
        
        self.search_var = tk.StringVar()
        self.search_entry = tk.Entry(
            search_wrap,
            textvariable=self.search_var,
            bg=PANEL_ALT,
            fg=TEXT,
            insertbackground=TEXT,
            relief="flat",
            bd=0,
            font=("Segoe UI", 10)
        )
        self.search_entry.pack(fill="x", ipady=9)
        self.search_entry.insert(0, "")
        self.search_var.trace_add("write", self._on_search)
        
        list_wrap = tk.Frame(library, bg=PANEL)
        list_wrap.grid(row=3, column=0, sticky="nsew", padx=14, pady=4)
        list_wrap.grid_columnconfigure(0, weight=1)
        list_wrap.grid_rowconfigure(0, weight=1)
        
        self.listbox = tk.Listbox(
            list_wrap,
            bg=PANEL,
            fg="#d4d4dc",
            selectbackground="#33284b",
            selectforeground="#ffffff",
            activestyle="none",
            highlightthickness=0,
            bd=0,
            font=("Segoe UI", 9),
            height=15,
            selectborderwidth=0
        )
        self.listbox.grid(row=0, column=0, sticky="nsew")
        
        scrollbar = ttk.Scrollbar(
            list_wrap,
            orient="vertical",
            command=self.listbox.yview,
            style="Noir.Vertical.TScrollbar"
        )
        scrollbar.grid(row=0, column=1, sticky="ns", padx=(8, 0))
        self.listbox.configure(yscrollcommand=scrollbar.set)
        
        self.listbox.bind("<Double-Button-1>", self._play_selected)
        self.listbox.bind("<Return>", self._play_selected)
        self.listbox.bind("<Button-3>", self._context_menu)
        
        self.menu = tk.Menu(
            self.root,
            tearoff=0,
            bg=PANEL_ALT,
            fg=TEXT,
            activebackground=ACCENT_DARK,
            activeforeground="#ffffff",
            bd=0,
            relief="flat"
        )
        self.menu.add_command(label="Воспроизвести", command=self._play_selected)
        self.menu.add_command(label="Удалить из плейлиста", command=self._remove_selected)
        
        library_actions = tk.Frame(library, bg=PANEL, padx=18, pady=18)
        library_actions.grid(row=4, column=0, sticky="ew")
        library_actions.grid_columnconfigure(0, weight=1)
        library_actions.grid_columnconfigure(1, weight=1)
        
        self._button(
            library_actions,
            "+  Добавить треки",
            self.add_files,
            primary=True
        ).grid(row=0, column=0, sticky="ew", padx=(0, 5))
        
        self._button(
            library_actions,
            "Сканировать папку",
            self.scan_folder
        ).grid(row=0, column=1, sticky="ew", padx=(5, 0))
        
        bottom = tk.Frame(self.root, bg=BG, padx=30, pady=13)
        bottom.pack(fill="x")
        
        self.status = self._label(bottom, "Готов к работе.", MUTED, 8, bg=BG)
        self.status.pack(side="left")
        
        self._button(bottom, "Очистить библиотеку", self.clear).pack(side="right")
    
    def _draw_visual(self, _event=None):
        if not hasattr(self, "visual"):
            return
        c = self.visual
        c.delete("all")
        w = max(c.winfo_width(), 1)
        h = max(c.winfo_height(), 1)
        
        c.create_oval(w * 0.34, h * -0.55, w * 0.82, h * 1.35, fill="#171321", outline="")
        c.create_oval(w * 0.44, h * -0.25, w * 0.72, h * 1.05, fill="#201832", outline="")
        
        bars = [18, 30, 48, 68, 92, 62, 42, 76, 54, 34, 64, 44, 24]
        gap = 8
        total = len(bars) * 9 + (len(bars) - 1) * gap
        start = (w - total) / 2
        
        for i, height in enumerate(bars):
            x = start + i * (9 + gap)
            y = (h - height) / 2
            c.create_rectangle(
                x, y, x + 9, y + height,
                fill=ACCENT if i in (4, 5, 6, 7, 8) else "#59477a",
                outline="",
                width=0
            )
        
        c.create_text(
            22, h - 22,
            text="М А Л Ы Ш   /   А У Д И О",
            fill="#777080",
            anchor="w",
            font=("Segoe UI", 8, "bold")
        )
    
    def _draw_timeline(self):
        c = self.timeline
        c.delete("all")
        w = max(c.winfo_width(), 1)
        y = 13
        pad = 2
        ratio = 0.0
        
        if self.duration > 0:
            ratio = max(0.0, min(1.0, self._get_current_position() / self.duration))
        
        c.create_line(pad, y, w - pad, y, fill="#34313b", width=4, capstyle="round")
        x = pad + (w - 2 * pad) * ratio
        
        if ratio > 0:
            c.create_line(pad, y, x, y, fill=ACCENT, width=4, capstyle="round")
        
        c.create_oval(x - 5, y - 5, x + 5, y + 5, fill=ACCENT, outline="#e9ddff", width=1)
    
    def _draw_volume(self):
        c = self.volume_canvas
        c.delete("all")
        w = max(c.winfo_width(), 1)
        y = 11
        x = 3 + (w - 6) * self.volume_value
        
        c.create_line(3, y, w - 3, y, fill="#34313b", width=4, capstyle="round")
        c.create_line(3, y, x, y, fill=ACCENT, width=4, capstyle="round")
        c.create_oval(x - 4, y - 4, x + 4, y + 4, fill=ACCENT, outline="")
    
    def _volume_click(self, event):
        w = max(self.volume_canvas.winfo_width(), 1)
        self.volume_value = max(0.0, min(1.0, (event.x - 3) / max(w - 6, 1)))
        pygame.mixer.music.set_volume(self.volume_value)
        self.lbl_volume.config(text=f"{int(self.volume_value * 100)}%")
        self._draw_volume()
    
    def _save_playlist(self):
        try:
            os.makedirs(APP_DIR, exist_ok=True)
            temp = PLAYLIST_FILE + ".tmp"
            with open(temp, "w", encoding="utf-8") as f:
                json.dump({"tracks": self.playlist}, f, ensure_ascii=False, indent=2)
            os.replace(temp, PLAYLIST_FILE)
        except OSError as e:
            self.status.config(text=f"Не удалось сохранить плейлист: {e}")
    
    def _load_playlist(self):
        legacy_file = os.path.join(
            os.environ.get("APPDATA", os.path.expanduser("~")),
            "HarmonyMusic",
            "playlist.json"
        )
        source_file = PLAYLIST_FILE if os.path.isfile(PLAYLIST_FILE) else legacy_file
        
        try:
            with open(source_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            tracks = data.get("tracks", []) if isinstance(data, dict) else []
            self.playlist = [
                p for p in tracks
                if isinstance(p, str) and os.path.isfile(p)
            ]
            if source_file == legacy_file and self.playlist:
                self._save_playlist()
        except FileNotFoundError:
            pass
        except (OSError, ValueError):
            self.status.config(text="Не удалось прочитать файл плейлиста.")
    
    def _refresh_list(self):
        query = self.search_var.get().strip().lower() if hasattr(self, "search_var") else ""
        view = self.listbox.yview()
        self.listbox.delete(0, "end")
        self._filtered_indices = []
        
        for index, path in enumerate(self.playlist):
            name = os.path.basename(path)
            if query and (query not in name.lower() and query not in path.lower()):
                continue
            self._filtered_indices.append(index)
            marker = "  ▶   " if index == self.current else "      "
            self.listbox.insert("end", marker + name)
            if index == self.current:
                self.listbox.itemconfig("end", fg=ACCENT)
        
        self.lbl_count.config(text=f"{len(self.playlist)} ТРЕКОВ")
        self.listbox.yview_moveto(view[0])
    
    def _on_search(self, *_args):
        if self._search_after:
            self.root.after_cancel(self._search_after)
        self._search_after = self.root.after(200, self._refresh_list)
    
    def _add_tracks(self, paths):
        known = {os.path.normcase(os.path.abspath(p)) for p in self.playlist}
        added = 0
        
        for path in paths:
            key = os.path.normcase(os.path.abspath(path))
            if key not in known and os.path.isfile(path):
                self.playlist.append(path)
                known.add(key)
                added += 1
        
        self._refresh_list()
        self._save_playlist()
        return added
    
    def _selected_real_index(self):
        selection = self.listbox.curselection()
        if not selection:
            return None
        row = int(selection[0])
        if 0 <= row < len(self._filtered_indices):
            return self._filtered_indices[row]
        return None
    
    def set_volume(self, value):
        self.volume_value = max(0.0, min(1.0, float(value) / 100))
        pygame.mixer.music.set_volume(self.volume_value)
        self.lbl_volume.config(text=f"{int(self.volume_value * 100)}%")
        self._draw_volume()
    
    def play_index(self, index):
        if not 0 <= index < len(self.playlist):
            return
        path = self.playlist[index]
        
        try:
            pygame.mixer.music.stop()
            pygame.mixer.music.load(path)
            pygame.mixer.music.play()
            self.current = index
            self.is_playing = True
            self.is_paused = False
            self.position = 0.0
            self.last_sync_time = time.monotonic()
            self.duration = get_track_duration(path)
            
            if not self.duration or self.duration <= 0:
                try:
                    self.duration = float(pygame.mixer.music.get_length())
                except Exception:
                    self.duration = 0.0
            
            self.lbl_title.config(text=os.path.splitext(os.path.basename(path))[0])
            folder = os.path.basename(os.path.dirname(path)) or "Локальное аудио"
            self.lbl_artist.config(text=folder)
            self.lbl_current.config(text="00:00")
            self.lbl_total.config(text=format_time(self.duration))
            self.header_status.config(text="●  ВОСПРОИЗВЕДЕНИЕ", fg=ACCENT)
            self.btn_play.config(text="||")
            self._refresh_list()
            self._draw_timeline()
            self.status.config(text=f"Воспроизведение  ·  {os.path.basename(path)}")
            
        except pygame.error as e:
            removed = self.playlist.pop(index)
            if self.current > index:
                self.current -= 1
            self._save_playlist()
            self._refresh_list()
            self.status.config(text=f"Файл недоступен: {os.path.basename(removed)} ({e})")
            
            if self.playlist:
                self.next_track()
            else:
                self.stop()
    
    def toggle_play(self):
        if self.current == -1:
            if self.playlist:
                self.play_index(0)
            else:
                self.status.config(text="Добавьте музыку для начала работы.")
            return
        
        if self.is_paused:
            pygame.mixer.music.unpause()
            self.is_paused = False
            self.last_sync_time = time.monotonic()
            self.btn_play.config(text="||")
            self.header_status.config(text="●  ВОСПРОИЗВЕДЕНИЕ", fg=ACCENT)
            self.status.config(text="Воспроизведение возобновлено.")
            return
        
        if self.is_playing:
            self.position = self._get_current_position()
            pygame.mixer.music.pause()
            self.is_paused = True
            self.btn_play.config(text="▶")
            self.header_status.config(text="●  ПАУЗА", fg=MUTED)
            self.status.config(text="Воспроизведение приостановлено.")
            return
        
        self.play_index(self.current)
    
    def stop(self):
        pygame.mixer.music.stop()
        self.is_playing = False
        self.is_paused = False
        self.position = 0.0
        self.last_sync_time = time.monotonic()
        self.lbl_current.config(text="00:00")
        self.btn_play.config(text="▶")
        self.header_status.config(text="●  ГОТОВ", fg=GREEN)
        self._draw_timeline()
        self.status.config(text="Воспроизведение остановлено.")
    
    def next_track(self):
        if self.playlist:
            self.play_index((self.current + 1) % len(self.playlist))
    
    def prev_track(self):
        if self.playlist:
            self.play_index(self.current - 1 if self.current > 0 else len(self.playlist) - 1)
    
    def _play_selected(self, _event=None):
        index = self._selected_real_index()
        if index is not None:
            self.play_index(index)
    
    def _context_menu(self, event):
        row = self.listbox.nearest(event.y)
        if 0 <= row < len(self._filtered_indices):
            self.listbox.selection_clear(0, "end")
            self.listbox.selection_set(row)
            self.menu.tk_popup(event.x_root, event.y_root)
    
    def _remove_selected(self):
        index = self._selected_real_index()
        if index is None:
            return
        removed = self.playlist[index]
        
        if index == self.current:
            self.stop()
            self.current = -1
            self.duration = 0.0
            self.lbl_title.config(text="Выберите трек")
            self.lbl_artist.config(text="Ваша библиотека ждёт")
            self.lbl_total.config(text="00:00")
        elif index < self.current:
            self.current -= 1
        
        self.playlist.pop(index)
        self._refresh_list()
        self._save_playlist()
        self.status.config(text=f"Удалено  ·  {os.path.basename(removed)}")
    
    def _get_current_position(self):
        if self.is_paused:
            return self.position
        if self.is_playing:
            elapsed = time.monotonic() - self.last_sync_time
            if self.duration > 0:
                return min(self.duration, self.position + elapsed)
            return self.position + elapsed
        return self.position
    
    def _position_from_x(self, x):
        width = max(self.timeline.winfo_width() - 4, 1)
        ratio = max(0.0, min(1.0, (x - 2) / width))
        return ratio * self.duration
    
    def _timeline_press(self, event):
        if self.duration <= 0 or self.current < 0:
            return
        self._seeking = True
        self._preview_seek(event.x)
    
    def _timeline_drag(self, event):
        if self._seeking:
            self._preview_seek(event.x)
    
    def _preview_seek(self, x):
        target = self._position_from_x(x)
        self.lbl_current.config(text=format_time(target))
        self._draw_timeline_at(target)
    
    def _draw_timeline_at(self, position):
        c = self.timeline
        c.delete("all")
        w = max(c.winfo_width(), 1)
        y = 13
        pad = 2
        ratio = max(0.0, min(1.0, position / self.duration)) if self.duration > 0 else 0
        x = pad + (w - 2 * pad) * ratio
        
        c.create_line(pad, y, w - pad, y, fill="#34313b", width=4, capstyle="round")
        
        if ratio > 0:
            c.create_line(pad, y, x, y, fill=ACCENT, width=4, capstyle="round")
        
        c.create_oval(x - 5, y - 5, x + 5, y + 5, fill=ACCENT, outline="#e9ddff", width=1)
    
    def _timeline_release(self, event):
        if not self._seeking:
            return
        self._seeking = False
        
        if self.duration <= 0 or self.current < 0:
            return
        
        target = self._position_from_x(event.x)
        self._seek_to(target)
    
    def _seek_to(self, target):
        path = self.playlist[self.current]
        was_paused = self.is_paused
        was_playing = self.is_playing
        
        try:
            pygame.mixer.music.stop()
            pygame.mixer.music.load(path)
            
            try:
                pygame.mixer.music.play(start=max(0.0, target))
            except (pygame.error, TypeError):
                pygame.mixer.music.play()
                if target > 0:
                    pygame.mixer.music.set_pos(target)
            
            self.position = target
            self.last_sync_time = time.monotonic()
            self.is_playing = was_playing or was_paused
            
            if was_paused:
                pygame.mixer.music.pause()
                self.is_paused = True
                self.btn_play.config(text="▶")
                self.header_status.config(text="●  ПАУЗА", fg=MUTED)
            else:
                self.is_paused = False
                self.btn_play.config(text="||")
                self.header_status.config(text="●  ВОСПРОИЗВЕДЕНИЕ", fg=ACCENT)
            
            self.lbl_current.config(text=format_time(target))
            self._draw_timeline_at(target)
            
        except pygame.error as e:
            self.status.config(text=f"Перемотка не поддерживается: {e}")
            self._draw_timeline()
    
    def _tick(self):
        try:
            if self.is_playing and not self.is_paused:
                current_pos = self._get_current_position()
                
                if not self._seeking:
                    self.lbl_current.config(text=format_time(current_pos))
                    self._draw_timeline_at(current_pos)
                
                if not pygame.mixer.music.get_busy():
                    if self.duration > 0 and current_pos >= self.duration * 0.96:
                        self.next_track()
                    elif current_pos < 1.0:
                        pass
                    else:
                        self.is_playing = False
                        self.is_paused = False
                        self.btn_play.config(text="▶")
                        self.header_status.config(text="●  ГОТОВ", fg=GREEN)
        except Exception:
            pass
        
        self.root.after(150, self._tick)
    
    def add_files(self):
        paths = filedialog.askopenfilenames(
            title="Добавить музыку",
            filetypes=[("Аудиофайлы", "*.mp3 *.wav *.ogg *.flac")]
        )
        
        if paths:
            valid = [p for p in paths if _is_valid_audio(p)]
            added = self._add_tracks(valid)
            self.status.config(text=f"Добавлено {added} новых треков.")
    
    def scan_folder(self):
        if self.scan_thread and self.scan_thread.is_alive():
            self.status.config(text="Сканирование папки уже запущено.")
            return
        
        folder = filedialog.askdirectory(title="Выберите папку с музыкой")
        if not folder:
            return
        
        self._scan_cancelled = False
        self.status.config(text="Сканирование папки с музыкой…")
        self.scan_thread = threading.Thread(
            target=self._scan_worker,
            args=(folder,),
            daemon=True
        )
        self.scan_thread.start()
    
    def _scan_worker(self, folder):
        found = []
        
        for root_dir, dirs, files in os.walk(folder):
            if self._scan_cancelled:
                break
            
            dirs[:] = [d for d in dirs if d.lower() not in EXCLUDED_FOLDERS]
            
            if _is_system(root_dir):
                dirs[:] = []
                continue
            
            for name in files:
                path = os.path.join(root_dir, name)
                if _is_valid_audio(path):
                    found.append(path)
        
        try:
            self.root.after(0, lambda items=found: self._finish_scan(items))
        except RuntimeError:
            pass
    
    def _finish_scan(self, found):
        added = self._add_tracks(found)
        self.status.config(
            text=f"Сканирование завершено  ·  Найдено {len(found)}, добавлено {added}"
        )
    
    def clear(self):
        if not self.playlist:
            return
        
        if not messagebox.askyesno("Очистить библиотеку", "Удалить все треки из плейлиста?"):
            return
        
        self.stop()
        self.playlist.clear()
        self.current = -1
        self.duration = 0.0
        self._refresh_list()
        self._save_playlist()
        self.lbl_title.config(text="Выберите трек")
        self.lbl_artist.config(text="Ваша библиотека ждёт")
        self.lbl_total.config(text="00:00")
        self._draw_timeline()
        self.header_status.config(text="●  ГОТОВ", fg=GREEN)
        self.status.config(text="Библиотека очищена.")
    
    def close(self):
        self._scan_cancelled = True
        self._save_playlist()
        
        try:
            pygame.mixer.music.stop()
            pygame.mixer.quit()
        except Exception:
            pass
        
        self.root.destroy()

# ============================================================
# ЗАПУСК
# ============================================================
if __name__ == "__main__":
    try:
        root = tk.Tk()
        app = MusicPlayer(root)
        root.mainloop()
    except pygame.error as error:
        messagebox.showerror("Ошибка аудиосистемы", str(error))