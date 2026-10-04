#!/usr/bin/env python3
import argparse
import base64
import json
import mimetypes
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlparse

try:
    import yaml
except ImportError:  # pragma: no cover - PyYAML is expected on the target dev box.
    yaml = None


ROOT = Path(__file__).resolve().parents[1]
STATIC_DIR = Path(__file__).resolve().parent / "static"
CONFIG_PATH = Path(os.environ.get("GOOFYDECK_CONFIG", ROOT / "config" / "configuration.yml")).resolve()
PAGING_SOCKET = os.environ.get("GOOFYDECK_PAGING_SOCKET", "/tmp/goofydeck_paging_control.sock")
HA_SOCKET = os.environ.get("GOOFYDECK_HA_SOCKET", "/tmp/goofydeck_ha.sock")
SESSION_DIR = Path(os.environ.get("GOOFYDECK_SESSION_DIR", "/dev/shm/goofydeck/paging"))
BUTTON_COUNT = 14
EVENTS = ("TAP", "HOLD", "LONGHOLD", "RELEASED")


def read_text(path):
    return Path(path).read_text(encoding="utf-8")


def write_text_atomic(path, content):
    path = Path(path)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(content, encoding="utf-8")
    tmp.replace(path)


def socket_command(command, timeout=2.0):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        sock.connect(PAGING_SOCKET)
        sock.sendall((command.rstrip("\n") + "\n").encode("utf-8"))
        data = sock.recv(4096)
    return data.decode("utf-8", errors="replace").strip()


def unix_socket_command(path, command, timeout=2.0, read_size=65536):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        sock.connect(path)
        sock.sendall((command.rstrip("\n") + "\n").encode("utf-8"))
        buffer = b""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data = sock.recv(read_size)
            if not data:
                break
            buffer += data
            while b"\n" in buffer:
                raw, buffer = buffer.split(b"\n", 1)
                line = raw.decode("utf-8", errors="replace").strip()
                if line.startswith("ok") or line.startswith("err"):
                    return line
    return buffer.decode("utf-8", errors="replace").strip()


def daemon_online():
    try:
        socket_command("ping", timeout=0.5)
        return True
    except OSError:
        return False


def safe_page_tag(page):
    if page.startswith("$"):
        page = page[1:]
    return re.sub(r"[^a-zA-Z0-9_]", "_", page)


def nav_page_tag(page):
    if page == "$root":
        return "root"
    return safe_page_tag(page) or "page"


def fnv1a32(text):
    h = 2166136261
    for byte in text.encode("utf-8"):
        h ^= byte
        h = (h * 16777619) & 0xFFFFFFFF
    return h


def item_file_hash(page, item_index):
    return fnv1a32(f"page:{page}\nbutton:{item_index + 1}\n")


def run_tool(args, timeout=15):
    try:
        subprocess.run(args, cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=timeout)
        return True
    except (OSError, subprocess.SubprocessError):
        return False


def first_preset_name(button):
    presets = button.get("presets") if isinstance(button, dict) else None
    if isinstance(presets, list) and presets:
        return str(presets[0])
    if isinstance(presets, str) and presets:
        return presets
    return "default"


def get_preset(parsed, name):
    presets = parsed.get("presets") if isinstance(parsed, dict) else {}
    preset = presets.get(name) if isinstance(presets, dict) else None
    if isinstance(preset, dict):
        return preset
    default = presets.get("default") if isinstance(presets, dict) else None
    return default if isinstance(default, dict) else {}


def page_config(parsed, page):
    pages = parsed.get("pages") if isinstance(parsed, dict) else {}
    cfg = pages.get(page) if isinstance(pages, dict) else None
    return cfg if isinstance(cfg, dict) else {}


def system_positions(parsed):
    system = parsed.get("system_buttons") if isinstance(parsed, dict) else {}
    def pos(key, default):
        value = system.get(key) if isinstance(system, dict) else None
        if isinstance(value, dict):
            try:
                return int(value.get("position", default))
            except (TypeError, ValueError):
                return default
        return default
    return {"back": pos("$page.back", 11), "prev": pos("$page.previous", 12), "next": pos("$page.next", 13)}


