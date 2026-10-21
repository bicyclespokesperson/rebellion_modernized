#!/usr/bin/env -S uv run
"""Serve a local, persistent QA review of encyclopedia source/cutout pairs."""
# /// script
# dependencies = []
# ///

import argparse
import hashlib
import json
import random
import re
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[1]
WEB = Path(__file__).with_name("encyclopedia_review")
SCOPES = {
    "focus": {"characters", "ships", "fighters"},
    "all": {"characters", "ships", "fighters", "facilities", "troops", "planets"},
    **{kind: {kind} for kind in ("characters", "ships", "fighters", "facilities", "troops", "planets", "missions")},
}
GRADES = {"correct", "missing", "extra", "both", None}


def catalog(cache: Path) -> list[dict[str, Any]]:
    items = []
    for entry in json.loads((cache / "inventory.json").read_text()):
        identity = entry["file"]
        if not re.fullmatch(r"EDATA\.\d{3}", identity):
            raise ValueError(f"Invalid inventory identity: {identity}")
        paths = [cache / identity / name for name in ("source.png", "cutout.png")]
        if not all(path.is_file() for path in paths) or entry.get("override", {}).get("skip"):
            continue
        items.append({
            "id": identity, "label": entry["label"], "category": entry["category"],
            "status": entry["status"], "size": entry["size"],
            "hashes": [hashlib.sha256(path.read_bytes()).hexdigest() for path in paths],
        })
    return items


class ReviewStore:
    def __init__(self, cache: Path, output: Path) -> None:
        self.cache = cache
        self.items = catalog(cache)
        self.by_id = {item["id"]: item for item in self.items}
        self.dataset = hashlib.sha256(json.dumps(self.items, sort_keys=True).encode()).hexdigest()
        output.mkdir(parents=True, exist_ok=True)
        self.path = output / f"review-{self.dataset[:16]}.json"
        self.lock = threading.Lock()
        self.state = json.loads(self.path.read_text()) if self.path.exists() else {
            "schema": "rebellion-encyclopedia-review-v1", "dataset": self.dataset,
            "items": self.items, "sessions": {},
        }
        if self.state["dataset"] != self.dataset:
            raise ValueError("Review dataset fingerprint mismatch")

    def save(self) -> None:
        temporary = self.path.with_suffix(".tmp")
        temporary.write_text(json.dumps(self.state, indent=2) + "\n")
        temporary.replace(self.path)

    def session(self, scope: str) -> dict[str, Any]:
        if scope not in SCOPES:
            raise ValueError("Unknown review scope")
        if scope not in self.state["sessions"]:
            order = [item["id"] for item in self.items if item["category"] in SCOPES[scope]]
            random.SystemRandom().shuffle(order)
            self.state["sessions"][scope] = {"order": order, "grades": {}}
            try:
                self.save()
            except OSError:
                del self.state["sessions"][scope]
                raise
        session = self.state["sessions"][scope]
        return {"dataset": self.dataset, "scope": scope, "save_path": str(self.path),
                "items": [self.by_id[identity] for identity in session["order"]],
                "grades": session["grades"]}


def handler_for(store: ReviewStore) -> type[BaseHTTPRequestHandler]:
    class Handler(BaseHTTPRequestHandler):
        def respond(self, payload: bytes, content_type: str, status: int = 200) -> None:
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(payload)

        def json_response(self, data: Any, status: int = 200) -> None:
            self.respond(json.dumps(data).encode(), "application/json", status)

        def do_GET(self) -> None:
            url = urlsplit(self.path)
            try:
                if url.path == "/api/review":
                    scope = parse_qs(url.query).get("scope", ["focus"])[0]
                    with store.lock:
                        self.json_response(store.session(scope))
                elif url.path == "/api/export":
                    with store.lock:
                        self.json_response(store.state)
                elif match := re.fullmatch(r"/images/(EDATA\.\d{3})/(source|cutout)\.png", url.path):
                    if match[1] not in store.by_id:
                        self.send_error(404)
                        return
                    self.respond((store.cache / match[1] / f"{match[2]}.png").read_bytes(), "image/png")
                elif url.path in ("/", "/app.js", "/style.css"):
                    name = "index.html" if url.path == "/" else url.path[1:]
                    mime = {"index.html": "text/html; charset=utf-8", "app.js": "text/javascript", "style.css": "text/css"}[name]
                    self.respond((WEB / name).read_bytes(), mime)
                else:
                    self.send_error(404)
            except (ValueError, KeyError) as error:
                self.json_response({"error": str(error)}, 400)
            except OSError:
                self.json_response({"error": "Could not read images or save review"}, 500)

        def do_POST(self) -> None:
            if self.path != "/api/grade":
                self.send_error(404)
                return
            # Browser writes must come from this same local origin.
            if self.headers.get("Origin") != f"http://{self.headers.get('Host')}":
                self.send_error(403)
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 < length < 4096:
                    raise ValueError("Invalid request size")
                data = json.loads(self.rfile.read(length))
                with store.lock:
                    if data["dataset"] != store.dataset:
                        raise ValueError("Dataset changed; reload before grading")
                    session = store.session(data["scope"])
                    identity, grade = data["id"], data["grade"]
                    if identity not in {item["id"] for item in session["items"]} or grade not in GRADES:
                        raise ValueError("Invalid item or grade")
                    grades = store.state["sessions"][data["scope"]]["grades"]
                    previous_grade = grades.get(identity)
                    if grade is None:
                        grades.pop(identity, None)
                    else:
                        grades[identity] = grade
                    try:
                        store.save()
                    except OSError:
                        if previous_grade is None:
                            grades.pop(identity, None)
                        else:
                            grades[identity] = previous_grade
                        raise
                    self.json_response({"saved": True})
            except (ValueError, KeyError, TypeError) as error:
                self.json_response({"error": str(error)}, 400)
            except OSError:
                self.json_response({"error": "Could not save review to disk"}, 500)

    return Handler


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache-dir", type=Path, default=ROOT / "work/hd/encyclopedia")
    parser.add_argument("--out", type=Path, default=ROOT / "work/hd/encyclopedia/reviews")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    store = ReviewStore(args.cache_dir.resolve(), args.out.resolve())
    server = ThreadingHTTPServer(("127.0.0.1", args.port), handler_for(store))
    print(f"Review app: http://127.0.0.1:{server.server_port}", flush=True)
    print(f"Grades: {store.path}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