def compute_sheet_layout(total_items, show_back, desired_offset):
    base_slots = 13 - (1 if show_back else 0)
    if base_slots <= 0:
        base_slots = 1
    if total_items <= base_slots:
        return {"start": 0, "cap": base_slots, "show_prev": False, "show_next": False}

    starts = []
    start = 0
    idx = 0
    while start < total_items and len(starts) < 256:
        prev = idx > 0
        cap_last = base_slots - (1 if prev else 0)
        if cap_last <= 0:
            cap_last = 1
        cap_next = base_slots - (1 if prev else 0) - 1
        if cap_next <= 0:
            cap_next = 1
        next_visible = start + cap_last < total_items
        cap = cap_next if next_visible else cap_last
        starts.append((start, cap, prev, next_visible))
        start += cap
        idx += 1

    selected = 0
    for i, (sheet_start, _, _, _) in enumerate(starts):
        next_start = starts[i + 1][0] if i + 1 < len(starts) else total_items
        if desired_offset == sheet_start or (desired_offset >= sheet_start and desired_offset < next_start):
            selected = i
            break
    sheet_start, cap, prev, next_visible = starts[selected]
    return {"start": sheet_start, "cap": cap, "show_prev": prev, "show_next": next_visible}


def visible_slot(parsed, page, position, offset=None):
    cfg = page_config(parsed, page)
    buttons = cfg.get("buttons", [])
    if not isinstance(buttons, list):
        buttons = []
    show_back = page != "$root"
    layout = compute_sheet_layout(len(buttons), show_back, current_offset() if offset is None else offset)
    pos = system_positions(parsed)
    if show_back and position == pos["back"]:
        return {"kind": "nav", "nav": "page_back", "mdi": "mdi:arrow-left", "item_index": None}
    if layout["show_prev"] and position == pos["prev"]:
        return {"kind": "nav", "nav": "page_prev", "mdi": "mdi:chevron-left", "item_index": None}
    if layout["show_next"] and position == pos["next"]:
        return {"kind": "nav", "nav": "page_next", "mdi": "mdi:chevron-right", "item_index": None}

    reserved = set()
    if show_back:
        reserved.add(pos["back"])
    if layout["show_prev"]:
        reserved.add(pos["prev"])
    if layout["show_next"]:
        reserved.add(pos["next"])
    item_index = layout["start"]
    for slot in range(1, 14):
        if slot in reserved:
            continue
        if slot == position:
            if item_index < len(buttons):
                return {"kind": "button", "button": buttons[item_index], "item_index": item_index}
            return {"kind": "empty", "item_index": None}
        item_index += 1
    return {"kind": "empty", "item_index": None}


def current_page():
    path = SESSION_DIR / "last_page"
    if path.exists():
        return path.read_text(encoding="utf-8", errors="ignore").strip().splitlines()[0]
    return ""


def current_offset():
    path = SESSION_DIR / "last_offset"
    if path.exists():
        try:
            return int(path.read_text(encoding="utf-8", errors="ignore").strip())
        except ValueError:
            return 0
    return 0


def load_config():
    raw = read_text(CONFIG_PATH)
    if yaml is None:
        return raw, None
    parsed = yaml.safe_load(raw) or {}
    return raw, parsed


def pages_from_config(parsed):
    pages = parsed.get("pages") if isinstance(parsed, dict) else {}
    if not isinstance(pages, dict):
        return []
    result = []
    for name, page in pages.items():
        buttons = page.get("buttons", []) if isinstance(page, dict) else []
        result.append({"name": str(name), "buttonCount": len(buttons) if isinstance(buttons, list) else 0})
    return result


def normalize_button(button):
    if not isinstance(button, dict):
        return {"name": ""}
    action = button.get("tap_action")
    tap_action = ""
    tap_data = ""
    if isinstance(action, dict):
        tap_action = str(action.get("action", "") or "")
        data = action.get("data", "")
        if isinstance(data, dict):
            tap_data = str(data.get("cmd", "") or "")
        else:
            tap_data = str(data or "")
    return {
        "name": str(button.get("name", "") or ""),
        "icon": str(button.get("icon", "") or ""),
        "text": str(button.get("text", "") or ""),
        "entity_id": str(button.get("entity_id", "") or ""),
        "presets": button.get("presets", []) if isinstance(button.get("presets", []), list) else [],
        "tap_action": tap_action,
        "tap_data": tap_data,
        "states": button.get("states", {}) if isinstance(button.get("states", {}), dict) else {},
    }


def page_detail(parsed, page_name):
    pages = parsed.get("pages") if isinstance(parsed, dict) else {}
    page = pages.get(page_name, {}) if isinstance(pages, dict) else {}
    buttons = page.get("buttons", []) if isinstance(page, dict) else []
    normalized = []
    if isinstance(buttons, list):
        for index in range(BUTTON_COUNT):
            slot = visible_slot(parsed, page_name, index + 1)
            if slot["kind"] == "nav":
                item = {
                    "name": slot["nav"].replace("page_", "").upper(),
                    "icon": slot["mdi"],
                    "text": "",
                    "entity_id": "",
                    "presets": ["$nav"],
                    "tap_action": "$page." + ("back" if slot["nav"] == "page_back" else "previous" if slot["nav"] == "page_prev" else "next"),
                    "tap_data": "",
                    "states": {},
                    "system": True,
                }
            elif slot["kind"] == "button":
                item = normalize_button(slot["button"])
                item["item_index"] = slot["item_index"]
            else:
                item = normalize_button({})
            item["position"] = index + 1
            normalized.append(item)
    return {"name": page_name, "buttons": normalized}


def config_signature():
    parts = [str(CONFIG_PATH)]
    for path in (CONFIG_PATH, SESSION_DIR / "last_page", SESSION_DIR / "last_offset", SESSION_DIR / "current" / ".last_update"):
        try:
            stat = path.stat()
            parts.append(f"{path}:{stat.st_mtime_ns}:{stat.st_size}")
        except OSError:
            parts.append(f"{path}:missing")
    return base64.urlsafe_b64encode("|".join(parts).encode()).decode().rstrip("=")


def newest_png(paths):
    existing = [p for p in paths if p.exists() and p.is_file()]
    if not existing:
        return None
    return max(existing, key=lambda p: p.stat().st_mtime_ns)


def wallpaper_effective(parsed, page):
    global_cfg = parsed.get("global") if isinstance(parsed, dict) else {}
    if isinstance(global_cfg, dict) and bool(global_cfg.get("disable_wallpapers", False)):
        return None
    page_cfg = page_config(parsed, page)
    wp = page_cfg.get("wallpaper")
    if isinstance(wp, str):
        return {"path": wp, "quality": 30, "magnify": 100, "dithering": True}
    if isinstance(wp, dict):
        path = wp.get("path")
        if path:
            return {
                "path": str(path),
                "quality": int(wp.get("quality", 30) or 30),
                "magnify": int(wp.get("magnify", 100) or 100),
                "dithering": bool(wp.get("dithering", True)),
            }
        return None
    wp = parsed.get("wallpaper") if isinstance(parsed, dict) else None
    if isinstance(wp, str):
        return {"path": wp, "quality": 30, "magnify": 100, "dithering": True}
    if isinstance(wp, dict) and wp.get("path"):
        return {
            "path": str(wp.get("path")),
            "quality": int(wp.get("quality", 30) or 30),
            "magnify": int(wp.get("magnify", 100) or 100),
            "dithering": bool(wp.get("dithering", True)),
        }
    return None


def wallpaper_sig(wp):
    if not wp:
        return 0
    return fnv1a32(f"path:{wp['path']}\nq:{wp['quality']}\nm:{wp['magnify']}\nd:{1 if wp['dithering'] else 0}\n")


def wallpaper_render_dir(wp):
    if not wp or not wp.get("path"):
        return None, ""
    src = Path(wp["path"])
    if not src.is_absolute():
        src = ROOT / src
    if src.suffix.lower() != ".png" or not src.exists():
        return None, ""
    return src.parent / src.stem, src.stem


def ensure_wallpaper_tiles(wp):
    render_dir, prefix = wallpaper_render_dir(wp)
    if not render_dir:
        return None, ""
    if all((render_dir / f"{prefix}-{i}.png").exists() for i in range(1, 15)):
        return render_dir, prefix
    script = ROOT / "bin" / "render_image_page_wrapper.sh"
    if not script.exists():
        return None, ""
    src = Path(wp["path"])
    if not src.is_absolute():
        src = ROOT / src
    args = [str(script), f"-q={max(10, min(100, int(wp['quality'])))}", f"-m={max(10, min(100, int(wp['magnify'])))}"]
    if wp.get("dithering", True):
        args.append("-d")
    args.append(str(src))
    if run_tool(args, timeout=60) and all((render_dir / f"{prefix}-{i}.png").exists() for i in range(1, 15)):
        return render_dir, prefix
    return None, ""


def wallpaper_tile(parsed, page, position):
    wp = wallpaper_effective(parsed, page)
    render_dir, prefix = ensure_wallpaper_tiles(wp)
    if not render_dir:
        return None
    tile = render_dir / f"{prefix}-{position}.png"
    return tile if tile.exists() else None


def compose_over(base, overlay, cache_key=None):
    draw_over = ROOT / "icons" / "draw_over"
    if not draw_over.exists() or not base or not overlay or not Path(base).exists() or not Path(overlay).exists():
        return None, False
    if cache_key:
        cached = SESSION_DIR / "web_comp" / cache_key
        if cached.exists():
            return cached, False
        cached.parent.mkdir(parents=True, exist_ok=True)
        tmp = cached.with_suffix(".tmp.png")
        shutil.copyfile(base, tmp)
        if run_tool([str(draw_over), str(overlay), str(tmp)]):
            tmp.replace(cached)
            return cached, False
        tmp.unlink(missing_ok=True)
        return None, False
    fd, out = tempfile.mkstemp(prefix="goofydeck-web-", suffix=".png", dir="/tmp")
    os.close(fd)
    out_path = Path(out)
    shutil.copyfile(base, out_path)
    if run_tool([str(draw_over), str(overlay), str(out_path)]):
        return out_path, True
    out_path.unlink(missing_ok=True)
    return None, False


def apply_wallpaper(parsed, page, position, icon):
    wp = wallpaper_effective(parsed, page)
    if not wp or not icon:
        return icon, False
    tile = wallpaper_tile(parsed, page, position)
    if not tile:
        return icon, False
    if same_file(icon, tile):
        return tile, False
    key = f"{wallpaper_sig(wp):08x}/{position:02d}/{Path(icon).name}"
    composed, delete_after = compose_over(tile, icon, key)
    return (composed, delete_after) if composed else (icon, False)


def same_file(a, b):
    try:
        return Path(a).resolve() == Path(b).resolve()
    except OSError:
        return False


def state_from_ha(entity_id):
    if not entity_id:
        return None
    try:
        line = unix_socket_command(HA_SOCKET, f"get {entity_id}", timeout=1.5)
    except OSError:
        return None
    if not line.startswith("ok "):
        return None
    try:
        obj = json.loads(line[3:])
    except json.JSONDecodeError:
        return None
    state = str(obj.get("state", "") or "").lower()  # Normalize to lowercase
    unit = ""
    attrs = obj.get("attributes")
    if isinstance(attrs, dict):
        unit = str(attrs.get("unit_of_measurement", "") or "")
    return {"state": state, "unit": unit}


def value_display_entity(entity_id):
    domain = entity_id.split(".", 1)[0] if entity_id else ""
    return domain in {"sensor", "binary_sensor", "number", "input_number", "counter"}


def format_ha_value(ha):
    if not ha:
        return ""
    text = ha["state"]
    if ha.get("unit"):
        text = f"{text} {ha['unit']}"
    return text.strip()


def cache_icon_for_variant(page, item_index, variant=None):
    page_cache = ROOT / ".cache" / page
    h = item_file_hash(page, item_index)
    pos = item_index + 1
    
    # Try exact variant match first
    if variant:
        candidate = page_cache / f"{pos}-{h:08x}-{safe_page_tag(str(variant))}.png"
        if candidate.exists():
            return candidate
    
    # Try base icon
    candidate = page_cache / f"{pos}-{h:08x}.png"
    if candidate.exists():
        return candidate
    
    # Try any variant if we're looking for a specific one
    if variant:
        matches = sorted(page_cache.glob(f"{pos}-{h:08x}-*.png")) if page_cache.exists() else []
        return newest_png(matches)
    
    # Fallback to any icon for this position
    matches = sorted(page_cache.glob(f"{pos}-*.png")) if page_cache.exists() else []
    return newest_png(matches)


def nav_icon_path(parsed, page, position, nav_name):
    wp = wallpaper_effective(parsed, page)
    sig = wallpaper_sig(wp)
    nav_dir = ROOT / ".cache" / "nav" / nav_page_tag(page)
    if sig:
        candidate = nav_dir / f"{nav_name}_{sig:08x}_{position:02d}.png"
        if candidate.exists():
            return candidate, False
    found = newest_png(nav_dir.glob(f"{nav_name}_*_{position:02d}.png")) if nav_dir.exists() else None
    if found:
        return found, False
    # Last fallback: compose the pregenerated nav icon on the current wallpaper tile.
    pregen = ROOT / "assets" / "pregen" / f"{nav_name}.png"
    if not pregen.exists() and nav_name == "page_prev":
        pregen = ROOT / "assets" / "pregen" / "page_back.png"
    if pregen.exists():
        return apply_wallpaper(parsed, page, position, pregen)
    return wallpaper_tile(parsed, page, position), False


def render_text_on_base(parsed, button, base_png, text):
    draw_text = ROOT / "icons" / "draw_text"
    draw_opt = ROOT / "icons" / "draw_optimize"
    if not draw_text.exists() or not draw_opt.exists() or not base_png or not Path(base_png).exists() or not text:
        return None, False
    preset = get_preset(parsed, first_preset_name(button))
    fd, out = tempfile.mkstemp(prefix="goofydeck-web-text-", suffix=".png", dir="/tmp")
    os.close(fd)
    out_path = Path(out)
    shutil.copyfile(base_png, out_path)
    
    # Use default.json style as fallback
    color = str(preset.get("text_color", "FFFFFF") or "FFFFFF")
    align = str(preset.get("text_align", "bottom") or "bottom")  # Default to bottom like default.json
    font = str(preset.get("text_font", "Roboto") or "Roboto")
    size = int(preset.get("text_size", 12) or 12)  # Default to 12 like default.json
    offset = str(preset.get("text_offset", "0,0") or "0,0")
    
    args = [
        str(draw_text),
        f"--text={text}",
        f"--text_color={color}",
        f"--text_align={align}",
        f"--text_font={font}",
        f"--text_size={max(1, min(64, size))}",
        f"--text_offset={offset}",
        str(out_path),
    ]
    if not run_tool(args):
        args.remove(f"--text_font={font}")
        if not run_tool(args):
            out_path.unlink(missing_ok=True)
            return None, False
    run_tool([str(draw_opt), "-d", "-c=4", str(out_path)])
    return out_path, True


def icon_path(page, position):
    _, parsed = load_config()
    if parsed is None:
        return None, False
    page_tag = safe_page_tag(page)
    current = SESSION_DIR / "current" / f"{page_tag}_{position:02d}.png"
    
    slot = visible_slot(parsed, page, position)
    if slot["kind"] == "nav":
        return nav_icon_path(parsed, page, position, slot["nav"])

    if slot["kind"] == "button":
        button = slot["button"]
        item_index = slot["item_index"]
        states = button.get("states") if isinstance(button, dict) else {}
        entity_id = str(button.get("entity_id", "") or "")
        ha = state_from_ha(entity_id) if entity_id else None

        # Handle state-based icons (e.g., light on/off)
        if isinstance(states, dict) and states and ha:
            variant = ha["state"]
            # Try case-insensitive match first
            matched_state = None
            if variant:
                for state_key in states.keys():
                    if state_key.lower() == variant:
                        matched_state = state_key
                        break
                if matched_state and matched_state in states:
                    # Try to get icon for this specific state
                    icon = cache_icon_for_variant(page, item_index, matched_state)
                    if icon:
                        composed, delete_after = apply_wallpaper(parsed, page, position, icon)
                        if composed:
                            return composed, delete_after
            # Fallback to base icon if state-specific icon not found
            icon = cache_icon_for_variant(page, item_index, "base")
            if icon:
                composed, delete_after = apply_wallpaper(parsed, page, position, icon)
                if composed:
                    return composed, delete_after

        # Regular icon handling
        icon = cache_icon_for_variant(page, item_index)
        if icon and entity_id and value_display_entity(entity_id):
            text = format_ha_value(ha)
            if text:
                base, base_tmp = apply_wallpaper(parsed, page, position, icon)
                text_icon, text_tmp = render_text_on_base(parsed, button, base, text)
                if base_tmp:
                    Path(base).unlink(missing_ok=True)
                if text_icon:
                    return text_icon, text_tmp
        if icon:
            composed, delete_after = apply_wallpaper(parsed, page, position, icon)
            if composed:
                return composed, delete_after

    # Fallback to current session icon
    if current.exists():
        wp = wallpaper_effective(parsed, page)
        if wp:
            composed, delete_after = apply_wallpaper(parsed, page, position, current)
            if composed:
                return composed, delete_after
        return current, False

    page_cache = ROOT / ".cache" / page
    if page_cache.exists():
        matches = sorted(page_cache.glob(f"{position}-*.png"))
        found = newest_png(matches)
        if found:
            composed, delete_after = apply_wallpaper(parsed, page, position, found)
            if composed:
                return composed, delete_after

    # Fallback for names like $root on filesystems where directories may be literal.
    alt_cache = ROOT / ".cache" / page_tag
    if alt_cache.exists():
        matches = sorted(alt_cache.glob(f"{position}-*.png"))
        found = newest_png(matches)
        if found:
            composed, delete_after = apply_wallpaper(parsed, page, position, found)
            if composed:
                return composed, delete_after

    tile = wallpaper_tile(parsed, page, position)
    if tile:
        return tile, False
    return None, False


def update_button(page_name, position, payload):
    if yaml is None:
        raise RuntimeError("PyYAML is not installed; structured editing is unavailable")
    _, parsed = load_config()
    if not isinstance(parsed, dict):
        parsed = {}
    parsed.setdefault("pages", {})
    pages = parsed["pages"]
    if page_name not in pages or not isinstance(pages[page_name], dict):
        raise KeyError(f"Page '{page_name}' does not exist")
    page = pages[page_name]
    buttons = page.setdefault("buttons", [])
    if not isinstance(buttons, list):
        buttons = []
        page["buttons"] = buttons
    while len(buttons) < position:
        buttons.append({"name": ""})

    button = buttons[position - 1]
    if not isinstance(button, dict):
        button = {"name": ""}
        buttons[position - 1] = button

    for key in ("name", "icon", "text", "entity_id"):
        value = payload.get(key)
        if value is None:
            continue
        if str(value).strip():
            button[key] = str(value).strip()
        else:
            button.pop(key, None)

    presets = payload.get("presets")
    if isinstance(presets, list):
        clean = [str(p).strip() for p in presets if str(p).strip()]
        if clean:
            button["presets"] = clean
        else:
            button.pop("presets", None)

    tap_action = str(payload.get("tap_action", "") or "").strip()
    tap_data = str(payload.get("tap_data", "") or "").strip()
    if tap_action:
        action = {"action": tap_action}
        if tap_data:
            if tap_action.startswith("$cmd."):
                action["data"] = {"cmd": tap_data}
            else:
                action["data"] = tap_data
        button["tap_action"] = action
    elif "tap_action" in payload:
        button.pop("tap_action", None)

    rendered = yaml.safe_dump(parsed, allow_unicode=True, sort_keys=False, default_flow_style=False)
    write_text_atomic(CONFIG_PATH, rendered)
    try:
        socket_command("reload-config", timeout=5.0)
    except OSError:
        pass
    return normalize_button(button)


class Handler(BaseHTTPRequestHandler):
    server_version = "GoofyDeckWeb/0.1"

    def log_message(self, fmt, *args):
        # Only log errors and important messages, not every request
        if "200" in fmt % args or "304" in fmt % args:
            return  # Skip successful requests
        sys.stderr.write("[%s] %s\n" % (self.log_date_time_string(), fmt % args))

    def send_json(self, data, status=HTTPStatus.OK):
        body = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def read_json(self):
        length = int(self.headers.get("Content-Length", "0") or "0")
        if length <= 0:
            return {}
        return json.loads(self.rfile.read(length).decode("utf-8"))

    def do_GET(self):
        parsed_url = urlparse(self.path)
        path = parsed_url.path

        if path == "/":
            return self.serve_file(STATIC_DIR / "index.html")
        if path.startswith("/static/"):
            return self.serve_file(STATIC_DIR / path.removeprefix("/static/"))
        if path == "/api/status":
            raw, parsed = load_config()
            page = current_page()
            if not page and parsed:
                pages = pages_from_config(parsed)
                page = pages[0]["name"] if pages else ""
            return self.send_json({
                "online": daemon_online(),
                "socket": PAGING_SOCKET,
                "configPath": str(CONFIG_PATH),
                "sessionDir": str(SESSION_DIR),
                "currentPage": page,
                "offset": current_offset(),
                "pages": pages_from_config(parsed) if parsed else [],
                "parsed": parsed if parsed else {},
                "configMtime": CONFIG_PATH.stat().st_mtime_ns if CONFIG_PATH.exists() else 0,
                "signature": config_signature(),
                "pyyaml": yaml is not None,
            })
        if path == "/api/config":
            raw, parsed = load_config()
            return self.send_json({"raw": raw, "parsed": parsed, "pyyaml": yaml is not None})
        if path.startswith("/api/page/"):
            _, parsed = load_config()
            if parsed is None:
                return self.send_json({"error": "PyYAML is not available"}, HTTPStatus.SERVICE_UNAVAILABLE)
            page_name = unquote(path.removeprefix("/api/page/"))
            return self.send_json(page_detail(parsed, page_name))
        if path.startswith("/api/icon/"):
            parts = path.removeprefix("/api/icon/").split("/")
            if len(parts) != 2:
                return self.send_error(HTTPStatus.BAD_REQUEST)
            page = unquote(parts[0])
            try:
                position = int(parts[1])
            except ValueError:
                return self.send_error(HTTPStatus.BAD_REQUEST)
            found, delete_after = icon_path(page, position)
            if not found:
                return self.send_error(HTTPStatus.NOT_FOUND)
            return self.serve_file(found, cache=False, delete_after=delete_after)
        if path == "/api/events":
            return self.serve_events()
        return self.send_error(HTTPStatus.NOT_FOUND)

    def do_HEAD(self):
        parsed_url = urlparse(self.path)
        path = parsed_url.path
        if path == "/":
            target = STATIC_DIR / "index.html"
        elif path.startswith("/static/"):
            target = STATIC_DIR / path.removeprefix("/static/")
        elif path.startswith("/api/icon/"):
            parts = path.removeprefix("/api/icon/").split("/")
            if len(parts) != 2:
                return self.send_error(HTTPStatus.BAD_REQUEST)
            try:
                target, _ = icon_path(unquote(parts[0]), int(parts[1]))
            except ValueError:
                return self.send_error(HTTPStatus.BAD_REQUEST)
            if target is None:
                return self.send_error(HTTPStatus.NOT_FOUND)
        else:
            return self.send_error(HTTPStatus.NOT_FOUND)
        target = target.resolve()
        if not target.exists() or not target.is_file():
            return self.send_error(HTTPStatus.NOT_FOUND)
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", mimetypes.guess_type(str(target))[0] or "application/octet-stream")
        self.send_header("Content-Length", str(target.stat().st_size))
        self.end_headers()

    def do_POST(self):
        try:
            payload = self.read_json()
            path = urlparse(self.path).path
            if path == "/api/button/simulate":
                event = str(payload.get("event", "TAP")).upper()
                button = int(payload.get("button", 0))
                if event not in EVENTS or button < 1 or button > BUTTON_COUNT:
                    return self.send_json({"error": "bad button or event"}, HTTPStatus.BAD_REQUEST)
                response = socket_command(f"simule-button {event}{button}")
                return self.send_json({"ok": response == "ok", "response": response})
            if path == "/api/control":
                command = str(payload.get("command", ""))
                allowed = {"ping", "stop-control", "start-control", "reload-config", "load-last-page"}
                if command not in allowed:
                    return self.send_json({"error": "command not allowed"}, HTTPStatus.BAD_REQUEST)
                response = socket_command(command, timeout=5.0)
                return self.send_json({"ok": response == "ok", "response": response})
        except Exception as exc:
            return self.send_json({"error": str(exc)}, HTTPStatus.INTERNAL_SERVER_ERROR)
        return self.send_error(HTTPStatus.NOT_FOUND)

    def do_PUT(self):
        try:
            path = urlparse(self.path).path
            payload = self.read_json()
            if path == "/api/config":
                raw = payload.get("raw")
                if not isinstance(raw, str):
                    return self.send_json({"error": "raw must be a string"}, HTTPStatus.BAD_REQUEST)
                if yaml is not None:
                    yaml.safe_load(raw)
                write_text_atomic(CONFIG_PATH, raw)
                if payload.get("reload", True):
                    try:
                        socket_command("reload-config", timeout=5.0)
                    except OSError:
                        pass
                return self.send_json({"ok": True, "signature": config_signature()})
            if path == "/api/button":
                page = str(payload.get("page", ""))
                position = int(payload.get("position", 0))
                if not page or position < 1 or position > BUTTON_COUNT:
                    return self.send_json({"error": "bad page or position"}, HTTPStatus.BAD_REQUEST)
                button = update_button(page, position, payload)
                return self.send_json({"ok": True, "button": button, "signature": config_signature()})
        except Exception as exc:
            return self.send_json({"error": str(exc)}, HTTPStatus.INTERNAL_SERVER_ERROR)
        return self.send_error(HTTPStatus.NOT_FOUND)

    def serve_file(self, path, cache=True, delete_after=False):
        path = Path(path).resolve()
        if not path.exists() or not path.is_file():
            return self.send_error(HTTPStatus.NOT_FOUND)
        content_type = mimetypes.guess_type(str(path))[0] or "application/octet-stream"
        data = path.read_bytes()
        if delete_after:
            try:
                path.unlink()
            except OSError:
                pass
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "public, max-age=3600" if cache else "no-store")
        self.end_headers()
        self.wfile.write(data)

    def serve_events(self):
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        last_sig = ""
        last_page = ""
        last_offset = -1
        while True:
            try:
                sig = config_signature()
                page = current_page()
                offset = current_offset()
                
                # Only send update if something actually changed
                if sig != last_sig or page != last_page or offset != last_offset:
                    last_sig = sig
                    last_page = page
                    last_offset = offset
                    data = json.dumps({"signature": sig, "page": page, "offset": offset})
                    self.wfile.write(f"event: update\ndata: {data}\n\n".encode("utf-8"))
                    self.wfile.flush()
                time.sleep(0.3)  # Slightly faster polling for better responsiveness
            except (BrokenPipeError, ConnectionResetError):
                break


def main():
    parser = argparse.ArgumentParser(description="GoofyDeck web GUI")
    parser.add_argument("--host", default=os.environ.get("GOOFYDECK_WEB_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("GOOFYDECK_WEB_PORT", "8765")))
    args = parser.parse_args()
    os.chdir(ROOT)
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"GoofyDeck web GUI: http://{args.host}:{args.port}")
    print(f"Config: {CONFIG_PATH}")
    print(f"Paging socket: {PAGING_SOCKET}")
    server.serve_forever()


if __name__ == "__main__":
    main()
